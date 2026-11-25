//	マテリアル設定のJSON保存/読込(仮実装)
//	・PMXモデル : 英人用パラメーター/色/フラグ
//	・glTFステージ : マテリアル毎の英人パラメーター + glTF本来のマテリアル値
//	モデル/マテリアルは番号で対応付ける(PMXは名前も一致した場合のみ反映)
#include "test2_scene.h"

#include <filesystem>
#include <fstream>

#include "tinygltf-release/json.hpp"

namespace
{
	using json = nlohmann::json;

	//	PMXのマテリアル名はCP932なので、JSON(UTF-8)用に変換する
	std::string material_name_to_utf8(const std::string& cp932)
	{
		if (cp932.empty())
			return {};
		const int wide_length = MultiByteToWideChar(932, 0, cp932.data(), static_cast<int>(cp932.size()), nullptr, 0);
		std::wstring wide(wide_length, L'\0');
		MultiByteToWideChar(932, 0, cp932.data(), static_cast<int>(cp932.size()), wide.data(), wide_length);
		const int utf8_length = WideCharToMultiByte(CP_UTF8, 0, wide.data(), wide_length, nullptr, 0, nullptr, nullptr);
		std::string utf8(utf8_length, '\0');
		WideCharToMultiByte(CP_UTF8, 0, wide.data(), wide_length, utf8.data(), utf8_length, nullptr, nullptr);
		return utf8;
	}

	json to_json(const DirectX::XMFLOAT2& v) { return json::array({ v.x, v.y }); }
	json to_json(const DirectX::XMFLOAT4& v) { return json::array({ v.x, v.y, v.z, v.w }); }
	json to_json(const float* v, size_t count) { json a = json::array(); for (size_t i = 0; i < count; ++i) a.push_back(v[i]); return a; }

	//	存在する項目だけ読み込む(無い項目は現在の値のまま)
	template<class T>
	void read(const json& j, const char* key, T& value)
	{
		auto it = j.find(key);
		if (it != j.end() && !it->is_null())
			value = it->get<T>();
	}
	void read(const json& j, const char* key, DirectX::XMFLOAT2& v)
	{
		auto it = j.find(key);
		if (it != j.end() && it->is_array() && it->size() >= 2)
			v = { (*it)[0].get<float>(), (*it)[1].get<float>() };
	}
	void read(const json& j, const char* key, DirectX::XMFLOAT4& v)
	{
		auto it = j.find(key);
		if (it != j.end() && it->is_array() && it->size() >= 4)
			v = { (*it)[0].get<float>(), (*it)[1].get<float>(), (*it)[2].get<float>(), (*it)[3].get<float>() };
	}
	void read(const json& j, const char* key, float* v, size_t count)
	{
		auto it = j.find(key);
		if (it != j.end() && it->is_array() && it->size() >= count)
			for (size_t i = 0; i < count; ++i)
				v[i] = (*it)[i].get<float>();
	}
}

const char* test2_scene::get_material_settings_path() const
{
	return ".\\resources\\material_settings\\test2_scene.json";
}

bool test2_scene::save_material_settings(const char* path)
{
	json root;
	root["version"] = 1;

	//	PMXモデル
	json pmx_models = json::array();
	for (size_t model_index = 0; model_index < model_instances.size(); ++model_index)
	{
		const auto& instance = model_instances[model_index];
		if (!instance.pmx_model)
			continue;

		json materials = json::array();
		const auto& model_materials = instance.pmx_model->get_materials();
		for (size_t material_index = 0; material_index < model_materials.size(); ++material_index)
		{
			const auto& material = model_materials[material_index];
			const auto& d = material.default;
			json m;
			m["index"] = material_index;
			m["name"] = material_name_to_utf8(material.name);
			m["pipeline_state_name"] = material.pipeline_state_name;
			m["material_attribute"] = material.material_attribute;
			m["diffuse_color"] = to_json(d.diffuse_color);
			m["specular_color"] = to_json(d.specular_color);
			m["edge_color"] = to_json(d.edge_color);
			m["edge_size"] = d.edge_size;
			m["alpha_clip_threshold"] = d.alpha_clip_threshold;
			m["alpha_dither_factor"] = d.alpha_dither_factor;
			m["color_diff_line_factor"] = d.color_diff_line_factor;
			m["color_diff_line_threshold"] = d.color_diff_line_threshold;
			m["color_line_adjust_saturate"] = d.color_line_adjust_saturate;
			m["color_line_adjust_brightness"] = d.color_line_adjust_brightness;
			m["color_posterize_factor"] = d.color_posterize_factor;
			m["depth_normal_outline_factor"] = d.depth_normal_outline_factor;
			m["shadow_threshold_texcoord_offset"] = to_json(d.shadow_threshold_texcoord_offset);
			m["outline_adjust_parameter"] = to_json(d.outline_adjust_parameter);
			materials.push_back(m);
		}

		json model;
		model["model_index"] = model_index;
		model["materials"] = materials;
		pmx_models.push_back(model);
	}
	root["pmx_models"] = pmx_models;

	//	glTFステージ
	json stages = json::array();
	for (size_t stage_index = 0; stage_index < gltf_stages.size(); ++stage_index)
	{
		const auto& stage = gltf_stages[stage_index];
		json materials = json::array();
		for (size_t material_index = 0; material_index < stage.material_constants.size(); ++material_index)
		{
			const auto& c = stage.material_constants[material_index];
			json eizin;
			eizin["material_attribute"] = c.material_attribute;
			eizin["shade_factor"] = c.shade_factor;
			eizin["specular_color"] = to_json(c.specular_color);
			eizin["color_diff_line_factor"] = c.color_diff_line_factor;
			eizin["color_diff_line_threshold"] = c.color_diff_line_threshold;
			eizin["color_line_adjust_saturate"] = c.color_line_adjust_saturate;
			eizin["color_line_adjust_brightness"] = c.color_line_adjust_brightness;
			eizin["color_posterize_factor"] = c.color_posterize_factor;
			eizin["depth_normal_outline_factor"] = c.depth_normal_outline_factor;
			eizin["outline_adjust_parameter"] = to_json(c.outline_adjust_parameter);

			json m;
			m["index"] = material_index;
			m["eizin"] = eizin;
			if (material_index < stage.model->materials.size())
			{
				const auto& d = stage.model->materials[material_index].data;
				json gltf;
				gltf["basecolor_factor"] = to_json(d.pbr_metallic_roughness.basecolor_factor, 4);
				gltf["emissive_factor"] = to_json(d.emissive_factor, 3);
				gltf["metallic_factor"] = d.pbr_metallic_roughness.metallic_factor;
				gltf["roughness_factor"] = d.pbr_metallic_roughness.roughness_factor;
				gltf["alpha_mode"] = d.alpha_mode;
				gltf["alpha_cutoff"] = d.alpha_cutoff;
				gltf["normal_scale"] = d.normal_texture.scale;
				gltf["occlusion_strength"] = d.occlusion_texture.strength;
				m["gltf"] = gltf;
			}
			materials.push_back(m);
		}

		json s;
		s["stage_index"] = stage_index;
		s["scaling"] = stage.scaling;
		s["rotation"] = json::array({ stage.rotation.x, stage.rotation.y, stage.rotation.z });
		s["translation"] = json::array({ stage.translation.x, stage.translation.y, stage.translation.z });
		s["cast_shadow"] = stage.cast_shadow;
		s["materials"] = materials;
		stages.push_back(s);
	}
	root["gltf_stages"] = stages;

	//	書き出し
	std::error_code error;
	std::filesystem::create_directories(std::filesystem::path(path).parent_path(), error);
	std::ofstream file(path);
	if (!file)
	{
		material_settings_message = std::string("save failed: ") + path;
		return false;
	}
	file << root.dump(2);
	material_settings_message = std::string("saved: ") + path;
	return true;
}

bool test2_scene::load_material_settings(const char* path)
{
	std::ifstream file(path);
	if (!file)
	{
		material_settings_message = std::string("load failed (not found): ") + path;
		return false;
	}

	json root = json::parse(file, nullptr, false);
	if (root.is_discarded())
	{
		material_settings_message = std::string("load failed (invalid json): ") + path;
		return false;
	}

	int skipped = 0;

	//	PMXモデル
	if (auto pmx_models = root.find("pmx_models"); pmx_models != root.end() && pmx_models->is_array())
	{
		for (const auto& model : *pmx_models)
		{
			size_t model_index = model.value("model_index", static_cast<size_t>(-1));
			if (model_index >= model_instances.size() || !model_instances[model_index].pmx_model)
			{
				++skipped;
				continue;
			}
			auto& model_materials = model_instances[model_index].pmx_model->get_materials();
			for (const auto& m : model.value("materials", json::array()))
			{
				size_t material_index = m.value("index", static_cast<size_t>(-1));
				if (material_index >= model_materials.size() ||
					m.value("name", std::string()) != material_name_to_utf8(model_materials[material_index].name))
				{
					++skipped;
					continue;
				}
				auto& material = model_materials[material_index];
				auto& d = material.default;
				read(m, "pipeline_state_name", material.pipeline_state_name);
				read(m, "material_attribute", material.material_attribute);
				read(m, "diffuse_color", d.diffuse_color);
				read(m, "specular_color", d.specular_color);
				read(m, "edge_color", d.edge_color);
				read(m, "edge_size", d.edge_size);
				read(m, "alpha_clip_threshold", d.alpha_clip_threshold);
				read(m, "alpha_dither_factor", d.alpha_dither_factor);
				read(m, "color_diff_line_factor", d.color_diff_line_factor);
				read(m, "color_diff_line_threshold", d.color_diff_line_threshold);
				read(m, "color_line_adjust_saturate", d.color_line_adjust_saturate);
				read(m, "color_line_adjust_brightness", d.color_line_adjust_brightness);
				read(m, "color_posterize_factor", d.color_posterize_factor);
				read(m, "depth_normal_outline_factor", d.depth_normal_outline_factor);
				read(m, "shadow_threshold_texcoord_offset", d.shadow_threshold_texcoord_offset);
				read(m, "outline_adjust_parameter", d.outline_adjust_parameter);
			}
		}
	}

	//	glTFステージ
	if (auto stages = root.find("gltf_stages"); stages != root.end() && stages->is_array())
	{
		for (const auto& s : *stages)
		{
			size_t stage_index = s.value("stage_index", static_cast<size_t>(-1));
			if (stage_index >= gltf_stages.size())
			{
				++skipped;
				continue;
			}
			auto& stage = gltf_stages[stage_index];
			read(s, "scaling", stage.scaling);
			read(s, "cast_shadow", stage.cast_shadow);
			if (auto r = s.find("rotation"); r != s.end() && r->is_array() && r->size() >= 3)
				stage.rotation = { (*r)[0].get<float>(), (*r)[1].get<float>(), (*r)[2].get<float>() };
			if (auto t = s.find("translation"); t != s.end() && t->is_array() && t->size() >= 3)
				stage.translation = { (*t)[0].get<float>(), (*t)[1].get<float>(), (*t)[2].get<float>() };

			bool gltf_material_changed = false;
			for (const auto& m : s.value("materials", json::array()))
			{
				size_t material_index = m.value("index", static_cast<size_t>(-1));
				if (material_index >= stage.material_constants.size())
				{
					++skipped;
					continue;
				}
				if (auto eizin = m.find("eizin"); eizin != m.end())
				{
					auto& c = stage.material_constants[material_index];
					read(*eizin, "material_attribute", c.material_attribute);
					read(*eizin, "shade_factor", c.shade_factor);
					read(*eizin, "specular_color", c.specular_color);
					read(*eizin, "color_diff_line_factor", c.color_diff_line_factor);
					read(*eizin, "color_diff_line_threshold", c.color_diff_line_threshold);
					read(*eizin, "color_line_adjust_saturate", c.color_line_adjust_saturate);
					read(*eizin, "color_line_adjust_brightness", c.color_line_adjust_brightness);
					read(*eizin, "color_posterize_factor", c.color_posterize_factor);
					read(*eizin, "depth_normal_outline_factor", c.depth_normal_outline_factor);
					read(*eizin, "outline_adjust_parameter", c.outline_adjust_parameter);
				}
				if (auto gltf = m.find("gltf"); gltf != m.end() && material_index < stage.model->materials.size())
				{
					auto& d = stage.model->materials[material_index].data;
					read(*gltf, "basecolor_factor", d.pbr_metallic_roughness.basecolor_factor, 4);
					read(*gltf, "emissive_factor", d.emissive_factor, 3);
					read(*gltf, "metallic_factor", d.pbr_metallic_roughness.metallic_factor);
					read(*gltf, "roughness_factor", d.pbr_metallic_roughness.roughness_factor);
					read(*gltf, "alpha_mode", d.alpha_mode);
					read(*gltf, "alpha_cutoff", d.alpha_cutoff);
					read(*gltf, "normal_scale", d.normal_texture.scale);
					read(*gltf, "occlusion_strength", d.occlusion_texture.strength);
					gltf_material_changed = true;
				}
			}
			if (gltf_material_changed)
				stage.model->update_material_resource(get_renderdata()->immediate_context.Get());
		}
	}

	material_settings_message = std::string("loaded: ") + path + (skipped ? " (skipped " + std::to_string(skipped) + ")" : "");
	return true;
}
