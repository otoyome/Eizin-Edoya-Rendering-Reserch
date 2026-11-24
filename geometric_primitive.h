#pragma once

#include <d3d11.h>
#include <wrl.h>

#include <directxmath.h>

class geometric_primitive
{
public:
	struct vertex
	{
		DirectX::XMFLOAT3 position;
		DirectX::XMFLOAT3 normal;
		DirectX::XMFLOAT2 texcoord;
	};
	struct constants
	{
		DirectX::XMFLOAT4X4 world;
		DirectX::XMFLOAT4 material_color;
	};

private:
	Microsoft::WRL::ComPtr<ID3D11Buffer> vertex_buffer;
	Microsoft::WRL::ComPtr<ID3D11Buffer> index_buffer;

	Microsoft::WRL::ComPtr<ID3D11VertexShader> vertex_shader;
	Microsoft::WRL::ComPtr<ID3D11PixelShader> pixel_shader;
	Microsoft::WRL::ComPtr<ID3D11InputLayout> input_layout;
	Microsoft::WRL::ComPtr<ID3D11Buffer> constant_buffer;

public:
	virtual ~geometric_primitive() = default;
	void render(ID3D11DeviceContext* immediate_context, const DirectX::XMFLOAT4X4& world, const DirectX::XMFLOAT4& material_color);

protected:
	geometric_primitive(ID3D11Device* device);
	void create_com_buffers(ID3D11Device* device, vertex* vertices, size_t vertex_count, uint32_t* indices, size_t index_count);
};

class geometric_plane : public geometric_primitive
{
public:
	geometric_plane(ID3D11Device* device, bool x_flip = false);
};

class geometric_cube : public geometric_primitive
{
public:
	geometric_cube(ID3D11Device* device);
};

class geometric_sphere : public geometric_primitive
{
public:
	geometric_sphere(ID3D11Device* device, uint32_t slices, uint32_t stacks);
};

