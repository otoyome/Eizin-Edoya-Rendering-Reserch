#include "sprite.hlsli"

//  FXAA (Fast Approximate Anti-Aliasing)
//  Algorithm based on Timothy Lottes' FXAA 3.11 (quality preset).
//  Input is the Eizin filter output (gamma-encoded LDR color), applied after the final pass.

Texture2D scene_map : register(t0);
SamplerState linear_clamp_sampler_state : register(s4);

cbuffer FXAA_CONSTANT_BUFFER : register(b12)
{
    float fxaa_subpixel_quality;        //  Amount of sub-pixel aliasing removal (0 = off, 1 = soft)
    float fxaa_edge_threshold;          //  Minimum local contrast required to apply the algorithm
    float fxaa_edge_threshold_min;      //  Trims the algorithm from processing darks
    float fxaa_dummy;
};

static const int FXAA_SEARCH_STEPS = 12;
static const float FXAA_SEARCH_STEP_SCALE[FXAA_SEARCH_STEPS] = { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.5f, 2.0f, 2.0f, 2.0f, 2.0f, 4.0f, 8.0f };

float fxaa_luma(float3 color)
{
    return dot(saturate(color), float3(0.299f, 0.587f, 0.114f));
}

float fxaa_luma_at(float2 uv)
{
    return fxaa_luma(scene_map.SampleLevel(linear_clamp_sampler_state, uv, 0).rgb);
}

float fxaa_luma_offset(float2 uv, int2 offset)
{
    return fxaa_luma(scene_map.SampleLevel(linear_clamp_sampler_state, uv, 0, offset).rgb);
}

float4 main(VS_OUT pin) : SV_TARGET
{
    float2 map_size;
    scene_map.GetDimensions(map_size.x, map_size.y);
    const float2 rcp_frame = 1.0f / map_size;
    const float2 uv = pin.texcoord;

    float4 center_color = scene_map.SampleLevel(linear_clamp_sampler_state, uv, 0);
    float luma_center = fxaa_luma(center_color.rgb);
    float luma_n = fxaa_luma_offset(uv, int2( 0, -1));
    float luma_s = fxaa_luma_offset(uv, int2( 0, +1));
    float luma_e = fxaa_luma_offset(uv, int2(+1,  0));
    float luma_w = fxaa_luma_offset(uv, int2(-1,  0));

    float luma_max = max(luma_center, max(max(luma_n, luma_s), max(luma_e, luma_w)));
    float luma_min = min(luma_center, min(min(luma_n, luma_s), min(luma_e, luma_w)));
    float luma_range = luma_max - luma_min;

    //  Low contrast area: no anti-aliasing
    if (luma_range < max(fxaa_edge_threshold_min, luma_max * fxaa_edge_threshold))
    {
        return center_color;
    }

    float luma_nw = fxaa_luma_offset(uv, int2(-1, -1));
    float luma_ne = fxaa_luma_offset(uv, int2(+1, -1));
    float luma_sw = fxaa_luma_offset(uv, int2(-1, +1));
    float luma_se = fxaa_luma_offset(uv, int2(+1, +1));

    float luma_ns = luma_n + luma_s;
    float luma_we = luma_w + luma_e;
    float luma_west_corners = luma_nw + luma_sw;
    float luma_east_corners = luma_ne + luma_se;
    float luma_north_corners = luma_nw + luma_ne;
    float luma_south_corners = luma_sw + luma_se;

    //  Edge direction estimation
    float edge_horizontal = abs(-2.0f * luma_w + luma_west_corners)
                          + abs(-2.0f * luma_center + luma_ns) * 2.0f
                          + abs(-2.0f * luma_e + luma_east_corners);
    float edge_vertical   = abs(-2.0f * luma_n + luma_north_corners)
                          + abs(-2.0f * luma_center + luma_we) * 2.0f
                          + abs(-2.0f * luma_s + luma_south_corners);
    bool is_horizontal = edge_horizontal >= edge_vertical;

    //  Choose the side of the edge with the steepest gradient
    float luma_negative = is_horizontal ? luma_n : luma_w;
    float luma_positive = is_horizontal ? luma_s : luma_e;
    float gradient_negative = abs(luma_negative - luma_center);
    float gradient_positive = abs(luma_positive - luma_center);
    float step_length = is_horizontal ? rcp_frame.y : rcp_frame.x;

    float luma_local_average;
    float gradient_scaled;
    if (gradient_negative >= gradient_positive)
    {
        step_length = -step_length;
        luma_local_average = 0.5f * (luma_negative + luma_center);
        gradient_scaled = 0.25f * gradient_negative;
    }
    else
    {
        luma_local_average = 0.5f * (luma_positive + luma_center);
        gradient_scaled = 0.25f * gradient_positive;
    }

    //  Move half a pixel onto the edge
    float2 edge_uv = uv;
    if (is_horizontal)
        edge_uv.y += step_length * 0.5f;
    else
        edge_uv.x += step_length * 0.5f;

    //  Search along the edge in both directions
    float2 search_step = is_horizontal ? float2(rcp_frame.x, 0.0f) : float2(0.0f, rcp_frame.y);
    float2 uv_negative = edge_uv - search_step;
    float2 uv_positive = edge_uv + search_step;
    float luma_end_negative = fxaa_luma_at(uv_negative) - luma_local_average;
    float luma_end_positive = fxaa_luma_at(uv_positive) - luma_local_average;
    bool reached_negative = abs(luma_end_negative) >= gradient_scaled;
    bool reached_positive = abs(luma_end_positive) >= gradient_scaled;

    [loop]
    for (int i = 1; i < FXAA_SEARCH_STEPS && !(reached_negative && reached_positive); ++i)
    {
        if (!reached_negative)
        {
            uv_negative -= search_step * FXAA_SEARCH_STEP_SCALE[i];
            luma_end_negative = fxaa_luma_at(uv_negative) - luma_local_average;
            reached_negative = abs(luma_end_negative) >= gradient_scaled;
        }
        if (!reached_positive)
        {
            uv_positive += search_step * FXAA_SEARCH_STEP_SCALE[i];
            luma_end_positive = fxaa_luma_at(uv_positive) - luma_local_average;
            reached_positive = abs(luma_end_positive) >= gradient_scaled;
        }
    }

    //  Distance to both ends of the edge
    float distance_negative = is_horizontal ? (uv.x - uv_negative.x) : (uv.y - uv_negative.y);
    float distance_positive = is_horizontal ? (uv_positive.x - uv.x) : (uv_positive.y - uv.y);
    bool is_negative_closer = distance_negative < distance_positive;
    float distance_final = min(distance_negative, distance_positive);
    float edge_length = distance_negative + distance_positive;

    //  Only blend when the luma variation at the closer end is coherent with the center
    bool is_luma_center_smaller = (luma_center - luma_local_average) < 0.0f;
    bool correct_variation = ((is_negative_closer ? luma_end_negative : luma_end_positive) < 0.0f) != is_luma_center_smaller;
    float pixel_offset = correct_variation ? (-distance_final / edge_length + 0.5f) : 0.0f;

    //  Sub-pixel anti-aliasing
    float luma_average = (1.0f / 12.0f) * (2.0f * (luma_ns + luma_we) + luma_west_corners + luma_east_corners);
    float subpixel_offset1 = saturate(abs(luma_average - luma_center) / luma_range);
    float subpixel_offset2 = (-2.0f * subpixel_offset1 + 3.0f) * subpixel_offset1 * subpixel_offset1;
    float subpixel_offset = subpixel_offset2 * subpixel_offset2 * fxaa_subpixel_quality;
    pixel_offset = max(pixel_offset, subpixel_offset);

    float2 final_uv = uv;
    if (is_horizontal)
        final_uv.y += pixel_offset * step_length;
    else
        final_uv.x += pixel_offset * step_length;

    float4 result = scene_map.SampleLevel(linear_clamp_sampler_state, final_uv, 0);
    result.a = center_color.a;
    return result;
}
