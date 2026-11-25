#pragma once

#include "render_data.h"
#include "light_data.h"

#include "sprite.h"
#include "geometric_primitive.h"
#include "static_mesh.h"
#include "gltf_model.h"
#include "pmx_model.h"

#include "shader.h"
#include "texture.h"

class scene_base
{
protected:
	static constexpr DirectX::XMFLOAT4X4 coordinate_system_transforms[]
	{
		{ -1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 },	// 0:RHS Y-UP
		{ 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 },		// 1:LHS Y-UP
		{ -1, 0, 0, 0, 0, 0, -1, 0, 0, 1, 0, 0, 0, 0, 0, 1 },	// 2:RHS Z-UP
		{ 1, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 1 },		// 3:LHS Z-UP
	};
	static constexpr float gltf_scale_factor = 1.0f;		//	単位をメートルにする場合は0.01に指定する事

	struct scene_constants
	{
		DirectX::XMFLOAT4 options;	//	xy : マウスの座標値, z : タイマー, w : フラグ
		DirectX::XMFLOAT4 z_buffer_parameteres;
		DirectX::XMFLOAT4 camera_position;
		DirectX::XMFLOAT4 camera_direction;
		DirectX::XMFLOAT4 camera_clip_distance;
		DirectX::XMFLOAT4 viewport_rect;
		DirectX::XMFLOAT4X4 view_transform;
		DirectX::XMFLOAT4X4 projection_transform;
		DirectX::XMFLOAT4X4 view_projection_transform;
		DirectX::XMFLOAT4X4 inverse_view_transform;
		DirectX::XMFLOAT4X4 inverse_projection_transform;
		DirectX::XMFLOAT4X4 inverse_view_projection_transform;
		DirectX::XMFLOAT4X4 previous_view_projection_transform;
	};

public:
	scene_base() = delete;
	scene_base(const scene_base&) = delete;
	scene_base(scene_base&&) = delete;
	scene_base(CONST HWND hwnd, render_data* renderdata);
	virtual ~scene_base();

	bool initialize();
	bool uninitialize();
	void update(float elapsed_time);
	void render(float elapsed_time);
#ifdef USE_IMGUI
	void imgui_render();
#endif

	void set_wheel(float wheel) { this->wheel = wheel; }
	void set_flag(bool flag) { this->flag = flag; }
	float get_timer() const { return timer; }
	float get_wheel() { return wheel; }
	bool get_flag() const { return flag; }
	DirectX::XMMATRIX get_gltf_coordinate_transform_matrix() const
	{
		static int  coordinate_index = -1;
		static DirectX::XMMATRIX m;
		if (coordinate_index == -1)
		{
			coordinate_index = 0;
			m = DirectX::XMLoadFloat4x4(&coordinate_system_transforms[coordinate_index]) * DirectX::XMMatrixScaling(gltf_scale_factor, gltf_scale_factor, gltf_scale_factor);
		}
		return m;
	}
	DirectX::XMFLOAT4X4 get_gltf_coordinate_transform_float4x4() const
	{
		DirectX::XMFLOAT4X4 c;
		DirectX::XMStoreFloat4x4(&c, get_gltf_coordinate_transform_matrix());
		return c;
	}
	void set_camera_upadte_flag(bool b) { camera_upadte_flag = b; }
	void set_camera_rotate_x(float a) { rotateX = a; }
	void set_camera_rotate_y(float a) { rotateY = a; }
	void set_camera_distance(float a) { distance = a; }
	void set_camera_focus(const DirectX::XMFLOAT3& p) { camera_focus = p; }

	CONST HWND get_hwnd() const { return hwnd; }
	render_data* get_renderdata() const { return renderdata; }

	bool get_camera_upadte_flag() const { return camera_upadte_flag; }
	float get_camera_rotate_x() const { return rotateX; }
	float get_camera_rotate_y() const { return rotateY; }
	float get_camera_distance() const { return distance; }
	const POINT& get_cursor_position() const { return cursor_position; }
	const DirectX::XMFLOAT2 get_cursor_position_float2() const { return { static_cast<float>(cursor_position.x), static_cast<float>(cursor_position.y) }; }
	const DirectX::XMFLOAT3& get_camera_position() const { return camera_position; }
	const DirectX::XMFLOAT3& get_camera_focus() const { return camera_focus; }
	const DirectX::XMFLOAT4X4& get_view() const { return view; }
	const DirectX::XMFLOAT4X4& get_projection() const { return projection; }
	const DirectX::XMFLOAT4X4& get_view_projection() const { return view_projection; }

	void update_view_projection();

protected:
	virtual bool initialize_core() { return true; }
	virtual bool uninitialize_core() { return true; }
	virtual void update_core(float elapsed_time) {}
	virtual void render_core(float elapsed_time) {}
#ifdef USE_IMGUI
	virtual void imgui_render_core() {}
#endif

	//	共通情報の設定
	void set_scene_constant(int start_slot = 1, bool is_update_resource = true);

	//	クォータニオンからオイラー角に変換
	DirectX::XMFLOAT3 ConvertQuaternionToEuler(DirectX::XMFLOAT4X4 rotation);
	DirectX::XMFLOAT3 ConvertQuaternionToEuler(DirectX::XMFLOAT4 quaternion);

	//	点とOBBの最短距離
	//	内部に入っていたら0を返す
	struct OBB
	{
		DirectX::XMFLOAT4 length;
		DirectX::XMFLOAT4 axis[3];
		DirectX::XMFLOAT4 position;
	};
	float LengthPointToOBB(const OBB& obb, DirectX::XMFLOAT3 point);

private:
	CONST HWND hwnd;
	render_data* renderdata{ nullptr };
	POINT cursor_position{ 0, 0 };
	float timer{ 0.0f };
	bool flag{ false };

	DirectX::XMFLOAT3 camera_position{ 0.0f, 0.0f, -10.0f };
	DirectX::XMFLOAT3 camera_focus{ 0.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT4X4 view;
	DirectX::XMFLOAT4X4 projection;
	DirectX::XMFLOAT4X4 view_projection;
	DirectX::XMFLOAT4X4 previous_view_projection;
	float rotateX{ 0.0f };
	float rotateY{ 0.0f };
	float wheel{ 0 };
	float distance{ 20.0f };
	bool camera_upadte_flag = true;

	Microsoft::WRL::ComPtr<ID3D11Buffer> scene_constant_buffer;
};
