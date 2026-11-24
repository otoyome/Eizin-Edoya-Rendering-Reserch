#include "deferred_rendering.hlsli"

//  シャドウマップ用テクスチャ
Texture2D shadow_map : register(t10);
SamplerState shadow_sampler_state : register(s10);

float4 main(VS_OUT pin) : SV_TARGET
{
    //  GBufferテクスチャから情報をデコードする
    GBufferData data = get_gbuffer_data(pin.texcoord);

    float3 total_diffuse = (float3) 0, total_specular = (float3) 0;
    if (data.shading_model != shading_model_unlit)
    {
        directional_lights directional_light = convert_directional_lights();
        float3 L = normalize(directional_light.direction.xyz);

        float shading_attenuation = 1;
        float shadowing_attenuation = 1;
        if (use_shadow)
        {
	        //  シャドウマップ用のパラメーター計算
            float3 shadow_texcoord;
	        {
		        // ライトから見たNDC座標を算出
                float4 wvpPos = mul(float4(data.w_position.xyz, 1.0f), light_view_projection);
		        // NDC座標からUV座標を算出する
                wvpPos /= wvpPos.w;
                wvpPos.y = -wvpPos.y;
                wvpPos.xy = 0.5f * wvpPos.xy + 0.5f;
                shadow_texcoord = wvpPos.xyz;
            }
		    //	平行光源用シャドウマップ
            float depth = shadow_map.Sample(shadow_sampler_state, shadow_texcoord.xy).r;
		    //	深度値を比較して影かどうかを判定する
            if (shadow_texcoord.z - depth > shadow_bias)
            {
                shadowing_attenuation = shadow_attenuation;
            }
            else if (contact_shadow > 0)
            {
                //  平行光源でやるもんじゃない
                contact_shadow_data shadow_data;
                shadow_data.normal_depth_map = gbuffer_normal_depth;
                shadow_data.samp_state = sampler_states[POINT];
                shadow_data.world_position = data.w_position;
                shadow_data.world_normal = data.w_normal;
                shadow_data.march_direction = -L;
                shadow_data.march_length = contact_shadow;
                shadowing_attenuation = lerp(shadow_attenuation, 1, integrate_contact_shadow(data, shadow_data));
                shadowing_attenuation = lerp(shadowing_attenuation, 1, smoothstep(0, 0.01f, data.linear_depth));
            }
        }
        else if (contact_shadow > 0)
        {
            //  平行光源でやるもんじゃない
            contact_shadow_data shadow_data;
            shadow_data.normal_depth_map = gbuffer_normal_depth;
            shadow_data.samp_state = sampler_states[POINT];
            shadow_data.world_position = data.w_position;
            shadow_data.world_normal = data.w_normal;
            shadow_data.march_direction = -L;
            shadow_data.march_length = contact_shadow;
            shadowing_attenuation = lerp(shadow_attenuation, 1, integrate_contact_shadow(data, shadow_data));
            shadowing_attenuation = lerp(shadowing_attenuation, 1, smoothstep(0, 0.01f, data.linear_depth));
        }

        LightingData lighting_data;
        lighting_data.shading_attenuation = shading_attenuation;
        lighting_data.shadowing_attenuation = shadowing_attenuation;
        lighting_data.direction = L;
        lighting_data.color = directional_light.color.rgb * directional_light.color.a;
        lighting_data.camera_position = camera_position.xyz;
        DirectLighting result = integrate_bxdf(data, lighting_data);
        total_diffuse += result.diffuse;
        total_specular += result.specular;
    }
    return float4(total_diffuse + total_specular, 1);
}
