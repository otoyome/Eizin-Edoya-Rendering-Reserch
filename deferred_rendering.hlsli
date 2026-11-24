#include "sprite.hlsli"
#include "deferred_light_data.hlsli"
#include "shading_models.hlsli"

#include "common_gbuffer_loader.hlsli"

#include "scene_constant_buffer.hlsli"
#include "contact_shadow.hlsli"

struct PS_OUT
{
    float4 diffuse : SV_TARGET0;
    float4 specular : SV_TARGET1;
};
