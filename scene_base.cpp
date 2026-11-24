#include "scene_base.h"

scene_base::scene_base(CONST HWND hwnd, render_data* renderdata)
: hwnd(hwnd)
, renderdata(renderdata)
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
		{
			buffer_desc.ByteWidth = sizeof(scene_constants);
			hr = renderdata->device->CreateBuffer(&buffer_desc, nullptr, scene_constant_buffer.GetAddressOf());
			_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
		}
	}
}

scene_base::~scene_base()
{
	renderdata = nullptr;
}

bool scene_base::initialize()
{
	return initialize_core();
}

bool scene_base::uninitialize()
{
	return uninitialize_core();
}

void scene_base::update(float elapsed_time)
{
	timer += elapsed_time;

	previous_view_projection = view_projection;	//	古い行列を保存

	DirectX::XMVECTOR up{ DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f) };

	// 視線行列を生成
	DirectX::XMMATRIX V;
	if (camera_upadte_flag)
	{
		// マウス操作
#if USE_IMGUI
		if(!ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow))
#endif // USE_IMGUI
		{
			POINT cursor;
			RECT rc;
			::GetCursorPos(&cursor);
			::ScreenToClient(hwnd, &cursor);
			GetClientRect(hwnd, &rc);
			UINT screenW = rc.right - rc.left;
			UINT screenH = rc.bottom - rc.top;
			POINT old_cursor_position;
			old_cursor_position.x = cursor_position.x;
			old_cursor_position.y = cursor_position.y;
			cursor_position.x = (LONG)(cursor.x / static_cast<float>(SCREEN_WIDTH) * static_cast<float>(screenW));
			cursor_position.y = (LONG)(cursor.y / static_cast<float>(SCREEN_HEIGHT) * static_cast<float>(screenH));

			float moveX = (cursor_position.x - old_cursor_position.x) * 0.02f;
			float moveY = (cursor_position.y - old_cursor_position.y) * 0.02f;
			if (::GetAsyncKeyState(VK_RBUTTON) & 0x8000)
			{
				// Y軸回転
				rotateY += moveX * 0.5f;
				if (rotateY > DirectX::XM_PI)
					rotateY -= DirectX::XM_2PI;
				else if (rotateY < -DirectX::XM_PI)
					rotateY += DirectX::XM_2PI;
				// X軸回転
				rotateX += moveY * 0.5f;
				if (rotateX > DirectX::XMConvertToRadians(89.9f))
					rotateX = DirectX::XMConvertToRadians(89.9f);
				else if (rotateX < -DirectX::XMConvertToRadians(89.9f))
					rotateX = -DirectX::XMConvertToRadians(89.9f);
			}
			else if (::GetAsyncKeyState(VK_MBUTTON) & 0x8000)
			{
				V = DirectX::XMMatrixLookAtLH(DirectX::XMLoadFloat3(&camera_position),
					DirectX::XMLoadFloat3(&camera_focus),
					up);
				DirectX::XMFLOAT4X4 W;
				DirectX::XMStoreFloat4x4(&W, DirectX::XMMatrixInverse(nullptr, V));
				// 平行移動
				float s = distance * 0.035f;
				float x = moveX * s;
				float y = moveY * s;
				camera_focus.x -= W._11 * x;
				camera_focus.y -= W._12 * x;
				camera_focus.z -= W._13 * x;

				camera_focus.x += W._21 * y;
				camera_focus.y += W._22 * y;
				camera_focus.z += W._23 * y;
			}
			if (wheel != 0)	// ズーム
			{
				distance -= static_cast<float>(wheel) * distance * 0.001f;
				wheel = 0;
			}
		}
	}

	update_view_projection();

	update_core(elapsed_time);
}

void scene_base::update_view_projection()
{
	DirectX::XMVECTOR up{ DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f) };

	float sx = ::sinf(rotateX), cx = ::cosf(rotateX);
	float sy = ::sinf(rotateY), cy = ::cosf(rotateY);
	DirectX::XMVECTOR Focus = DirectX::XMLoadFloat3(&camera_focus);
	DirectX::XMVECTOR Front = DirectX::XMVectorSet(-cx * sy, -sx, -cx * cy, 0.0f);
	DirectX::XMVECTOR Distance = DirectX::XMVectorSet(distance, distance, distance, 0.0f);
	Front = DirectX::XMVectorMultiply(Front, Distance);
	DirectX::XMVECTOR Eye = DirectX::XMVectorSubtract(Focus, Front);
	DirectX::XMStoreFloat3(&camera_position, Eye);

	DirectX::XMMATRIX V;
	V = DirectX::XMMatrixLookAtLH(DirectX::XMLoadFloat3(&camera_position),
		DirectX::XMLoadFloat3(&camera_focus),
		up);

	// 射影行列を生成
	DirectX::XMMATRIX P;
	D3D11_VIEWPORT viewport;
	UINT num_viewports{ 1 };
	renderdata->immediate_context->RSGetViewports(&num_viewports, &viewport);
	float aspect_ratio{ static_cast<float>(SCREEN_WIDTH) / static_cast<float>(SCREEN_HEIGHT) };
	P = DirectX::XMMatrixPerspectiveFovLH(DirectX::XMConvertToRadians(30),
		aspect_ratio,
		NEAR_CLIP_DISTANCE,
		FAR_CLIP_DISTANCE);

	DirectX::XMStoreFloat4x4(&view, V);
	DirectX::XMStoreFloat4x4(&projection, P);
	DirectX::XMStoreFloat4x4(&view_projection, V * P);
}

void scene_base::render(float elapsed_time)
{
	//	バックバッファクリア
	FLOAT color[]{ 0.2f, 0.2f, 0.2f, 1.0f };
	renderdata->immediate_context->ClearRenderTargetView(renderdata->render_target_view.Get(), color);
	renderdata->immediate_context->ClearDepthStencilView(renderdata->depth_stencil_view.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
	renderdata->immediate_context->OMSetRenderTargets(1, renderdata->render_target_view.GetAddressOf(), renderdata->depth_stencil_view.Get());

	// ビューポートの設定
	D3D11_VIEWPORT viewport{};
	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.Width = static_cast<float>(SCREEN_WIDTH);
	viewport.Height = static_cast<float>(SCREEN_HEIGHT);
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	renderdata->immediate_context->RSSetViewports(1, &viewport);

	render_core(elapsed_time);
}

#ifdef USE_IMGUI
void scene_base::imgui_render()
{
	imgui_render_core();
}
#endif

void scene_base::set_scene_constant(int start_slot, bool is_update_resource)
{
	if (is_update_resource)
	{
		scene_constants scene{};
		scene.options.x = static_cast<float>(cursor_position.x);
		scene.options.y = static_cast<float>(cursor_position.y);
		scene.options.z = timer;
		scene.options.w = flag;
		scene.camera_position.x = camera_position.x;
		scene.camera_position.y = camera_position.y;
		scene.camera_position.z = camera_position.z;
		scene.camera_position.w = 1;
		scene.camera_direction.x = camera_focus.x - camera_position.x;
		scene.camera_direction.y = camera_focus.y - camera_position.y;
		scene.camera_direction.z = camera_focus.z - camera_position.z;
		float d = sqrtf(scene.camera_direction.x * scene.camera_direction.x + scene.camera_direction.y * scene.camera_direction.y + scene.camera_direction.z * scene.camera_direction.z);
		scene.camera_direction.x /= d;
		scene.camera_direction.y /= d;
		scene.camera_direction.z /= d;
		scene.camera_direction.w = 1;
		DirectX::XMFLOAT2	clip_length = { NEAR_CLIP_DISTANCE, FAR_CLIP_DISTANCE };
		scene.z_buffer_parameteres.y = clip_length.y / clip_length.x;
		scene.z_buffer_parameteres.x = 1.0f - scene.z_buffer_parameteres.y;
		scene.z_buffer_parameteres.z = scene.z_buffer_parameteres.x / clip_length.y;
		scene.z_buffer_parameteres.w = scene.z_buffer_parameteres.y / clip_length.y;
		scene.camera_clip_distance.x = clip_length.x;
		scene.camera_clip_distance.y = clip_length.y;
		scene.camera_clip_distance.z = clip_length.x * clip_length.y;
		scene.camera_clip_distance.w = clip_length.y - clip_length.x;
		scene.viewport_rect.x = 0;
		scene.viewport_rect.y = 0;
		scene.viewport_rect.z = SCREEN_WIDTH;
		scene.viewport_rect.w = SCREEN_HEIGHT;
		scene.view_transform = view;
		scene.projection_transform = projection;
		scene.view_projection_transform = view_projection;
		DirectX::XMStoreFloat4x4(&scene.inverse_view_transform, DirectX::XMMatrixInverse(nullptr, DirectX::XMLoadFloat4x4(&view)));
		DirectX::XMStoreFloat4x4(&scene.inverse_projection_transform, DirectX::XMMatrixInverse(nullptr, DirectX::XMLoadFloat4x4(&projection)));
		DirectX::XMStoreFloat4x4(&scene.inverse_view_projection_transform, DirectX::XMMatrixInverse(nullptr, DirectX::XMLoadFloat4x4(&view_projection)));

		scene.previous_view_projection_transform = previous_view_projection;

		renderdata->immediate_context->UpdateSubresource(scene_constant_buffer.Get(), 0, 0, &scene, 0, 0);
	}
	renderdata->set_constant_buffer(start_slot, 1, scene_constant_buffer.GetAddressOf());
}

DirectX::XMFLOAT3 scene_base::ConvertQuaternionToEuler(DirectX::XMFLOAT4X4 rotation)
{
	//	ZXY回転
	DirectX::XMFLOAT3 euler;
	if (1.0f - fabs(rotation.m[2][1]) < 1.0e-6f)
	{
		euler.x = rotation.m[2][1] < 0 ? DirectX::XM_PIDIV2 : -DirectX::XM_PIDIV2;
		euler.y = atan2f(rotation.m[1][0], rotation.m[0][0]);
		euler.z = 0;
	}
	else
	{
		euler.x = asinf(-rotation.m[2][1]);
		euler.y = atan2f(rotation.m[2][0], rotation.m[2][2]);
		euler.z = atan2f(rotation.m[0][1], rotation.m[1][1]);
	}
	return euler;
}

DirectX::XMFLOAT3 scene_base::ConvertQuaternionToEuler(DirectX::XMFLOAT4 quaternion)
{
	DirectX::XMMATRIX Rotation = DirectX::XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&quaternion));
	DirectX::XMFLOAT4X4 rotation;
	DirectX::XMStoreFloat4x4(&rotation, Rotation);
	return ConvertQuaternionToEuler(rotation);
}

float scene_base::LengthPointToOBB(const OBB& obb, DirectX::XMFLOAT3 point)
{
	DirectX::XMVECTOR Vector = DirectX::XMVectorSet(0, 0, 0, 0);   // 最終的に長さを求めるベクトル

	DirectX::XMVECTOR Point = DirectX::XMLoadFloat3(&point);
	DirectX::XMVECTOR PositionObb = DirectX::XMLoadFloat4(&obb.position);

	//	各軸についてはみ出た部分のベクトルを算出
	const float* length = &obb.length.x;
	for(int axis = 0; axis < 3; axis++)
	{
		if (length[axis] <= 0)
			continue;	//	L = 0は計算できない

		DirectX::XMVECTOR PointToPositionObb = DirectX::XMVectorSubtract(Point, PositionObb);
		DirectX::XMVECTOR ObbAxis = DirectX::XMLoadFloat4(&obb.axis[axis]);
		float s = DirectX::XMVectorGetX(DirectX::XMVector3Dot(PointToPositionObb, ObbAxis)) / length[axis];

		//	sの値から、はみ出した部分があればそのベクトルを加算
		s = fabs(s);
		if (s > 1)
		{
			s = (1 - s) * length[axis];
			//	はみ出した部分のベクトル算出
			Vector = DirectX::XMVectorAdd(Vector,
									DirectX::XMVectorMultiply(ObbAxis, DirectX::XMVectorSet(s, s, s, s)));
		}
	}

	//	長さを出力
	return	DirectX::XMVectorGetX(DirectX::XMVector3Length(Vector));
}
