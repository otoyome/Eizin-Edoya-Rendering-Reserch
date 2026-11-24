#include "fullscreen_quad.hlsli"

#include "scene_constant_buffer.hlsli"
#include "physical_based_camera.hlsli"

Texture2D<float4> color_maps : register(t0);
Texture2D<float> depth_map : register(t3);

static const float cos_vignetting_factor = 0.5f;

//  レンズ性能曲線の周辺光量比曲線にて数値的なものの確認は可能。
float4 main(PS_IN pin) : SV_TARGET
{
    uint2 viewport_size = viewport_rect.zw - viewport_rect.xy;
    float aspect_ratio = viewport_size.x / viewport_size.y;

    float2 sensor_position = image_sensor_size * pin.texcoord;
    float2 sensor_center = image_sensor_size * 0.5f;

    float image_sensor_radius = length(image_sensor_size * 0.5f);
    float image_circle_radius = image_circle_diameter * 0.5f;

    //  周辺減光
    float vignetting_factor = 0.0f;

    //  メカニカルケラレ
    {
        //  レンズのイメージサークルがイメージセンサーを覆っていない場合、
        //  イメージサークルの外側に出てしまった部分に光が入らないため発生する
        float sensor_length = length(sensor_position - sensor_center);
        float mechanical_factor = smoothstep(image_sensor_radius, image_circle_radius, sensor_length);
        vignetting_factor += vignette_mecanical_factor * (1.0f - mechanical_factor) * smoothstep(0.0f, 0.01f, max(0, image_sensor_radius - image_circle_radius));
    }

    //  光学的なケラレ
    {
	    //	コサイン4乗則による係数
        //  レンズの性能による問題なのでここは何をしようと解消しない。
        float2 d = abs(sensor_position - sensor_center) * float2(2.0f * aspect_ratio, 2.0f);
        float r2 = dot(d, d);
        float h_per_f = image_sensor_size.y / focal_distance;
        float cos_theta = 1.0f / sqrt(r2 * (h_per_f * h_per_f) + 1.0f);
        vignetting_factor += (1.0f - pow(cos_theta, 4.0f)) * cos_vignetting_factor * vignette_lens_factor;

	    //	口径食による係数
        //  入射角の関係で、絞りを通って入る光が削られてしまう。なので絞る事で改善可能。
        float aperture_radius = calculate_apeture(f_number) * 0.5f; //  現在の絞りの状態
        float aperture_factor = 1.0f - smoothstep(image_circle_radius * 2.0f, image_circle_radius - aperture_radius, length(sensor_position - sensor_center));
        vignetting_factor += aperture_factor * vignette_apeture_factor;
    }

    //  ・ピクセルケラレ
    //  　デジタルカメラにのみ発生する模様
    //  　デジタルセンサーに入射する光の角度に応じて減衰する
    //  　いったん無視する。

    //  ・その他要因によるケラレ
    //    例えばフィルターカバーなどによる問題
    //  　これも無視する。

    //  周辺減光色の処理
    float3 vignetting_color = lerp(float3(1.0f, 1.0f, 1.0f), vignette_color.a * vignette_color.rgb, saturate(vignetting_factor) * saturate(vignette_color.a));
    return float4(vignetting_color, 1);
}