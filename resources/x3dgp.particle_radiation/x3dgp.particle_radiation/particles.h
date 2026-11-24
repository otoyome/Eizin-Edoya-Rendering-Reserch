#pragma once

// https://www.docswell.com/s/CAPCOM_RandD/Z7DPJK-2022-07-15-133910

#include <d3d11.h>
#include <wrl.h>
#include <directxmath.h>

#include <vector>
#include "skinned_mesh.h"


#define NUMTHREADS_X 16

struct particle
{
	DirectX::XMFLOAT4 color = { 1, 1, 1, 1 };
	DirectX::XMFLOAT3 position = { 0, 0, 0 };
	DirectX::XMFLOAT3 velocity = { 0, 0, 0 };
	float size = 0;
};
struct particle_system
{
	const int max_particle_count;

	struct particle_system_constants
	{
		DirectX::XMFLOAT4 emission_position = {};
		float time = 0.0f;
		float delta_time = 0.0f;
		int max_particle_count = 0;
		float pads[1];
	};
	particle_system_constants particle_system_data;

	Microsoft::WRL::ComPtr<ID3D11Buffer> particle_buffer;
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> particle_buffer_uav;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> particle_buffer_srv;

	Microsoft::WRL::ComPtr<ID3D11VertexShader> particle_vs;
	Microsoft::WRL::ComPtr<ID3D11PixelShader> particle_ps;
	Microsoft::WRL::ComPtr<ID3D11GeometryShader> particle_gs;
	Microsoft::WRL::ComPtr<ID3D11InputLayout> input_layout;
	Microsoft::WRL::ComPtr<ID3D11ComputeShader> particle_cs;
	Microsoft::WRL::ComPtr<ID3D11ComputeShader> particle_initializer_cs;
	Microsoft::WRL::ComPtr<ID3D11Buffer> constant_buffer;

	particle_system(ID3D11Device* pDevice, int particle_count);
	particle_system(const particle_system&) = delete;
	particle_system& operator=(const particle_system&) = delete;
	particle_system(particle_system&&) noexcept = delete;
	particle_system& operator=(particle_system&&) noexcept = delete;
	virtual ~particle_system() = default;

	void integrate(ID3D11DeviceContext* immediate_context, float delta_time);
	void initialize(ID3D11DeviceContext* immediate_context, float delta_time);
	void render(ID3D11DeviceContext* immediate_context);
};