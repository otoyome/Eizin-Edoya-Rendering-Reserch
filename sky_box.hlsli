#include "gbuffer.hlsli"

#include "common.hlsli"

#include "scene_constant_buffer.hlsli"

struct VS_OUT
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
    float4 world_position : WORLD_POSITION;
    float2 velocity : VELOCITY;
};
