#include "particle.hlsli"

RWStructuredBuffer<particle> particle_buffer : register(u0);

[numthreads(NUMTHREADS_X, 1, 1)]
void main(uint3 dtid : SV_DISPATCHTHREADID)
{
	uint id = dtid.x;

	particle p = particle_buffer[id];
	
	spawn(id, p);
	
	particle_buffer[id] = p;

}
