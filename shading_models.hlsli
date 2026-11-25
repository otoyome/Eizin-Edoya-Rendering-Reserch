#ifndef __SHADING_MODEL_HLSLI__
#define __SHADING_MODEL_HLSLI__

#include "gbuffer.hlsli"
#include "shading_functions.hlsli"
#include "sdf_shadow_threshold_mask.hlsli"

struct DirectLighting
{
    float3 diffuse;
    float3 specular;
};

struct OcclusionResult
{
    float3 diffuse;
    float3 specular;
};

struct LightingData
{
    float3 direction;               //  ライト方向
    float shading_attenuation;      //  シェーディングによる減衰値
    float3 color;                   //  ライトカラー
    float shadowing_attenuation;    //  シャドウイングによる減衰値
    float3 camera_position;         //  視点
    float dummy;
};

DirectLighting integrate_bxdf(in GBufferData gbuffer_data, in LightingData lighting_data)
{
    DirectLighting result = (DirectLighting)0;
    switch (gbuffer_data.shading_model)
    {
        case shading_model_unlit:
        {
            //  ライティングは行わないので何もしない
            break;
        }
        case shading_model_pbr:
        {
            float3 albedo = gbuffer_data.base_color;
            float3 normal = normalize(gbuffer_data.w_normal);
            float3 view_vector = normalize(gbuffer_data.w_position.xyz - lighting_data.camera_position.xyz);
            float3 diffuse_reflectance = lerp(albedo.rgb, 0.0f, gbuffer_data.metalness);
            float3 fresnel0 = lerp(0.04f, albedo.rgb, gbuffer_data.metalness);
            DirectBDRF(diffuse_reflectance, fresnel0, normal, view_vector, lighting_data.direction,
                       lighting_data.color, gbuffer_data.roughness, result.diffuse, result.specular);
            result.diffuse *= lighting_data.shading_attenuation * lighting_data.shadowing_attenuation;
            result.specular *= lighting_data.shading_attenuation * lighting_data.shadowing_attenuation;

	        //	自己遮蔽
            result.diffuse = lerp(result.diffuse, result.diffuse * gbuffer_data.occlusion_factor, gbuffer_data.occlusion_strength);
            result.specular = lerp(result.specular, result.specular * gbuffer_data.occlusion_factor, gbuffer_data.occlusion_strength);
            break;
        }
        case shading_model_pmx_toon:
        {
            float3 shade_color = gbuffer_data.shade_color;
            float3 normal = normalize(gbuffer_data.w_normal);
            float3 lighting_direction = -normalize(lighting_data.direction);
            float shading_factor = dot(normal, lighting_direction) > 0.7071f ? 1 : 0;
            float3 color = lerp(shade_color, gbuffer_data.base_color, shading_factor * lighting_data.shadowing_attenuation);

            result.diffuse = color * lighting_data.color;
            result.specular = 0;

	        //	自己遮蔽
            result.diffuse = lerp(result.diffuse, result.diffuse * gbuffer_data.occlusion_factor, gbuffer_data.occlusion_strength);
            result.specular = lerp(result.specular, result.specular * gbuffer_data.occlusion_factor, gbuffer_data.occlusion_strength);
            break;
        }
        case shading_model_eizin:
        {
            float3 shade_color = gbuffer_data.shade_color;
            float3 normal = normalize(gbuffer_data.w_normal);
            float3 lighting_direction = -normalize(lighting_data.direction);
            float3 color;

            //  XZ平面ライティング
            if (IsGBufferFlags(gbuffer_data, g_buffer_stencil_xz_plane_lighting))
            {
                static const float max_sin = sin(15.0 * (3.14159265f / 180.0f));   //  sin(30°) : 仰角の上限

                float3 L = lighting_direction;
                float  horizontal = length(L.xz);
                if (horizontal < 1e-4f)
                {
                    //  真上/真下からの光 : 水平方向が決まらないので顔の正面方向を使う
                    L.xz = gbuffer_data.mesh_direction.zw;
                    horizontal = 1.0f;
                }
                L.y  = clamp(L.y, -max_sin, max_sin);
                L.xz = L.xz / horizontal * sqrt(1.0f - L.y * L.y);

                float shading_factor = posterize((dot(normal, L) * 0.5f + 0.5f) * lighting_data.shading_attenuation, gbuffer_data.occlusion_factor * 255.0f);
                color = lerp(shade_color, gbuffer_data.base_color, shading_factor);
            }
            else
            {
                //  シェーディングによる元色を算出
                float shading_factor = posterize((dot(normal, lighting_direction) * 0.5f + 0.5f) * lighting_data.shading_attenuation, gbuffer_data.occlusion_factor * 255.0f);
                color = lerp(shade_color, gbuffer_data.base_color, shading_factor);
            }

            //  拡散反射のみを利用
            result.diffuse = color * lighting_data.color * lighting_data.shadowing_attenuation;

            //  鏡面反射は使用上あまりしたくない(海の時に使いたい気持ちはあるので、海専用処理として実装は有)
            result.specular = 0;
            break;
        }
    }

    return result;
}

OcclusionResult integrate_occlusion(in GBufferData gbuffer_data, DirectLighting lighting_result)
{
    OcclusionResult result;
    result.diffuse = lighting_result.diffuse;
    result.specular = lighting_result.specular;
    switch (gbuffer_data.shading_model)
    {
        case shading_model_pbr:
        case shading_model_pmx_toon:
        {
	        //	自己遮蔽
            result.diffuse = lerp(lighting_result.diffuse, lighting_result.diffuse * gbuffer_data.occlusion_factor, gbuffer_data.occlusion_strength);
            result.specular = lerp(lighting_result.specular, lighting_result.specular * gbuffer_data.occlusion_factor, gbuffer_data.occlusion_strength);
            break;
        }
    }

    return result;
}

float4 get_shading_model_color(in GBufferData gbuffer_data)
{
    float4 result = (float4) 0;
    switch (gbuffer_data.shading_model)
    {
        case shading_model_unlit:       return float4(0.0f, 0.0f, 0.0f, 1);
        case shading_model_pbr:         return float4(0.5f, 0.5f, 0.5f, 1);
        case shading_model_pmx_toon:    return float4(0.5f, 0.5f, 1.0f, 1);
        case shading_model_eizin:       return float4(0.5f, 1.0f, 1.0f, 1);
    }

    return result;
}

#endif // __SHADING_MODEL_HLSLI__
