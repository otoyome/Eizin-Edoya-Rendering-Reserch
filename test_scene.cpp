#include "test_scene.h"

#define	USE_EIZIN_SHADER

test_scene::test_scene(CONST HWND hwnd, render_data* renderdata)
: test2_scene(hwnd, renderdata)
{
}

test_scene::~test_scene()
{
}

void test_scene::initialize_character_models()
{
	//	踊り手1 : モノ・クロコ
	model_instances[ModelId::Dancer1].pmx_model = std::make_unique<pmx_model>(get_renderdata()->device.Get(), ".\\resources\\Kuroko\\Kuroko.pmx");
	model_instances[ModelId::Dancer1].pmx_model->add_vmd_animation("default", ".\\resources\\Kuroko\\Animation\\Idle.vmd");
	{
		for (auto& material : model_instances[ModelId::Dancer1].pmx_model->get_materials())
		{
			material.pipeline_state_name = get_gbuffer_pipeline_state_name();

			bool is_edge_line = (material.draw_flag & static_cast<unsigned char>(pmx_material_draw_bitflag::use_edge_line)) == static_cast<unsigned char>(pmx_material_draw_bitflag::use_edge_line);
			material.default.edge_size = is_edge_line ? 0.01f : 0.0f;
			if (is_edge_line)
				material.draw_flag &= ~static_cast<unsigned char>(pmx_material_draw_bitflag::cull_both);
#if defined(USE_EIZIN_SHADER)
			material.default.shadow_threshold_texture_index = 0xFFFFFFFF;
			material.default.specular_color.x = 1.0f;				//	粗さ
			material.default.specular_color.y = 0.0f;				//	金属質
			material.default.specular_color.z = 2.0f;				//	陰影階調数
			material.default.specular_color.w = 0.0f;				//	ノイズ係数

			material.default.color_diff_line_factor = 1.0f;			//	カラーライン係数
			material.default.color_diff_line_threshold = 0.01f;		//	カラーライン検知閾値
			material.default.color_line_adjust_saturate = 0.75f;	//  ライン彩度調整係数
			material.default.color_line_adjust_brightness = 0.75f;	//  ライン明度調整係数
			material.default.outline_adjust_parameter.x = 0.0f;		//	カラーアウトラインの係数減衰範囲(最小)
			material.default.outline_adjust_parameter.y = 50.0f;	//	カラーアウトラインの係数減衰表示範囲(最大)

			material.default.color_posterize_factor = 64.0f;		//	カラーポスタライズ係数

			material.default.depth_normal_outline_factor = 1.0f;	//	深度法線アウトライン閾値
			material.default.outline_adjust_parameter.z = 0.0f;		//	深度法線アウトライン係数減衰範囲(最小)
			material.default.outline_adjust_parameter.w = 50.0f;	//	深度法線アウトライン係数減衰表示範囲(最大)
			material.SetFlags(pmx_model::material_attribute::normal_depth_outline_off, true);
			material.SetFlags(pmx_model::material_attribute::color_line_off, false);
			material.SetFlags(pmx_model::material_attribute::color_postarize_off, true);
			material.SetFlags(pmx_model::material_attribute::noise_off, true);
			material.SetFlags(pmx_model::material_attribute::indirect_lighting_off, true);

			//	顔は影閾値マップで顔用の影を作る
			if (material.name == "顔")
			{
				material.default.color_line_adjust_saturate = 0.7f;
				material.default.color_line_adjust_brightness = 0.8f;

				material.SetFlags(pmx_model::material_attribute::xz_plane_lighting, true);
			}
			if (material.name == "肌")
			{
				material.default.color_line_adjust_saturate = 0.7f;
				material.default.color_line_adjust_brightness = 1.2f;
			}
			if (material.name == "体")
			{
				material.default.color_diff_line_threshold = 0.0002f;		//	カラーライン検知閾値
				material.default.color_line_adjust_saturate = 1.5f;
				material.default.color_line_adjust_brightness = 0.5f;
			}
#endif	//	defined(USE_EIZIN_SHADER)
		}
	}

	//	踊り手2 : セラフィム
	model_instances[ModelId::Dancer2].pmx_model = std::make_unique<pmx_model>(get_renderdata()->device.Get(), ".\\resources\\Seraphim\\Seraphim.pmx");
	model_instances[ModelId::Dancer2].pmx_model->add_vmd_animation("default", ".\\resources\\Seraphim\\Animation\\Idle.vmd");
	{
		for (auto& material : model_instances[ModelId::Dancer2].pmx_model->get_materials())
		{
			material.pipeline_state_name = get_gbuffer_pipeline_state_name();

			bool is_edge_line = (material.draw_flag & static_cast<unsigned char>(pmx_material_draw_bitflag::use_edge_line)) == static_cast<unsigned char>(pmx_material_draw_bitflag::use_edge_line);
			material.default.edge_size = is_edge_line ? 0.01f : 0.0f;
			if (is_edge_line)
				material.draw_flag &= ~static_cast<unsigned char>(pmx_material_draw_bitflag::cull_both);
#if defined(USE_EIZIN_SHADER)
			material.default.shadow_threshold_texture_index = 0xFFFFFFFF;
			material.default.specular_color.x = 1.0f;				//	粗さ
			material.default.specular_color.y = 0.0f;				//	金属質
			material.default.specular_color.z = 2.0f;				//	陰影階調数
			material.default.specular_color.w = 0.0f;				//	ノイズ係数

			material.default.color_diff_line_factor = 1.0f;			//	カラーライン係数
			material.default.color_diff_line_threshold = 0.01f;		//	カラーライン検知閾値
			material.default.color_line_adjust_saturate = 0.75f;	//  ライン彩度調整係数
			material.default.color_line_adjust_brightness = 0.75f;	//  ライン明度調整係数
			material.default.outline_adjust_parameter.x = 0.0f;		//	カラーアウトラインの係数減衰範囲(最小)
			material.default.outline_adjust_parameter.y = 50.0f;	//	カラーアウトラインの係数減衰表示範囲(最大)

			material.default.color_posterize_factor = 64.0f;		//	カラーポスタライズ係数

			material.default.depth_normal_outline_factor = 1.0f;	//	深度法線アウトライン閾値
			material.default.outline_adjust_parameter.z = 0.0f;		//	深度法線アウトライン係数減衰範囲(最小)
			material.default.outline_adjust_parameter.w = 50.0f;	//	深度法線アウトライン係数減衰表示範囲(最大)
			material.SetFlags(pmx_model::material_attribute::normal_depth_outline_off, true);
			material.SetFlags(pmx_model::material_attribute::color_line_off, false);
			material.SetFlags(pmx_model::material_attribute::color_postarize_off, true);
			material.SetFlags(pmx_model::material_attribute::noise_off, true);
			material.SetFlags(pmx_model::material_attribute::indirect_lighting_off, true);

			//	顔は影閾値マップで顔用の影を作る
			if (material.name == "顔")
			{
				material.default.color_line_adjust_saturate = 0.8f;
				material.default.color_line_adjust_brightness = 1.5f;

				material.SetFlags(pmx_model::material_attribute::xz_plane_lighting, true);
			}
			if (material.name == "肌")
			{
				material.default.color_line_adjust_saturate = 0.7f;
				material.default.color_line_adjust_brightness = 1.2f;
			}
			if (material.name == "体")
			{
				material.default.color_diff_line_threshold = 0.0002f;		//	カラーライン検知閾値
				material.default.color_line_adjust_saturate = 1.5f;
				material.default.color_line_adjust_brightness = 0.5f;
			}
#endif	//	defined(USE_EIZIN_SHADER)
		}
	}
}

void test_scene::initialize_stage_models()
{
	//	ステージ : Sponza(glTF)
	auto& stage = add_gltf_stage(".\\resources\\Sponza\\glTF\\Sponza.gltf");
	stage.scaling = 12.5f;		//	メートル → MMD単位(1単位 = 8cm)
	stage.rotation.y = DirectX::XM_PIDIV2;	//	回廊の長手方向をカメラの奥行き方向に合わせる
	stage.cast_shadow = false;		//	回廊の壁で床がほぼ影になるので初期値はオフ(ImGuiで切り替え可)
#if defined(USE_EIZIN_SHADER)
	stage.constants.material_attribute = pmx_model::material_attribute::indirect_lighting_off | pmx_model::material_attribute::noise_off;
	stage.constants.shade_factor = 0.6f;
	stage.constants.specular_color = { 1.0f, 0.0f, 8.0f, 1.0f };		//	粗さ, 金属質, 陰影階調数, ノイズ係数
	stage.constants.color_diff_line_factor = 1.0f;
	stage.constants.color_diff_line_threshold = 0.005f;
	stage.constants.color_line_adjust_saturate = 1.0f;
	stage.constants.color_line_adjust_brightness = 1.0f;
	stage.constants.color_posterize_factor = 24.0f;
	stage.constants.depth_normal_outline_factor = 0.02f;
	stage.constants.outline_adjust_parameter = { 1000.0f, 5000.0f, 1000.0f, 5000.0f };
#endif	//	defined(USE_EIZIN_SHADER)
}
