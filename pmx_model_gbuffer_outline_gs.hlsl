#include "pmx_model.hlsli"

[maxvertexcount(6)]
void main(triangle VS_OUT input[3], inout TriangleStream<VS_OUT> output)
{
	//	表面登録
    int i;
    for (i = 0; i < 3; i++)
    {
        VS_OUT element = (VS_OUT) 0;
        element.position = input[i].position;
        element.w_position = input[i].w_position;
        element.w_normal = input[i].w_normal;
        element.w_tangent = input[i].w_tangent;
        element.texcoord = input[i].texcoord;
        element.outline = false;
        output.Append(element);
    }
    output.RestartStrip();

    if (edge_size != 0)
    {
	    //	裏面登録
        for (i = 2; i >= 0; i--)
        {
            VS_OUT element = (VS_OUT) 0;

            //  本来は頂点カラーに登録した情報を元にして頂点を膨らます処理を調整したりします。
            //  R : 陰影調整で使用したりするのでここでは使用しない
            //  G : カメラからの距離に応じて輪郭線をどの程度膨張させるかの係数
            //  B : 輪郭線のZオフセット値
            //  A : 輪郭線の太さ係数。0.5が標準で，1にするとMAXの太さ，0にすると輪郭線なしになる

            float3  outline_parameter       = float3(1.0f, 0.2f, 0.5f); //  頂点単位の情報が必要だが、いったんは定数的に設定しておく
            float   outline_depth_factor    = 10.0f;                    //  膨張係数(マテリアルパラメーター)
            float2  outline_depth_range     = {40.0f, 200.0f};          //  膨張範囲(マテリアルパラメーター)
            float   outline_z_offset_max    = 3.0f;                     //  Zオフセット距離(マテリアルパラメーター)

            //  あんまり視線空間でやるもんじゃない。
            float3 v_position = mul(float4(input[i].w_position.xyz, 1), view_transform).xyz;
            float3 v_normal = mul(float4(input[i].w_normal.xyz, 0), view_transform).xyz;

            //  カメラからの距離による係数算出
            float depth_factor = saturate(smoothstep(outline_depth_range.x, outline_depth_range.y, v_position.z));
            depth_factor = lerp(0.0f, outline_depth_factor * depth_factor, outline_parameter.x);

            //  Zオフセット
            v_position.xyz += normalize(v_position.xyz) * outline_parameter.y * outline_z_offset_max;

            //  輪郭線の膨張処理
            v_position.xyz += v_normal * ((edge_size * outline_parameter.z * 2.0f) + (edge_size * depth_factor));

            //  
            element.position = mul(float4(v_position.xyz, 1), projection_transform);
            element.w_normal = input[i].w_normal;
            element.w_tangent = input[i].w_tangent;
            element.texcoord = input[i].texcoord;
            element.outline = true;
            output.Append(element);
        }
        output.RestartStrip();
    }
}
