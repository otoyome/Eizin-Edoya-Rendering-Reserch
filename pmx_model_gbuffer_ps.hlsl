#include "pmx_model.hlsli"
#include "gbuffer.hlsli"

#define DIFFUSE 0
#define SPHERE 1
#define RAMP 2
Texture2D<float4> textures[3] : register(t0);

SamplerState samplerstate : register(s0);

PSGBufferOut main(VS_OUT pin, bool is_front_face : SV_IsFrontFace)
{
    GBufferData data = (GBufferData) 0;
    
    float4 base_color = 0;
    float3 emisive_color = 0;
    float roughness = 0.8f;
    float metalness = 0;
    float3 shade_color = 0;
    float occlusion_factor = 0;
    float occlusion_strength = 0;
    float3 normal;
    if (pin.outline)
    {
        base_color.xyz = edge_color.xyz;
        roughness = 1.0f;
        metalness = 0;
        shade_color.xyz = edge_color.xyz;
        normal = normalize(pin.w_normal.xyz) * (is_front_face ? +1 : -1);
    }
    else
    {
        base_color = diffuse_color;
        if (texture_index.x > -1)
        {
            float4 sampled = textures[DIFFUSE].Sample(samplerstate, pin.texcoord);
            base_color *= sampled;
        }
        base_color.rgb = pow(base_color.rgb, GammaFactor);
        //  透明度調整
        clip(base_color.a - 0.01f);

        //	ディザリング対応
        clip(base_color.a - calculate_alpha_dithering(pin.position.xy));
    
	    //	法線
        normal = normalize(pin.w_normal.xyz) * (is_front_face ? +1 : -1);

        //  ベースカラーをトップ色/シェードカラーをボトム色として算出
        //  本来は専用のテクスチャにしておくべきだがMMDのテクスチャ的にこれでやっておく。
        shade_color = base_color.rgb;
        if (texture_index.z > -1)
        {
            shade_color *= textures[RAMP].Sample(samplerstate, float2(0.5f, 0.99f)).rgb;
        }
    }

    //  GBufferDataに出力情報をまとめる
    data.base_color = base_color.rgb;
    data.shading_model = shading_model_pmx_toon;
    data.emissive_color = emisive_color;
    data.w_normal = normal;
    data.w_position = pin.w_position.xyz;
    data.velocity = pin.velocity;
//  data.depth はEncode時はいらない
    data.roughness = roughness;
    data.metalness = metalness;
    data.occlusion_factor = occlusion_factor;
    data.occlusion_strength = occlusion_strength;

    data.shade_color = shade_color;
    data.shadow_mask = shadow_mask;
    return EncodeGBuffer(data, view_projection_transform, z_buffer_parameteres);
}

