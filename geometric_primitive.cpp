#include "shader.h"
#include "misc.h"
#include "geometric_primitive.h"
#include <vector>

geometric_primitive::geometric_primitive(ID3D11Device* device)
{
	HRESULT hr{ S_OK };

	D3D11_BUFFER_DESC buffer_desc{};
	buffer_desc.ByteWidth = sizeof(constants);
	buffer_desc.Usage = D3D11_USAGE_DEFAULT;
	buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	hr = device->CreateBuffer(&buffer_desc, nullptr, constant_buffer.GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
}

void geometric_primitive::render(ID3D11DeviceContext* immediate_context, const DirectX::XMFLOAT4X4& world, const DirectX::XMFLOAT4& material_color)
{
	uint32_t stride{ sizeof(vertex) };
	uint32_t offset{ 0 };
	immediate_context->IASetVertexBuffers(0, 1, vertex_buffer.GetAddressOf(), &stride, &offset);
	immediate_context->IASetIndexBuffer(index_buffer.Get(), DXGI_FORMAT_R32_UINT, 0);

	constants data{ world, material_color };
	immediate_context->UpdateSubresource(constant_buffer.Get(), 0, 0, &data, 0, 0);
	immediate_context->VSSetConstantBuffers(0, 1, constant_buffer.GetAddressOf());
	immediate_context->HSSetConstantBuffers(0, 1, constant_buffer.GetAddressOf());
	immediate_context->DSSetConstantBuffers(0, 1, constant_buffer.GetAddressOf());
	immediate_context->GSSetConstantBuffers(0, 1, constant_buffer.GetAddressOf());
	immediate_context->PSSetConstantBuffers(0, 1, constant_buffer.GetAddressOf());

	D3D11_BUFFER_DESC buffer_desc{};
	index_buffer->GetDesc(&buffer_desc);
	immediate_context->DrawIndexed(buffer_desc.ByteWidth / sizeof(uint32_t), 0, 0);
}

void geometric_primitive::create_com_buffers(ID3D11Device* device, vertex* vertices, size_t vertex_count, uint32_t* indices, size_t index_count)
{
	HRESULT hr{ S_OK };

	D3D11_BUFFER_DESC buffer_desc{};
	D3D11_SUBRESOURCE_DATA subresource_data{};
	buffer_desc.ByteWidth = static_cast<UINT>(sizeof(vertex) * vertex_count);
	buffer_desc.Usage = D3D11_USAGE_DEFAULT;
	buffer_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	buffer_desc.CPUAccessFlags = 0;
	buffer_desc.MiscFlags = 0;
	buffer_desc.StructureByteStride = 0;
	subresource_data.pSysMem = vertices;
	subresource_data.SysMemPitch = 0;
	subresource_data.SysMemSlicePitch = 0;
	hr = device->CreateBuffer(&buffer_desc, &subresource_data, vertex_buffer.ReleaseAndGetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	buffer_desc.ByteWidth = static_cast<UINT>(sizeof(uint32_t) * index_count);
	buffer_desc.Usage = D3D11_USAGE_DEFAULT;
	buffer_desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	subresource_data.pSysMem = indices;
	hr = device->CreateBuffer(&buffer_desc, &subresource_data, index_buffer.ReleaseAndGetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
}

geometric_plane::geometric_plane(ID3D11Device* device, bool x_flip) : geometric_primitive(device)
{
	// create a mesh for a cube
	vertex vertices[4]{};
	uint32_t indices[6]{};

	if (x_flip)
	{
		vertices[1].position = { +0.5f, +0.0f, +0.5f };
		vertices[0].position = { -0.5f, +0.0f, +0.5f };
		vertices[3].position = { +0.5f, +0.0f, -0.5f };
		vertices[2].position = { -0.5f, +0.0f, -0.5f };
		indices[0] = 0;
		indices[1] = 2;
		indices[2] = 1;
		indices[3] = 1;
		indices[4] = 2;
		indices[5] = 3;
	}
	else
	{
		vertices[0].position = { +0.5f, +0.0f, +0.5f };
		vertices[1].position = { -0.5f, +0.0f, +0.5f };
		vertices[2].position = { +0.5f, +0.0f, -0.5f };
		vertices[3].position = { -0.5f, +0.0f, -0.5f };
		indices[0] = 0;
		indices[1] = 1;
		indices[2] = 2;
		indices[3] = 1;
		indices[4] = 3;
		indices[5] = 2;
	}
	vertices[0].normal = { +0.0f, +1.0f, +0.0f };
	vertices[1].normal = { +0.0f, +1.0f, +0.0f };
	vertices[2].normal = { +0.0f, +1.0f, +0.0f };
	vertices[3].normal = { +0.0f, +1.0f, +0.0f };
	vertices[0].texcoord = { +0.0f, +0.0f };
	vertices[1].texcoord = { +1.0f, +0.0f };
	vertices[2].texcoord = { +0.0f, +1.0f };
	vertices[3].texcoord = { +1.0f, +1.0f };
	create_com_buffers(device, vertices, 4, indices, 6);
}

geometric_cube::geometric_cube(ID3D11Device* device) : geometric_primitive(device)
{
	// create a mesh for a cube
	vertex vertices[24]{};
	uint32_t indices[36]{};

	uint32_t face{ 0 };

	// top-side
	face = 0;
	vertices[face * 4 + 0].position = { +0.5f, +0.5f, +0.5f };
	vertices[face * 4 + 1].position = { -0.5f, +0.5f, +0.5f };
	vertices[face * 4 + 2].position = { +0.5f, +0.5f, -0.5f };
	vertices[face * 4 + 3].position = { -0.5f, +0.5f, -0.5f };
	vertices[face * 4 + 0].normal = { +0.0f, +1.0f, +0.0f };
	vertices[face * 4 + 1].normal = { +0.0f, +1.0f, +0.0f };
	vertices[face * 4 + 2].normal = { +0.0f, +1.0f, +0.0f };
	vertices[face * 4 + 3].normal = { +0.0f, +1.0f, +0.0f };
	vertices[face * 4 + 0].texcoord = { +0.0f, +0.0f };
	vertices[face * 4 + 1].texcoord = { +1.0f, +0.0f };
	vertices[face * 4 + 2].texcoord = { +0.0f, +1.0f };
	vertices[face * 4 + 3].texcoord = { +1.0f, +1.0f };
	indices[face * 6 + 0] = face * 4 + 0;
	indices[face * 6 + 1] = face * 4 + 1;
	indices[face * 6 + 2] = face * 4 + 2;
	indices[face * 6 + 3] = face * 4 + 1;
	indices[face * 6 + 4] = face * 4 + 3;
	indices[face * 6 + 5] = face * 4 + 2;

	// bottom-side
	face += 1;
	vertices[face * 4 + 0].position = { -0.5f, -0.5f, +0.5f };
	vertices[face * 4 + 1].position = { +0.5f, -0.5f, +0.5f };
	vertices[face * 4 + 2].position = { -0.5f, -0.5f, -0.5f };
	vertices[face * 4 + 3].position = { +0.5f, -0.5f, -0.5f };
	vertices[face * 4 + 0].normal = { +0.0f, -1.0f, +0.0f };
	vertices[face * 4 + 1].normal = { +0.0f, -1.0f, +0.0f };
	vertices[face * 4 + 2].normal = { +0.0f, -1.0f, +0.0f };
	vertices[face * 4 + 3].normal = { +0.0f, -1.0f, +0.0f };
	vertices[face * 4 + 0].texcoord = { +0.0f, +0.0f };
	vertices[face * 4 + 1].texcoord = { +1.0f, +0.0f };
	vertices[face * 4 + 2].texcoord = { +0.0f, +1.0f };
	vertices[face * 4 + 3].texcoord = { +1.0f, +1.0f };
	indices[face * 6 + 0] = face * 4 + 0;
	indices[face * 6 + 1] = face * 4 + 1;
	indices[face * 6 + 2] = face * 4 + 2;
	indices[face * 6 + 3] = face * 4 + 1;
	indices[face * 6 + 4] = face * 4 + 3;
	indices[face * 6 + 5] = face * 4 + 2;

	// front-side
	face += 1;
	vertices[face * 4 + 0].position = { +0.5f, +0.5f, -0.5f };
	vertices[face * 4 + 1].position = { -0.5f, +0.5f, -0.5f };
	vertices[face * 4 + 2].position = { +0.5f, -0.5f, -0.5f };
	vertices[face * 4 + 3].position = { -0.5f, -0.5f, -0.5f };
	vertices[face * 4 + 0].normal = { +0.0f, +0.0f, -1.0f };
	vertices[face * 4 + 1].normal = { +0.0f, +0.0f, -1.0f };
	vertices[face * 4 + 2].normal = { +0.0f, +0.0f, -1.0f };
	vertices[face * 4 + 3].normal = { +0.0f, +0.0f, -1.0f };
	vertices[face * 4 + 0].texcoord = { +0.0f, +0.0f };
	vertices[face * 4 + 1].texcoord = { +1.0f, +0.0f };
	vertices[face * 4 + 2].texcoord = { +0.0f, +1.0f };
	vertices[face * 4 + 3].texcoord = { +1.0f, +1.0f };
	indices[face * 6 + 0] = face * 4 + 0;
	indices[face * 6 + 1] = face * 4 + 1;
	indices[face * 6 + 2] = face * 4 + 2;
	indices[face * 6 + 3] = face * 4 + 1;
	indices[face * 6 + 4] = face * 4 + 3;
	indices[face * 6 + 5] = face * 4 + 2;

	// back-side
	face += 1;
	vertices[face * 4 + 0].position = { -0.5f, +0.5f, +0.5f };
	vertices[face * 4 + 1].position = { +0.5f, +0.5f, +0.5f };
	vertices[face * 4 + 2].position = { -0.5f, -0.5f, +0.5f };
	vertices[face * 4 + 3].position = { +0.5f, -0.5f, +0.5f };
	vertices[face * 4 + 0].normal = { +0.0f, +0.0f, +1.0f };
	vertices[face * 4 + 1].normal = { +0.0f, +0.0f, +1.0f };
	vertices[face * 4 + 2].normal = { +0.0f, +0.0f, +1.0f };
	vertices[face * 4 + 3].normal = { +0.0f, +0.0f, +1.0f };
	vertices[face * 4 + 0].texcoord = { +0.0f, +0.0f };
	vertices[face * 4 + 1].texcoord = { +1.0f, +0.0f };
	vertices[face * 4 + 2].texcoord = { +0.0f, +1.0f };
	vertices[face * 4 + 3].texcoord = { +1.0f, +1.0f };
	indices[face * 6 + 0] = face * 4 + 0;
	indices[face * 6 + 1] = face * 4 + 1;
	indices[face * 6 + 2] = face * 4 + 2;
	indices[face * 6 + 3] = face * 4 + 1;
	indices[face * 6 + 4] = face * 4 + 3;
	indices[face * 6 + 5] = face * 4 + 2;

	// right-side
	face += 1;
	vertices[face * 4 + 1].position = { +0.5f, +0.5f, -0.5f };
	vertices[face * 4 + 0].position = { +0.5f, +0.5f, +0.5f };
	vertices[face * 4 + 3].position = { +0.5f, -0.5f, -0.5f };
	vertices[face * 4 + 2].position = { +0.5f, -0.5f, +0.5f };
	vertices[face * 4 + 0].normal = { +1.0f, +0.0f, +0.0f };
	vertices[face * 4 + 1].normal = { +1.0f, +0.0f, +0.0f };
	vertices[face * 4 + 2].normal = { +1.0f, +0.0f, +0.0f };
	vertices[face * 4 + 3].normal = { +1.0f, +0.0f, +0.0f };
	vertices[face * 4 + 0].texcoord = { +0.0f, +0.0f };
	vertices[face * 4 + 1].texcoord = { +1.0f, +0.0f };
	vertices[face * 4 + 2].texcoord = { +0.0f, +1.0f };
	vertices[face * 4 + 3].texcoord = { +1.0f, +1.0f };
	indices[face * 6 + 0] = face * 4 + 0;
	indices[face * 6 + 1] = face * 4 + 1;
	indices[face * 6 + 2] = face * 4 + 2;
	indices[face * 6 + 3] = face * 4 + 1;
	indices[face * 6 + 4] = face * 4 + 3;
	indices[face * 6 + 5] = face * 4 + 2;

	// left-side
	face += 1;
	vertices[face * 4 + 0].position = { -0.5f, +0.5f, -0.5f };
	vertices[face * 4 + 1].position = { -0.5f, +0.5f, +0.5f };
	vertices[face * 4 + 2].position = { -0.5f, -0.5f, -0.5f };
	vertices[face * 4 + 3].position = { -0.5f, -0.5f, +0.5f };
	vertices[face * 4 + 0].normal = { -1.0f, +0.0f, +0.0f };
	vertices[face * 4 + 1].normal = { -1.0f, +0.0f, +0.0f };
	vertices[face * 4 + 2].normal = { -1.0f, +0.0f, +0.0f };
	vertices[face * 4 + 3].normal = { -1.0f, +0.0f, +0.0f };
	vertices[face * 4 + 0].texcoord = { +0.0f, +0.0f };
	vertices[face * 4 + 1].texcoord = { +1.0f, +0.0f };
	vertices[face * 4 + 2].texcoord = { +0.0f, +1.0f };
	vertices[face * 4 + 3].texcoord = { +1.0f, +1.0f };
	indices[face * 6 + 0] = face * 4 + 0;
	indices[face * 6 + 1] = face * 4 + 1;
	indices[face * 6 + 2] = face * 4 + 2;
	indices[face * 6 + 3] = face * 4 + 1;
	indices[face * 6 + 4] = face * 4 + 3;
	indices[face * 6 + 5] = face * 4 + 2;

	create_com_buffers(device, vertices, 24, indices, 36);
}

geometric_sphere::geometric_sphere(ID3D11Device* device, uint32_t slices, uint32_t stacks) : geometric_primitive(device)
{
	std::vector<vertex> vertices;
	std::vector<uint32_t> indices;

	float r{ 0.5f };

	//
	// Compute the vertices stating at the top pole and moving down the stacks.
	//

	// Poles: note that there will be texture coordinate distortion as there is
	// not a unique point on the texture map to assign to the pole when mapping
	// a rectangular texture onto a sphere.
	vertex top_vertex{};
	top_vertex.position = { 0.0f, +r, 0.0f };
	top_vertex.normal = { 0.0f, +1.0f, 0.0f };
	top_vertex.texcoord = { 0.5f, +0.0f };

	vertex bottom_vertex{};
	bottom_vertex.position = { 0.0f, -r, 0.0f };
	bottom_vertex.normal = { 0.0f, -1.0f, 0.0f };
	bottom_vertex.texcoord = { 0.5f, +1.0f };

	vertices.emplace_back(top_vertex);

	float phi_step{ DirectX::XM_PI / stacks };
	float theta_step{ 2.0f * DirectX::XM_PI / slices };

	// Compute vertices for each stack ring (do not count the poles as rings).
	for (uint32_t i = 1; i <= stacks - 1; ++i)
	{
		float phi{ i * phi_step };

		// Vertices of ring.
		for (uint32_t j = 0; j <= slices; ++j)
		{
			float theta{ j * theta_step };

			vertex v{};

			// spherical to cartesian
			v.position.x = r * sinf(phi) * cosf(theta);
			v.position.y = r * cosf(phi);
			v.position.z = r * sinf(phi) * sinf(theta);

			v.texcoord.x = theta / DirectX::XM_2PI;
			v.texcoord.y = phi / DirectX::XM_PI;

			DirectX::XMVECTOR p{ XMLoadFloat3(&v.position) };
			DirectX::XMStoreFloat3(&v.normal, DirectX::XMVector3Normalize(p));

			vertices.emplace_back(v);
		}
	}

	vertices.emplace_back(bottom_vertex);

	//
	// Compute indices for top stack.  The top stack was written first to the vertex buffer
	// and connects the top pole to the first ring.
	//
	for (uint32_t i = 1; i <= slices; ++i)
	{
		indices.emplace_back(0);
		indices.emplace_back(i);
		indices.emplace_back(i + 1);
	}

	//
	// Compute indices for inner stacks (not connected to poles).
	//

	// Offset the indices to the index of the first vertex in the first ring.
	// This is just skipping the top pole vertex.
	uint32_t base_index{ 1 };
	uint32_t ring_vertex_count{ slices + 1 };
	for (uint32_t i = 0; i < stacks - 2; ++i)
	{
		for (uint32_t j = 0; j < slices; ++j)
		{
			indices.emplace_back(base_index + i * ring_vertex_count + j);
			indices.emplace_back(base_index + (i + 1) * ring_vertex_count + j);
			indices.emplace_back(base_index + i * ring_vertex_count + j + 1);

			indices.emplace_back(base_index + (i + 1) * ring_vertex_count + j);
			indices.emplace_back(base_index + (i + 1) * ring_vertex_count + j + 1);
			indices.emplace_back(base_index + i * ring_vertex_count + j + 1);
		}
	}

	//
	// Compute indices for bottom stack.  The bottom stack was written last to the vertex buffer
	// and connects the bottom pole to the bottom ring.
	//

	// South pole vertex was added last.
	uint32_t south_pole_index{ static_cast<uint32_t>(vertices.size() - 1) };

	// Offset the indices to the index of the first vertex in the last ring.
	base_index = south_pole_index - ring_vertex_count;

	for (uint32_t i = 0; i < slices; ++i)
	{
		indices.emplace_back(south_pole_index);
		indices.emplace_back(base_index + i + 1);
		indices.emplace_back(base_index + i);
	}
	create_com_buffers(device, vertices.data(), vertices.size(), indices.data(), indices.size());
}
