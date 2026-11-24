#include "particle.hlsli"


RWStructuredBuffer<particle> particle_buffer : register(u0);

[numthreads(NUMTHREADS_X, 1, 1)]
void main(uint3 dtid : SV_DISPATCHTHREADID)
{
	uint id = dtid.x;

	particle p = particle_buffer[id];
	
	float3 d = emission_position.xyz - p.position;
	float r = max(0.1, length(d));
	
	float G = 1.0;
	float3 g = G / (r * r) * d;
	
	p.velocity += g * delta_time;
	p.position += p.velocity * delta_time;

	particle_buffer[id] = p;
}
