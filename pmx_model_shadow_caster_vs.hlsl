#include "pmx_model.hlsli"

VS_OUT main(VS_IN vin)
{
    VS_OUT vout = (VS_OUT)0;
    float3 position = 0, normal = 0;
    for (int index = 0; index < 4; ++index)
    {
        position += mul(float4(vin.position, 1), bone_transforms[vin.joints[index]]).xyz * vin.weights[index];
        normal += mul(float4(vin.normal, 0), bone_transforms[vin.joints[index]]).xyz * vin.weights[index];
    }
    vout.w_position = mul(float4(position, 1), world).xyz;
    vout.position = mul(float4(vout.w_position, 1), view_projection_transform);

    return vout;
}
