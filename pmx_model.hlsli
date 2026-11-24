#include	"lights.hlsli"
#include    "common.hlsli"

#include	"scene_constant_buffer.hlsli"

#include    "gbuffer.hlsli"

struct VS_IN
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 texcoord : TEXCOORD;
    float4 additional_uv[4] : ADDITIONAL_TEXCOORD;
    uint4 joints : JOINTS;
    float4 weights : WEIGHTS;
};

struct VS_OUT
{
    float4 position : SV_POSITION;
    float3 w_position : POSITION;
    float3 w_normal : NORMAL;
    float4 w_tangent : TANGENT;
    float2 texcoord : TEXCOORD;
    float3 shadow_texcoord : TEXCOORD1;
    float2 velocity : VELOCITY;
    bool outline : OUTLINE;
};

#define BONE_MAX 1000
cbuffer PRIMITIVE_CONSTANT_BUFFER : register(b0)
{
    row_major float4x4 world;
    row_major float4x4 previous_world;
    int4 texture_index;
    float4 diffuse_color;
    float4 specular_color;
    float4 edge_color;

    float edge_size;
    float alpha_clip_threshold;
	float alpha_dither_factor;
    uint material_attribute;

    //  ポストエフェクトの色差によるライン算出係数
    float color_diff_line_factor; // アウトライン係数
    float color_diff_line_threshold; // 検知係数
    float color_line_adjust_saturate;   //  ライン彩度調整係数
    float color_line_adjust_brightness; //  ライン明度調整係数

    //  色のポスタライズ数
    float color_posterize_factor;
    //  ポストエフェクトの法線/深度によるアウトライン算出係数
    float depth_normal_outline_factor;
	//	影閾値マップのオフセット
    float2 shadow_threshold_texcoord_offset;
    
    float4 outline_adjust_parameter;
    row_major float4x4 bone_transforms[BONE_MAX];
};


cbuffer PRIMITIVE_CONSTANT_BUFFER : register(b2)
{
	uint shadow_mask;	//	適応シャドウマップマスク
    uint primitive_constant_dummy1;
    uint primitive_constant_dummy2;
    uint primitive_constant_dummy3;
	//	メッシュの方向
    //  xy : 右ベクトル, zw : 左ベクトル
    float4 mesh_direction;
};

cbuffer LIGHT_CONSTANT_BUFFER : register(b3)
{
    directional_lights directional_light;
    point_lights point_light[8];
    spot_lights spot_light[8];
};

cbuffer SHADOWMAP_CONSTANT_BUFFER : register(b4)
{
    row_major float4x4 light_view_projection;
    float shadow_attenuation;
    float shadow_bias;
    float2 shadow_dummy;
};


#include "shading_functions.hlsli"
