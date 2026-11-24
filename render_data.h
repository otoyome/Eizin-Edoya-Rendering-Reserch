#pragma once
#define NOMINMAX
#include <windows.h>
#include <tchar.h>
#include <sstream>
#include <algorithm>

#include "misc.h"
#include "high_resolution_timer.h"

#ifdef USE_IMGUI
#include "imgui/imgui.h"
#endif

#include <d3d11.h>
#include <wrl.h>
#include <directxmath.h>

CONST LONG SCREEN_WIDTH{ 1600 };
CONST LONG SCREEN_HEIGHT{ 900 };
CONST FLOAT NEAR_CLIP_DISTANCE{ 0.1f };
CONST FLOAT FAR_CLIP_DISTANCE{ 10000.0f };
CONST BOOL FULLSCREEN{ FALSE };
CONST LPWSTR APPLICATION_NAME{ L"X3DGP" };

struct render_data
{
	Microsoft::WRL::ComPtr<ID3D11Device> device;
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> immediate_context;
	Microsoft::WRL::ComPtr<IDXGISwapChain> swap_chain;
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> render_target_view;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilView> depth_stencil_view;

	enum class SAMPLER_STATE { POINT, LINEAR, ANISOTROPIC, POINT_CLAMP, LINEAR_CLAMP, ANISOTROPIC_CLAMP, LINEAR_BORDER_BLACK, LINEAR_BORDER_WHITE };
	Microsoft::WRL::ComPtr<ID3D11SamplerState> sampler_states[8];

	enum class DEPTH_STATE { ZT_ON_ZW_ON, ZT_ON_ZW_OFF, ZT_OFF_ZW_ON, ZT_OFF_ZW_OFF };
	Microsoft::WRL::ComPtr<ID3D11DepthStencilState> depth_stencil_states[4];

	enum class BLEND_STATE { NONE, ALPHA, ADD, MULTIPLY, COLOR_ADD_ALPHA_ADD };
	Microsoft::WRL::ComPtr<ID3D11BlendState> blend_states[5];

	enum class RASTER_STATE { SOLID, SOLID_CULL_FRONT, WIREFRAME, CULL_NONE, WIREFRAME_CULL_NONE };
	Microsoft::WRL::ComPtr<ID3D11RasterizerState> rasterizer_states[5];

	render_data(CONST HWND hwnd);
	~render_data();

	//	CBV設定
	void set_constant_buffer(int start_slot, int num, ID3D11Buffer* const* constant_buffers);

	//	SRVの設定
	void set_shader_resource(int start_slot, int num, ID3D11ShaderResourceView* const* shader_resources);

	//	Samplerの設定
	void set_sampler(int start_slot, int num, ID3D11SamplerState* const* sampler_state);
	void set_default_sampler();

	//	設定していたシェーダーを全て解除する
	void clear_shader_slots();

	//	設定されている頂点バッファを外す
	void clear_vertex_buffer();

	//	設定されているインデックスバッファを外す
	void clear_index_buffer();

	//	描画ターゲットのクリア
	void clear_render_target_slots();

	//	CBVのクリア設定
	void clear_constant_buffers(int start_slot = 0, int num = D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT);

	//	設定していたSRVを全て解除する
	void clear_shader_resorce_views(int start_slot = 0, int num = D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT);

	//	設定していたUAVを全て解除する
	//	FEATURELEVELが11.1ならnumはD3D11_1_UAV_SLOT_COUNTになる
	void clear_unordered_access_views(int start_slot = 0, int num = 8);

	//	設定していたSamplerを全て解除する
	void clear_sampler(int start_slot = 0, int num = D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT);

};
