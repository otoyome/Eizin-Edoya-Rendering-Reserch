
RWStructuredBuffer<particle_header> particle_header_buffer : register(u3);

bool comparer(in particle_header x0, in particle_header x1)
{
    //  ¶‘¶‚Í+1AŽ€–S‚Í0
    //  “¯‚¶ƒtƒ‰ƒO‚Å‚ ‚ê‚Î[“x”äŠr
    return (x0.alive > x1.alive || (x0.alive == x1.alive && x0.depth > x1.depth));
}
