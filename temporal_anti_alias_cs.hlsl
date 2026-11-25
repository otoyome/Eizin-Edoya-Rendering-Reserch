#include "scene_constant_buffer.hlsli"
#include "gbuffer.hlsli"

//  TAA (Temporal Anti-Aliasing)
//  Ported from Lighting-Sample (temporal_anti_alias_upscaling_cs.hlsl, native resolution only).
//  Differences from the original:
//   - Velocity is read from this project's G-Buffer (packed in GB_Velocity.r, NDC delta * 0.5)
//   - Depth is the linear depth in GB_Normal_Depth.a (smaller = closer)
//   - Jitter is undone with the full sub-pixel offset (taa_jitter is the UV shift applied to the projection)
//   - Runs after the final pass (input is the gamma-encoded Eizin filter output)

RWTexture2D<float4> current_uav : register(u0);
RWTexture2D<float4> history_uav : register(u1);

Texture2D current_color_map : register(t0);
Texture2D accumlate_color_map : register(t1);
Texture2D velocity_map : register(t2);
Texture2D normal_depth_map : register(t3);

SamplerState taa_point_sampler : register(s0);
SamplerState taa_linear_sampler : register(s1);

static const float TAA_FLT_MIN = 0.00000001f;
static const float TAA_FLT_MAX_16U = 65504.0f;

cbuffer TEMPORAL_ANTI_ALIAS_CONSTANT_BUFFER : register(b3)
{
    uint taa_reset;
    uint taa_history_display;
    float2 taa_jitter;              //  Current frame jitter (UV)

    float2 taa_previous_jitter;     //  Previous frame jitter (UV)
    float taa_blend_min;            //  History blend weight of the current frame (static pixels)
    float taa_blend_max;            //  History blend weight of the current frame (moving / high contrast pixels)
};

float3 convert_rgb_to_ycocg(float3 rgb)
{
    const float y = dot(rgb, float3(0.25f, 0.5f, 0.25f));
    const float co = dot(rgb, float3(0.5f, 0.0f, -0.5f));
    const float cg = dot(rgb, float3(-0.25f, 0.5f, -0.25f));
    return float3(y, co, cg);
}

float3 convert_ycocg_to_rgb(float3 ycocg)
{
    const float r = dot(ycocg, float3(1.0f, 1.0f, -1.0f));
    const float g = dot(ycocg, float3(1.0f, 0.0f, 1.0f));
    const float b = dot(ycocg, float3(1.0f, -1.0f, -1.0f));
    return float3(r, g, b);
}

float3 saturate_16(float3 x)
{
    return clamp(x, 0.0f, TAA_FLT_MAX_16U);
}

//  Reversible tonemap so bright pixels do not dominate the filter
float3 tonemap_for_taa(float3 c)
{
    float l = max(c.r, max(c.g, c.b));
    return c * rcp(1.0f + l);
}

float3 tonemap_for_taa_inv(float3 c)
{
    float l = max(c.r, max(c.g, c.b));
    return c * rcp(max(1.0f - l, 1e-3f));
}

float reconstruct_weight(float d_sq)
{
    return exp(-2.29f * d_sq);
}

float3 clip_aabb(float3 aabb_min, float3 aabb_max, float3 p, float3 q)
{
    float3 r = q - p;
    float3 rmax = aabb_max - p;
    float3 rmin = aabb_min - p;

    const float eps = TAA_FLT_MIN;
    if (r.x > rmax.x + eps) r *= (rmax.x / r.x);
    if (r.y > rmax.y + eps) r *= (rmax.y / r.y);
    if (r.z > rmax.z + eps) r *= (rmax.z / r.z);
    if (r.x < rmin.x - eps) r *= (rmin.x / r.x);
    if (r.y < rmin.y - eps) r *= (rmin.y / r.y);
    if (r.z < rmin.z - eps) r *= (rmin.z / r.z);

    return p + r;
}

//  5-tap Catmull-Rom history sampling
float3 sample_history_catmull_rom(float2 uv, float2 resolution)
{
    float2 sample_position = uv * resolution;
    float2 tex_pos_1 = floor(sample_position - 0.5f) + 0.5f;
    float2 f = sample_position - tex_pos_1;

    float2 w0 = f * (-0.5f + f * (1.0f - 0.5f * f));
    float2 w1 = 1.0f + f * f * (-2.5f + 1.5f * f);
    float2 w2 = f * (0.5f + f * (2.0f - 1.5f * f));
    float2 w3 = f * f * (-0.5f + 0.5f * f);

    float2 w12 = w1 + w2;
    float2 offset_12 = w2 / w12;

    float2 inv_res = 1.0f / resolution;
    float2 tex_pos_0 = (tex_pos_1 - 1.0f) * inv_res;
    float2 tex_pos_3 = (tex_pos_1 + 2.0f) * inv_res;
    float2 tex_pos_12 = (tex_pos_1 + offset_12) * inv_res;

    float3 h0 = accumlate_color_map.SampleLevel(taa_linear_sampler, float2(tex_pos_12.x, tex_pos_0.y), 0.0f).rgb;
    float3 h1 = accumlate_color_map.SampleLevel(taa_linear_sampler, float2(tex_pos_0.x, tex_pos_12.y), 0.0f).rgb;
    float3 h2 = accumlate_color_map.SampleLevel(taa_linear_sampler, float2(tex_pos_12.x, tex_pos_12.y), 0.0f).rgb;
    float3 h3 = accumlate_color_map.SampleLevel(taa_linear_sampler, float2(tex_pos_3.x, tex_pos_12.y), 0.0f).rgb;
    float3 h4 = accumlate_color_map.SampleLevel(taa_linear_sampler, float2(tex_pos_12.x, tex_pos_3.y), 0.0f).rgb;

    float3 result = 0.0f.xxx;
    result += h0 * w12.x * w0.y;
    result += h1 * w0.x * w12.y;
    result += h2 * w12.x * w12.y;
    result += h3 * w3.x * w12.y;
    result += h4 * w12.x * w3.y;

    float weight_sum = w12.x + w12.y - w12.x * w12.y;
    float3 history_min = min(min(min(h0, h1), min(h2, h3)), h4);
    float3 history_max = max(max(max(h0, h1), max(h2, h3)), h4);
    return clamp(result * rcp(weight_sum), history_min, history_max);
}

//  UV-space motion (previous uv - current uv) without the jitter difference
float2 load_velocity_uv(int2 position)
{
    float2 ndc_delta_half = UnpackFloatToFloat2(velocity_map[position].r * 2.0f - 1.0f);    //  (current - previous) NDC * 0.5
    float2 velocity_uv = float2(-ndc_delta_half.x, ndc_delta_half.y);
    return velocity_uv + (taa_jitter - taa_previous_jitter);
}

float3 temporal_anti_alias(uint2 px_out, float2 resolution)
{
    float2 uv_out = (px_out + 0.5f) / resolution;
    int2 px_max = max(int2(resolution) - 1, int2(0, 0));

    //  The rendered image is shifted by +jitter, so sample it at the jittered position
    float2 p_render = uv_out * resolution + taa_jitter * resolution;
    p_render = clamp(p_render, float2(0.5f, 0.5f), resolution - 0.5f);
    int2 center = clamp(int2(floor(p_render)), int2(0, 0), px_max);

    //  Current frame reconstruction and neighborhood statistics
    float3 m1 = 0.0f.xxx;
    float3 m2 = 0.0f.xxx;
    float3 cmin = TAA_FLT_MAX_16U.xxx;
    float3 cmax = -TAA_FLT_MAX_16U.xxx;
    float3 current_rgb_tm = 0.0f.xxx;
    float weight_sum = 0.0f;
    float count = 0.0f;

    float2 d_base = (float2(center) + 0.5f) - p_render;
    float3 wx = float3(reconstruct_weight((d_base.x - 1.0f) * (d_base.x - 1.0f)), reconstruct_weight(d_base.x * d_base.x), reconstruct_weight((d_base.x + 1.0f) * (d_base.x + 1.0f)));
    float3 wy = float3(reconstruct_weight((d_base.y - 1.0f) * (d_base.y - 1.0f)), reconstruct_weight(d_base.y * d_base.y), reconstruct_weight((d_base.y + 1.0f) * (d_base.y + 1.0f)));

    float closest_depth = normal_depth_map[center].a;
    int2 closest_pos = center;

    [unroll]
    for (int dy = -1; dy <= 1; ++dy)
    {
        [unroll]
        for (int dx = -1; dx <= 1; ++dx)
        {
            int2 tap = center + int2(dx, dy);
            if (any(tap < 0) || any(tap > px_max))
            {
                continue;
            }

            float3 s_rgb_raw = current_color_map[tap].rgb;
            float3 s_rgb = isnan(s_rgb_raw.x + s_rgb_raw.y + s_rgb_raw.z) ? 0.0f.xxx : max(s_rgb_raw, 0.0f.xxx);
            float3 s_rgb_tm = tonemap_for_taa(s_rgb);
            float3 s_ycocg_tm = convert_rgb_to_ycocg(s_rgb_tm);

            m1 += s_ycocg_tm;
            m2 += s_ycocg_tm * s_ycocg_tm;
            cmin = min(cmin, s_ycocg_tm);
            cmax = max(cmax, s_ycocg_tm);
            count += 1.0f;

            float w = wx[dx + 1] * wy[dy + 1];
            current_rgb_tm += s_rgb_tm * w;
            weight_sum += w;

            //  Closest depth in the 3x3 neighborhood (linear depth: smaller is closer)
            if (dx != 0 || dy != 0)
            {
                float z = normal_depth_map[tap].a;
                if (z < closest_depth)
                {
                    closest_depth = z;
                    closest_pos = tap;
                }
            }
        }
    }

    current_rgb_tm = current_rgb_tm * rcp(weight_sum);
    float3 current_ycocg_tm = convert_rgb_to_ycocg(current_rgb_tm);

    //  History reprojection
    float2 velocity_uv = load_velocity_uv(closest_pos);
    float2 uv_prev = uv_out + velocity_uv;

    float2 inset = 1.5f / resolution;
    bool uv_prev_valid = all(uv_prev > inset) && all(uv_prev < 1.0f - inset);
    bool history_invalid = taa_reset || !uv_prev_valid;
    if (history_invalid)
    {
        return saturate_16(max(tonemap_for_taa_inv(current_rgb_tm), 0.0f.xxx));
    }

    float3 history_rgb = sample_history_catmull_rom(uv_prev, resolution);
    float3 history_rgb_tm = tonemap_for_taa(history_rgb);
    float3 history_ycocg_tm = convert_rgb_to_ycocg(history_rgb_tm);

    //  History clipping (variance clipping in YCoCg)
    float inv_count = rcp(count);
    float3 mean = m1 * inv_count;
    float3 sigma = sqrt(max(m2 * inv_count - mean * mean, 0.0f.xxx));
    sigma = max(sigma, 0.005f);
    float motion = saturate(length(velocity_uv * resolution) * (1.0f / 64.0f));
    float contrast = saturate((cmax.x - cmin.x) * 4.0f);
    float gamma = lerp(1.5f, 1.0f, motion);
    gamma = lerp(gamma, 0.75f, contrast);
    float3 aabb_min = max(mean - sigma * gamma, cmin);
    float3 aabb_max = min(mean + sigma * gamma, cmax);
    float3 history_c = clip_aabb(aabb_min, aabb_max, mean, history_ycocg_tm);

    //  Accumulation
    float blend = lerp(taa_blend_min, taa_blend_max, max(motion, contrast));

    float3 result_ycocg_tm = lerp(history_c, current_ycocg_tm, blend);
    if (taa_history_display)
        result_ycocg_tm = blend.rrr;
    float3 result_rgb_tm = max(convert_ycocg_to_rgb(result_ycocg_tm), 0.0f.xxx);

    float l_tm = max(result_rgb_tm.r, max(result_rgb_tm.g, result_rgb_tm.b));
    float l_tm_safe = min(l_tm, 1.0f - 1e-3f);
    result_rgb_tm *= (l_tm > 0.0f) ? (l_tm_safe / l_tm) : 1.0f;
    float3 result_rgb = result_rgb_tm * rcp(1.0f - l_tm_safe);
    return saturate_16(result_rgb);
}

[numthreads(8, 8, 1)]
void main(uint3 thread_id : SV_DispatchThreadID)
{
    float2 resolution;
    current_color_map.GetDimensions(resolution.x, resolution.y);
    if (any(thread_id.xy >= uint2(resolution)))
    {
        return;
    }

    float4 color = float4(temporal_anti_alias(thread_id.xy, resolution), 1.0f);
    current_uav[thread_id.xy] = color;
    history_uav[thread_id.xy] = color;
}
