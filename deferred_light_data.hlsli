#ifndef __DEFERRED_LIGHT_HLSLI__
#define __DEFERRED_LIGHT_HLSLI__

#include "lights.hlsli"

static const int light_kind_directional_light = 1; //  平行光源
static const int light_kind_point_light = 2; //  点光源
static const int light_kind_spot_light = 3; //  スポットライト

//  纏めたライト情報
struct compund_lights
{
    //  共有の情報
    //  work_data[3].w は共有でライトの種別番号を入れておく

    //  平行光源の場合
    //  work_data[0] = direction
    //  work_data[1] = color
    //  work_data[2] = dummy
    //  work_data[3] = xyz = dummy, w = ライト種別

    //  点光源の場合
    //  work_data[0] = position
    //  work_data[1] = color
    //  work_data[2] = x = range, yzw = dummy
    //  work_data[3] = xyz = dummy, w = ライト種別

    //  スポットライトの場合
    //  work_data[0] = position
    //  work_data[1] = direction
    //  work_data[2] = color
    //  work_data[3] = x = range, y = inner cone, z = outer cone, w = ライト種別
    float4 work_data[4];
};

//  光源情報
cbuffer LIGHT_CONSTANT_BUFFER : register(b2)
{
    compund_lights compund_light_data;

    //  コンタクトシャドウ
    float contact_shadow;
    float3 light_data_dummy;

    //  シャドウマップ関係
    int use_shadow; //	影を使用しているかどうか
    float shadow_attenuation; //	影色
    float shadow_bias; //	深度バイアス
    float shadow_mask; //  シャドウマスク係数
    row_major float4x4 light_view_projection; //	ライトの位置から見た射影行列
};

//  ライト種別の取得
int get_light_kinds()
{
    return (int) (compund_light_data.work_data[3].w + 0.5f);
}

directional_lights convert_directional_lights()
{
    directional_lights data;
    data.direction = compund_light_data.work_data[0];
    data.color = compund_light_data.work_data[1];
    return data;
}

point_lights convert_point_lights()
{
    point_lights data;
    data.position = compund_light_data.work_data[0];
    data.color = compund_light_data.work_data[1];
    data.range = compund_light_data.work_data[2].x;
    return data;
}

spot_lights convert_spot_lights()
{
    spot_lights data;
    data.position = compund_light_data.work_data[0];
    data.direction = compund_light_data.work_data[1];
    data.color = compund_light_data.work_data[2];
    data.range = compund_light_data.work_data[3].x;
    data.innerCorn = compund_light_data.work_data[3].y;
    data.outerCorn = compund_light_data.work_data[3].z;
    return data;
}

#endif  //  __DEFERRED_LIGHT_HLSLI__
