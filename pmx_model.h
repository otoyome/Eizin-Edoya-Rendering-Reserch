#pragma once

#include "pmx_loader.h"
#include "vmd_loader.h"

#include <d3d11.h>
#include <wrl.h>
#include <directxmath.h>

#include <vector>
#include <unordered_map>

class pmx_model
{
	static constexpr size_t UseDefaultRamp = 1 << 16;
	static constexpr const char* DefaultShaderName = "pmx_default_shader";
	enum class plane_lock
	{
		free,
		lock,
		plane_x,
		plane_y,
		plane_z,
	};

	struct calculate_node_data
	{
		DirectX::XMFLOAT3 init_scale{ 1, 1, 1 };
		DirectX::XMFLOAT4 init_rotation{ 0, 0, 0, 1 };
		DirectX::XMFLOAT3 init_translation{ 0, 0, 0 };

		DirectX::XMFLOAT4X4 initialize_matrix;

		DirectX::XMFLOAT3 animation_scale{ 1, 1, 1 };
		DirectX::XMFLOAT4 animation_rotation{ 0, 0, 0, 1 };
		DirectX::XMFLOAT3 animation_translation{ 0, 0, 0 };

		DirectX::XMFLOAT4 append_rotation{ 0, 0, 0, 1 };
		DirectX::XMFLOAT3 append_translation{ 0, 0, 0 };

		DirectX::XMFLOAT4 ik_rotation{ 0, 0, 0, 1 };

		DirectX::XMFLOAT3 scale{ 1, 1, 1 };
		DirectX::XMFLOAT4 rotation{ 0, 0, 0, 1 };
		DirectX::XMFLOAT3 position{ 0, 0, 0 };

		DirectX::XMFLOAT4X4 locll_matrix{ 1, 0, 0, 0,  0, 1, 0, 0,  0, 0, 1, 0,  0, 0, 0, 1 };
		DirectX::XMFLOAT4X4 global_matrix{ 1, 0, 0, 0,  0, 1, 0, 0,  0, 0, 1, 0,  0, 0, 0, 1 };

		void begin();
		void update_local_matrix();
		void update_global_matrix(DirectX::XMMATRIX parent_global_matrix);
	};

public:
	struct vertex
	{
		DirectX::XMFLOAT3 position;
		DirectX::XMFLOAT3 normal;
		DirectX::XMFLOAT2 texcoord;
		DirectX::XMFLOAT4 additional_uv[4];
		DirectX::XMUINT4  bone_indices;
		DirectX::XMFLOAT4 bone_weights;
	};

	struct subset
	{
		size_t start;
		size_t count;
		size_t material_index;
	};

	//  マテリアル属性
	enum material_attribute
	{
		indirect_lighting_off		= 1 <<  0,	//	間接光の処理を適応させない

		filter_off					= 1 << 20,	//	フィルター効果の適応フラグ
		normal_depth_outline_off	= 1 << 21,	//	深度/法線による法線描画フラグ
		color_line_off				= 1 << 22,	//	色によるライン描画フラグ
		noise_off					= 1 << 23,	//	英人特有の影部分に対してのノイズ描画フラグ
		color_postarize_off			= 1 << 24,	//	色のポスタライズ適応フラグ
		face_shadow_on				= 1 << 25,	//	顔用シャドウ処理適応フラグ
		xz_plane_lighting			= 1 << 26,	//	XZ平面でのライティング処理適応フラグ
	};

	struct material
	{
		std::string name;
		std::string pipeline_state_name = DefaultShaderName;
		unsigned char draw_flag;
		union
		{
			//	もう既にデフォルトではなくなっているのでどこかで分離が必要
			struct
			{
				UINT diffuse_texture_index;
				UINT sphere_texture_index;
				UINT ramp_texture_index;
				UINT shadow_threshold_texture_index;
				DirectX::XMFLOAT4 diffuse_color;
				DirectX::XMFLOAT4 specular_color;
				DirectX::XMFLOAT4 edge_color;

				float edge_size;
				float alpha_clip_threshold;
				float alpha_dither_factor;

				//  ポストエフェクトの色差によるライン算出係数
				float color_diff_line_factor;		//	アウトライン係数
				float color_diff_line_threshold;	//	検知係数
				float color_line_adjust_saturate;	//  ライン彩度調整係数
				float color_line_adjust_brightness;	//  ライン明度調整係数

				//  色のポスタライズ数
				float color_posterize_factor;
				//  ポストエフェクトの法線/深度によるアウトライン算出係数
				float depth_normal_outline_factor;

				//	影閾値マップのオフセット
				DirectX::XMFLOAT2 shadow_threshold_texcoord_offset;
				float dummy_2;

				DirectX::XMFLOAT4 outline_adjust_parameter;
			} default;
		};

		UINT material_attribute = 0;
		void SetFlags(UINT bit_flag, bool flag)
		{
			if (flag)
				material_attribute |= bit_flag;
			else
				material_attribute &= ~bit_flag;
		}
	};

	struct node
	{
		friend class pmx_model;

		std::string name;
		size_t level = 0;
		size_t parent_node_index = pmx_invalid_target_id;
		size_t append_node_index = pmx_invalid_target_id;
		size_t ik_node_index = pmx_invalid_target_id;

		DirectX::XMFLOAT4X4 offset_matrix;
		DirectX::XMFLOAT4X4 local_matrix{ 1, 0, 0, 0,  0, 1, 0, 0,  0, 0, 1, 0,  0, 0, 0, 1 };
		DirectX::XMFLOAT4X4 global_matrix{ 1, 0, 0, 0,  0, 1, 0, 0,  0, 0, 1, 0,  0, 0, 0, 1 };

		calculate_node_data calculate_data;
	};

	struct append_data
	{
		size_t source_index;
		size_t target_index;
		float weight;
		bool is_rotation;
		bool is_translation;
	};

	struct ik_link_data
	{
		size_t target_index;
		plane_lock lock = plane_lock::free;
		DirectX::XMFLOAT3 angle_max{ 1, 1, 1 };
		DirectX::XMFLOAT3 angle_min{ 1, 1, 1 };

		float plane_mode_angle = 0;
		DirectX::XMFLOAT3 prev_angle = {0, 0, 0};
	};

	struct ik_data
	{
		size_t source_index = -1;
		size_t target_index = -1;

		int loop;
		float angile_limit;
		size_t ik_link_start = -1;
		size_t ik_link_end = -1;

		bool enable;
	};
	struct node_animation
	{
		float time;
		//	MMDに拡縮は無い。
		DirectX::XMFLOAT4 rotation{ 0, 0, 0, 1 };
		DirectX::XMFLOAT3 translation{ 0, 0, 0 };
	};

	struct animation
	{
		//	MMDは30FPSを基準として作られている。
		std::string name;
		float end_time;
		std::vector<std::vector<node_animation>>	nodes;
	};

#define BONE_MAX 1000
	struct primitive_constants
	{
		DirectX::XMFLOAT4X4 world;
		DirectX::XMFLOAT4X4 previous_world;
		DirectX::XMINT4 texture_index;
		DirectX::XMFLOAT4 diffuse_color;
		DirectX::XMFLOAT4 specular_color;
		DirectX::XMFLOAT4 edge_color;

		float edge_size;
		float alpha_clip_threshold = 0.01f;
		float alpha_dither_factor = 0.5f;
		UINT material_attribute;

		//  ポストエフェクトの色差によるライン算出係数
		float color_diff_line_factor;
		float color_diff_line_threshold;
		float color_line_adjust_saturate;
		float color_line_adjust_brightness;

		//  色のポスタライズ数
		float color_posterize_factor;
		//  ポストエフェクトの法線/深度によるアウトライン算出係数
		float depth_normal_outline_factor;

		//	影閾値マップのオフセット
		DirectX::XMFLOAT2 shadow_threshold_texcoord_offset;

		DirectX::XMFLOAT4 outline_adjust_parameter;

		DirectX::XMFLOAT4X4 node_transforms[BONE_MAX];
	};

public:
	pmx_model(ID3D11Device* device, const std::string& filename);
	~pmx_model();

	struct node_animation_data
	{
		DirectX::XMFLOAT3 scale{1, 1, 1};
		DirectX::XMFLOAT4 rotation{ 0, 0, 0, 1 };
		DirectX::XMFLOAT3 translation{ 0, 0, 0 };
	};
	void compute_animation(std::string_view animation_name, float time, std::vector<node_animation_data>& node_datas);
	void update_transforms(std::vector<node_animation_data>& node_datas);
	void render(ID3D11DeviceContext* immediate_context, const DirectX::XMFLOAT4X4& world, const DirectX::XMFLOAT4X4& previous_world, std::string_view override_pipeline_stae = "", bool wire = false);

	bool add_vmd_animation(const char* motion_name, const char* filepath);
	bool add_vmd_animation(const char* motion_name, const vmd_loader& loader);
	UINT add_texture(ID3D11Device* device, const char* filepath);

	std::vector<std::string> get_animation_names();
	animation* search_animation(std::string_view name);
	node* search_node(std::string_view name);
	std::vector<node>& get_nodes(){ return nodes; }
	std::vector<material>& get_materials(){ return materials; }
	std::vector<append_data>& get_append_datas(){ return append_datas; }
	std::vector<ik_data>& get_ik_datas(){ return ik_datas; }
	std::vector<ik_link_data>& get_ik_link_datas(){ return ik_link_datas; }
	std::vector<std::string>& get_texture_names(){ return texture_names; }
	std::vector<Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>>& get_texture_resource_views(){ return texture_resource_views; }

public:
	// 描画シェーダー登録用構造体
	// 授業ようにかなり強引に作っているのでマネしちゃだめ
	// シェーダーマネージャーなどを作る方が良い。
	struct pipeline_state_desc
	{
		std::string name;		//	シェーダーセット名

		std::string vs_path;	//	頂点シェーダーファイルパス
		std::string hs_path;	//	ハルシェーダーファイルパス
		std::string ds_path;	//	ドメインシェーダーファイルパス
		std::string gs_path;	//	ジオメトリシェーダーファイルパス
		std::string ps_path;	//	ピクセルシェーダーファイルパス
		D3D_PRIMITIVE_TOPOLOGY primitive_toporogy = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;	//	図形指定
	};
	struct pipeline_state
	{
		Microsoft::WRL::ComPtr<ID3D11VertexShader> vertex_shader;
		Microsoft::WRL::ComPtr<ID3D11HullShader> hull_shader;
		Microsoft::WRL::ComPtr<ID3D11DomainShader> domain_shader;
		Microsoft::WRL::ComPtr<ID3D11GeometryShader> geometry_shader;
		Microsoft::WRL::ComPtr<ID3D11PixelShader> pixel_shader;

		Microsoft::WRL::ComPtr<ID3D11InputLayout> input_layout;
		D3D_PRIMITIVE_TOPOLOGY primitive_toporogy;
	};
	std::unordered_map<std::string, pipeline_state> pipeline_states;
	bool add_pipeline_state(ID3D11Device* device, pipeline_state_desc desc);

private:
	void update_ik(size_t node_index);
	void update_ik_plane(int iteration, ik_data& ik_data, ik_link_data& link_data, plane_lock axis);
	void update_node_transforms(size_t node_index);

	void update_append(size_t node_index);

private:
	std::vector<node> nodes;
	std::vector<subset> subsets;
	std::vector<size_t> levels;
	Microsoft::WRL::ComPtr<ID3D11Buffer> vertex_buffer;
	Microsoft::WRL::ComPtr<ID3D11Buffer> index_buffer;
	std::vector<material> materials;
	std::vector<append_data> append_datas;
	std::vector<ik_data> ik_datas;
	std::vector<ik_link_data> ik_link_datas;

	std::unordered_map<std::string, animation> animations;

	std::vector<std::string> texture_names;
	std::vector<Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>> texture_resource_views;
	std::vector<Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>> default_toon_texture_resource_views;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> dummy_texture_resource_views;
	Microsoft::WRL::ComPtr<ID3D11SamplerState> sampler_state;

	enum class RASTER_STATE { SOLID, CULL_NONE, WIRE_SOLID, WIRE_CULL_NONE };
	Microsoft::WRL::ComPtr<ID3D11RasterizerState> rasterizer_states[4];
	Microsoft::WRL::ComPtr<ID3D11Buffer> primitive_cbuffer;
};
