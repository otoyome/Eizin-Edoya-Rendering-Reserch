#ifndef __CONTACT_SHADOW_HLSLI__
#define __CONTACT_SHADOW_HLSLI__

#include "gbuffer.hlsli"
#include "shading_models.hlsli"

#include "scene_constant_buffer.hlsli"
#include "sdf_shadow_threshold_mask.hlsli"

//  コンタクトシャドウ
//  スクリーンスペースで光源方向に向かってレイマーチングを行う事で細かい影を算出する。
//  仕組み上ノイズが出やすいので、短めにしたりソフトシャドウ的なアプローチも用いるのがベスト


struct contact_shadow_data
{
    Texture2D<float4> normal_depth_map;
    SamplerState samp_state;
    float3 world_position;
    float3 world_normal;
    float3 march_direction;
    float march_length;
};

float ___inner_calculate_contact_shadow_check___(contact_shadow_data data, float2 texcoord, float projection_depth)
{
    static const float2 depth_range = { 0.00001f, 0.0001f };
    
    float linear_depth = convert_projection_depth_to_linear_depth(projection_depth, z_buffer_parameteres);
    float gbuffer_depth = data.normal_depth_map.Sample(data.samp_state, texcoord).a;
    float diff = linear_depth - gbuffer_depth;
    return  diff > depth_range.x && diff < depth_range.y;
}

float ___inner_calculate_contact_shadow___(contact_shadow_data data, int steps, bool soft, float ray_resolution = 1.0f)
{
    float3 step = data.march_direction * data.march_length / steps;
    if (soft)
    {
        float attenuation = 0;
        float2 rand_uv = 0;
        [loop]
        for (int i = 0; i < steps; ++i)
        {
            data.world_position += step * ((random(rand_uv) * 0.5f + 0.5f) * ray_resolution);

            float4 ndc = mul(float4(data.world_position, 1.0f), view_projection_transform);
            ndc /= ndc.w;
            float2 uv = ndc.xy * float2(0.5f, -0.5f) + float2(0.5f, 0.5f);
            if (saturate(uv.x) != uv.x || saturate(uv.y) != uv.y || saturate(ndc.z) != ndc.z)
                break;
            rand_uv = uv;
            attenuation += ___inner_calculate_contact_shadow_check___(data, uv.xy, ndc.z);
        }
        return 1.0f - attenuation / steps;
    }
    
    [loop]
    for (int i = 0; i < steps; ++i)
    {
        data.world_position += step;

        float4 ndc = mul(float4(data.world_position, 1.0f), view_projection_transform);
        ndc /= ndc.w;
        float2 uv = ndc.xy * float2(0.5f, -0.5f) + float2(0.5f, 0.5f);
        if (saturate(uv.x) != uv.x || saturate(uv.y) != uv.y || saturate(ndc.z) != ndc.z)
            break;

        if (___inner_calculate_contact_shadow_check___(data, uv.xy, ndc.z) >= 0.1f)
            return 0.0f;
    }
    return 1.0f;
}

//  コンタクトシャドウ
float calculate_contact_shadow(in contact_shadow_data data, int steps = 32, bool soft = true)
{
    return ___inner_calculate_contact_shadow___(data, steps, soft);
}

//  江戸谷犬八表現用コンタクトシャドウ
float calculate_contact_shadow_for_edoya(in GBufferData gbuffer_data, in contact_shadow_data data, int steps = 32, bool soft = true)
{
#if 0
    //  ソフトシャドウは強制的に無視
    return ___inner_calculate_contact_shadow___(data, steps, false);
#elif 01
    //  マーチング方向が裏の場合は陰面なので除外
    //if (dot(data.world_normal, data.march_direction) <= 0.0f)
    //    return 1.0f;
    float contanct_shadow = ___inner_calculate_contact_shadow___(data, steps, false);
    float shading_factor = 0;
    if (IsGBufferFlags(gbuffer_data, g_buffer_stencil_face_shadow_on))
    {
        shading_factor = sdf_shadow_threshold_mask(gbuffer_data, -data.march_direction);
    }

    return lerp(contanct_shadow, 1, shading_factor);
#else
    //  一定の角度から減衰させる
    float dot_factor = dot(data.world_normal, data.march_direction);
    dot_factor = saturate(step(-0.3f, dot_factor)); //  ここを制御する仕組みが必要

    if (IsGBufferFlags(gbuffer_data, g_buffer_stencil_face_shadow_on))
    {
        float shading_factor = sdf_shadow_threshold_mask(gbuffer_data, -data.march_direction);
        return dot_factor * shading_factor * ___inner_calculate_contact_shadow___(data, steps, false);
    }

    //  ポスタライズ情報取得
    return dot_factor * ___inner_calculate_contact_shadow___(data, steps, false);
    //float attenuation = dot_factor * ___inner_calculate_contact_shadow___(data, steps, false);
    //float posterize_step = UnpackFloatToFloat2(gbuffer_data.unique_parameter.z).y * 255.0f;
    //return posterize(attenuation, posterize_step);
#endif
}

//  コンタクトシャドウ
//  スクリーンスペースで細かい遮蔽チェックを行い影を算出する。
float integrate_contact_shadow(in GBufferData gbuffer_data, in contact_shadow_data cs_data, int steps = 32, bool soft = true)
{
    switch (gbuffer_data.shading_model)
    {
        case shading_model_unlit:       return 1.0f;
        case shading_model_pbr:         return calculate_contact_shadow(cs_data, steps, soft);
        case shading_model_pmx_toon:    return calculate_contact_shadow(cs_data, steps, soft);
        case shading_model_eizin:       return calculate_contact_shadow_for_edoya(gbuffer_data, cs_data, steps, soft);
    }

    return 1.0f;
}
#endif  //  __CONTACT_SHADOW_HLSLI__
