#pragma once

#include <d3d11.h>

HRESULT create_vs_from_cso(ID3D11Device* device, const char* cso_name, ID3D11VertexShader** vertex_shader, ID3D11InputLayout** input_layout, D3D11_INPUT_ELEMENT_DESC* input_element_desc, UINT num_elements);
HRESULT create_hs_from_cso(ID3D11Device* device, const char* cso_name, ID3D11HullShader** hull_shader);
HRESULT create_ds_from_cso(ID3D11Device* device, const char* cso_name, ID3D11DomainShader** domain_shader);
HRESULT create_gs_from_cso(ID3D11Device* device, const char* cso_name, ID3D11GeometryShader** geometry_shader);
HRESULT create_gs_with_streamout_from_cso(ID3D11Device* device, const char* cso_name, ID3D11GeometryShader** geometry_shader, D3D11_SO_DECLARATION_ENTRY* streamout_declaration_desc, UINT num_entry, UINT* buffer_strides, UINT num_strides, UINT rasterized_stream);
HRESULT create_ps_from_cso(ID3D11Device* device, const char* cso_name, ID3D11PixelShader** pixel_shader);
HRESULT create_cs_from_cso(ID3D11Device* device, const char* cso_name, ID3D11ComputeShader** compute_shader);

