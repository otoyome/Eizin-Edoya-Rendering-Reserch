#pragma once
#include "scene_base.h"
#include "geometric_primitive.h"

class test2_scene : public scene_base
{
private:
	//	G-Buffer識別子
	enum GBufferId
	{
		GB_BaseColor = 0,
		GB_Emissive,
		GB_Normal_Depth,
		GB_Parameters,
		GB_Velocity,
		GB_UniqueData,

		//	追加分
		GB_ShadeColor,

		GB_Max,
		GB_Start = GB_BaseColor,
	};

	//	ライトの種別
	static const int light_kind_directional_light = 1; //  平行光源
	static const int light_kind_point_light = 2; //  点光源
	static const int light_kind_spot_light = 3; //  スポットライト

	struct directional_light_constants
	{
		directional_lights	directional_light;
		int					use_shadow{ false };		//	影を使用しているかどうか
		float				shadow_attenuation{ 0.5f };	//	影色
		float				shadow_bias{ 0.008f };		//	深度バイアス
		UINT				shadow_mask{ 0 };			//	シャドウマスク(影専用ライトフラグとして利用している)
		float				contact_shadow{ 0 };		//	コンタクトシャドウ算出距離
		DirectX::XMFLOAT3	dummy;
		DirectX::XMFLOAT4X4 light_view_projection;		//	ライトの位置から見た射影行列
	};

	struct point_light_constants
	{
		point_lights		point_light;
		//	ポイントライトを対応したい場合、
		//	スポットライトを応用して作れるが6方向レンダリングするためよく考えて作ること
		//	もしくは全方位シャドウマップを実装するか。

		float				contact_shadow{ 0 };		//	コンタクトシャドウ算出距離
		float				shadow_attenuation{ 0.5f };	//	影色
		DirectX::XMFLOAT2	dummy;
	};

	struct spot_light_constants
	{
		spot_lights			spot_light;
		int					use_shadow{ false };		//	影を使用しているかどうか
		float				shadow_attenuation{ 0.5f };	//	影色
		float				shadow_bias{ 0.003f };		//	深度バイアス
		UINT				shadow_mask{ 0 };			//	シャドウマスク(影専用ライトフラグとして利用している)

		float				contact_shadow{ 0 };		//	コンタクトシャドウ算出距離
		DirectX::XMFLOAT3	dummy;
		DirectX::XMFLOAT4X4 light_view_projection;		//	ライトの位置から見た射影行列
	};

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
		DirectX::XMFLOAT4 work_data[4];
	};

	struct light_constants
	{
		compund_lights compund_light_data;

		//  コンタクトシャドウ
		float contact_shadow;
		DirectX::XMFLOAT3 light_data_dummy;

		//  シャドウマップ関係
		int use_shadow; //	影を使用しているかどうか
		float shadow_attenuation; //	影色
		float shadow_bias; //	深度バイアス
		float shadow_mask; //  シャドウマスク係数
		DirectX::XMFLOAT4X4 light_view_projection; //	ライトの位置から見た射影行列
	};

	struct shadowmap_constants
	{
		DirectX::XMFLOAT4X4 light_view_projection;		//	ライトの位置から見た射影行列
		float				shadow_attenuation{ 0.3f };	//	影色
		float				shadow_bias{ 0.003f };		//	深度バイアス
		DirectX::XMFLOAT2	shadow_dummy;
	};

	struct light_probes
	{
		DirectX::XMFLOAT4 position{ 0, 0, 0, 0 };
		DirectX::XMFLOAT3 area{ 10, 10, 10 };
		bool is_unbind = false;	//	全体用
		float blend_rate = 1.0f;	//	unbind時は無視される
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> diffuse_iem_shader_resource_view;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> specular_pmrem_shader_resource_view;
	};

	struct probe_constants
	{
		DirectX::XMFLOAT4 position;
		DirectX::XMFLOAT3 area;
		float blend_rate;
	};

	struct model_constants
	{
		UINT	shadow_mask;	//	適応シャドウマップマスク値
		UINT	dummy1;
		UINT	dummy2;
		UINT	dummy3;

		//	メッシュの方向
		//  xy : 右ベクトル, zw : 左ベクトル
		DirectX::XMFLOAT4 mesh_direction;
	};

	struct model_instance
	{
		std::unique_ptr<pmx_model>	pmx_model;
	};

	struct model_data
	{
		using	node_animation_datas	= std::vector<pmx_model::node_animation_data>;
		std::size_t				model_index{ 0 };
		UINT					shadow_mask{ 0 };
		DirectX::XMFLOAT3		translation{ 0, 0, 0 };
		DirectX::XMFLOAT3		scaling{ 1, 1, 1 };
		DirectX::XMFLOAT3		rotation{ 0, 0, 0 };
		DirectX::XMFLOAT4X4		world_matrix;
		DirectX::XMFLOAT4X4		previous_world_matrix;

		float					animation_speed{ 1.0f };
		float					animation_time{ 0 };
		node_animation_datas	node_datas;
		std::size_t				follow_light_index{ 0xFFFFFFFFFFFFFFFF };
	};

public:
	test2_scene(CONST HWND hwnd, render_data* renderdata);
	~test2_scene() override;

private:
	bool initialize_core() override;
	bool uninitialize_core() override;
	void update_core(float elapsed_time) override;
	void render_core(float elapsed_time) override;
#ifdef USE_IMGUI
	void imgui_render_core() override;
#endif

private:
	void initialize_scene();

protected:
	//	モデル識別子
	enum ModelId
	{
		Dancer1,
		Dancer2,
		Beach,
		Object1,
		Object2,

		Max,
	};
	static constexpr UINT Dancer1ShadowValue = 0x01;
	static constexpr UINT Dancer2ShadowValue = 0x02;

	//	踊り手(キャラクター)モデルの読み込みとマテリアル設定
	//	派生シーンでオーバーライドすることでキャラクターモデルを差し替える
	virtual void initialize_character_models();

	//	ステージモデルの読み込みと配置
	//	派生シーンでオーバーライドすることでステージを差し替える
	virtual void initialize_stage_models();

	//	glTFステージ用の英人シェーダーパラメーター(eizin_gltf_gbuffer_ps.hlsl と合わせること)
	struct eizin_gltf_constants
	{
		UINT				material_attribute{ 0 };				//	pmx_model::material_attribute と同じビットフラグ
		float				shade_factor{ 0.6f };					//	陰色 = ベースカラー * shade_factor
		DirectX::XMFLOAT2	dummy0{};
		DirectX::XMFLOAT4	specular_color{ 1.0f, 0.0f, 8.0f, 1.0f };	//	粗さ, 金属質, 陰影階調数, ノイズ係数

		float				color_diff_line_factor{ 1.0f };			//	カラーライン係数
		float				color_diff_line_threshold{ 0.005f };	//	カラーライン検知閾値
		float				color_line_adjust_saturate{ 1.0f };		//	ライン彩度調整係数
		float				color_line_adjust_brightness{ 1.0f };	//	ライン明度調整係数

		float				color_posterize_factor{ 24.0f };		//	カラーポスタライズ係数
		float				depth_normal_outline_factor{ 0.02f };	//	深度法線アウトライン閾値
		DirectX::XMFLOAT2	dummy1{};

		//	x,y : カラーアウトラインの係数減衰範囲(最小/最大), z,w : 深度法線アウトライン係数減衰範囲(最小/最大)
		DirectX::XMFLOAT4	outline_adjust_parameter{ 1000.0f, 5000.0f, 1000.0f, 5000.0f };
	};

	struct gltf_stage
	{
		std::unique_ptr<gltf_model>	model;
		DirectX::XMFLOAT3			translation{ 0, 0, 0 };
		DirectX::XMFLOAT3			rotation{ 0, 0, 0 };
		float						scaling{ 1.0f };
		DirectX::XMFLOAT4X4			world_matrix{ 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
		DirectX::XMFLOAT4X4			previous_world_matrix{ 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
		bool						cast_shadow{ true };
		eizin_gltf_constants		constants;
	};

	//	glTFステージを追加する(シェーダー登録も行う)
	gltf_stage& add_gltf_stage(const char* filename);

	//	GBuffer生成用パイプラインステート名
	static const char* get_gbuffer_pipeline_state_name();

private:
	void generate_gbuffer_pass(float elapsed_time);
	void set_deferred_common_resource();
	void deferred_pass(float elapsed_time);
	void shadow_light_pass(float elapsed_time);
	void final_pass(float elapsed_time);

	void directional_shadow_rendering(directional_light_constants& light_data);
	void point_shadow_rendering(point_light_constants& light_data);
	void spot_shadow_rendering(spot_light_constants& light_data);

protected:
	std::vector<model_instance> model_instances;
	std::vector<model_data> model_datas;
	std::vector<int> deferred_models;
	std::vector<int> shadow_models;
	std::vector<gltf_stage> gltf_stages;
	Microsoft::WRL::ComPtr<ID3D11Buffer> eizin_gltf_constant_buffer;

	//	glTFステージ描画
	void render_gltf_stages_gbuffer();
	void render_gltf_stages_shadow(UINT light_shadow_mask);

public:
	//	FXAA(鈴木英人フィルターの前に適応する)
	struct fxaa_constants
	{
		float subpixel_quality = 0.75f;		//	サブピクセルのエイリアス除去量
		float edge_threshold = 0.166f;		//	処理対象とする最小コントラスト
		float edge_threshold_min = 0.0833f;	//	暗部を処理対象外にする閾値
		float dummy = 0.0f;
	};
	bool use_fxaa = true;
	fxaa_constants fxaa_constant;
	Microsoft::WRL::ComPtr<ID3D11Buffer> fxaa_constant_buffer;
	Microsoft::WRL::ComPtr<ID3D11PixelShader> fxaa_pixel_shader;
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> fxaa_render_target_view;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> fxaa_shader_resource_view;
	std::unique_ptr<sprite> fxaa_render_sprite;			//	シーンカラー → FXAA
	std::unique_ptr<sprite> final_pass_fxaa_render_sprite;	//	FXAA結果 → 鈴木英人フィルター
	void fxaa_pass();

public:
	struct color_filter_constants
	{
		float color_filter_weight = 1;
		DirectX::XMFLOAT3 color_filter_dummy;
	};

	float slow_space_time = 0.0f;
	color_filter_constants color_filter_constant;
	Microsoft::WRL::ComPtr<ID3D11Buffer> color_filter_constant_buffer;
	Microsoft::WRL::ComPtr<ID3D11PixelShader> color_filter_pixel_shader;


public:
	std::unique_ptr<geometric_sphere> pointlight_sphere;
	std::unique_ptr<geometric_sphere> spotlight_sphere;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilState> deferred_light_depth_stencil_state;
	Microsoft::WRL::ComPtr<ID3D11VertexShader> geometric_primitive_vertex_shader;
	Microsoft::WRL::ComPtr<ID3D11InputLayout> geometric_primitive_input_layout;
	Microsoft::WRL::ComPtr<ID3D11PixelShader> geometric_primitive_pixel_shader;
	Microsoft::WRL::ComPtr<ID3D11VertexShader> deferred_primitive_lighting_vertex_shader;
	Microsoft::WRL::ComPtr<ID3D11InputLayout> deferred_primitive_lighting_input_layout;
	Microsoft::WRL::ComPtr<ID3D11PixelShader> deferred_primitive_lighting_spot_pixel_shader;
	Microsoft::WRL::ComPtr<ID3D11PixelShader> deferred_primitive_lighting_shadow_spot_light_pixel_shader;
	Microsoft::WRL::ComPtr<ID3D11PixelShader> deferred_primitive_lighting_point_pixel_shader;
	Microsoft::WRL::ComPtr<ID3D11PixelShader> deferred_primitive_lighting_indirect_pixel_shader;
	Microsoft::WRL::ComPtr<ID3D11PixelShader> deferred_primitive_lighting_indirect_inner_pixel_shader;

	std::vector<directional_light_constants> directional_lights;
	std::vector<point_light_constants> point_lights;
	std::vector<spot_light_constants> spot_lights;

	Microsoft::WRL::ComPtr<ID3D11Buffer> probe_constant_constant_buffer;
	Microsoft::WRL::ComPtr<ID3D11Buffer> model_constant_constant_buffer;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilView> shadowmap_depth_stencil_view;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> shadowmap_shader_resource_view;
	Microsoft::WRL::ComPtr<ID3D11SamplerState> shadowmap_sampler_state;

	Microsoft::WRL::ComPtr<ID3D11BlendState> g_buffer_blend_state;
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> g_buffer_render_target_view[GB_Max];
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> g_buffer_shader_resource_view[GB_Max];
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> g_buffer_unordered_access_view[GB_Max];

	std::vector<light_probes> light_probes;
	std::unique_ptr<geometric_cube> light_probe_cube;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> lut_ggx_shader_resource_view;

	Microsoft::WRL::ComPtr<ID3D11VertexShader> sprite_vertex_shader;
	Microsoft::WRL::ComPtr<ID3D11InputLayout> sprite_input_layout;
	Microsoft::WRL::ComPtr<ID3D11PixelShader> sprite_pixel_shader;

	Microsoft::WRL::ComPtr<ID3D11PixelShader> deferred_rendering_emissive_pixel_shader;
	Microsoft::WRL::ComPtr<ID3D11PixelShader> deferred_rendering_indirect_pixel_shader;
	Microsoft::WRL::ComPtr<ID3D11PixelShader> deferred_rendering_directional_pixel_shader;
	Microsoft::WRL::ComPtr<ID3D11PixelShader> deferred_rendering_point_pixel_shader;
	Microsoft::WRL::ComPtr<ID3D11PixelShader> deferred_rendering_spot_pixel_shader;
	Microsoft::WRL::ComPtr<ID3D11PixelShader> deferred_rendering_shadow_spot_light_pixel_shader;
	std::unique_ptr<sprite> deferred_rendering_sprite;

	Microsoft::WRL::ComPtr<ID3D11VertexShader> sky_box_vertex_shader;
	Microsoft::WRL::ComPtr<ID3D11InputLayout> sky_box_input_layout;
	Microsoft::WRL::ComPtr<ID3D11PixelShader> sky_box_pixel_shader;
	std::unique_ptr<sprite> sky_box_render_sprite;

	Microsoft::WRL::ComPtr<ID3D11Buffer> light_constant_buffer;
	Microsoft::WRL::ComPtr<ID3D11Buffer> shadowmap_scene_constant_buffer;
	shadowmap_constants shadowmap_constant;
	Microsoft::WRL::ComPtr<ID3D11Buffer> shadowmap_constant_buffer;

	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> scene_render_target_view;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> scene_shader_resource_view;
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> scene_unordered_access_view;
	Microsoft::WRL::ComPtr<ID3D11PixelShader> final_pass_pixel_shader;
	std::unique_ptr<sprite> final_pass_render_sprite;
};


