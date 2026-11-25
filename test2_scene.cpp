#include "test2_scene.h"

#include "implot/implot.h"

//	SMAA用ルックアップテクスチャ
#include "subpixel_morphological_anti_aliasing/Textures/AreaTex.h"
#include "subpixel_morphological_anti_aliasing/Textures/SearchTex.h"

//	
#define	USE_EIZIN_SHADER

#define	CHECK_SETTING

namespace
{
	std::string cp932_to_utf8(const std::string& cp932)
	{
		const int wide_length = MultiByteToWideChar(932, 0, cp932.data(), static_cast<int>(cp932.size()), nullptr, 0);
		std::wstring wide(wide_length, L'\0');
		MultiByteToWideChar(932, 0, cp932.data(), static_cast<int>(cp932.size()), wide.data(), wide_length);
		const int utf8_length = WideCharToMultiByte(CP_UTF8, 0, wide.data(), wide_length, nullptr, 0, nullptr, nullptr);
		std::string utf8(utf8_length, '\0');
		WideCharToMultiByte(CP_UTF8, 0, wide.data(), wide_length, utf8.data(), utf8_length, nullptr, nullptr);
		return utf8;
	}

	enum SHADER_NAMES
	{
		SHADOW_CASTER,
		GENARATE_GBUFFER,
		EIZIN_WAVE,
	};

	CONST char* pmx_model_shader_names[] =
	{
		"shadow_caster",
		"eizin_generate_gbuffer",
		"eizin_generate_gbuffer_wave",
	};

	//	glTFステージ用シェーダー名
	CONST char* gltf_gbuffer_shader_name = "eizin_gltf_generate_gbuffer";
	CONST char* gltf_shadow_caster_shader_name = "gltf_shadow_caster";

	//	glTFステージ用マテリアル毎英人パラメーターのスロット(eizin_gltf_gbuffer_ps.hlsl と合わせること)
	static constexpr int EizinGltfMaterialSRVIndex = 20;

	//	アンチエイリアス用の定数バッファスロット(各シェーダーと合わせること)
	static constexpr int FxaaCBVIndex = 12;
	static constexpr int SmaaCBVIndex = 13;
	static constexpr int TaaCBVIndex = 3;

	//	ハルトン数列によるサブピクセルジッター(-0.5～+0.5ピクセル)
	DirectX::XMFLOAT2 calculate_halton_jitter(int frame_index)
	{
		static constexpr int jitter_sequence_length = 8;
		auto halton = [](int index, int base)
		{
			float result = 0.0f;
			float fraction = 1.0f / base;
			while (index > 0)
			{
				result += (index % base) * fraction;
				index /= base;
				fraction /= base;
			}
			return result;
		};
		int index = (frame_index % jitter_sequence_length) + 1;
		return { halton(index, 2) - 0.5f, halton(index, 3) - 0.5f };
	}

	CONST LONG SHADOWMAP_WIDTH{ 4096 };
	CONST LONG SHADOWMAP_HEIGHT{ 4096 };
	CONST float SHADOWMAP_DRAWRECT{ 100 };
	static constexpr int IBLSRVIndex[3] = { 33, 34, 35 };

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	int lerp(int a, int b, float value)
	{
		return (int)(a * (1 - value) + b * value);
	}
	float lerp(float a, float b, float value)
	{
		return a * (1 - value) + b * value;
	}
	DirectX::XMFLOAT2 lerp(DirectX::XMFLOAT2 a, DirectX::XMFLOAT2 b, float value)
	{
		return	{ a.x * (1 - value) + b.x * value
				, a.y * (1 - value) + b.y * value };
	}
	DirectX::XMFLOAT3 lerp(DirectX::XMFLOAT3 a, DirectX::XMFLOAT3 b, float value)
	{
		return	{ a.x * (1 - value) + b.x * value
				, a.y * (1 - value) + b.y * value
				, a.z * (1 - value) + b.z * value };
	}
	DirectX::XMFLOAT4 lerp(DirectX::XMFLOAT4 a, DirectX::XMFLOAT4 b, float value)
	{
		return	{ a.x * (1 - value) + b.x * value
				, a.y * (1 - value) + b.y * value
				, a.z * (1 - value) + b.z * value
				, a.w * (1 - value) + b.w * value };
	}
}

test2_scene::test2_scene(CONST HWND hwnd, render_data* renderdata)
: scene_base(hwnd, renderdata)
{
#if defined(CHECK_SETTING)
	set_camera_rotate_y(DirectX::XM_PI);
	set_camera_focus({ 0, 20, 0 });
	set_camera_distance(50.0f);
#else	//	defined(CHECK_SETTING)
	set_camera_rotate_x(get_camera_rotate_x() + DirectX::XM_PI * 0.01f);
	set_camera_rotate_y(DirectX::XM_PI + DirectX::XM_PI / 2 + DirectX::XM_PI / 7.0f);
	set_camera_distance(175.0f);
	set_camera_focus({0, 10, 0});
#endif	//	defined(CHECK_SETTING)
}

test2_scene::~test2_scene()
{
}

bool test2_scene::initialize_core()
{
	HRESULT hr{ S_OK };

	//	定数バッファの生成
	{
		D3D11_BUFFER_DESC buffer_desc{};
		buffer_desc.Usage = D3D11_USAGE_DEFAULT;
		buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		buffer_desc.CPUAccessFlags = 0;
		buffer_desc.MiscFlags = 0;
		buffer_desc.StructureByteStride = 0;
		//	ライト用定数バッファ
		{
			buffer_desc.ByteWidth = sizeof(light_constants);
			hr = get_renderdata()->device->CreateBuffer(&buffer_desc, nullptr, light_constant_buffer.GetAddressOf());
			_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
		}
		//	シャドウマップ用定数バッファ
		{
			buffer_desc.ByteWidth = sizeof(shadowmap_constants);
			hr = get_renderdata()->device->CreateBuffer(&buffer_desc, nullptr, shadowmap_constant_buffer.GetAddressOf());
			_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
		}
		//	シャドウマップ用シーン定数バッファ
		{
			buffer_desc.ByteWidth = sizeof(scene_constants);
			hr = get_renderdata()->device->CreateBuffer(&buffer_desc, nullptr, shadowmap_scene_constant_buffer.GetAddressOf());
			_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
		}
		//	プローブ用定数バッファ
		{
			buffer_desc.ByteWidth = sizeof(probe_constants);
			hr = get_renderdata()->device->CreateBuffer(&buffer_desc, nullptr, probe_constant_constant_buffer.GetAddressOf());
			_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
		}
		//	モデル用定数バッファ
		{
			buffer_desc.ByteWidth = sizeof(model_constants);
			hr = get_renderdata()->device->CreateBuffer(&buffer_desc, nullptr, model_constant_constant_buffer.GetAddressOf());
			_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
		}
		//	カラーフィルター用定数バッファ
		{
			buffer_desc.ByteWidth = sizeof(color_filter_constants);
			hr = get_renderdata()->device->CreateBuffer(&buffer_desc, nullptr, color_filter_constant_buffer.GetAddressOf());
			_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
		}
		//	FXAA用定数バッファ
		{
			buffer_desc.ByteWidth = sizeof(fxaa_constants);
			hr = get_renderdata()->device->CreateBuffer(&buffer_desc, nullptr, fxaa_constant_buffer.GetAddressOf());
			_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
		}
		//	SMAA用定数バッファ
		{
			buffer_desc.ByteWidth = sizeof(smaa_constants);
			hr = get_renderdata()->device->CreateBuffer(&buffer_desc, nullptr, smaa_constant_buffer.GetAddressOf());
			_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
		}
		//	TAA用定数バッファ
		{
			buffer_desc.ByteWidth = sizeof(taa_constants);
			hr = get_renderdata()->device->CreateBuffer(&buffer_desc, nullptr, taa_constant_buffer.GetAddressOf());
			_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
		}
	}

	//	シャドウマップ準備
	{
		Microsoft::WRL::ComPtr<ID3D11Texture2D> depth_buffer{};
		D3D11_TEXTURE2D_DESC texture2d_desc{};
		texture2d_desc.Width = SHADOWMAP_WIDTH;
		texture2d_desc.Height = SHADOWMAP_HEIGHT;
		texture2d_desc.MipLevels = 1;
		texture2d_desc.ArraySize = 1;
		texture2d_desc.Format = DXGI_FORMAT_R32_TYPELESS;
		texture2d_desc.SampleDesc.Count = 1;
		texture2d_desc.SampleDesc.Quality = 0;
		texture2d_desc.Usage = D3D11_USAGE_DEFAULT;
		texture2d_desc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
		texture2d_desc.CPUAccessFlags = 0;
		texture2d_desc.MiscFlags = 0;
		hr = get_renderdata()->device->CreateTexture2D(&texture2d_desc, NULL, depth_buffer.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

		//	深度ステンシルビュー生成
		D3D11_DEPTH_STENCIL_VIEW_DESC depth_stencil_view_desc{};
		depth_stencil_view_desc.Format = DXGI_FORMAT_D32_FLOAT;
		depth_stencil_view_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
		depth_stencil_view_desc.Texture2D.MipSlice = 0;
		hr = get_renderdata()->device->CreateDepthStencilView(depth_buffer.Get(),
			&depth_stencil_view_desc,
			shadowmap_depth_stencil_view.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

		//	シェーダーリソースビュー生成
		D3D11_SHADER_RESOURCE_VIEW_DESC shader_resource_view_desc{};
		shader_resource_view_desc.Format = DXGI_FORMAT_R32_FLOAT;
		shader_resource_view_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		shader_resource_view_desc.Texture2D.MostDetailedMip = 0;
		shader_resource_view_desc.Texture2D.MipLevels = 1;
		hr = get_renderdata()->device->CreateShaderResourceView(depth_buffer.Get(),
			&shader_resource_view_desc,
			shadowmap_shader_resource_view.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

		//	シャドウマップ用サンプラー
		D3D11_SAMPLER_DESC sampler_desc{};
		sampler_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
		sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
		sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
		sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
		sampler_desc.MipLODBias = 0;
		sampler_desc.MaxAnisotropy = 16;
		sampler_desc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
		sampler_desc.BorderColor[0] = FLT_MAX;
		sampler_desc.BorderColor[1] = FLT_MAX;
		sampler_desc.BorderColor[2] = FLT_MAX;
		sampler_desc.BorderColor[3] = FLT_MAX;
		sampler_desc.MinLOD = 0;
		sampler_desc.MaxLOD = D3D11_FLOAT32_MAX;
		hr = get_renderdata()->device->CreateSamplerState(&sampler_desc, shadowmap_sampler_state.GetAddressOf());
	}

	//	GBuffer生成
	{
		D3D11_TEXTURE2D_DESC texture2d_desc{};
		texture2d_desc.Width = SCREEN_WIDTH;
		texture2d_desc.Height = SCREEN_HEIGHT;
		texture2d_desc.MipLevels = 1;
		texture2d_desc.ArraySize = 1;
		texture2d_desc.SampleDesc.Count = 1;
		texture2d_desc.SampleDesc.Quality = 0;
		texture2d_desc.Usage = D3D11_USAGE_DEFAULT;
		texture2d_desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
		texture2d_desc.CPUAccessFlags = 0;
		texture2d_desc.MiscFlags = 0;

		DXGI_FORMAT formats[] =
		{
			DXGI_FORMAT_R8G8B8A8_UNORM,			//	GB_BaseColor
			DXGI_FORMAT_R8G8B8A8_UNORM,			//	GB_Emissive
			DXGI_FORMAT_R32G32B32A32_FLOAT,		//	GB_Normal_Depth
			DXGI_FORMAT_R8G8B8A8_UNORM,			//	GB_Parameters
			DXGI_FORMAT_R32G32B32A32_FLOAT,		//	GB_Velocity
			DXGI_FORMAT_R32G32B32A32_UINT,		//	GB_UniqueData

			//	追加分
			DXGI_FORMAT_R8G8B8A8_UNORM,			//	GB_ShadeColor
		};
		for (int i = GB_BaseColor; i < GB_Max; ++i)
		{
			texture2d_desc.Format = formats[i];

			Microsoft::WRL::ComPtr<ID3D11Texture2D> color_buffer{};
			hr = get_renderdata()->device->CreateTexture2D(&texture2d_desc, NULL, color_buffer.GetAddressOf());
			_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
			//	レンダーターゲットビュー生成
			hr = get_renderdata()->device->CreateRenderTargetView(color_buffer.Get(), NULL, g_buffer_render_target_view[i].GetAddressOf());
			_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
			//	シェーダーリソースビュー生成
			hr = get_renderdata()->device->CreateShaderResourceView(color_buffer.Get(), NULL, g_buffer_shader_resource_view[i].GetAddressOf());
			_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
			//	アンオーダードアクセスビュー生成
			hr = get_renderdata()->device->CreateUnorderedAccessView(color_buffer.Get(), NULL, g_buffer_unordered_access_view[i].GetAddressOf());
			_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
		}

		//	GBuffer用ブレンドステート生成
		//	GBufferの作成の際はアルファブレンドを機能させるとおかしなことになるためブレンド無し設定
		D3D11_BLEND_DESC blend_desc{};
		blend_desc.AlphaToCoverageEnable = FALSE;
		blend_desc.IndependentBlendEnable = FALSE;
		for (int i = GB_BaseColor; i < GB_Max; ++i)
		{
			blend_desc.RenderTarget[i].BlendEnable = FALSE;
			blend_desc.RenderTarget[i].SrcBlend = D3D11_BLEND_ONE;
			blend_desc.RenderTarget[i].DestBlend = D3D11_BLEND_ZERO;
			blend_desc.RenderTarget[i].BlendOp = D3D11_BLEND_OP_ADD;
			blend_desc.RenderTarget[i].SrcBlendAlpha = D3D11_BLEND_ONE;
			blend_desc.RenderTarget[i].DestBlendAlpha = D3D11_BLEND_ZERO;
			blend_desc.RenderTarget[i].BlendOpAlpha = D3D11_BLEND_OP_ADD;
			blend_desc.RenderTarget[i].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		}
		hr = get_renderdata()->device->CreateBlendState(&blend_desc, g_buffer_blend_state.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr)); 
	}

	//	シーンバッファ生成
	{
		D3D11_TEXTURE2D_DESC texture2d_desc{};
		texture2d_desc.Width = SCREEN_WIDTH;
		texture2d_desc.Height = SCREEN_HEIGHT;
		texture2d_desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		texture2d_desc.MipLevels = 1;
		texture2d_desc.ArraySize = 1;
		texture2d_desc.SampleDesc.Count = 1;
		texture2d_desc.SampleDesc.Quality = 0;
		texture2d_desc.Usage = D3D11_USAGE_DEFAULT;
		texture2d_desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
		texture2d_desc.CPUAccessFlags = 0;
		texture2d_desc.MiscFlags = 0;

		Microsoft::WRL::ComPtr<ID3D11Texture2D> color_buffer{};
		hr = get_renderdata()->device->CreateTexture2D(&texture2d_desc, NULL, color_buffer.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
		//	レンダーターゲットビュー生成
		hr = get_renderdata()->device->CreateRenderTargetView(color_buffer.Get(), NULL, scene_render_target_view.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
		//	シェーダーリソースビュー生成
		hr = get_renderdata()->device->CreateShaderResourceView(color_buffer.Get(), NULL, scene_shader_resource_view.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
		//	アンオーダードアクセスビュー生成
		hr = get_renderdata()->device->CreateUnorderedAccessView(color_buffer.Get(), NULL, scene_unordered_access_view.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

		//	鈴木英人フィルター出力先(アンチエイリアスの入力。ガンマ補正済みの色が入る)
		{
			D3D11_TEXTURE2D_DESC filtered_desc = texture2d_desc;
			filtered_desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
			Microsoft::WRL::ComPtr<ID3D11Texture2D> filtered_buffer{};
			hr = get_renderdata()->device->CreateTexture2D(&filtered_desc, NULL, filtered_buffer.GetAddressOf());
			_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
			hr = get_renderdata()->device->CreateRenderTargetView(filtered_buffer.Get(), NULL, filtered_render_target_view.GetAddressOf());
			_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
			hr = get_renderdata()->device->CreateShaderResourceView(filtered_buffer.Get(), NULL, filtered_shader_resource_view.GetAddressOf());
			_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
		}

		//	アンチエイリアス出力先(シーンカラーと同じ形式。TAAはUAVで書き込む)
		{
			Microsoft::WRL::ComPtr<ID3D11Texture2D> antialias_buffer{};
			hr = get_renderdata()->device->CreateTexture2D(&texture2d_desc, NULL, antialias_buffer.GetAddressOf());
			_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
			hr = get_renderdata()->device->CreateRenderTargetView(antialias_buffer.Get(), NULL, antialias_render_target_view.GetAddressOf());
			_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
			hr = get_renderdata()->device->CreateShaderResourceView(antialias_buffer.Get(), NULL, antialias_shader_resource_view.GetAddressOf());
			_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
			hr = get_renderdata()->device->CreateUnorderedAccessView(antialias_buffer.Get(), NULL, antialias_unordered_access_view.GetAddressOf());
			_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
		}

		//	TAA履歴バッファ(2枚を交互に使う)
		for (int index = 0; index < 2; ++index)
		{
			D3D11_TEXTURE2D_DESC history_desc = texture2d_desc;
			history_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
			Microsoft::WRL::ComPtr<ID3D11Texture2D> history_buffer{};
			hr = get_renderdata()->device->CreateTexture2D(&history_desc, NULL, history_buffer.GetAddressOf());
			_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
			hr = get_renderdata()->device->CreateShaderResourceView(history_buffer.Get(), NULL, taa_accumlate_shader_resource_view[index].GetAddressOf());
			_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
			hr = get_renderdata()->device->CreateUnorderedAccessView(history_buffer.Get(), NULL, taa_accumlate_unordered_access_view[index].GetAddressOf());
			_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
		}

		//	SMAA用バッファ(エッジ : R8G8, ブレンド係数 : R8G8B8A8)
		{
			D3D11_TEXTURE2D_DESC smaa_desc = texture2d_desc;
			smaa_desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
			DXGI_FORMAT formats[smaa_buffer_max] = { DXGI_FORMAT_R8G8_UNORM, DXGI_FORMAT_R8G8B8A8_UNORM };
			for (int index = 0; index < smaa_buffer_max; ++index)
			{
				smaa_desc.Format = formats[index];
				Microsoft::WRL::ComPtr<ID3D11Texture2D> smaa_buffer{};
				hr = get_renderdata()->device->CreateTexture2D(&smaa_desc, NULL, smaa_buffer.GetAddressOf());
				_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
				hr = get_renderdata()->device->CreateRenderTargetView(smaa_buffer.Get(), NULL, smaa_buffer_render_target_view[index].GetAddressOf());
				_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
				hr = get_renderdata()->device->CreateShaderResourceView(smaa_buffer.Get(), NULL, smaa_buffer_shader_resource_view[index].GetAddressOf());
				_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
			}
		}

		//	SMAA用ルックアップテクスチャ(Area / Search)
		{
			D3D11_TEXTURE2D_DESC lookup_desc{};
			lookup_desc.MipLevels = 1;
			lookup_desc.ArraySize = 1;
			lookup_desc.SampleDesc.Count = 1;
			lookup_desc.Usage = D3D11_USAGE_IMMUTABLE;
			lookup_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

			Microsoft::WRL::ComPtr<ID3D11Texture2D> lookup_texture{};
			D3D11_SUBRESOURCE_DATA initialize_data{};

			lookup_desc.Width = AREATEX_WIDTH;
			lookup_desc.Height = AREATEX_HEIGHT;
			lookup_desc.Format = DXGI_FORMAT_R8G8_UNORM;
			initialize_data.pSysMem = areaTexBytes;
			initialize_data.SysMemPitch = AREATEX_PITCH;
			hr = get_renderdata()->device->CreateTexture2D(&lookup_desc, &initialize_data, lookup_texture.ReleaseAndGetAddressOf());
			_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
			hr = get_renderdata()->device->CreateShaderResourceView(lookup_texture.Get(), NULL, smaa_area_shader_resource_view.GetAddressOf());
			_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

			lookup_desc.Width = SEARCHTEX_WIDTH;
			lookup_desc.Height = SEARCHTEX_HEIGHT;
			lookup_desc.Format = DXGI_FORMAT_R8_UNORM;
			initialize_data.pSysMem = searchTexBytes;
			initialize_data.SysMemPitch = SEARCHTEX_PITCH;
			hr = get_renderdata()->device->CreateTexture2D(&lookup_desc, &initialize_data, lookup_texture.ReleaseAndGetAddressOf());
			_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
			hr = get_renderdata()->device->CreateShaderResourceView(lookup_texture.Get(), NULL, smaa_search_shader_resource_view.GetAddressOf());
			_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
		}
	}

	//	描画範囲制限用モデル準備
	{
		pointlight_sphere = std::make_unique<geometric_sphere>(get_renderdata()->device.Get(), 16, 16);
		spotlight_sphere = std::make_unique<geometric_sphere>(get_renderdata()->device.Get(), 16, 16);

		//	制限用ステンシルステート生成
		//	深度比較は行うが書き込まない(後で使えなくなるので)
		D3D11_DEPTH_STENCIL_DESC depth_stencil_desc{};
		depth_stencil_desc.DepthEnable = TRUE;
		depth_stencil_desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		depth_stencil_desc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;

		depth_stencil_desc.StencilEnable = TRUE;

		depth_stencil_desc.StencilReadMask = 0xFF;
		depth_stencil_desc.StencilWriteMask = 0xFF;
		depth_stencil_desc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;		//	ステンシルステートは常に合格
		depth_stencil_desc.BackFace.StencilPassOp = D3D11_STENCIL_OP_REPLACE;	//	合格したら1を入れる
		depth_stencil_desc.BackFace.StencilFailOp = D3D11_STENCIL_OP_ZERO;
		depth_stencil_desc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_ZERO;
		depth_stencil_desc.FrontFace.StencilFunc = D3D11_COMPARISON_EQUAL;	//	ステンシルステートは指定
		depth_stencil_desc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_ZERO;
		depth_stencil_desc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_ZERO;
		depth_stencil_desc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_ZERO;
		hr = get_renderdata()->device->CreateDepthStencilState(&depth_stencil_desc, deferred_light_depth_stencil_state.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
	}

	//	ジオメトリプリミティブ用シェーダー準備
	{
		D3D11_INPUT_ELEMENT_DESC input_element_desc[]
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		};
		create_vs_from_cso(get_renderdata()->device.Get(), "geometric_primitive_vs.cso", geometric_primitive_vertex_shader.GetAddressOf(), geometric_primitive_input_layout.GetAddressOf(), input_element_desc, ARRAYSIZE(input_element_desc));
		create_ps_from_cso(get_renderdata()->device.Get(), "geometric_primitive_ps.cso", geometric_primitive_pixel_shader.GetAddressOf());

		//	ライティング範囲制限用シェーダー準備
		create_vs_from_cso(get_renderdata()->device.Get(), "deferred_primitive_lighting_vs.cso", deferred_primitive_lighting_vertex_shader.GetAddressOf(), deferred_primitive_lighting_input_layout.GetAddressOf(), input_element_desc, ARRAYSIZE(input_element_desc));
		create_ps_from_cso(get_renderdata()->device.Get(), "deferred_primitive_lighting_point_ps.cso", deferred_primitive_lighting_point_pixel_shader.GetAddressOf());
		create_ps_from_cso(get_renderdata()->device.Get(), "deferred_primitive_lighting_spot_ps.cso", deferred_primitive_lighting_spot_pixel_shader.GetAddressOf());
		create_ps_from_cso(get_renderdata()->device.Get(), "deferred_primitive_lighting_shadow_spot_light_ps.cso", deferred_primitive_lighting_shadow_spot_light_pixel_shader.GetAddressOf());
		create_ps_from_cso(get_renderdata()->device.Get(), "deferred_primitive_lighting_indirect_ps.cso", deferred_primitive_lighting_indirect_pixel_shader.GetAddressOf());
	}

	//	スプライトシェーダー準備
	{
		D3D11_INPUT_ELEMENT_DESC input_element_desc[]
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		};
		create_vs_from_cso(get_renderdata()->device.Get(), "sprite_vs.cso", sprite_vertex_shader.GetAddressOf(), sprite_input_layout.GetAddressOf(), input_element_desc, _countof(input_element_desc));
		create_ps_from_cso(get_renderdata()->device.Get(), "sprite_ps.cso", sprite_pixel_shader.GetAddressOf());

		//	空描画用シェーダー
		create_vs_from_cso(get_renderdata()->device.Get(), "sky_box_vs.cso", sky_box_vertex_shader.GetAddressOf(), sky_box_input_layout.GetAddressOf(), input_element_desc, _countof(input_element_desc));
	//	create_ps_from_cso(get_renderdata()->device.Get(), "sky_box_gbuffer_ps.cso", sky_box_pixel_shader.GetAddressOf());
		create_ps_from_cso(get_renderdata()->device.Get(), "eizin_sky_box_gbuffer_ps.cso", sky_box_pixel_shader.GetAddressOf());

		//	遅延ライティングシェーダー
		create_ps_from_cso(get_renderdata()->device.Get(), "deferred_rendering_emissive_ps.cso", deferred_rendering_emissive_pixel_shader.GetAddressOf());
		create_ps_from_cso(get_renderdata()->device.Get(), "deferred_rendering_indirect_ps.cso", deferred_rendering_indirect_pixel_shader.GetAddressOf());
		create_ps_from_cso(get_renderdata()->device.Get(), "deferred_rendering_directional_ps.cso", deferred_rendering_directional_pixel_shader.GetAddressOf());
		create_ps_from_cso(get_renderdata()->device.Get(), "deferred_rendering_point_ps.cso", deferred_rendering_point_pixel_shader.GetAddressOf());
		create_ps_from_cso(get_renderdata()->device.Get(), "deferred_rendering_spot_ps.cso", deferred_rendering_spot_pixel_shader.GetAddressOf());
		create_ps_from_cso(get_renderdata()->device.Get(), "deferred_rendering_shadow_spot_light_ps.cso", deferred_rendering_shadow_spot_light_pixel_shader.GetAddressOf());

		create_ps_from_cso(get_renderdata()->device.Get(), "deferred_primitive_lighting_indirect_inner_ps.cso", deferred_primitive_lighting_indirect_inner_pixel_shader.GetAddressOf());

		//	ファイナルパス
		create_ps_from_cso(get_renderdata()->device.Get(), "final_pass_ps.cso", final_pass_pixel_shader.GetAddressOf());

		//	カラーフィルター
		create_ps_from_cso(get_renderdata()->device.Get(), "color_filter_ps.cso", color_filter_pixel_shader.GetAddressOf());

		//	FXAA
		create_ps_from_cso(get_renderdata()->device.Get(), "fxaa_ps.cso", fxaa_pixel_shader.GetAddressOf());

		//	SMAA(頂点はSV_VertexIDから生成するので入力レイアウトなし)
		create_vs_from_cso(get_renderdata()->device.Get(), "smaa_edge_detection_vs.cso", smaa_edge_detection_vertex_shader.GetAddressOf(), nullptr, nullptr, 0);
		create_ps_from_cso(get_renderdata()->device.Get(), "smaa_edge_detection_ps.cso", smaa_edge_detection_pixel_shader.GetAddressOf());
		create_vs_from_cso(get_renderdata()->device.Get(), "smaa_blending_weight_calculation_vs.cso", smaa_blending_weight_calculation_vertex_shader.GetAddressOf(), nullptr, nullptr, 0);
		create_ps_from_cso(get_renderdata()->device.Get(), "smaa_blending_weight_calculation_ps.cso", smaa_blending_weight_calculation_pixel_shader.GetAddressOf());
		create_vs_from_cso(get_renderdata()->device.Get(), "smaa_neighborhood_blending_vs.cso", smaa_neighborhood_blending_vertex_shader.GetAddressOf(), nullptr, nullptr, 0);
		create_ps_from_cso(get_renderdata()->device.Get(), "smaa_neighborhood_blending_ps.cso", smaa_neighborhood_blending_pixel_shader.GetAddressOf());

		//	TAA
		create_cs_from_cso(get_renderdata()->device.Get(), "temporal_anti_alias_cs.cso", temporal_anti_alias_compute_shader.GetAddressOf());
	}

	//	シーンの初期化処理
	initialize_scene();
	return true;
}

void test2_scene::initialize_character_models()
{
	//	踊り手
	model_instances[ModelId::Dancer1].pmx_model = std::make_unique<pmx_model>(get_renderdata()->device.Get(), ".\\resources\\ellen\\ellen.pmx");
	model_instances[ModelId::Dancer1].pmx_model->add_vmd_animation("default", ".\\resources\\animation\\あったかいとトレス\\マジカルミライあったかいとトレスボーンフレーム.vmd");
	//	エレン用
	int face_threshold_map_id = model_instances[ModelId::Dancer1].pmx_model->add_texture(get_renderdata()->device.Get(), ".\\resources\\ellen\\tex\\shadow_threshold_map.png");
	for (auto& material : model_instances[ModelId::Dancer1].pmx_model->get_materials())
	{
		material.pipeline_state_name = pmx_model_shader_names[GENARATE_GBUFFER];

		bool is_edge_line = (material.draw_flag & static_cast<unsigned char>(pmx_material_draw_bitflag::use_edge_line)) == static_cast<unsigned char>(pmx_material_draw_bitflag::use_edge_line);
		material.default.edge_size = is_edge_line ? 0.01f : 0.0f;
		if (is_edge_line)
			material.draw_flag &= ~static_cast<unsigned char>(pmx_material_draw_bitflag::cull_both);
#if defined(USE_EIZIN_SHADER)
		material.default.shadow_threshold_texture_index = 0xFFFFFFFF;
		material.default.specular_color.x				= 1.0f;		//	粗さ
		material.default.specular_color.y				= 0.0f;		//	金属質
		material.default.specular_color.z				= 1.0f;		//	陰影階調数
		material.default.specular_color.w				= 0.0f;		//	ノイズ係数

		material.default.color_diff_line_factor			= 1.0f;		//	カラーライン係数
		material.default.color_diff_line_threshold		= 0.0025f;	//	カラーライン検知閾値
		material.default.color_line_adjust_saturate		= 0.75f;	//  ライン彩度調整係数
		material.default.color_line_adjust_brightness	= 0.75f;	//  ライン明度調整係数
		material.default.outline_adjust_parameter.x		= 0.0f;		//	深度法線アウトライン係数減衰範囲(最小)
		material.default.outline_adjust_parameter.y		= 50.0f;	//	深度法線アウトライン係数減衰表示範囲(最大)

		material.default.color_posterize_factor			= 128.0f;	//	カラーポスタライズ係数

		material.default.depth_normal_outline_factor	= 1.0f;		//	深度法線アウトライン閾値
		material.default.outline_adjust_parameter.z		= 0.0f;		//	深度法線アウトライン係数減衰範囲(最小)
		material.default.outline_adjust_parameter.w		= 50.0f;	//	深度法線アウトライン係数減衰表示範囲(最大)
		material.SetFlags(pmx_model::material_attribute::normal_depth_outline_off, true);
		material.SetFlags(pmx_model::material_attribute::color_line_off, false);
		material.SetFlags(pmx_model::material_attribute::color_postarize_off, true);
		material.SetFlags(pmx_model::material_attribute::noise_off, true);
		material.SetFlags(pmx_model::material_attribute::indirect_lighting_off, true);
		if (material.name.find("顔") != std::string::npos)
		{
			material.default.color_line_adjust_saturate = 0.7f;
			material.default.color_line_adjust_brightness = 1.2f;

			material.default.shadow_threshold_texture_index = face_threshold_map_id;
			material.default.shadow_threshold_texcoord_offset = { 0.0f, +296.0f / 2048.0f };
			material.SetFlags(pmx_model::material_attribute::face_shadow_on, true);
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

	//	アンドー用
	model_instances[ModelId::Dancer2].pmx_model = std::make_unique<pmx_model>(get_renderdata()->device.Get(), ".\\resources\\ando\\ando.pmx");
	model_instances[ModelId::Dancer2].pmx_model->add_vmd_animation("default", ".\\resources\\animation\\あったかいとトレス\\マジカルミライあったかいとトレスボーンフレーム.vmd");
	for (auto& material : model_instances[ModelId::Dancer2].pmx_model->get_materials())
	{
		material.pipeline_state_name = pmx_model_shader_names[GENARATE_GBUFFER];
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
		material.default.color_diff_line_threshold = 0.04f;		//	カラーライン検知閾値
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
#endif	//	defined(USE_EIZIN_SHADER)
	}
}

const char* test2_scene::get_gbuffer_pipeline_state_name()
{
	return pmx_model_shader_names[GENARATE_GBUFFER];
}

//	ステージモデルの読み込みと配置(海辺/椅子/日傘)
void test2_scene::initialize_stage_models()
{
	//	モデル読み込み
	{
		//	海辺モデル
		model_instances[ModelId::Beach].pmx_model = std::make_unique<pmx_model>(get_renderdata()->device.Get(), ".\\resources\\海辺\\海辺.pmx");
	//	model_instances[ModelId::Beach].pmx_model->add_vmd_animation("default", ".\\resources\\海辺\\波雲動かす.vmd");
		for (auto& material : model_instances[ModelId::Beach].pmx_model->get_materials())
		{
			material.pipeline_state_name = pmx_model_shader_names[GENARATE_GBUFFER];
			material.default.edge_size = 0.0f;
#if defined(USE_EIZIN_SHADER)
			material.default.shadow_threshold_texture_index = 0xFFFFFFFF;
			material.default.specular_color.x = 1.0f;				//	粗さ
			material.default.specular_color.y = 0.0f;				//	金属質
			material.default.specular_color.z = 2.0f;				//	陰影階調数
			material.default.specular_color.w = 1.0f;				//	ノイズ係数

			material.default.color_diff_line_factor = 1.0f;			//	カラーライン係数
			material.default.color_diff_line_threshold = 0.005f;	//	カラーライン検知閾値
			material.default.color_line_adjust_saturate = 1.0f;		//  ライン彩度調整係数
			material.default.color_line_adjust_brightness = 1.0f;	//  ライン明度調整係数
			material.default.outline_adjust_parameter.x = 50000.0f;	//	カラーアウトラインの係数減衰範囲(最小)
			material.default.outline_adjust_parameter.y = 70000.0f;	//	カラーアウトラインの係数減衰表示範囲(最大)

			material.default.color_posterize_factor = 8.0f;	//	カラーポスタライズ係数

			material.default.depth_normal_outline_factor = 0.1f;	//	深度法線アウトライン閾値
			material.default.outline_adjust_parameter.z = 5000.0f;	//	深度法線アウトライン係数減衰範囲(最小)
			material.default.outline_adjust_parameter.w = 10000.0f;	//	深度法線アウトライン係数減衰表示範囲(最大)
			material.SetFlags(pmx_model::material_attribute::indirect_lighting_off, true);
			if (material.name.find("地面") != std::string::npos)
			{
				material.default.diffuse_color.x *= 1.5f;
				material.default.diffuse_color.y *= 1.5f;
				material.default.diffuse_color.z *= 1.5f;
				material.default.color_diff_line_threshold = 0.00025f;	//	カラーライン検知閾値
				material.default.color_line_adjust_saturate = 0.5f;		//  ライン彩度調整係数
				material.default.color_line_adjust_brightness = 0.5f;	//  ライン明度調整係数
			}
			if( material.name == "空" )
			{
				material.default.specular_color.x = 1.0f;
				material.default.specular_color.y = 0.0f;
				material.default.specular_color.z = 1.0f;
				material.default.specular_color.w = 0.0f;
				material.default.color_diff_line_factor = 0.0f;
				material.default.color_posterize_factor = 64.0f;
				material.default.depth_normal_outline_factor = 1.0f;
			}
			if (material.name == "波" || material.name == "波影")
			{
				material.default.diffuse_color.w = 0.0f;
			}
			if( material.name == "海" )
			{
				material.pipeline_state_name = pmx_model_shader_names[EIZIN_WAVE];
				material.default.diffuse_color.x *= 1.5f;
				material.default.diffuse_color.y *= 1.5f;
				material.default.diffuse_color.z *= 1.5f;
				material.default.specular_color.x = 0.4f;
				material.default.specular_color.y = 0.5f;
				material.default.specular_color.z = 4.0f;
				material.default.specular_color.w = 0.0f;
				material.default.alpha_dither_factor = 2.0f;
				material.default.color_diff_line_factor = 1.0f;
				material.default.color_posterize_factor = 8.0f;
				material.default.depth_normal_outline_factor = 0.01f;
			}
			if( material.name == "波隠し" )
			{
				material.pipeline_state_name = pmx_model_shader_names[EIZIN_WAVE];
				material.default.diffuse_color.x *= 1.5f;
				material.default.diffuse_color.y *= 1.5f;
				material.default.diffuse_color.z *= 1.5f;
				material.default.specular_color.x = 0.4f;
				material.default.specular_color.y = 0.5f;
				material.default.specular_color.z = 4.0f;
				material.default.specular_color.w = 0.0f;
				material.default.alpha_dither_factor = 2.0f;
				material.default.color_diff_line_threshold = 0.000025f;	//	カラーライン検知閾値
				material.default.color_diff_line_factor = 1.0f;
				material.default.color_posterize_factor = 8.0f;
				material.default.color_line_adjust_saturate = 0.75f;
				material.default.color_line_adjust_brightness = 0.75f;
				material.default.depth_normal_outline_factor = 0.01f;
				auto& texture_names = model_instances[ModelId::Beach].pmx_model->get_texture_names();
				for (int i = 0; i < texture_names.size(); ++i)
				{
					if (texture_names[i].find("wave.png") != std::string::npos)
					{
						material.default.sphere_texture_index = i;
						break;
					}
				}
			}
			if (material.name == "岩")
			{
				material.default.specular_color.x = 1.0f;
				material.default.specular_color.y = 0.0f;
				material.default.specular_color.z = 8.0f;
				material.default.specular_color.w = 1.0f;
				material.default.color_diff_line_factor = 1.0f;
				material.default.color_posterize_factor = 8.0f;
				material.default.depth_normal_outline_factor = 0.1f;
			}
			if (material.name.find("雲") != std::string::npos)
			{
				material.default.specular_color.x = 1.0f;
				material.default.specular_color.y = 0.0f;
				material.default.specular_color.z = 1.0f;
				material.default.specular_color.w = 0.0f;
				material.default.alpha_dither_factor = 0.99f;
				material.default.color_diff_line_factor = 1.0f;
				material.default.color_posterize_factor = 6.0f;
				material.default.depth_normal_outline_factor = 0.01f;
				if (material.name == "雲1")
					material.default.color_posterize_factor = 256.0f;
			}
			if (material.name.find("ヤシ") != std::string::npos)
			{
				material.default.diffuse_color.x *= 1.5f;
				material.default.diffuse_color.y *= 1.5f;
				material.default.diffuse_color.z *= 1.5f;
				material.default.specular_color.x = 1.0f;
				material.default.specular_color.y = 0.0f;
				material.default.specular_color.z = 4.0f;
				material.default.specular_color.w = 1.0f;
				material.default.color_diff_line_factor = 1.0f;
				material.default.color_posterize_factor = 8.0f;
				material.default.depth_normal_outline_factor = 0.1f;
			}
			if (material.name.find("植物") != std::string::npos)
			{
				material.default.diffuse_color.x *= 1.5f;
				material.default.diffuse_color.y *= 1.5f;
				material.default.diffuse_color.z *= 1.5f;
				material.default.specular_color.x = 1.0f;
				material.default.specular_color.y = 0.0f;
				material.default.specular_color.z = 128.0f;
				material.default.specular_color.w = 1.0f;
				material.default.color_diff_line_factor = 1.0f;
				material.default.color_posterize_factor = 16.0f;
				material.default.depth_normal_outline_factor = 0.01f;
			}
#endif	//	defined(USE_EIZIN_SHADER)
		}

		//	椅子モデル
		model_instances[ModelId::Object1].pmx_model = std::make_unique<pmx_model>(get_renderdata()->device.Get(), ".\\resources\\海辺\\椅子.pmx");
		for (auto& material : model_instances[ModelId::Object1].pmx_model->get_materials())
		{
			material.pipeline_state_name = pmx_model_shader_names[GENARATE_GBUFFER];
			material.default.edge_size = 0.0f;
#if defined(USE_EIZIN_SHADER)
			material.default.shadow_threshold_texture_index = 0xFFFFFFFF;
			material.default.specular_color.x = 1.0f;				//	粗さ
			material.default.specular_color.y = 0.0f;				//	金属質
			material.default.specular_color.z = 8.0f;				//	陰影階調数
			material.default.specular_color.w = 1.0f;				//	ノイズ係数

			material.default.color_diff_line_factor = 1.0f;			//	カラーライン係数
			material.default.color_diff_line_threshold = 0.005f;	//	カラーライン検知閾値
			material.default.color_line_adjust_saturate = 1.0f;		//  ライン彩度調整係数
			material.default.color_line_adjust_brightness = 1.0f;	//  ライン明度調整係数
			material.default.outline_adjust_parameter.x = 1000.0f;	//	カラーアウトラインの係数減衰範囲(最小)
			material.default.outline_adjust_parameter.y = 5000.0f;	//	カラーアウトラインの係数減衰表示範囲(最大)

			material.default.color_posterize_factor = 24.0f;		//	カラーポスタライズ係数

			material.default.depth_normal_outline_factor = 0.02f;	//	深度法線アウトライン閾値
			material.default.outline_adjust_parameter.z = 1000.0f;	//	深度法線アウトライン係数減衰範囲(最小)
			material.default.outline_adjust_parameter.w = 5000.0f;	//	深度法線アウトライン係数減衰表示範囲(最大)
			material.SetFlags(pmx_model::material_attribute::indirect_lighting_off, true);
#endif	//	defined(USE_EIZIN_SHADER)
		}

		//	日傘モデル
		model_instances[ModelId::Object2].pmx_model = std::make_unique<pmx_model>(get_renderdata()->device.Get(), ".\\resources\\海辺\\日傘.pmx");
		for (auto& material : model_instances[ModelId::Object2].pmx_model->get_materials())
		{
			material.pipeline_state_name = pmx_model_shader_names[GENARATE_GBUFFER];
			material.default.edge_size = 0.0f;
#if defined(USE_EIZIN_SHADER)
			material.default.shadow_threshold_texture_index = 0xFFFFFFFF;
			material.default.specular_color.x = 1.0f;				//	粗さ
			material.default.specular_color.y = 0.0f;				//	金属質
			material.default.specular_color.z = 8.0f;				//	陰影階調数
			material.default.specular_color.w = 1.0f;				//	ノイズ係数

			material.default.color_diff_line_factor = 1.0f;			//	カラーライン係数
			material.default.color_diff_line_threshold = 0.005f;	//	カラーライン検知閾値
			material.default.color_line_adjust_saturate = 1.0f;		//  ライン彩度調整係数
			material.default.color_line_adjust_brightness = 1.0f;	//  ライン明度調整係数
			material.default.outline_adjust_parameter.x = 1000.0f;	//	カラーアウトラインの係数減衰範囲(最小)
			material.default.outline_adjust_parameter.y = 5000.0f;	//	カラーアウトラインの係数減衰表示範囲(最大)

			material.default.color_posterize_factor = 24.0f;		//	カラーポスタライズ係数

			material.default.depth_normal_outline_factor = 0.02f;	//	深度法線アウトライン閾値
			material.default.outline_adjust_parameter.z = 1000.0f;	//	深度法線アウトライン係数減衰範囲(最小)
			material.default.outline_adjust_parameter.w = 5000.0f;	//	深度法線アウトライン係数減衰表示範囲(最大)
			material.SetFlags(pmx_model::material_attribute::indirect_lighting_off, true);
#endif	//	defined(USE_EIZIN_SHADER)
		}
	}

	//	配置
	{
		//	ステージモデル
		{
			deferred_models.emplace_back(static_cast<int>(model_datas.size()));
		//	shadow_models.emplace_back(static_cast<int>(model_datas.size()));
			model_datas.emplace_back();
			model_datas.back().model_index = ModelId::Beach;
			model_datas.back().node_datas.resize(model_instances[ModelId::Beach].pmx_model->get_nodes().size());
		}

		//	椅子モデル
		{
			deferred_models.emplace_back(static_cast<int>(model_datas.size()));
			shadow_models.emplace_back(static_cast<int>(model_datas.size()));
			model_datas.emplace_back();
			model_datas.back().model_index = ModelId::Object1;
			model_datas.back().node_datas.resize(model_instances[ModelId::Object1].pmx_model->get_nodes().size());
			model_datas.back().translation.x = -10.0f;
			model_datas.back().translation.z = 20.0f;
		}
		{
			deferred_models.emplace_back(static_cast<int>(model_datas.size()));
			shadow_models.emplace_back(static_cast<int>(model_datas.size()));
			model_datas.emplace_back();
			model_datas.back().model_index = ModelId::Object1;
			model_datas.back().node_datas.resize(model_instances[ModelId::Object1].pmx_model->get_nodes().size());
			model_datas.back().translation.x = 10.0f;
			model_datas.back().translation.z = 20.0f;
		}

		//	日傘モデル
		{
			deferred_models.emplace_back(static_cast<int>(model_datas.size()));
			shadow_models.emplace_back(static_cast<int>(model_datas.size()));
			model_datas.emplace_back();
			model_datas.back().model_index = ModelId::Object2;
			model_datas.back().node_datas.resize(model_instances[ModelId::Object2].pmx_model->get_nodes().size());
			model_datas.back().translation.x = 0.0f;
			model_datas.back().translation.z = 20.0f;
		}
	}
}

test2_scene::gltf_stage& test2_scene::add_gltf_stage(const char* filename, const eizin_gltf_constants& default_constants)
{
	HRESULT hr{ S_OK };

	gltf_stage& stage = gltf_stages.emplace_back();
	stage.model = std::make_unique<gltf_model>(get_renderdata()->device.Get(), filename);

	//	シャドウマップ描画用シェーダー
	{
		gltf_model::pipeline_state_desc desc;
		desc.name = gltf_shadow_caster_shader_name;
		desc.vs_path = "gltf_model_shadow_caster_vs.cso";
		stage.model->add_pipeline_state(get_renderdata()->device.Get(), desc);
	}

	//	GBuffer描画用シェーダー
	{
		gltf_model::pipeline_state_desc desc;
		desc.name = gltf_gbuffer_shader_name;
		desc.vs_path = "gltf_model_gbuffer_vs.cso";
#if defined(USE_EIZIN_SHADER)
		desc.ps_path = "eizin_gltf_gbuffer_ps.cso";
#else	//	defined(USE_EIZIN_SHADER)
		desc.ps_path = "gltf_model_gbuffer_ps.cso";
#endif	//	defined(USE_EIZIN_SHADER)
		stage.model->add_pipeline_state(get_renderdata()->device.Get(), desc);
	}

	//	マテリアル毎の英人パラメーター(StructuredBuffer)
	{
		stage.material_constants.assign((std::max)(stage.model->materials.size(), static_cast<size_t>(1)), default_constants);

		D3D11_BUFFER_DESC buffer_desc{};
		buffer_desc.ByteWidth = static_cast<UINT>(sizeof(eizin_gltf_constants) * stage.material_constants.size());
		buffer_desc.Usage = D3D11_USAGE_DEFAULT;
		buffer_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		buffer_desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		buffer_desc.StructureByteStride = sizeof(eizin_gltf_constants);
		D3D11_SUBRESOURCE_DATA initialize_data{};
		initialize_data.pSysMem = stage.material_constants.data();
		hr = get_renderdata()->device->CreateBuffer(&buffer_desc, &initialize_data, stage.material_constant_buffer.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

		D3D11_SHADER_RESOURCE_VIEW_DESC shader_resource_view_desc{};
		shader_resource_view_desc.Format = DXGI_FORMAT_UNKNOWN;
		shader_resource_view_desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		shader_resource_view_desc.Buffer.NumElements = static_cast<UINT>(stage.material_constants.size());
		hr = get_renderdata()->device->CreateShaderResourceView(stage.material_constant_buffer.Get(), &shader_resource_view_desc, stage.material_constant_shader_resource_view.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
	}
	return stage;
}

void test2_scene::render_gltf_stages_gbuffer()
{
	for (auto& stage : gltf_stages)
	{
		//	モデル情報を設定(ステージなのでシャドウマスクは0)
		{
			model_constants constant{};
			get_renderdata()->immediate_context->UpdateSubresource(model_constant_constant_buffer.Get(), 0, 0, &constant, 0, 0);
		}

		//	マテリアル毎の英人パラメーター(ImGuiで編集されるので毎フレーム更新)
		get_renderdata()->immediate_context->UpdateSubresource(stage.material_constant_buffer.Get(), 0, 0, stage.material_constants.data(), 0, 0);
		get_renderdata()->immediate_context->PSSetShaderResources(EizinGltfMaterialSRVIndex, 1, stage.material_constant_shader_resource_view.GetAddressOf());

		stage.model->render(get_renderdata()->immediate_context.Get(), stage.world_matrix, stage.previous_world_matrix, gltf_gbuffer_shader_name);

		ID3D11ShaderResourceView* null_shader_resource_view = nullptr;
		get_renderdata()->immediate_context->PSSetShaderResources(EizinGltfMaterialSRVIndex, 1, &null_shader_resource_view);
	}
}

void test2_scene::render_gltf_stages_shadow(UINT light_shadow_mask)
{
	//	ステージのシャドウマスクは0なので、マスク0のライトにのみ影を落とす
	if (light_shadow_mask != 0)
		return;

	for (auto& stage : gltf_stages)
	{
		if (!stage.cast_shadow)
			continue;
		stage.model->render(get_renderdata()->immediate_context.Get(), stage.world_matrix, stage.previous_world_matrix, gltf_shadow_caster_shader_name);
	}
}

void test2_scene::update_taa_jitter()
{
	//	TAA以外ではジッターしない
	DirectX::XMFLOAT2 jitter_uv{ 0.0f, 0.0f };
	if (antialias == antialias_mode::taa && taa_is_sub_pixel_jitter)
	{
		DirectX::XMFLOAT2 jitter_pixel = calculate_halton_jitter(taa_jitter_index++);
		jitter_uv.x = jitter_pixel.x / static_cast<float>(SCREEN_WIDTH);
		jitter_uv.y = jitter_pixel.y / static_cast<float>(SCREEN_HEIGHT);
	}
	set_jitter_uv_offset(jitter_uv);
}

void test2_scene::antialias_pass()
{
	switch (antialias)
	{
	case antialias_mode::fxaa:
		fxaa_pass();
		break;
	case antialias_mode::smaa:
		smaa_pass();
		break;
	case antialias_mode::taa:
		taa_pass();
		break;
	default:
		break;
	}

	//	次のパスでSRVとして使うので外す
	get_renderdata()->immediate_context->OMSetRenderTargets(0, nullptr, nullptr);
	ID3D11UnorderedAccessView* null_unordered_access_views[2] = {};
	get_renderdata()->immediate_context->CSSetUnorderedAccessViews(0, 2, null_unordered_access_views, nullptr);
	ID3D11ShaderResourceView* null_shader_resource_views[4] = {};
	get_renderdata()->immediate_context->PSSetShaderResources(0, 4, null_shader_resource_views);
	get_renderdata()->immediate_context->CSSetShaderResources(0, 4, null_shader_resource_views);
}

void test2_scene::fxaa_pass()
{
	get_renderdata()->immediate_context->OMSetRenderTargets(1, antialias_render_target_view.GetAddressOf(), nullptr);

	get_renderdata()->set_default_sampler();
	get_renderdata()->immediate_context->OMSetBlendState(get_renderdata()->blend_states[static_cast<size_t>(render_data::BLEND_STATE::NONE)].Get(), nullptr, 0xFFFFFFFF);
	get_renderdata()->immediate_context->OMSetDepthStencilState(get_renderdata()->depth_stencil_states[static_cast<size_t>(render_data::DEPTH_STATE::ZT_OFF_ZW_OFF)].Get(), 0);
	get_renderdata()->immediate_context->RSSetState(get_renderdata()->rasterizer_states[static_cast<size_t>(render_data::RASTER_STATE::CULL_NONE)].Get());
	get_renderdata()->immediate_context->VSSetShader(sprite_vertex_shader.Get(), nullptr, 0);
	get_renderdata()->immediate_context->IASetInputLayout(sprite_input_layout.Get());
	get_renderdata()->immediate_context->UpdateSubresource(fxaa_constant_buffer.Get(), 0, 0, &fxaa_constant, 0, 0);
	get_renderdata()->immediate_context->PSSetConstantBuffers(FxaaCBVIndex, 1, fxaa_constant_buffer.GetAddressOf());
	get_renderdata()->immediate_context->PSSetShader(fxaa_pixel_shader.Get(), nullptr, 0);
	fxaa_render_sprite->render(get_renderdata()->immediate_context.Get(), 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
}

void test2_scene::smaa_pass()
{
	static constexpr int SceneCBVIndex = 1;
	auto* immediate_context = get_renderdata()->immediate_context.Get();

	//	共通設定
	set_scene_constant(SceneCBVIndex, false);
	immediate_context->UpdateSubresource(smaa_constant_buffer.Get(), 0, 0, &smaa_constant, 0, 0);
	immediate_context->VSSetConstantBuffers(SmaaCBVIndex, 1, smaa_constant_buffer.GetAddressOf());
	immediate_context->PSSetConstantBuffers(SmaaCBVIndex, 1, smaa_constant_buffer.GetAddressOf());

	ID3D11SamplerState* sampler_states[] =
	{
		get_renderdata()->sampler_states[static_cast<size_t>(render_data::SAMPLER_STATE::POINT_CLAMP)].Get(),
		get_renderdata()->sampler_states[static_cast<size_t>(render_data::SAMPLER_STATE::LINEAR_CLAMP)].Get(),
	};
	immediate_context->PSSetSamplers(0, ARRAYSIZE(sampler_states), sampler_states);

	immediate_context->OMSetBlendState(get_renderdata()->blend_states[static_cast<size_t>(render_data::BLEND_STATE::NONE)].Get(), nullptr, 0xFFFFFFFF);
	immediate_context->OMSetDepthStencilState(get_renderdata()->depth_stencil_states[static_cast<size_t>(render_data::DEPTH_STATE::ZT_OFF_ZW_OFF)].Get(), 0);
	immediate_context->RSSetState(get_renderdata()->rasterizer_states[static_cast<size_t>(render_data::RASTER_STATE::CULL_NONE)].Get());
	immediate_context->IASetInputLayout(nullptr);
	immediate_context->IASetVertexBuffers(0, 0, nullptr, nullptr, nullptr);
	immediate_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	FLOAT clear_color[] = { 0.0f, 0.0f, 0.0f, 0.0f };
	ID3D11ShaderResourceView* null_shader_resource_views[3] = {};

	//	エッジ検出
	{
		immediate_context->ClearRenderTargetView(smaa_buffer_render_target_view[smaa_edge].Get(), clear_color);
		immediate_context->OMSetRenderTargets(1, smaa_buffer_render_target_view[smaa_edge].GetAddressOf(), nullptr);
		immediate_context->VSSetShader(smaa_edge_detection_vertex_shader.Get(), nullptr, 0);
		immediate_context->PSSetShader(smaa_edge_detection_pixel_shader.Get(), nullptr, 0);
		immediate_context->PSSetShaderResources(0, 1, filtered_shader_resource_view.GetAddressOf());
		immediate_context->Draw(4, 0);
		immediate_context->PSSetShaderResources(0, 3, null_shader_resource_views);
	}

	//	ブレンド係数計算
	{
		immediate_context->ClearRenderTargetView(smaa_buffer_render_target_view[smaa_blend].Get(), clear_color);
		immediate_context->OMSetRenderTargets(1, smaa_buffer_render_target_view[smaa_blend].GetAddressOf(), nullptr);
		immediate_context->VSSetShader(smaa_blending_weight_calculation_vertex_shader.Get(), nullptr, 0);
		immediate_context->PSSetShader(smaa_blending_weight_calculation_pixel_shader.Get(), nullptr, 0);
		ID3D11ShaderResourceView* shader_resource_views[] =
		{
			smaa_buffer_shader_resource_view[smaa_edge].Get(),
			smaa_area_shader_resource_view.Get(),
			smaa_search_shader_resource_view.Get(),
		};
		immediate_context->PSSetShaderResources(0, ARRAYSIZE(shader_resource_views), shader_resource_views);
		immediate_context->Draw(4, 0);
		immediate_context->PSSetShaderResources(0, 3, null_shader_resource_views);
	}

	//	近傍ブレンド
	{
		immediate_context->OMSetRenderTargets(1, antialias_render_target_view.GetAddressOf(), nullptr);
		immediate_context->VSSetShader(smaa_neighborhood_blending_vertex_shader.Get(), nullptr, 0);
		immediate_context->PSSetShader(smaa_neighborhood_blending_pixel_shader.Get(), nullptr, 0);
		ID3D11ShaderResourceView* shader_resource_views[] =
		{
			filtered_shader_resource_view.Get(),
			smaa_buffer_shader_resource_view[smaa_blend].Get(),
			smaa_buffer_shader_resource_view[smaa_edge].Get(),
		};
		immediate_context->PSSetShaderResources(0, ARRAYSIZE(shader_resource_views), shader_resource_views);
		immediate_context->Draw(4, 0);
		immediate_context->PSSetShaderResources(0, 3, null_shader_resource_views);
	}

	//	デフォルトのサンプラーに戻す
	get_renderdata()->set_default_sampler();
}

void test2_scene::taa_pass()
{
	auto* immediate_context = get_renderdata()->immediate_context.Get();

	//	定数バッファ更新
	taa_constant.taa_jitter = get_current_jitter_uv_offset();
	taa_constant.taa_previous_jitter = get_previous_jitter_uv_offset();
	immediate_context->UpdateSubresource(taa_constant_buffer.Get(), 0, 0, &taa_constant, 0, 0);
	taa_constant.taa_reset = 0;
	immediate_context->CSSetConstantBuffers(TaaCBVIndex, 1, taa_constant_buffer.GetAddressOf());

	const int current_buffer_index = (taa_frame_index + 0) % 2;
	const int previous_buffer_index = (taa_frame_index + 1) % 2;
	++taa_frame_index;

	immediate_context->OMSetRenderTargets(0, nullptr, nullptr);

	ID3D11ShaderResourceView* shader_resource_views[] =
	{
		filtered_shader_resource_view.Get(),
		taa_accumlate_shader_resource_view[previous_buffer_index].Get(),
		g_buffer_shader_resource_view[GB_Velocity].Get(),
		g_buffer_shader_resource_view[GB_Normal_Depth].Get(),
	};
	immediate_context->CSSetShaderResources(0, ARRAYSIZE(shader_resource_views), shader_resource_views);

	ID3D11SamplerState* sampler_states[] =
	{
		get_renderdata()->sampler_states[static_cast<size_t>(render_data::SAMPLER_STATE::POINT_CLAMP)].Get(),
		get_renderdata()->sampler_states[static_cast<size_t>(render_data::SAMPLER_STATE::LINEAR_CLAMP)].Get(),
	};
	immediate_context->CSSetSamplers(0, ARRAYSIZE(sampler_states), sampler_states);

	ID3D11UnorderedAccessView* unordered_access_views[] =
	{
		antialias_unordered_access_view.Get(),
		taa_accumlate_unordered_access_view[current_buffer_index].Get(),
	};
	immediate_context->CSSetUnorderedAccessViews(0, ARRAYSIZE(unordered_access_views), unordered_access_views, nullptr);

	immediate_context->CSSetShader(temporal_anti_alias_compute_shader.Get(), nullptr, 0);
	static constexpr UINT thread_group_size = 8;
	immediate_context->Dispatch((SCREEN_WIDTH + thread_group_size - 1) / thread_group_size, (SCREEN_HEIGHT + thread_group_size - 1) / thread_group_size, 1);
	immediate_context->CSSetShader(nullptr, nullptr, 0);
}

void test2_scene::initialize_scene()
{
#if !defined(USE_EIZIN_SHADER)
	//	真っ暗になるので除外
	color_filter_constant.color_filter_weight = 0;
#endif	//	!defined(USE_EIZIN_SHADER)

	{
		model_instances.resize(ModelId::Max);
		initialize_character_models();
	}

	//	光源を配置
	{
		//	平行光源
		directional_lights.resize(1);
		directional_lights[0].directional_light.color = { 1.0f, 1.0f, 1.0f, 1.0f };
#if defined(CHECK_SETTING)
		DirectX::XMStoreFloat4(&directional_lights[0].directional_light.direction, DirectX::XMVector3Normalize(DirectX::XMVectorSet(1, -1, +1, 0)));
#else	//	defined(CHECK_SETTING)
		DirectX::XMStoreFloat4(&directional_lights[0].directional_light.direction, DirectX::XMVector3Normalize(DirectX::XMVectorSet(1, -1, -1, 0)));
#endif	//	defined(CHECK_SETTING)
		directional_lights[0].use_shadow = true;	//	影使用フラグ
		directional_lights[0].contact_shadow = 0.05f;
		directional_lights[0].shadow_bias = 0.0001f;

		//	スポットライト
	//	spot_lights.resize(1);
	//	spot_lights[0].spot_light.position = { 10.0f, 10.0f, 10.0f, 1.0f };
	//	spot_lights[0].spot_light.color = { 1.0f, 1.0f, 1.0f, 1.0f };
	//	spot_lights[0].spot_light.range = 100;
	//	spot_lights[0].spot_light.innerCorn = DirectX::XMConvertToRadians(20.0f);
	//	spot_lights[0].spot_light.outerCorn = DirectX::XMConvertToRadians(30.0f);
	//	DirectX::XMStoreFloat4(&spot_lights[0].spot_light.direction, DirectX::XMVector3Normalize(DirectX::XMVectorSet(1, -1, -1, 0)));
	//	spot_lights[0].use_shadow = false;	//	影使用フラグ
	//	spot_lights[0].contact_shadow = 0.1f;
	//	spot_lights[0].shadow_bias = 0.000f;
	}

	{
		//	プレイヤーモデル
		{
			deferred_models.emplace_back(static_cast<int>(model_datas.size()));
			shadow_models.emplace_back(static_cast<int>(model_datas.size()));
			model_datas.emplace_back();
			model_datas.back().model_index = ModelId::Dancer1;
			model_datas.back().shadow_mask = Dancer1ShadowValue;
			model_datas.back().node_datas.resize(model_instances[ModelId::Dancer1].pmx_model->get_nodes().size());
#if defined(CHECK_SETTING)
			model_datas.back().animation_speed = 0.0f;
#endif	//	defined(CHECK_SETTING)
			model_datas.back().animation_time = 0.0f;
			model_datas.back().translation.x = -10.0f;
			//	影オンリーライト
			{
				int spot_light_index = static_cast<int>(spot_lights.size());
				spot_lights.emplace_back();
				spot_lights.back().spot_light.position = { 0, 0, 0, 0 };
				spot_lights.back().spot_light.direction = directional_lights[0].directional_light.direction;
				spot_lights.back().spot_light.range = 1000;
				spot_lights.back().spot_light.color.x = 1;
				spot_lights.back().spot_light.color.y = 1;
				spot_lights.back().spot_light.color.z = 1;
				spot_lights.back().spot_light.color.w = 1;
				spot_lights.back().spot_light.innerCorn = DirectX::XMConvertToRadians(0.0f);
				spot_lights.back().spot_light.outerCorn = DirectX::XMConvertToRadians(3.0f);
				spot_lights.back().use_shadow = true;
				spot_lights.back().shadow_mask = Dancer1ShadowValue;
				spot_lights.back().shadow_bias = 0.000001f;
				model_datas.back().follow_light_index = spot_light_index;
			}
		}
		//	プレイヤーモデル
		{
			deferred_models.emplace_back(static_cast<int>(model_datas.size()));
			shadow_models.emplace_back(static_cast<int>(model_datas.size()));
			model_datas.emplace_back();
			model_datas.back().model_index = ModelId::Dancer2;
			model_datas.back().shadow_mask = Dancer2ShadowValue;
			model_datas.back().node_datas.resize(model_instances[ModelId::Dancer2].pmx_model->get_nodes().size());
#if defined(CHECK_SETTING)
			model_datas.back().animation_speed = 0.0f;
#endif	//	defined(CHECK_SETTING)
			model_datas.back().animation_time = 0.0f;
			model_datas.back().translation.x = +10.0f;
			//	影オンリーライト
			{
				int spot_light_index = static_cast<int>(spot_lights.size());
				spot_lights.emplace_back();
				spot_lights.back().spot_light.position = { 0, 0, 0, 0 };
				spot_lights.back().spot_light.direction = directional_lights[0].directional_light.direction;
				spot_lights.back().spot_light.range = 1000;
				spot_lights.back().spot_light.color.x = 1;
				spot_lights.back().spot_light.color.y = 1;
				spot_lights.back().spot_light.color.z = 1;
				spot_lights.back().spot_light.color.w = 1;
				spot_lights.back().spot_light.innerCorn = DirectX::XMConvertToRadians(0.0f);
				spot_lights.back().spot_light.outerCorn = DirectX::XMConvertToRadians(3.0f);
				spot_lights.back().use_shadow = true;
				spot_lights.back().shadow_mask = Dancer2ShadowValue;
				spot_lights.back().shadow_bias = 0.000001f;
				model_datas.back().follow_light_index = spot_light_index;
			}
		}

		//	ステージモデル
		initialize_stage_models();
	}

	//	間接光関係準備
	{
		D3D11_TEXTURE2D_DESC texture2d_desc;
		light_probes.resize(1);
		//	全体IBL
		int	id = 0;
		light_probes[id].position = { 0, 0, 0, 0 };
		light_probes[id].is_unbind = true;
		load_texture_from_file(get_renderdata()->device.Get(), L".\\resources\\hilly_terrain_01_puresky_4k\\diffuse_iem.dds", light_probes[id].diffuse_iem_shader_resource_view.GetAddressOf(), &texture2d_desc);
		load_texture_from_file(get_renderdata()->device.Get(), L".\\resources\\hilly_terrain_01_puresky_4k\\specular_pmrem.dds", light_probes[id].specular_pmrem_shader_resource_view.GetAddressOf(), &texture2d_desc);
		//	箱
		light_probe_cube = std::make_unique<geometric_cube>(get_renderdata()->device.Get());
		load_texture_from_file(get_renderdata()->device.Get(), L".\\resources\\lut_ggx.dds", lut_ggx_shader_resource_view.GetAddressOf(), &texture2d_desc);

		//	スカイボックス
		sky_box_render_sprite = std::make_unique<sprite>(get_renderdata()->device.Get(), light_probes[0].specular_pmrem_shader_resource_view);
	}

	//	スプライト関係準備
	{
		deferred_rendering_sprite = std::make_unique<sprite>(get_renderdata()->device.Get(), g_buffer_shader_resource_view[GB_BaseColor]);
		final_pass_render_sprite = std::make_unique<sprite>(get_renderdata()->device.Get(), scene_shader_resource_view);
		fxaa_render_sprite = std::make_unique<sprite>(get_renderdata()->device.Get(), filtered_shader_resource_view);
		final_pass_antialias_render_sprite = std::make_unique<sprite>(get_renderdata()->device.Get(), antialias_shader_resource_view);
	}

	//	モデルシェーダー準備
	for (auto& model_instance : model_instances)
	{
		if (model_instance.pmx_model)
		{
			//	シャドウマップ描画用シェーダー
			{
				pmx_model::pipeline_state_desc desc;
				desc.name = pmx_model_shader_names[SHADOW_CASTER];
				desc.vs_path = "pmx_model_shadow_caster_vs.cso";
				model_instance.pmx_model->add_pipeline_state(get_renderdata()->device.Get(), desc);
			}

			//	GBuffer描画用シェーダー
			{
				pmx_model::pipeline_state_desc desc;
				desc.name = pmx_model_shader_names[GENARATE_GBUFFER];
				desc.vs_path = "pmx_model_gbuffer_vs.cso";
				desc.gs_path = "pmx_model_gbuffer_outline_gs.cso";
#if defined(USE_EIZIN_SHADER)
				desc.ps_path = "eizin_model_gbuffer_ps.cso";
#else	//	defined(USE_EIZIN_SHADER)
				desc.ps_path = "pmx_model_gbuffer_ps.cso";
#endif	//	defined(USE_EIZIN_SHADER)
				model_instance.pmx_model->add_pipeline_state(get_renderdata()->device.Get(), desc);
			}

			//	GBuffer描画用シェーダー
			{
				pmx_model::pipeline_state_desc desc;
				desc.name = pmx_model_shader_names[EIZIN_WAVE];
				desc.vs_path = "eizin_wave_gbuffer_vs.cso";
				desc.ps_path = "eizin_wave_gbuffer_ps.cso";
				model_instance.pmx_model->add_pipeline_state(get_renderdata()->device.Get(), desc);
			}
		}
	}
}

bool test2_scene::uninitialize_core()
{
	model_instances.clear();
	model_datas.clear();
	deferred_models.clear();
	shadow_models.clear();
	return true;
}

#ifdef USE_IMGUI
void test2_scene::imgui_render_core()
{
	if (ImGui::Begin("scene"))
	{
		//	カラーフィルター
		{
			ImGui::SliderFloat("color_filter_weight", &color_filter_constant.color_filter_weight, 0.0f, 1.0f);
			ImGui::Separator();
		}

		//	アンチエイリアス(ファイナルパスの後に適応)
		{
			const char* antialias_names[] = { "none", "fxaa", "smaa", "taa" };
			int antialias_index = static_cast<int>(antialias);
			if (ImGui::Combo("antialias", &antialias_index, antialias_names, IM_ARRAYSIZE(antialias_names)))
			{
				antialias = static_cast<antialias_mode>(antialias_index);
				taa_constant.taa_reset = 1;
			}
			switch (antialias)
			{
			case antialias_mode::fxaa:
				ImGui::SliderFloat("fxaa_subpixel_quality", &fxaa_constant.subpixel_quality, 0.0f, 1.0f);
				ImGui::SliderFloat("fxaa_edge_threshold", &fxaa_constant.edge_threshold, 0.063f, 0.333f);
				ImGui::SliderFloat("fxaa_edge_threshold_min", &fxaa_constant.edge_threshold_min, 0.0f, 0.0833f);
				break;
			case antialias_mode::smaa:
			{
				ImGui::Checkbox("smaa_draw_edge", reinterpret_cast<bool*>(&smaa_constant.smaa_draw_edge));
				ImGui::SliderFloat("smaa_threshold", &smaa_constant.smaa_threshold, 0.0f, 0.5f);
				int max_search_steps = static_cast<int>(smaa_constant.smaa_max_search_steps);
				int max_search_steps_diagonal = static_cast<int>(smaa_constant.smaa_max_search_steps_diagonal);
				int corner_rounding = static_cast<int>(smaa_constant.smaa_corner_rounding);
				ImGui::SliderInt("smaa_max_search_steps", &max_search_steps, 0, 112);
				ImGui::SliderInt("smaa_max_search_steps_diagonal", &max_search_steps_diagonal, 0, 20);
				ImGui::SliderInt("smaa_corner_rounding", &corner_rounding, 0, 100);
				ImGui::SliderFloat("smaa_local_contrast_adaptation_factor", &smaa_constant.smaa_local_contrast_adaptation_factor, 0.0f, 5.0f);
				smaa_constant.smaa_max_search_steps = static_cast<UINT>(max_search_steps);
				smaa_constant.smaa_max_search_steps_diagonal = static_cast<UINT>(max_search_steps_diagonal);
				smaa_constant.smaa_corner_rounding = static_cast<UINT>(corner_rounding);
				break;
			}
			case antialias_mode::taa:
				if (ImGui::Button("taa_reset"))
					taa_constant.taa_reset = 1;
				ImGui::Checkbox("taa_sub_pixel_jitter", &taa_is_sub_pixel_jitter);
				ImGui::Checkbox("taa_history_display", reinterpret_cast<bool*>(&taa_constant.taa_history_display));
				ImGui::SliderFloat("taa_blend_min", &taa_constant.taa_blend_min, 0.01f, 1.0f);
				ImGui::SliderFloat("taa_blend_max", &taa_constant.taa_blend_max, 0.01f, 1.0f);
				break;
			default:
				break;
			}
			ImGui::Separator();
		}

		//	glTFステージ
		if (!gltf_stages.empty() && ImGui::TreeNode("gltf stages"))
		{
			for (std::size_t i = 0; i < gltf_stages.size(); ++i)
			{
				ImGui::PushID(0xB00F + static_cast<int>(i));
				if (ImGui::TreeNode((std::string("gltf stage") + std::to_string(i)).data()))
				{
					auto& stage = gltf_stages[i];
					ImGui::DragFloat("scale", &stage.scaling, 0.1f);
					ImGui::DragFloat3("rotation", &stage.rotation.x, 0.1f);
					ImGui::DragFloat3("translation", &stage.translation.x, 0.1f);
					ImGui::Checkbox("cast_shadow", &stage.cast_shadow);

					//	マテリアル毎の英人パラメーター(名前が無いのでベースカラーテクスチャで判別する)
					for (std::size_t m = 0; m < stage.material_constants.size(); ++m)
					{
						ImGui::PushID(static_cast<int>(m));
						ID3D11ShaderResourceView* thumbnail = nullptr;
						if (m < stage.model->materials.size())
						{
							int texture_index = stage.model->materials[m].data.pbr_metallic_roughness.basecolor_texture.index;
							if (texture_index > -1)
								thumbnail = stage.model->texture_resource_views.at(stage.model->textures.at(texture_index).source).Get();
						}
						if (thumbnail)
						{
							ImGui::Image(thumbnail, { 32, 32 });
							ImGui::SameLine();
						}
						if (ImGui::TreeNode((std::string("material") + std::to_string(m)).data()))
						{
							auto& c = stage.material_constants[m];
							bool noise_off = (c.material_attribute & pmx_model::material_attribute::noise_off) != 0;
							bool color_line_off = (c.material_attribute & pmx_model::material_attribute::color_line_off) != 0;
							bool normal_depth_outline_off = (c.material_attribute & pmx_model::material_attribute::normal_depth_outline_off) != 0;
							bool color_postarize_off = (c.material_attribute & pmx_model::material_attribute::color_postarize_off) != 0;
							bool filter_off = (c.material_attribute & pmx_model::material_attribute::filter_off) != 0;
							ImGui::Checkbox("noise_off", &noise_off);
							ImGui::Checkbox("color_line_off", &color_line_off);
							ImGui::Checkbox("normal_depth_outline_off", &normal_depth_outline_off);
							ImGui::Checkbox("color_postarize_off", &color_postarize_off);
							ImGui::Checkbox("filter_off", &filter_off);
							auto set_flag = [&c](UINT flag, bool on) { c.material_attribute = on ? (c.material_attribute | flag) : (c.material_attribute & ~flag); };
							set_flag(pmx_model::material_attribute::noise_off, noise_off);
							set_flag(pmx_model::material_attribute::color_line_off, color_line_off);
							set_flag(pmx_model::material_attribute::normal_depth_outline_off, normal_depth_outline_off);
							set_flag(pmx_model::material_attribute::color_postarize_off, color_postarize_off);
							set_flag(pmx_model::material_attribute::filter_off, filter_off);

							ImGui::DragFloat("shade_factor", &c.shade_factor, 0.01f, 0.0f, 1.0f);
							ImGui::DragFloat("shading_steps", &c.specular_color.z, 0.1f);
							ImGui::DragFloat("noise_factor", &c.specular_color.w, 0.01f);
							ImGui::DragFloat("color_diff_line_factor", &c.color_diff_line_factor, 0.01f);
							ImGui::DragFloat("color_diff_line_threshold", &c.color_diff_line_threshold, 0.0001f, 0.0f, 1.0f, "%.5f");
							ImGui::DragFloat("color_line_adjust_saturate", &c.color_line_adjust_saturate, 0.01f);
							ImGui::DragFloat("color_line_adjust_brightness", &c.color_line_adjust_brightness, 0.01f);
							ImGui::DragFloat2("color_line_range", &c.outline_adjust_parameter.x);
							ImGui::DragFloat("depth_normal_outline_factor", &c.depth_normal_outline_factor, 0.001f);
							ImGui::DragFloat2("depth_normal_outline_range", &c.outline_adjust_parameter.z);
							ImGui::DragFloat("color_posterize_factor", &c.color_posterize_factor, 0.1f);
							ImGui::TreePop();
						}
						ImGui::PopID();
					}
					ImGui::TreePop();
				}
				ImGui::PopID();
			}
			ImGui::TreePop();
		}

		//	カメラ情報
		{
			auto camera_position = get_camera_position();
			ImGui::DragFloat3("camera_position", &camera_position.x);

			auto camera_rotate_x = get_camera_rotate_x();
			ImGui::DragFloat("camera_rotate_x", &camera_rotate_x);

			auto camera_rotate_y = get_camera_rotate_y();
			ImGui::DragFloat("camera_rotate_y", &camera_rotate_y);

			auto camera_distance = get_camera_distance();
			ImGui::DragFloat("camera_distance", &camera_distance);
		}

		if (ImGui::TreeNode("models"))
		{
			for (std::size_t i = 0; i < model_datas.size(); ++i)
			{
				ImGui::PushID(0xFDEF + static_cast<int>(i));
				if (ImGui::TreeNode((std::string("model") + std::to_string(i)).data()))
				{
					auto& model = model_datas[i];
					ImGui::DragFloat3("scale", &model.scaling.x, 0.1f);
					ImGui::DragFloat3("rotation", &model.rotation.x, 0.1f);
					ImGui::DragFloat3("translation", &model.translation.x, 0.1f);

					ImGui::DragFloat("animation_speed", &model.animation_speed, 0.1f);
					ImGui::DragFloat("animation_time", &model.animation_time, 0.1f);
					ImGui::TreePop();
				}
				ImGui::PopID();
			}
			for (std::size_t i = 0; i < model_instances.size(); ++i)
			{
				//	ステージによっては使わないモデル枠がある
				if (!model_instances[i].pmx_model)
					continue;
				if (ImGui::TreeNode((std::string("model instance") + std::to_string(i)).data()))
				{
					int j = 0;
					auto& instance = model_instances[i];
					for (auto& material : instance.pmx_model->get_materials())
					{
						ImGui::PushID(0xDEAD + i * 10000 + static_cast<int>(j));
						++j;
						ImGui::TextUnformatted(cp932_to_utf8(material.name).c_str());

			#if defined(USE_EIZIN_SHADER)
						ImGui::DragFloat("roughness", &material.default.specular_color.x);
						ImGui::DragFloat("metalness", &material.default.specular_color.y);
						ImGui::DragFloat("shading_steps", &material.default.specular_color.y);
						ImGui::DragFloat("noise_factor", &material.default.specular_color.y);

						ImGui::DragFloat("color_diff_line_factor", &material.default.color_diff_line_factor);
						ImGui::DragFloat("color_diff_line_threshold", &material.default.color_diff_line_threshold);
						ImGui::DragFloat("color_line_adjust_saturate", &material.default.color_line_adjust_saturate);
						ImGui::DragFloat("color_line_adjust_brightness", &material.default.color_line_adjust_brightness);
						ImGui::DragFloat2("color_line_range", &material.default.outline_adjust_parameter.x);

						ImGui::DragFloat("depth_normal_outline_factor", &material.default.depth_normal_outline_factor);
						ImGui::DragFloat2("depth_normal_outline_range", &material.default.outline_adjust_parameter.z);

						ImGui::DragFloat("color_posterize_factor", &material.default.color_posterize_factor);
						ImGui::PopID();
					}
			#endif	//	defined(USE_EIZIN_SHADER)
					ImGui::TreePop();
				}
			}
			ImGui::TreePop();
		}
		ImGui::Separator();

		if (ImGui::TreeNode("lights"))
		{
			if (ImGui::TreeNode("directionals"))
			{
				for (int i = 0; i < directional_lights.size(); ++i)
				{
					if (ImGui::TreeNode((std::string("directional ") + std::to_string(i)).data()))
					{
						if(ImGui::SliderFloat3("direction", &directional_lights[i].directional_light.direction.x, -1.0f, +1.0f))
						{
							float x = directional_lights[i].directional_light.direction.x * directional_lights[i].directional_light.direction.x
									+ directional_lights[i].directional_light.direction.y * directional_lights[i].directional_light.direction.y
									+ directional_lights[i].directional_light.direction.z * directional_lights[i].directional_light.direction.z;
							directional_lights[i].directional_light.direction.x /= sqrtf(x);
							directional_lights[i].directional_light.direction.y /= sqrtf(x);
							directional_lights[i].directional_light.direction.z /= sqrtf(x);
						}
						ImGui::ColorEdit3("color", &directional_lights[i].directional_light.color.x);
						ImGui::SliderFloat("intensity", &directional_lights[i].directional_light.color.w, 0.0f, +100.0f);
						bool use_shadow = static_cast<bool>(directional_lights[i].use_shadow);
						ImGui::Checkbox("use shadow", &use_shadow);
						directional_lights[i].use_shadow = static_cast<int>(use_shadow);
						ImGui::SliderFloat("contact_shadow", &directional_lights[i].contact_shadow, 0.0f, +0.2f);
						if (directional_lights[i].use_shadow || directional_lights[i].contact_shadow > 0)
						{
							ImGui::SliderFloat("shadow_attenuation", &directional_lights[i].shadow_attenuation, 0.0f, 1.0f);
							if (directional_lights[i].use_shadow)
								ImGui::SliderFloat("shadow_bias", &directional_lights[i].shadow_bias, 0.0f, +0.01f);
						}
						ImGui::Separator();
						ImGui::TreePop();
					}
				}
				ImGui::TreePop();
			}
			if (ImGui::TreeNode("points"))
			{
				for (int i = 0; i < point_lights.size(); ++i)
				{
					if (ImGui::TreeNode((std::string("point ") + std::to_string(i)).data()))
					{
						ImGui::SliderFloat3("position", &point_lights[i].point_light.position.x, -100.0f, +100.0f);
						ImGui::ColorEdit3("color", &point_lights[i].point_light.color.x);
						ImGui::SliderFloat("intensity", &point_lights[i].point_light.color.w, 0.0f, +100.0f);
						ImGui::SliderFloat("range", &point_lights[i].point_light.range, 0.1f, +100.0f);
						ImGui::SliderFloat("contact_shadow", &point_lights[i].contact_shadow, 0.0f, +0.2f);
						if (point_lights[i].contact_shadow > 0)
						{
							ImGui::SliderFloat("shadow_attenuation", &point_lights[i].contact_shadow, 0.0f, 1.0f);
						}
						ImGui::Separator();
						ImGui::TreePop();
					}
				}
				ImGui::TreePop();
			}
			if (ImGui::TreeNode("spots"))
			{
				for (int i = 0; i < spot_lights.size(); ++i)
				{
					if (ImGui::TreeNode((std::string("spot ") + std::to_string(i)).data()))
					{
						ImGui::SliderFloat3("position", &spot_lights[i].spot_light.position.x, -100.0f, +100.0f);
						if (ImGui::SliderFloat3("direction", &spot_lights[i].spot_light.direction.x, -1.0f, +1.0f))
						{
							float x = spot_lights[i].spot_light.direction.x * spot_lights[i].spot_light.direction.x
									+ spot_lights[i].spot_light.direction.y * spot_lights[i].spot_light.direction.y
									+ spot_lights[i].spot_light.direction.z * spot_lights[i].spot_light.direction.z;
							spot_lights[i].spot_light.direction.x /= sqrtf(x);
							spot_lights[i].spot_light.direction.y /= sqrtf(x);
							spot_lights[i].spot_light.direction.z /= sqrtf(x);
						}
						ImGui::ColorEdit3("color", &spot_lights[i].spot_light.color.x);
						ImGui::SliderFloat("intensity", &spot_lights[i].spot_light.color.w, 0.0f, +100.0f);
						ImGui::SliderFloat("range", &spot_lights[i].spot_light.range, 0.1f, +1000.0f);
						ImGui::SliderFloat("contact_shadow", &spot_lights[i].contact_shadow, 0.0f, +0.2f);

						float innerCorn = DirectX::XMConvertToDegrees(spot_lights[i].spot_light.innerCorn);
						if(ImGui::SliderFloat("inner", &innerCorn, 0.0f, +89.0f))
						{
							spot_lights[i].spot_light.innerCorn = DirectX::XMConvertToRadians(innerCorn);
						}

						float outerCorn = DirectX::XMConvertToDegrees(spot_lights[i].spot_light.outerCorn);
						if(ImGui::SliderFloat("outer", &outerCorn, 0.0f, +89.0f))
						{
							spot_lights[i].spot_light.outerCorn = DirectX::XMConvertToRadians(outerCorn);
						}

						bool use_shadow = static_cast<bool>(spot_lights[i].use_shadow);
						ImGui::Checkbox("use shadow", &use_shadow);
						spot_lights[i].use_shadow = static_cast<int>(use_shadow);
						if (spot_lights[i].use_shadow || spot_lights[i].contact_shadow > 0)
						{
							ImGui::SliderFloat("shadow_attenuation", &spot_lights[i].shadow_attenuation, 0.0f, 1.0f);
							if (spot_lights[i].use_shadow)
								ImGui::SliderFloat("shadow_bias", &spot_lights[i].shadow_bias, 0.0f, +0.01f);
						}
						ImGui::Separator();
						ImGui::TreePop();
					}
				}
				ImGui::TreePop();
			}
			if (ImGui::TreeNode("probes"))
			{
				for (int i = 0; i < light_probes.size(); ++i)
				{
					if (ImGui::TreeNode((std::string("probes ") + std::to_string(i)).data()))
					{
						ImGui::SliderFloat3("position", &light_probes[i].position.x, -100.0f, +100.0f);
						ImGui::SliderFloat3("area", &light_probes[i].area.x, -20.0f, +20.0f);
						ImGui::SliderFloat("blend rate", &light_probes[i].blend_rate, 0.0f, +1.0f);
						ImGui::Separator();
						ImGui::TreePop();
					}
				}
				ImGui::TreePop();
			}
			ImGui::TreePop();
		}
		ImGui::Separator();

		if (ImGui::TreeNode("texture"))
		{
			ImGui::Text("scene_map");
			ImGui::Image(scene_shader_resource_view.Get(), { 256, 256 }, { 0, 0 }, { 1, 1 }, { 1, 1, 1, 1 });

			//	GBufferを確認するが、もう既に何が何かわからない状態ではある
			static constexpr char* GBufferNames[]
			{
				"base_color",
				"emissive_color",
				"normal_depth",
				"parameters",
				"velocity",
				"unique_data",

				"shade_color",
			};
			for (int i = GB_Start; i < GB_Max; ++i)
			{
				ImGui::Text(GBufferNames[i]);
				ImGui::Image(g_buffer_shader_resource_view[i].Get(), { 256, 144 }, { 0, 0 }, { 1, 1 }, { 1, 1, 1, 1 });
			}

			ImGui::TreePop();
		}
	}
	ImGui::End();
}
#endif

void test2_scene::update_core(float elapsed_time)
{
	//	glTFステージの姿勢行列更新(glTFの座標系からこのシーンの座標系に変換する)
	for (auto& stage : gltf_stages)
	{
		stage.previous_world_matrix = stage.world_matrix;
		DirectX::XMMATRIX S{ DirectX::XMMatrixScaling(stage.scaling, stage.scaling, stage.scaling) };
		DirectX::XMMATRIX R{ DirectX::XMMatrixRotationRollPitchYaw(stage.rotation.x, stage.rotation.y, stage.rotation.z) };
		DirectX::XMMATRIX T{ DirectX::XMMatrixTranslation(stage.translation.x, stage.translation.y, stage.translation.z) };
		DirectX::XMStoreFloat4x4(&stage.world_matrix, get_gltf_coordinate_transform_matrix() * S * R * T);
	}

	//	姿勢行列更新
	for (auto& data : model_datas)
	{
		data.previous_world_matrix = data.world_matrix;
		DirectX::XMMATRIX S{ DirectX::XMMatrixScaling(data.scaling.x, data.scaling.y, data.scaling.z) };
		DirectX::XMMATRIX R{ DirectX::XMMatrixRotationRollPitchYaw(data.rotation.x, data.rotation.y, data.rotation.z) };
		DirectX::XMMATRIX T{ DirectX::XMMatrixTranslation(data.translation.x, data.translation.y, data.translation.z) };
		DirectX::XMMATRIX PMXSystemCoordinateMatrix = DirectX::XMMatrixSet(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1);
		//get_gltf_coordinate_transform_matrix();
		DirectX::XMStoreFloat4x4(&data.world_matrix, PMXSystemCoordinateMatrix * S * R * T);

		//	アニメーション計算
		auto& model_instance = model_instances[data.model_index];
		data.animation_time += elapsed_time * data.animation_speed;
		if(auto current_animation = model_instance.pmx_model->search_animation("default"))
		{
			if(data.animation_time > current_animation->end_time)
			{
				data.animation_time = 0;
			}
		}
		model_instance.pmx_model->compute_animation("default", data.animation_time, data.node_datas);

		//	シャドウライトを追従させる
		if (data.follow_light_index != 0xFFFFFFFFFFFFFFFF)
		{
			auto* follow_light = spot_lights.data() + data.follow_light_index;
			auto center_node = model_instance.pmx_model->search_node("センター");
			auto node_matrix = DirectX::XMLoadFloat4x4(&center_node->global_matrix) * DirectX::XMLoadFloat4x4(&data.world_matrix);
			follow_light->spot_light.position.x = DirectX::XMVectorGetX(node_matrix.r[3]) - follow_light->spot_light.direction.x * follow_light->spot_light.range * 0.5f;
			follow_light->spot_light.position.y = DirectX::XMVectorGetY(node_matrix.r[3]) - follow_light->spot_light.direction.y * follow_light->spot_light.range * 0.5f;
			follow_light->spot_light.position.z = DirectX::XMVectorGetZ(node_matrix.r[3]) - follow_light->spot_light.direction.z * follow_light->spot_light.range * 0.5f;
		}
	}
}

void test2_scene::render_core(float elapsed_time)
{
	//	TAA用サブピクセルジッター更新(シーン定数のビュー射影行列に反映される)
	update_taa_jitter();

	//	GBuffer生成
	generate_gbuffer_pass(elapsed_time);

	//	ディファードレンダリング
	deferred_pass(elapsed_time);

	//	シャドウライトパス
	shadow_light_pass(elapsed_time);

	//	最終パス
	final_pass(elapsed_time);

	get_renderdata()->clear_constant_buffers();
	get_renderdata()->clear_render_target_slots();
	get_renderdata()->clear_sampler();
	get_renderdata()->clear_shader_resorce_views();
	get_renderdata()->clear_unordered_access_views();
}

void test2_scene::generate_gbuffer_pass(float elapsed_time)
{
	static constexpr int SceneCBVIndex = 1;
	static constexpr int ModelDataCBVIndex = 2;
	static constexpr int SamplerStateIndex[3] = { 0, 1 , 2 };

	//	出力先をGBufferに変更
	{
		ID3D11RenderTargetView* render_targets[GB_Max] =
		{
			g_buffer_render_target_view[GB_BaseColor].Get(),
			g_buffer_render_target_view[GB_Emissive].Get(),
			g_buffer_render_target_view[GB_Normal_Depth].Get(),
			g_buffer_render_target_view[GB_Parameters].Get(),
			g_buffer_render_target_view[GB_Velocity].Get(),
			g_buffer_render_target_view[GB_UniqueData].Get(),

			g_buffer_render_target_view[GB_ShadeColor].Get(),
		};
		FLOAT clear_color[][4] =
		{
			{ 0.0f, 0.0f, 0.0f, 0.0f },
			{ 0.0f, 0.0f, 0.0f, 0.0f },
			{ 0.0f, 0.0f, 0.0f, 1.0f },
			{ 0.0f, 0.0f, 0.0f, 0.0f },
			{ 0.0f, 0.0f, 0.0f, 0.0f },
			{ 0.0f, 0.0f, 0.0f, 0.0f },

			{ 0.0f, 0.0f, 0.0f, 0.0f },
		};
		for (int i = GB_Start; i < GB_Max; ++i)
		{
			get_renderdata()->immediate_context->ClearRenderTargetView(render_targets[i], clear_color[i]);
		}
		get_renderdata()->immediate_context->ClearDepthStencilView(get_renderdata()->depth_stencil_view.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

		get_renderdata()->immediate_context->OMSetRenderTargets(GB_Max, render_targets, get_renderdata()->depth_stencil_view.Get());
	}

	//	リソース設定
	{
		set_scene_constant(SceneCBVIndex);

		//	専用の定数バッファを設定
		get_renderdata()->set_constant_buffer(ModelDataCBVIndex, 1, model_constant_constant_buffer.GetAddressOf());

		//	サンプラステート設定
		get_renderdata()->set_default_sampler();
	}

	//	モデル描画
	{
		get_renderdata()->immediate_context->OMSetBlendState(g_buffer_blend_state.Get(), nullptr, 0xFFFFFFFF);
		get_renderdata()->immediate_context->OMSetDepthStencilState(get_renderdata()->depth_stencil_states[static_cast<size_t>(render_data::DEPTH_STATE::ZT_ON_ZW_ON)].Get(), 0);
		get_renderdata()->immediate_context->RSSetState(get_renderdata()->rasterizer_states[static_cast<size_t>(render_data::RASTER_STATE::SOLID)].Get());

		for (auto& modedl_index : deferred_models)
		{
			auto& data = model_datas[modedl_index];
			auto& model_instance = model_instances[data.model_index];

			//	ノード更新
			model_instance.pmx_model->update_transforms(data.node_datas);

			//	モデル情報を設定
			{
				model_constants constant;
				constant.shadow_mask = data.shadow_mask;
				if( auto* face_node = model_instance.pmx_model->search_node( "頭" ) )
				{
					DirectX::XMMATRIX G = DirectX::XMLoadFloat4x4(&face_node->global_matrix);
					DirectX::XMMATRIX W = DirectX::XMLoadFloat4x4(&data.world_matrix);	//	インスタンシングするならワールドはシェーダー内で行う
					DirectX::XMMATRIX M = G * W;
					DirectX::XMVECTOR F = DirectX::XMVector3TransformNormal(DirectX::XMVectorSet(0, 0, 1, 0), M);
					DirectX::XMVECTOR R = DirectX::XMVector3TransformNormal(DirectX::XMVectorSet(1, 0, 0, 0), M);
					DirectX::XMFLOAT3 f, r;
					DirectX::XMStoreFloat3(&r, DirectX::XMVector3Normalize(R));
					DirectX::XMStoreFloat3(&f, DirectX::XMVector3Normalize(F));
					constant.mesh_direction.x = r.x;
					constant.mesh_direction.y = r.z;
					constant.mesh_direction.z = f.x;
					constant.mesh_direction.w = f.z;
				}
				get_renderdata()->immediate_context->UpdateSubresource(model_constant_constant_buffer.Get(),
																		0, 0, &constant, 0, 0);
			}

			//	描画
			model_instance.pmx_model->render(get_renderdata()->immediate_context.Get(), data.world_matrix, data.previous_world_matrix);
		}

		//	glTFステージ描画
		render_gltf_stages_gbuffer();
	}

	//	空描画
	if (sky_box_render_sprite)
	{
		//	モデル情報を設定
		{
			model_constants constant;
			constant.shadow_mask = 0xFF;
			get_renderdata()->immediate_context->UpdateSubresource(model_constant_constant_buffer.Get(), 0, 0, &constant, 0, 0);
		}

		get_renderdata()->immediate_context->OMSetBlendState(g_buffer_blend_state.Get(), nullptr, 0xFFFFFFFF);
		get_renderdata()->immediate_context->OMSetDepthStencilState(get_renderdata()->depth_stencil_states[static_cast<size_t>(render_data::DEPTH_STATE::ZT_ON_ZW_ON)].Get(), 0);
		get_renderdata()->immediate_context->RSSetState(get_renderdata()->rasterizer_states[static_cast<size_t>(render_data::RASTER_STATE::CULL_NONE)].Get());
		get_renderdata()->immediate_context->IASetInputLayout(sky_box_input_layout.Get());
		get_renderdata()->immediate_context->VSSetShader(sky_box_vertex_shader.Get(), nullptr, 0);
		get_renderdata()->immediate_context->PSSetShader(sky_box_pixel_shader.Get(), nullptr, 0);
		sky_box_render_sprite->render(get_renderdata()->immediate_context.Get(), 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
	}
}

void test2_scene::set_deferred_common_resource()
{
	static constexpr int SceneCBVIndex = 1;
	static constexpr int GBufferSRVIndex = 0;	//	0はSprite側で指定しているのでそれを考慮
	static constexpr int SamplerStateIndex = 0;

	//	リソース設定
	set_scene_constant(SceneCBVIndex, false);

	get_renderdata()->set_default_sampler();

	//	事前計算BRDF設定
	get_renderdata()->immediate_context->PSSetShaderResources(IBLSRVIndex[2], 1, lut_ggx_shader_resource_view.GetAddressOf());

	//	GBuffer設定
	ID3D11ShaderResourceView* shader_resource_views[GB_Max] =
	{
		g_buffer_shader_resource_view[GB_BaseColor].Get(),
		g_buffer_shader_resource_view[GB_Emissive].Get(),
		g_buffer_shader_resource_view[GB_Normal_Depth].Get(),
		g_buffer_shader_resource_view[GB_Parameters].Get(),
		g_buffer_shader_resource_view[GB_Velocity].Get(),
		g_buffer_shader_resource_view[GB_UniqueData].Get(),

		g_buffer_shader_resource_view[GB_ShadeColor].Get(),
	};
	get_renderdata()->immediate_context->PSSetShaderResources(GBufferSRVIndex, GB_Max, shader_resource_views);
}

void test2_scene::deferred_pass(float elapsed_time)
{
	static constexpr int LightCBVIndex = 2;
	static constexpr int ShadowSRVIndex = 10;

	//	出力先を変更
	{
		FLOAT clear_color[] = { 0.0f, 0.0f, 0.0f, 0.0f };
		get_renderdata()->immediate_context->ClearRenderTargetView(scene_render_target_view.Get(), clear_color);
		get_renderdata()->immediate_context->OMSetRenderTargets(1, scene_render_target_view.GetAddressOf(), nullptr);
	}

	// ビューポートの設定
	D3D11_VIEWPORT scene_viewport{};
	scene_viewport.TopLeftX = 0;
	scene_viewport.TopLeftY = 0;
	scene_viewport.Width = static_cast<float>(SCREEN_WIDTH);
	scene_viewport.Height = static_cast<float>(SCREEN_HEIGHT);
	scene_viewport.MinDepth = 0.0f;
	scene_viewport.MaxDepth = 1.0f;
	get_renderdata()->immediate_context->RSSetViewports(1, &scene_viewport);

	//	ライティング
	{
		//	間接光処理
		for (auto& probe : light_probes)
		{
			//	共通設定
			set_deferred_common_resource();

			//	IBLテクスチャを設定
			get_renderdata()->immediate_context->PSSetShaderResources(IBLSRVIndex[0], 1, probe.diffuse_iem_shader_resource_view.GetAddressOf());
			get_renderdata()->immediate_context->PSSetShaderResources(IBLSRVIndex[1], 1, probe.specular_pmrem_shader_resource_view.GetAddressOf());

			//	スカイボックス用と考えればOK
			if (probe.is_unbind)
			{
				//	全画面描画
				get_renderdata()->immediate_context->OMSetDepthStencilState(get_renderdata()->depth_stencil_states[static_cast<size_t>(render_data::DEPTH_STATE::ZT_OFF_ZW_OFF)].Get(), 0);
				get_renderdata()->immediate_context->RSSetState(get_renderdata()->rasterizer_states[static_cast<size_t>(render_data::RASTER_STATE::CULL_NONE)].Get());
				get_renderdata()->immediate_context->OMSetBlendState(get_renderdata()->blend_states[static_cast<size_t>(render_data::BLEND_STATE::ALPHA)].Get(), nullptr, 0xFFFFFFFF);
				get_renderdata()->immediate_context->RSSetState(get_renderdata()->rasterizer_states[static_cast<size_t>(render_data::RASTER_STATE::CULL_NONE)].Get());

				get_renderdata()->immediate_context->VSSetShader(sprite_vertex_shader.Get(), nullptr, 0);
				get_renderdata()->immediate_context->IASetInputLayout(sprite_input_layout.Get());
				get_renderdata()->immediate_context->PSSetShader(deferred_rendering_indirect_pixel_shader.Get(), nullptr, 0);
				deferred_rendering_sprite->render(get_renderdata()->immediate_context.Get(), 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
			}
			else	//	場所に応じて間接光をブレンドする(別のIBLで上書きするだけだが……)
			{
				//	描画範囲制限
				probe_constants probe_constant;
				probe_constant.position = probe.position;
				probe_constant.area = probe.area;
				probe_constant.blend_rate = probe.blend_rate;
				get_renderdata()->immediate_context->UpdateSubresource(probe_constant_constant_buffer.Get(), 0, 0, &probe_constant, 0, 0);
				get_renderdata()->immediate_context->PSSetConstantBuffers(LightCBVIndex, 1, probe_constant_constant_buffer.GetAddressOf());

				//	描画ターゲット設定
				//	ステンシルのみ使用
				get_renderdata()->immediate_context->OMSetRenderTargets(1, scene_render_target_view.GetAddressOf(), get_renderdata()->depth_stencil_view.Get());

				//	姿勢行列生成
				DirectX::XMMATRIX World = DirectX::XMMatrixScaling(probe.area.x * 2, probe.area.y * 2, probe.area.z * 2);
				World *= DirectX::XMMatrixTranslation(probe.position.x, probe.position.y, probe.position.z);
				DirectX::XMFLOAT4X4 world;
				DirectX::XMStoreFloat4x4(&world, World);

				get_renderdata()->immediate_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

				//	1.裏面描画
				{
					get_renderdata()->immediate_context->RSSetState(get_renderdata()->rasterizer_states[static_cast<size_t>(render_data::RASTER_STATE::SOLID_CULL_FRONT)].Get());
					get_renderdata()->immediate_context->OMSetBlendState(get_renderdata()->blend_states[static_cast<size_t>(render_data::BLEND_STATE::NONE)].Get(), nullptr, 0xFFFFFFFF);
					get_renderdata()->immediate_context->OMSetDepthStencilState(deferred_light_depth_stencil_state.Get(), 1);
					get_renderdata()->immediate_context->VSSetShader(geometric_primitive_vertex_shader.Get(), nullptr, 0);
					get_renderdata()->immediate_context->IASetInputLayout(geometric_primitive_input_layout.Get());
					get_renderdata()->immediate_context->PSSetShader(nullptr, nullptr, 0);
					light_probe_cube->render(get_renderdata()->immediate_context.Get(), world, { 1, 1, 1, 1 });
				}

				//	2.表面描画
				get_renderdata()->immediate_context->OMSetDepthStencilState(deferred_light_depth_stencil_state.Get(), 0);
				get_renderdata()->immediate_context->OMSetBlendState(get_renderdata()->blend_states[static_cast<size_t>(render_data::BLEND_STATE::ALPHA)].Get(), nullptr, 0xFFFFFFFF);
				bool is_inner = true;
				if (!(probe.position.x - probe.area.x <= get_camera_position().x && get_camera_position().x <= probe.position.x + probe.area.x))
					is_inner = false;
				if (!(probe.position.y - probe.area.y <= get_camera_position().y && get_camera_position().y <= probe.position.y + probe.area.y))
					is_inner = false;
				if (!(probe.position.z - probe.area.z <= get_camera_position().z && get_camera_position().z <= probe.position.z + probe.area.z))
					is_inner = false;
				if (is_inner)
				{
					get_renderdata()->immediate_context->RSSetState(get_renderdata()->rasterizer_states[static_cast<size_t>(render_data::RASTER_STATE::CULL_NONE)].Get());

					get_renderdata()->immediate_context->VSSetShader(sprite_vertex_shader.Get(), nullptr, 0);
					get_renderdata()->immediate_context->IASetInputLayout(sprite_input_layout.Get());
					get_renderdata()->immediate_context->PSSetShader(deferred_primitive_lighting_indirect_inner_pixel_shader.Get(), nullptr, 0);
					deferred_rendering_sprite->render(get_renderdata()->immediate_context.Get(), 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
				}
				else
				{
					get_renderdata()->immediate_context->RSSetState(get_renderdata()->rasterizer_states[static_cast<size_t>(render_data::RASTER_STATE::SOLID)].Get());
					get_renderdata()->immediate_context->VSSetShader(deferred_primitive_lighting_vertex_shader.Get(), nullptr, 0);
					get_renderdata()->immediate_context->IASetInputLayout(deferred_primitive_lighting_input_layout.Get());
					get_renderdata()->immediate_context->PSSetShader(deferred_primitive_lighting_indirect_pixel_shader.Get(), nullptr, 0);
					light_probe_cube->render(get_renderdata()->immediate_context.Get(), world, { 1, 1, 1, 1 });
				}
			}
		}

		//	自己発光処理
		{
			//	共通設定
			set_deferred_common_resource();
			get_renderdata()->immediate_context->OMSetDepthStencilState(get_renderdata()->depth_stencil_states[static_cast<size_t>(render_data::DEPTH_STATE::ZT_OFF_ZW_OFF)].Get(), 0);
			get_renderdata()->immediate_context->RSSetState(get_renderdata()->rasterizer_states[static_cast<size_t>(render_data::RASTER_STATE::CULL_NONE)].Get());
			get_renderdata()->immediate_context->OMSetBlendState(get_renderdata()->blend_states[static_cast<size_t>(render_data::BLEND_STATE::ADD)].Get(), nullptr, 0xFFFFFFFF);
			get_renderdata()->immediate_context->RSSetState(get_renderdata()->rasterizer_states[static_cast<size_t>(render_data::RASTER_STATE::CULL_NONE)].Get());

			get_renderdata()->immediate_context->VSSetShader(sprite_vertex_shader.Get(), nullptr, 0);
			get_renderdata()->immediate_context->IASetInputLayout(sprite_input_layout.Get());
			get_renderdata()->immediate_context->PSSetShader(deferred_rendering_emissive_pixel_shader.Get(), nullptr, 0);
			deferred_rendering_sprite->render(get_renderdata()->immediate_context.Get(), 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
		}

		//	平行光源
		for (auto& light : directional_lights)
		{
			//	シャドウマップ生成
			{
				get_renderdata()->clear_shader_resorce_views(ShadowSRVIndex, 1);
				directional_shadow_rendering(light);
			}

			//	リソース設定
			{
				//	共通設定
				set_deferred_common_resource();

				//	ライト情報設定
				light_constants light_constant;
				light_constant.compund_light_data.work_data[0] = light.directional_light.direction;
				light_constant.compund_light_data.work_data[1] = light.directional_light.color;
				light_constant.compund_light_data.work_data[3].w = light_kind_directional_light;
				light_constant.contact_shadow = light.contact_shadow;
				light_constant.use_shadow = light.use_shadow;
				light_constant.shadow_attenuation = light.shadow_attenuation;
				light_constant.shadow_bias = light.shadow_bias;
				light_constant.shadow_mask = light.shadow_mask;
				light_constant.light_view_projection = light.light_view_projection;
				get_renderdata()->immediate_context->UpdateSubresource(light_constant_buffer.Get(), 0, 0, &light_constant, 0, 0);
				get_renderdata()->immediate_context->PSSetConstantBuffers(LightCBVIndex, 1, light_constant_buffer.GetAddressOf());

				//	シャドウマップ設定
				get_renderdata()->immediate_context->PSSetShaderResources(ShadowSRVIndex, 1, shadowmap_shader_resource_view.GetAddressOf());
				get_renderdata()->immediate_context->PSSetSamplers(ShadowSRVIndex, 1, shadowmap_sampler_state.GetAddressOf());
			}

			get_renderdata()->immediate_context->RSSetViewports(1, &scene_viewport);
			get_renderdata()->immediate_context->OMSetRenderTargets(1, scene_render_target_view.GetAddressOf(), get_renderdata()->depth_stencil_view.Get());

			get_renderdata()->immediate_context->OMSetDepthStencilState(get_renderdata()->depth_stencil_states[static_cast<size_t>(render_data::DEPTH_STATE::ZT_OFF_ZW_OFF)].Get(), 0);
			get_renderdata()->immediate_context->OMSetBlendState(get_renderdata()->blend_states[static_cast<size_t>(render_data::BLEND_STATE::ADD)].Get(), nullptr, 0xFFFFFFFF);
			get_renderdata()->immediate_context->RSSetState(get_renderdata()->rasterizer_states[static_cast<size_t>(render_data::RASTER_STATE::CULL_NONE)].Get());

			get_renderdata()->immediate_context->VSSetShader(sprite_vertex_shader.Get(), nullptr, 0);
			get_renderdata()->immediate_context->IASetInputLayout(sprite_input_layout.Get());
			get_renderdata()->immediate_context->PSSetShader(deferred_rendering_directional_pixel_shader.Get(), nullptr, 0);
			deferred_rendering_sprite->render(get_renderdata()->immediate_context.Get(), 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
		}

		//	点光源
		//	本来はライトの種別に応じてライティング範囲を制限するべき
		for (auto& light : point_lights)
		{
			//	シャドウマップ生成
			{
				get_renderdata()->clear_shader_resorce_views(ShadowSRVIndex, 1);
				point_shadow_rendering(light);
			}

			//	リソース設定
			{
				//	共通設定
				set_deferred_common_resource();

				//	ライト情報設定
				light_constants light_constant;
				light_constant.compund_light_data.work_data[0] = light.point_light.position;
				light_constant.compund_light_data.work_data[1] = light.point_light.color;
				light_constant.compund_light_data.work_data[2].x = light.point_light.range;
				light_constant.compund_light_data.work_data[3].w = light_kind_point_light;
				light_constant.contact_shadow = light.contact_shadow;
				light_constant.shadow_attenuation = light.shadow_attenuation;
				get_renderdata()->immediate_context->UpdateSubresource(light_constant_buffer.Get(), 0, 0, &light_constant, 0, 0);
				get_renderdata()->immediate_context->PSSetConstantBuffers(LightCBVIndex, 1, light_constant_buffer.GetAddressOf());

				//	シャドウマップ設定
				get_renderdata()->immediate_context->PSSetShaderResources(ShadowSRVIndex, 1, shadowmap_shader_resource_view.GetAddressOf());
				get_renderdata()->immediate_context->PSSetSamplers(ShadowSRVIndex, 1, shadowmap_sampler_state.GetAddressOf());
			}

			get_renderdata()->immediate_context->RSSetViewports(1, &scene_viewport);

			//	描画ターゲット設定
			//	ステンシルのみ使用
			get_renderdata()->immediate_context->OMSetRenderTargets(1, scene_render_target_view.GetAddressOf(), get_renderdata()->depth_stencil_view.Get());

			//	姿勢行列生成
			float range = light.point_light.range * 2;
			DirectX::XMMATRIX World = DirectX::XMMatrixScaling(range, range, range);
			World *= DirectX::XMMatrixTranslation(light.point_light.position.x, light.point_light.position.y, light.point_light.position.z);
			DirectX::XMFLOAT4X4 world;
			DirectX::XMStoreFloat4x4(&world, World);

			get_renderdata()->immediate_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

			//	1.裏面描画
			{
				get_renderdata()->immediate_context->RSSetState(get_renderdata()->rasterizer_states[static_cast<size_t>(render_data::RASTER_STATE::SOLID_CULL_FRONT)].Get());
				get_renderdata()->immediate_context->OMSetBlendState(get_renderdata()->blend_states[static_cast<size_t>(render_data::BLEND_STATE::NONE)].Get(), nullptr, 0xFFFFFFFF);
				get_renderdata()->immediate_context->OMSetDepthStencilState(deferred_light_depth_stencil_state.Get(), 1);
				get_renderdata()->immediate_context->VSSetShader(geometric_primitive_vertex_shader.Get(), nullptr, 0);
				get_renderdata()->immediate_context->IASetInputLayout(geometric_primitive_input_layout.Get());
				get_renderdata()->immediate_context->PSSetShader(nullptr, nullptr, 0);
				pointlight_sphere->render(get_renderdata()->immediate_context.Get(), world, { 1, 1, 1, 1 });
			}

			//	2.表面描画
			get_renderdata()->immediate_context->OMSetBlendState(get_renderdata()->blend_states[static_cast<size_t>(render_data::BLEND_STATE::ADD)].Get(), nullptr, 0xFFFFFFFF);
			get_renderdata()->immediate_context->OMSetDepthStencilState(deferred_light_depth_stencil_state.Get(), 0);
			DirectX::XMVECTOR camera_position = DirectX::XMLoadFloat3(&get_camera_position());
			DirectX::XMVECTOR light_position = DirectX::XMLoadFloat4(&light.point_light.position);
			float l = DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMVectorSubtract(camera_position, light_position)));
			if (l <= light.point_light.range + 1.0f)
			{
				//	範囲内の場合はフルスクリーン
				get_renderdata()->immediate_context->RSSetState(get_renderdata()->rasterizer_states[static_cast<size_t>(render_data::RASTER_STATE::CULL_NONE)].Get());

				get_renderdata()->immediate_context->VSSetShader(sprite_vertex_shader.Get(), nullptr, 0);
				get_renderdata()->immediate_context->IASetInputLayout(sprite_input_layout.Get());
				get_renderdata()->immediate_context->PSSetShader(deferred_rendering_point_pixel_shader.Get(), nullptr, 0);
				deferred_rendering_sprite->render(get_renderdata()->immediate_context.Get(), 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
			}
			else
			{
				get_renderdata()->immediate_context->RSSetState(get_renderdata()->rasterizer_states[static_cast<size_t>(render_data::RASTER_STATE::SOLID)].Get());

				get_renderdata()->immediate_context->VSSetShader(deferred_primitive_lighting_vertex_shader.Get(), nullptr, 0);
				get_renderdata()->immediate_context->IASetInputLayout(deferred_primitive_lighting_input_layout.Get());
				get_renderdata()->immediate_context->PSSetShader(deferred_primitive_lighting_point_pixel_shader.Get(), nullptr, 0);
				pointlight_sphere->render(get_renderdata()->immediate_context.Get(), world, { 1, 1, 1, 1 });
			}
		}

		//	スポットライト光源
		//	本来はライトの種別に応じてライティング範囲を制限するべき
		for (auto& light : spot_lights)
		{
			//	シャドウライトであればここでは描画しない
			if(light.shadow_mask != 0)
				continue;

			//	シャドウマップ生成
			{
				get_renderdata()->clear_shader_resorce_views(ShadowSRVIndex, 1);
				spot_shadow_rendering(light);
			}

			//	リソース設定
			{
				//	共通設定
				set_deferred_common_resource();

				//	ライト情報設定
				light_constants light_constant;
				light_constant.compund_light_data.work_data[0] = light.spot_light.position;
				light_constant.compund_light_data.work_data[1] = light.spot_light.direction;
				light_constant.compund_light_data.work_data[2] = light.spot_light.color;
				light_constant.compund_light_data.work_data[3].x = light.spot_light.range;
				light_constant.compund_light_data.work_data[3].y = light.spot_light.innerCorn;
				light_constant.compund_light_data.work_data[3].z = light.spot_light.outerCorn;
				light_constant.compund_light_data.work_data[3].w = light_kind_spot_light;
				light_constant.contact_shadow = light.contact_shadow;
				light_constant.use_shadow = light.use_shadow;
				light_constant.shadow_attenuation = light.shadow_attenuation;
				light_constant.shadow_bias = light.shadow_bias;
				light_constant.shadow_mask = light.shadow_mask;
				light_constant.light_view_projection = light.light_view_projection;
				get_renderdata()->immediate_context->UpdateSubresource(light_constant_buffer.Get(), 0, 0, &light_constant, 0, 0);
				get_renderdata()->immediate_context->PSSetConstantBuffers(LightCBVIndex, 1, light_constant_buffer.GetAddressOf());

				//	シャドウマップ設定
				get_renderdata()->immediate_context->PSSetShaderResources(ShadowSRVIndex, 1, shadowmap_shader_resource_view.GetAddressOf());
				get_renderdata()->immediate_context->PSSetSamplers(ShadowSRVIndex, 1, shadowmap_sampler_state.GetAddressOf());
			}

			get_renderdata()->immediate_context->RSSetViewports(1, &scene_viewport);

			//	描画ターゲット設定
			//	ステンシルのみ使用
			get_renderdata()->immediate_context->OMSetRenderTargets(1, scene_render_target_view.GetAddressOf(), get_renderdata()->depth_stencil_view.Get());

			//	姿勢行列生成
			float range = light.spot_light.range * 2;
			DirectX::XMMATRIX World = DirectX::XMMatrixScaling(range, range, range);
			World *= DirectX::XMMatrixTranslation(light.spot_light.position.x, light.spot_light.position.y, light.spot_light.position.z);
			DirectX::XMFLOAT4X4 world;
			DirectX::XMStoreFloat4x4(&world, World);

			get_renderdata()->immediate_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

			//	1.裏面描画
			{
				get_renderdata()->immediate_context->RSSetState(get_renderdata()->rasterizer_states[static_cast<size_t>(render_data::RASTER_STATE::SOLID_CULL_FRONT)].Get());
				get_renderdata()->immediate_context->OMSetBlendState(get_renderdata()->blend_states[static_cast<size_t>(render_data::BLEND_STATE::NONE)].Get(), nullptr, 0xFFFFFFFF);
				get_renderdata()->immediate_context->OMSetDepthStencilState(deferred_light_depth_stencil_state.Get(), 1);
				get_renderdata()->immediate_context->VSSetShader(geometric_primitive_vertex_shader.Get(), nullptr, 0);
				get_renderdata()->immediate_context->IASetInputLayout(geometric_primitive_input_layout.Get());
				get_renderdata()->immediate_context->PSSetShader(nullptr, nullptr, 0);
				spotlight_sphere->render(get_renderdata()->immediate_context.Get(), world, { 1, 1, 1, 1 });
			}

			//	2.表面描画
			get_renderdata()->immediate_context->OMSetBlendState(get_renderdata()->blend_states[static_cast<size_t>(render_data::BLEND_STATE::ADD)].Get(), nullptr, 0xFFFFFFFF);
			get_renderdata()->immediate_context->OMSetDepthStencilState(deferred_light_depth_stencil_state.Get(), 0);

			DirectX::XMVECTOR camera_position = DirectX::XMLoadFloat3(&get_camera_position());
			DirectX::XMVECTOR light_position = DirectX::XMLoadFloat4(&light.spot_light.position);
			float l = DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMVectorSubtract(camera_position, light_position)));
			if (l <= light.spot_light.range + 1.0f)
			{
				get_renderdata()->immediate_context->OMSetRenderTargets(1, scene_render_target_view.GetAddressOf(), get_renderdata()->depth_stencil_view.Get());

				get_renderdata()->immediate_context->RSSetState(get_renderdata()->rasterizer_states[static_cast<size_t>(render_data::RASTER_STATE::CULL_NONE)].Get());

				get_renderdata()->immediate_context->VSSetShader(sprite_vertex_shader.Get(), nullptr, 0);
				get_renderdata()->immediate_context->IASetInputLayout(sprite_input_layout.Get());
				get_renderdata()->immediate_context->PSSetShader(deferred_rendering_spot_pixel_shader.Get(), nullptr, 0);
				deferred_rendering_sprite->render(get_renderdata()->immediate_context.Get(), 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
			}
			else
			{
				//	描画範囲を制限する方式
				//	本当はコーンなわけですが……
				get_renderdata()->immediate_context->RSSetState(get_renderdata()->rasterizer_states[static_cast<size_t>(render_data::RASTER_STATE::SOLID)].Get());

				get_renderdata()->immediate_context->VSSetShader(deferred_primitive_lighting_vertex_shader.Get(), nullptr, 0);
				get_renderdata()->immediate_context->IASetInputLayout(deferred_primitive_lighting_input_layout.Get());
				get_renderdata()->immediate_context->PSSetShader(deferred_primitive_lighting_spot_pixel_shader.Get(), nullptr, 0);
				spotlight_sphere->render(get_renderdata()->immediate_context.Get(), world, { 1, 1, 1, 1 });
			}
		}
	}

	//	次回GBuffer生成時に警告が出てしまうため外しておく。
	get_renderdata()->clear_shader_resorce_views(0, GB_Max);
	get_renderdata()->clear_shader_resorce_views(ShadowSRVIndex, 1);
}

void test2_scene::shadow_light_pass(float elapsed_time)
{
	static constexpr int LightCBVIndex = 2;
	static constexpr int ShadowSRVIndex = 10;

	// ビューポートの設定
	D3D11_VIEWPORT scene_viewport{};
	scene_viewport.TopLeftX = 0;
	scene_viewport.TopLeftY = 0;
	scene_viewport.Width = static_cast<float>(SCREEN_WIDTH);
	scene_viewport.Height = static_cast<float>(SCREEN_HEIGHT);
	scene_viewport.MinDepth = 0.0f;
	scene_viewport.MaxDepth = 1.0f;

	//	シャドウスポットライト
	//	@remainer	別に平行光源でも良いが、取り回しやすいためスポットライトにしている
	for (auto& light : spot_lights)
	{
		if(light.shadow_mask == 0)
			continue;

		//	シャドウマップ生成
		{
			get_renderdata()->clear_shader_resorce_views(ShadowSRVIndex, 1);
			spot_shadow_rendering(light);
		}

		//	リソース設定
		{
			//	共通設定
			set_deferred_common_resource();

			//	ライト情報設定
			light_constants light_constant;
			light_constant.compund_light_data.work_data[0] = light.spot_light.position;
			light_constant.compund_light_data.work_data[1] = light.spot_light.direction;
			light_constant.compund_light_data.work_data[2] = light.spot_light.color;
			light_constant.compund_light_data.work_data[3].x = light.spot_light.range;
			light_constant.compund_light_data.work_data[3].y = light.spot_light.innerCorn;
			light_constant.compund_light_data.work_data[3].z = light.spot_light.outerCorn;
			light_constant.compund_light_data.work_data[3].w = light_kind_spot_light;
			light_constant.contact_shadow = light.contact_shadow;
			light_constant.use_shadow = light.use_shadow;
			light_constant.shadow_attenuation = light.shadow_attenuation;
			light_constant.shadow_bias = light.shadow_bias;
			light_constant.shadow_mask = light.shadow_mask;
			light_constant.light_view_projection = light.light_view_projection;
			get_renderdata()->immediate_context->UpdateSubresource(light_constant_buffer.Get(), 0, 0, &light_constant, 0, 0);
			get_renderdata()->immediate_context->PSSetConstantBuffers(LightCBVIndex, 1, light_constant_buffer.GetAddressOf());

			//	シャドウマップ設定
			get_renderdata()->immediate_context->PSSetShaderResources(ShadowSRVIndex, 1, shadowmap_shader_resource_view.GetAddressOf());
			get_renderdata()->immediate_context->PSSetSamplers(ShadowSRVIndex, 1, shadowmap_sampler_state.GetAddressOf());
		}

		get_renderdata()->immediate_context->RSSetViewports(1, &scene_viewport);

		//	描画ターゲット設定
		//	ステンシルのみ使用
		get_renderdata()->immediate_context->OMSetRenderTargets(1, scene_render_target_view.GetAddressOf(), get_renderdata()->depth_stencil_view.Get());

		//	姿勢行列生成
		float range = light.spot_light.range * 2;
		DirectX::XMMATRIX World = DirectX::XMMatrixScaling(range, range, range);
		World *= DirectX::XMMatrixTranslation(light.spot_light.position.x, light.spot_light.position.y, light.spot_light.position.z);
		DirectX::XMFLOAT4X4 world;
		DirectX::XMStoreFloat4x4(&world, World);

		get_renderdata()->immediate_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		//	1.裏面描画
		{
			get_renderdata()->immediate_context->RSSetState(get_renderdata()->rasterizer_states[static_cast<size_t>(render_data::RASTER_STATE::SOLID_CULL_FRONT)].Get());
			get_renderdata()->immediate_context->OMSetBlendState(get_renderdata()->blend_states[static_cast<size_t>(render_data::BLEND_STATE::NONE)].Get(), nullptr, 0xFFFFFFFF);
			get_renderdata()->immediate_context->OMSetDepthStencilState(deferred_light_depth_stencil_state.Get(), 1);
			get_renderdata()->immediate_context->VSSetShader(geometric_primitive_vertex_shader.Get(), nullptr, 0);
			get_renderdata()->immediate_context->IASetInputLayout(geometric_primitive_input_layout.Get());
			get_renderdata()->immediate_context->PSSetShader(nullptr, nullptr, 0);
			spotlight_sphere->render(get_renderdata()->immediate_context.Get(), world, { 1, 1, 1, 1 });
		}

		//	2.表面描画
		get_renderdata()->immediate_context->OMSetBlendState(get_renderdata()->blend_states[static_cast<size_t>(render_data::BLEND_STATE::MULTIPLY)].Get(), nullptr, 0xFFFFFFFF);
		get_renderdata()->immediate_context->OMSetDepthStencilState(deferred_light_depth_stencil_state.Get(), 0);

		DirectX::XMVECTOR camera_position = DirectX::XMLoadFloat3(&get_camera_position());
		DirectX::XMVECTOR light_position = DirectX::XMLoadFloat4(&light.spot_light.position);
		float l = DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMVectorSubtract(camera_position, light_position)));
		if (l <= light.spot_light.range + 1.0f)
		{
			get_renderdata()->immediate_context->OMSetRenderTargets(1, scene_render_target_view.GetAddressOf(), get_renderdata()->depth_stencil_view.Get());

			get_renderdata()->immediate_context->RSSetState(get_renderdata()->rasterizer_states[static_cast<size_t>(render_data::RASTER_STATE::CULL_NONE)].Get());

			get_renderdata()->immediate_context->VSSetShader(sprite_vertex_shader.Get(), nullptr, 0);
			get_renderdata()->immediate_context->IASetInputLayout(sprite_input_layout.Get());
			get_renderdata()->immediate_context->PSSetShader(deferred_rendering_shadow_spot_light_pixel_shader.Get(), nullptr, 0);
			deferred_rendering_sprite->render(get_renderdata()->immediate_context.Get(), 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
		}
		else
		{
			//	描画範囲を制限する方式
			//	本当はコーンなわけですが……
			get_renderdata()->immediate_context->RSSetState(get_renderdata()->rasterizer_states[static_cast<size_t>(render_data::RASTER_STATE::SOLID)].Get());

			get_renderdata()->immediate_context->VSSetShader(deferred_primitive_lighting_vertex_shader.Get(), nullptr, 0);
			get_renderdata()->immediate_context->IASetInputLayout(deferred_primitive_lighting_input_layout.Get());
			get_renderdata()->immediate_context->PSSetShader(deferred_primitive_lighting_shadow_spot_light_pixel_shader.Get(), nullptr, 0);
			spotlight_sphere->render(get_renderdata()->immediate_context.Get(), world, { 1, 1, 1, 1 });
		}
	}

	//	次回GBuffer生成時に警告が出てしまうため外しておく。
	get_renderdata()->clear_shader_resorce_views(0, GB_Max);
	get_renderdata()->clear_shader_resorce_views(ShadowSRVIndex, 1);
}

void test2_scene::final_pass(float elapsed_time)
{
	const bool use_antialias = antialias != antialias_mode::none;

	//	出力先を変更(アンチエイリアスを使う場合は中間バッファ、使わない場合はバックバッファ)
	{
		FLOAT clear_color[] = { 0.0f, 0.0f, 0.0f, 0.0f };
		ID3D11RenderTargetView* render_target_view = use_antialias ? filtered_render_target_view.Get() : get_renderdata()->render_target_view.Get();
		get_renderdata()->immediate_context->ClearRenderTargetView(render_target_view, clear_color);
//		get_renderdata()->immediate_context->ClearDepthStencilView(get_renderdata()->depth_stencil_view.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
		get_renderdata()->immediate_context->OMSetRenderTargets(1, &render_target_view, nullptr);
	}

	//	鈴木英人フィルター
	{
		set_deferred_common_resource();
		get_renderdata()->immediate_context->OMSetBlendState(get_renderdata()->blend_states[static_cast<size_t>(render_data::BLEND_STATE::NONE)].Get(), nullptr, 0xFFFFFFFF);
		get_renderdata()->immediate_context->OMSetDepthStencilState(get_renderdata()->depth_stencil_states[static_cast<size_t>(render_data::DEPTH_STATE::ZT_OFF_ZW_OFF)].Get(), 0);
		get_renderdata()->immediate_context->RSSetState(get_renderdata()->rasterizer_states[static_cast<size_t>(render_data::RASTER_STATE::CULL_NONE)].Get());
		get_renderdata()->immediate_context->VSSetShader(sprite_vertex_shader.Get(), nullptr, 0);
		get_renderdata()->immediate_context->IASetInputLayout(sprite_input_layout.Get());
		get_renderdata()->immediate_context->UpdateSubresource(color_filter_constant_buffer.Get(), 0, 0, &color_filter_constant, 0, 0);
		get_renderdata()->immediate_context->PSSetConstantBuffers(11, 1, color_filter_constant_buffer.GetAddressOf());

		get_renderdata()->immediate_context->PSSetShader(color_filter_pixel_shader.Get(), nullptr, 0);
		final_pass_render_sprite->render(get_renderdata()->immediate_context.Get(), 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
	}

	if (!use_antialias)
		return;

	//	鈴木英人フィルターの後にアンチエイリアスをかける(入力はガンマ補正済みの色)
	antialias_pass();

	//	アンチエイリアス結果をバックバッファに書き出す
	{
		get_renderdata()->immediate_context->OMSetRenderTargets(1, get_renderdata()->render_target_view.GetAddressOf(), nullptr);
		get_renderdata()->set_default_sampler();
		get_renderdata()->immediate_context->OMSetBlendState(get_renderdata()->blend_states[static_cast<size_t>(render_data::BLEND_STATE::NONE)].Get(), nullptr, 0xFFFFFFFF);
		get_renderdata()->immediate_context->OMSetDepthStencilState(get_renderdata()->depth_stencil_states[static_cast<size_t>(render_data::DEPTH_STATE::ZT_OFF_ZW_OFF)].Get(), 0);
		get_renderdata()->immediate_context->RSSetState(get_renderdata()->rasterizer_states[static_cast<size_t>(render_data::RASTER_STATE::CULL_NONE)].Get());
		get_renderdata()->immediate_context->VSSetShader(sprite_vertex_shader.Get(), nullptr, 0);
		get_renderdata()->immediate_context->IASetInputLayout(sprite_input_layout.Get());
		get_renderdata()->immediate_context->PSSetShader(sprite_pixel_shader.Get(), nullptr, 0);
		final_pass_antialias_render_sprite->render(get_renderdata()->immediate_context.Get(), 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
	}
}

void test2_scene::directional_shadow_rendering(directional_light_constants& light_data)
{
	if (!light_data.use_shadow)
		return;

	static constexpr int SceneCBVIndex = 1;

	//	出力先をバックバッファに変更
	get_renderdata()->immediate_context->ClearDepthStencilView(shadowmap_depth_stencil_view.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
	get_renderdata()->immediate_context->OMSetRenderTargets(0, nullptr, shadowmap_depth_stencil_view.Get());

	// ビューポートの設定
	D3D11_VIEWPORT viewport{};
	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.Width = static_cast<float>(SHADOWMAP_WIDTH);
	viewport.Height = static_cast<float>(SHADOWMAP_HEIGHT);
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	get_renderdata()->immediate_context->RSSetViewports(1, &viewport);

	{
		// ライトの位置から見た視線行列を生成
		DirectX::XMVECTOR LightPosition = DirectX::XMLoadFloat4(&light_data.directional_light.direction);
		LightPosition = DirectX::XMVectorScale(LightPosition, -1000);
		DirectX::XMMATRIX V = DirectX::XMMatrixLookAtLH(LightPosition,
			DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f),
			DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));

		// シャドウマップに描画したい範囲の射影行列を生成
		DirectX::XMMATRIX P = DirectX::XMMatrixOrthographicLH(SHADOWMAP_DRAWRECT, SHADOWMAP_DRAWRECT, 0.1f, 5000.0f);
		// ライトビュー行列を保存
		DirectX::XMStoreFloat4x4(&light_data.light_view_projection, V * P);

		// 定数バッファの更新
		{
			// 0番はメッシュ側で更新している

			scene_constants scene{};
			scene.options.x = static_cast<float>(get_cursor_position().x);
			scene.options.y = static_cast<float>(get_cursor_position().y);
			scene.options.z = get_timer();
			scene.options.w = get_flag();
			scene.camera_position.x = get_camera_position().x;
			scene.camera_position.y = get_camera_position().y;
			scene.camera_position.z = get_camera_position().z;
			scene.camera_position.w = 1;
			DirectX::XMStoreFloat4x4(&scene.view_transform, V);
			DirectX::XMStoreFloat4x4(&scene.projection_transform, P);
			scene.view_projection_transform = light_data.light_view_projection;
			DirectX::XMStoreFloat4x4(&scene.inverse_view_transform, DirectX::XMMatrixInverse(nullptr, V));
			DirectX::XMStoreFloat4x4(&scene.inverse_projection_transform, DirectX::XMMatrixInverse(nullptr, P));
			DirectX::XMStoreFloat4x4(&scene.inverse_view_projection_transform, DirectX::XMMatrixInverse(nullptr, V * P));

			get_renderdata()->immediate_context->UpdateSubresource(shadowmap_scene_constant_buffer.Get(), 0, 0, &scene, 0, 0);
			get_renderdata()->set_constant_buffer(SceneCBVIndex, 1, shadowmap_scene_constant_buffer.GetAddressOf());
		}

		get_renderdata()->immediate_context->OMSetBlendState(get_renderdata()->blend_states[static_cast<size_t>(render_data::BLEND_STATE::ALPHA)].Get(), nullptr, 0xFFFFFFFF);
		get_renderdata()->immediate_context->OMSetDepthStencilState(get_renderdata()->depth_stencil_states[static_cast<size_t>(render_data::DEPTH_STATE::ZT_ON_ZW_ON)].Get(), 0);

		//	モデル描画
		for (auto& shadow_model : shadow_models)
		{
			auto& data = model_datas[shadow_model];
			auto& instance = model_instances[data.model_index];

			//	インスタンスのシャドウマスクが同じ値であれば影生成
			if(data.shadow_mask != light_data.shadow_mask)
				continue;

			//	ノード更新
			instance.pmx_model->update_transforms(data.node_datas);

			//	描画
			instance.pmx_model->render(get_renderdata()->immediate_context.Get(), data.world_matrix, data.previous_world_matrix, pmx_model_shader_names[SHADOW_CASTER]);
		}

		//	glTFステージ描画
		render_gltf_stages_shadow(light_data.shadow_mask);
	}

	//	不要だから外す
	get_renderdata()->immediate_context->OMSetRenderTargets(0, nullptr, nullptr);
}

void test2_scene::point_shadow_rendering(point_light_constants& light_data)
{
	//	特に何もしていない
}

void test2_scene::spot_shadow_rendering(spot_light_constants& light_data)
{
	if (!light_data.use_shadow)
		return;

	static constexpr int SceneCBVIndex = 1;

	//	出力先をバックバッファに変更
	get_renderdata()->immediate_context->ClearDepthStencilView(shadowmap_depth_stencil_view.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
	get_renderdata()->immediate_context->OMSetRenderTargets(0, nullptr, shadowmap_depth_stencil_view.Get());

	// ビューポートの設定
	D3D11_VIEWPORT viewport{};
	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.Width = static_cast<float>(SHADOWMAP_WIDTH);
	viewport.Height = static_cast<float>(SHADOWMAP_HEIGHT);
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	get_renderdata()->immediate_context->RSSetViewports(1, &viewport);

	{
		// ライトの位置から見た視線行列を生成
		DirectX::XMVECTOR LightPosition = DirectX::XMLoadFloat4(&light_data.spot_light.position);
		DirectX::XMVECTOR LightFocus = DirectX::XMVectorScale(DirectX::XMLoadFloat4(&light_data.spot_light.direction), light_data.spot_light.range);
		LightFocus = DirectX::XMVectorAdd(LightPosition, LightFocus);
		DirectX::XMMATRIX V = DirectX::XMMatrixLookAtLH(LightPosition, LightFocus, DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));

		// シャドウマップに描画したい範囲の射影行列を生成
		DirectX::XMMATRIX P = DirectX::XMMatrixPerspectiveFovLH(light_data.spot_light.outerCorn * 2.0f, 1.0f, 1.0f, light_data.spot_light.range);
		// ライトビュー行列を保存
		DirectX::XMStoreFloat4x4(&light_data.light_view_projection, V * P);

		// 定数バッファの更新
		{
			// 0番はメッシュ側で更新している

			scene_constants scene{};
			scene.options.x = static_cast<float>(get_cursor_position().x);
			scene.options.y = static_cast<float>(get_cursor_position().y);
			scene.options.z = get_timer();
			scene.options.w = get_flag();
			scene.camera_position.x = get_camera_position().x;
			scene.camera_position.y = get_camera_position().y;
			scene.camera_position.z = get_camera_position().z;
			scene.camera_position.w = 1;
			DirectX::XMStoreFloat4x4(&scene.view_transform, V);
			DirectX::XMStoreFloat4x4(&scene.projection_transform, P);
			scene.view_projection_transform = light_data.light_view_projection;
			DirectX::XMStoreFloat4x4(&scene.inverse_view_transform, DirectX::XMMatrixInverse(nullptr, V));
			DirectX::XMStoreFloat4x4(&scene.inverse_projection_transform, DirectX::XMMatrixInverse(nullptr, P));
			DirectX::XMStoreFloat4x4(&scene.inverse_view_projection_transform, DirectX::XMMatrixInverse(nullptr, V * P));

			get_renderdata()->immediate_context->UpdateSubresource(shadowmap_scene_constant_buffer.Get(), 0, 0, &scene, 0, 0);
			get_renderdata()->set_constant_buffer(SceneCBVIndex, 1, shadowmap_scene_constant_buffer.GetAddressOf());
		}

		get_renderdata()->immediate_context->OMSetBlendState(get_renderdata()->blend_states[static_cast<size_t>(render_data::BLEND_STATE::ALPHA)].Get(), nullptr, 0xFFFFFFFF);
		get_renderdata()->immediate_context->OMSetDepthStencilState(get_renderdata()->depth_stencil_states[static_cast<size_t>(render_data::DEPTH_STATE::ZT_ON_ZW_ON)].Get(), 0);

		//	モデル描画
		for (auto& shadow_model : shadow_models)
		{
			auto& data = model_datas[shadow_model];
			auto& instance = model_instances[data.model_index];

			//	インスタンスのシャドウマスクが同じ値であれば影生成
			if(data.shadow_mask != light_data.shadow_mask)
				continue;

			//	ノード更新
			instance.pmx_model->update_transforms(data.node_datas);

			//	描画
			instance.pmx_model->render(get_renderdata()->immediate_context.Get(), data.world_matrix, data.previous_world_matrix, pmx_model_shader_names[SHADOW_CASTER]);
		}

		//	glTFステージ描画
		render_gltf_stages_shadow(light_data.shadow_mask);
	}

	//	不要だから外す
	get_renderdata()->immediate_context->OMSetRenderTargets(0, nullptr, nullptr);
}
