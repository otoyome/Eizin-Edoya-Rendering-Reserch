#include "gltf_model.hlsli"
#include "gbuffer.hlsli"

//  G-Buffer output for glTF models (stages such as Sponza) rendered with the Eizin shading model.
//  Material colors/textures come from glTF. The Eizin parameters are stored per glTF material
//  in eizin_materials (indexed by the glTF material index, see test2_scene::eizin_gltf_constants).

// https://www.khronos.org/registry/glTF/specs/2.0/glTF-2.0.html#reference-textureinfo
struct texture_info
{
    int index;
    int texcoord;
};
struct normal_texture_info
{
    int index;
    int texcoord;
    float scale;
};
struct occlusion_texture_info
{
    int index;
    int texcoord;
    float strength;
};
struct pbr_metallic_roughness
{
    float4 basecolor_factor;
    texture_info basecolor_texture;
    float metallic_factor;
    float roughness_factor;
    texture_info metallic_roughness_texture;
};
struct material_constants
{
    float3 emissive_factor;
    int alpha_mode; // "OPAQUE" : 0, "MASK" : 1, "BLEND" : 2
    float alpha_cutoff;
    int double_sided;

    pbr_metallic_roughness pbr_metallic_roughness;

    normal_texture_info normal_texture;
    occlusion_texture_info occlusion_texture;
    texture_info emissive_texture;
};
StructuredBuffer<material_constants> materials : register(t0);

#define BASECOLOR_TEXTURE 0
#define METALLIC_ROUGHNESS_TEXTURE 1
#define NORMAL_TEXTURE 2
#define EMISSIVE_TEXTURE 3
#define OCCLUSION_TEXTURE 4
Texture2D<float4> material_textures[5] : register(t1);

#define POINT 0
#define LINEAR 1
#define ANISOTROPIC 2
SamplerState sampler_states[3] : register(s0);

//  Per glTF material Eizin parameters (layout must match test2_scene::eizin_gltf_constants)
struct eizin_material_constants
{
    uint material_attribute;            //  Same bit flags as pmx_model::material_attribute
    float shade_factor;                 //  shade color = base color * shade factor
    float2 dummy0;
    float4 specular_color;              //  roughness, metalness, shading steps, noise factor

    float color_diff_line_factor;
    float color_diff_line_threshold;
    float color_line_adjust_saturate;
    float color_line_adjust_brightness;

    float color_posterize_factor;
    float depth_normal_outline_factor;
    float2 dummy1;

    float4 outline_adjust_parameter;    //  xy : color line fade range, zw : depth/normal line fade range
};
StructuredBuffer<eizin_material_constants> eizin_materials : register(t20);

PSGBufferOut main(VS_OUT pin, bool is_front_face : SV_IsFrontFace)
{
    material_constants m = materials[material];
    eizin_material_constants e = eizin_materials[max(material, 0)];

    //  Base color
    float4 base_color = m.pbr_metallic_roughness.basecolor_factor;
    if (m.pbr_metallic_roughness.basecolor_texture.index > -1)
    {
        float4 sampled = material_textures[BASECOLOR_TEXTURE].Sample(sampler_states[ANISOTROPIC], pin.texcoord);
        sampled.rgb = pow(sampled.rgb, GammaFactor);
        base_color *= sampled;
    }

    //  Alpha
    if (m.alpha_mode == 1)
    {
        clip(base_color.a - m.alpha_cutoff);
    }
    else
    {
        clip(base_color.a - 0.01f);
        clip(base_color.a - calculate_alpha_dithering(pin.position.xy));
    }

    //  Normal (with normal mapping)
    float3 N = normalize(pin.w_normal.xyz);
    float3 T = has_tangent ? normalize(pin.w_tangent.xyz) : float3(1, 0, 0);
    float sigma = has_tangent ? pin.w_tangent.w : 1.0;
    T = normalize(T - N * dot(N, T));
    float3 B = normalize(cross(N, T) * sigma);
    if (is_front_face == false)
    {
        T = -T;
        B = -B;
        N = -N;
    }
    if (m.normal_texture.index > -1)
    {
        float4 sampled = material_textures[NORMAL_TEXTURE].Sample(sampler_states[LINEAR], pin.texcoord);
        float3 normal_factor = sampled.xyz * 2.0f - 1.0f;
        normal_factor = normalize(normal_factor * float3(m.normal_texture.scale, m.normal_texture.scale, 1.0f));
        N = normalize((normal_factor.x * T) + (normal_factor.y * B) + (normal_factor.z * N));
    }

    //  Pack into GBufferData
    GBufferData data = (GBufferData)0;
    data.base_color = base_color.rgb;
    data.w_normal = N;
    data.w_position = pin.w_position.xyz;
    data.velocity = pin.velocity;
    data.shading_model = shading_model_eizin;
    data.emissive_color = 0;
    data.roughness = e.specular_color.x;
    data.metalness = e.specular_color.y;
    data.occlusion_factor = e.specular_color.z / 255.0f;
    data.occlusion_strength = e.specular_color.w;
    data.shade_color = base_color.rgb * e.shade_factor;
    data.shadow_threshold = 1;
    data.stencil = e.material_attribute;
    data.shadow_mask = 0;
    data.mesh_direction = float4(1, 0, 0, 1);

    //  Color line
    {
        float2 pack_data;
        pack_data.x = e.color_diff_line_factor;
        pack_data.x *= saturate(smoothstep(e.outline_adjust_parameter.y, e.outline_adjust_parameter.x, pin.position.w));
        pack_data.y = e.color_diff_line_threshold;
        data.unique_parameter.x = PackFloat2ToFloat(pack_data);

        pack_data.x = e.color_line_adjust_saturate;
        pack_data.y = e.color_line_adjust_brightness;
        data.unique_parameter.y = PackFloat2ToFloat(pack_data);
    }

    //  Depth/normal line and posterize
    {
        float2 pack_data;
        pack_data.x = e.depth_normal_outline_factor;
        pack_data.x *= saturate(smoothstep(e.outline_adjust_parameter.w, e.outline_adjust_parameter.z, pin.position.w));
        pack_data.y = e.color_posterize_factor / 255.0f;
        data.unique_parameter.z = PackFloat2ToFloat(pack_data);
    }
    return EncodeGBuffer(data, view_projection_transform, z_buffer_parameteres);
}
