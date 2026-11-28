#include "subpixel_morphological_anti_aliasing.hlsli"

Texture2D color_map : register(t0);
Texture2D blend_map : register(t1);
Texture2D edge_map : register(t2);

float4 main(NEIGHBORHOOD_BLENDING_OUT pin) : SV_TARGET0
{
    float4 color = SMAANeighborhoodBlendingPS(
        pin.uv,
        pin.offset,
        color_map,
        blend_map
    );

    //  デバッグ用 : 検出エッジ表示
    if (smaa_draw_edge)
        color.rg += edge_map.Load(int3(pin.position.xy, 0)).rg;

    return color;
}
