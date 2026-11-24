#include "compute_particle.hlsli"

GS_IN main(uint vertex_id : SV_VertexID)
{
    //  ’¸“_”Ô†‚ğ‘—‚é‚¾‚¯
    GS_IN vout;
    vout.vertex_id = vertex_id;
	return vout;
}