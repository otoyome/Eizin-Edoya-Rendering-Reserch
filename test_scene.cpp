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
	//	Sponza全体の基本設定(石材)
	eizin_gltf_constants base{};
#if defined(USE_EIZIN_SHADER)
	base.material_attribute = pmx_model::material_attribute::indirect_lighting_off | pmx_model::material_attribute::noise_off;
	base.shade_factor = 0.55f;									//	陰色 = ベースカラー * 0.55
	base.specular_color = { 1.0f, 0.0f, 2.0f, 0.0f };			//	粗さ, 金属質, 陰影階調数, ノイズ係数
	base.color_diff_line_factor = 1.0f;
	base.color_diff_line_threshold = 0.005f;
	base.color_line_adjust_saturate = 1.0f;
	base.color_line_adjust_brightness = 0.8f;
	base.color_posterize_factor = 64.0f;
	base.depth_normal_outline_factor = 0.02f;
	base.outline_adjust_parameter = { 1000.0f, 5000.0f, 1000.0f, 5000.0f };
#endif	//	defined(USE_EIZIN_SHADER)

	//	ステージ : Sponza(glTF)
	auto& stage = add_gltf_stage(".\\resources\\Sponza\\glTF\\Sponza.gltf", base);
	stage.scaling = 12.5f;		//	メートル → MMD単位(1単位 = 8cm)
	stage.rotation.y = DirectX::XM_PIDIV2;	//	回廊の長手方向をカメラの奥行き方向に合わせる
	stage.cast_shadow = false;		//	回廊の壁で床がほぼ影になるので初期値はオフ(ImGuiで切り替え可)

#if defined(USE_EIZIN_SHADER)
	//	Sponzaのマテリアルには名前が無いので番号で指定する(ベースカラーテクスチャで判別)
	auto set_material = [&stage](std::initializer_list<int> indices, auto&& setter)
	{
		for (int index : indices)
		{
			if (index < static_cast<int>(stage.material_constants.size()))
				setter(stage.material_constants[index]);
		}
	};

	//	植物(0, 3) : 鮮やかな色を残しつつ細かく段階化、ラインは彩度高めで暗く
	set_material({ 0, 3 }, [](eizin_gltf_constants& c)
	{
		c.shade_factor = 0.45f;
		c.color_posterize_factor = 12.0f;
		c.color_diff_line_threshold = 0.002f;
		c.color_line_adjust_saturate = 1.5f;
		c.color_line_adjust_brightness = 0.5f;
		c.depth_normal_outline_factor = 0.01f;
	});

	//	布(幕・垂れ幕 14～19) : 平坦な色面にして、模様はカラーラインで拾う
	set_material({ 14, 15, 16, 17, 18, 19 }, [](eizin_gltf_constants& c)
	{
		c.shade_factor = 0.5f;
		c.color_posterize_factor = 8.0f;
		c.color_diff_line_threshold = 0.002f;
		c.color_line_adjust_saturate = 1.2f;
		c.color_line_adjust_brightness = 0.6f;
		c.material_attribute |= pmx_model::material_attribute::normal_depth_outline_off;	//	布のしわで線が出すぎるので
	});

	//	彫刻(盾 4、獅子 23) : 深度/法線のラインで彫りを強調
	set_material({ 4, 23 }, [](eizin_gltf_constants& c)
	{
		c.shade_factor = 0.5f;
		c.specular_color.z = 3.0f;
		c.depth_normal_outline_factor = 0.005f;
		c.color_diff_line_threshold = 0.01f;
	});

	//	鎖(20) : 金属なので陰影階調を増やす
	set_material({ 20 }, [](eizin_gltf_constants& c)
	{
		c.shade_factor = 0.35f;
		c.specular_color.z = 4.0f;
		c.color_posterize_factor = 8.0f;
	});

	//	屋根瓦(24) : 細かい模様でラインがうるさくならないよう閾値を上げる
	set_material({ 24 }, [](eizin_gltf_constants& c)
	{
		c.color_diff_line_threshold = 0.02f;
		c.color_posterize_factor = 8.0f;
	});
#endif	//	defined(USE_EIZIN_SHADER)
}

const char* test_scene::get_material_settings_path() const
{
	return ".\\resources\\material_settings\\test_scene.json";
}
