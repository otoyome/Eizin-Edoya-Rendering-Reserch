// UNIT.32
#include "fullscreen_quad.hlsli"

cbuffer PARAMETRIC_CONSTANT_BUFFER : register(b2)
{
	float extraction_threshold;
	float gaussian_sigma;
	float bloom_intensity;
};

#define POINT 0
#define LINEAR 1
#define ANISOTROPIC 2
SamplerState sampler_states[3] : register(s0);
Texture2D texture_maps[4] : register(t0);
float4 main(VS_OUT pin) : SV_TARGET
{
	float4 color = texture_maps[0].Sample(sampler_states[ANISOTROPIC], pin.texcoord);
	float alpha = color.a;

	color.rgb = step(extraction_threshold, dot(color.rgb, float3(0.299, 0.587, 0.114))) * color.rgb;
	return float4(color.rgb, alpha);
}
