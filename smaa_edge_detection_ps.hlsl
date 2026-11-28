#include "subpixel_morphological_anti_aliasing.hlsli"

Texture2D color_map : register(t0);

float smaa_luma(float2 uv)
{
    float3 color = saturate(color_map.SampleLevel(smaa_point_sampler, uv, 0).rgb);
    return dot(color, float3(0.2126f, 0.7152f, 0.0722f));
}

float2 main(EDGE_DETECTION_OUT pin) : SV_TARGET0
{
    float2 threshold = float2(SMAA_THRESHOLD, SMAA_THRESHOLD);

    float L = smaa_luma(pin.uv);
    float Lleft = smaa_luma(pin.offset[0].xy);
    float Ltop = smaa_luma(pin.offset[0].zw);

    float4 delta;
    delta.xy = abs(L - float2(Lleft, Ltop));
    float2 edges = step(threshold, delta.xy);
    if (dot(edges, float2(1.0f, 1.0f)) == 0.0f)
        discard;

    float Lright = smaa_luma(pin.offset[1].xy);
    float Lbottom = smaa_luma(pin.offset[1].zw);
    delta.zw = abs(L - float2(Lright, Lbottom));

    float2 max_delta = max(delta.xy, delta.zw);

    float Lleftleft = smaa_luma(pin.offset[2].xy);
    float Ltoptop = smaa_luma(pin.offset[2].zw);
    delta.zw = abs(float2(Lleft, Ltop) - float2(Lleftleft, Ltoptop));

    max_delta = max(max_delta.xy, delta.zw);
    float final_delta = max(max_delta.x, max_delta.y);

    edges.xy *= step(final_delta, SMAA_LOCAL_CONTRAST_ADAPTATION_FACTOR * delta.xy);
    return edges;
}
