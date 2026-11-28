#ifndef __SUBPIXEL_MORPHOLOGICAL_ANTI_ALIASING_HLSLI__
#define __SUBPIXEL_MORPHOLOGICAL_ANTI_ALIASING_HLSLI__

#include "scene_constant_buffer.hlsli"

SamplerState smaa_point_sampler : register(s0);
SamplerState smaa_linear_sampler : register(s1);

#define SMAA_HLSL_4_1

cbuffer SMAA_CONSTANT_BUFFER : register(b13)
{
    float smaa_threshold;
    uint smaa_max_search_steps;
    uint smaa_max_search_steps_diagonal;
    uint smaa_corner_rounding;

    float smaa_local_contrast_adaptation_factor;
    uint smaa_draw_edge;
    uint2 smaa_dummy;
};
#define SMAA_THRESHOLD smaa_threshold
#define SMAA_MAX_SEARCH_STEPS smaa_max_search_steps
#define SMAA_MAX_SEARCH_STEPS_DIAG smaa_max_search_steps_diagonal
#define SMAA_CORNER_ROUNDING smaa_corner_rounding
#define SMAA_LOCAL_CONTRAST_ADAPTATION_FACTOR smaa_local_contrast_adaptation_factor

#define SMAA_LINEAR_SAMPLER smaa_linear_sampler
#define SMAA_POINT_SAMPLER smaa_point_sampler
#define SMAA_RT_METRICS float4(1.0f / viewport_rect.z, 1.0f / viewport_rect.w, viewport_rect.z, viewport_rect.w)
#include "subpixel_morphological_anti_aliasing/SMAA.hlsli"

struct EDGE_DETECTION_OUT
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
    float4 offset[3] : TEXCOORD1;
};

struct BLENDING_WEIGHT_OUT
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
    float2 texcoord : TEXCOORD1;
    float4 offset[3] : TEXCOORD2;
};

struct NEIGHBORHOOD_BLENDING_OUT
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
    float4 offset : TEXCOORD1;
};

void GetPositionUV(out float4 position, out float2 uv, in uint index)
{
    const float2 vertex_id = float2(index / 2u, index % 2u);
    uv = vertex_id;
    position = float4(vertex_id * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.1f, 1.0f);
}

#endif  //  __SUBPIXEL_MORPHOLOGICAL_ANTI_ALIASING_HLSLI__
