#include "framework.h"

#include "shader.h"
#include "texture.h"

#include "test2_scene.h"
#include "test_scene.h"

framework::framework(HWND hwnd) : hwnd(hwnd)
{
}

framework::~framework()
{
}

bool framework::initialize()
{
	HRESULT hr{ S_OK };

	//	描画情報の生成
	renderdata = std::make_unique<render_data>(hwnd);

	//	初回シーン登録
	//	再配布禁止モデルを含まないシーンを初回シーンとする
	scene = std::make_unique<test_scene>(hwnd, renderdata.get());
	if (scene)
		return scene->initialize();
	return true;
}

void framework::scene_button_gui()
{
#ifdef USE_IMGUI
	if (ImGui::Begin("scene select"))
	{
		//	test2_scene は再配布禁止モデルを使っているためシーン選択から除外する
		change_scene_button_gui<test_scene>(u8"test scene");
	}
	ImGui::End();
#endif
}

template<class T>
void framework::change_scene_button_gui(const char* name)
{
	if (ImGui::Button(name))
	{
		scene->uninitialize();
		scene = std::make_unique<T>(hwnd, renderdata.get());
		scene->initialize();
		scene->set_wheel(wheel);
		scene->update(elapsed_time);
	}
}

void framework::update(float elapsed_time/*Elapsed seconds from last frame*/)
{
	if (scene)
	{
		scene->set_wheel(wheel);
		wheel = 0;
		scene->update(elapsed_time);
	}

#ifdef USE_IMGUI
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	if (scene)
		scene->imgui_render();
#endif
}

void framework::render(float elapsed_time/*Elapsed seconds from last frame*/)
{
	HRESULT hr{ S_OK };

	if (scene)
		scene->render(elapsed_time);

	//	バックバッファに描画
	renderdata->immediate_context->OMSetRenderTargets(1, renderdata->render_target_view.GetAddressOf(), renderdata->depth_stencil_view.Get());

#ifdef USE_IMGUI
	scene_button_gui();

	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

	if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault();
	}
#endif

	UINT sync_interval{ 0 };
	renderdata->swap_chain->Present(sync_interval, 0);
}

bool framework::uninitialize()
{
	if (scene)
	{
		bool r = scene->uninitialize();
		return r;
	}
	return true;
}
