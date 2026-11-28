#pragma once

#include <windows.h>
#include <tchar.h>
#include <sstream>

#include "misc.h"
#include "high_resolution_timer.h"

#ifdef USE_IMGUI
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "imgui/imgui_impl_dx11.h"
#include "imgui/imgui_impl_win32.h"
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
extern ImWchar glyphRangesJapanese[];


#include "implot/implot.h"

#endif

#include "scene_base.h"

class framework
{
public:
	CONST HWND hwnd;

	std::unique_ptr<render_data> renderdata;
	std::unique_ptr<scene_base> scene;

	float wheel{ 0 };

	framework(HWND hwnd);
	~framework();

	framework(const framework&) = delete;
	framework& operator=(const framework&) = delete;
	framework(framework&&) noexcept = delete;
	framework& operator=(framework&&) noexcept = delete;

	int run()
	{
		MSG msg{};

		if (!initialize())
		{
			return 0;
		}

#ifdef USE_IMGUI
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImPlot::CreateContext();
		ImGui::GetIO().Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\consola.ttf", 14.0f, nullptr, ImGui::GetIO().Fonts->GetGlyphRangesDefault());
		{
			ImFontConfig japanese_font_config;
			japanese_font_config.MergeMode = true;
			ImGui::GetIO().Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\meiryo.ttc", 14.0f, &japanese_font_config, glyphRangesJapanese);
		}
		ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;
		ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
		ImGui_ImplWin32_Init(hwnd);
		ImGui_ImplDX11_Init(renderdata->device.Get(), renderdata->immediate_context.Get());
		ImGui::StyleColorsDark();

		ImGuiStyle& style = ImGui::GetStyle();
		if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			style.WindowRounding = 0.0f;
			style.Colors[ImGuiCol_WindowBg].w = 1.0f;
		}
#endif

		while (WM_QUIT != msg.message)
		{
			if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
			else
			{
				tictoc.tick();
				calculate_frame_stats();
				update(tictoc.time_interval());
				render(tictoc.time_interval());
			}
		}

#ifdef USE_IMGUI
		ImGui_ImplDX11_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImPlot::DestroyContext();
		ImGui::DestroyContext();
#endif

		BOOL fullscreen{};
		renderdata->swap_chain->GetFullscreenState(&fullscreen, 0);
		if (fullscreen)
		{
			renderdata->swap_chain->SetFullscreenState(FALSE, 0);
		}

		return uninitialize() ? static_cast<int>(msg.wParam) : 0;
	}

	LRESULT CALLBACK handle_message(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
	{
#ifdef USE_IMGUI
		if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam)) { return true; }
#endif
		switch (msg)
		{
		case WM_PAINT:
		{
			PAINTSTRUCT ps;
			BeginPaint(hwnd, &ps);
			
			EndPaint(hwnd, &ps);
			break;
		}
		case WM_DESTROY:
			PostQuitMessage(0);
			break;
		case WM_CREATE:
			break;
		case WM_KEYDOWN:
			if (wparam == VK_ESCAPE)
			{
				PostMessage(hwnd, WM_CLOSE, 0, 0);
			}
			break;
		case WM_ENTERSIZEMOVE:
			tictoc.stop();
			break;
		case WM_EXITSIZEMOVE:
			tictoc.start();
			break;
		case WM_MOUSEWHEEL:
			wheel = GET_WHEEL_DELTA_WPARAM(wparam);
			break;
		default:
			return DefWindowProc(hwnd, msg, wparam, lparam);
		}
		return 0;
	}

private:
	bool initialize();
	void update(float elapsed_time/*Elapsed seconds from last frame*/);
	void render(float elapsed_time/*Elapsed seconds from last frame*/);
	bool uninitialize();

	template<class T>
	void change_scene_button_gui(const char* name);
	void scene_button_gui();

private:
	high_resolution_timer tictoc;
	uint32_t frames{ 0 };
	float elapsed_time{ 0.0f };
	void calculate_frame_stats()
	{
		if (++frames, (tictoc.time_stamp() - elapsed_time) >= 1.0f)
		{
			float fps = static_cast<float>(frames);
			std::wostringstream outs;
			outs.precision(6);
			outs << APPLICATION_NAME << L" : FPS : " << fps << L" / " << L"Frame Time : " << 1000.0f / fps << L" (ms)";
			SetWindowTextW(hwnd, outs.str().c_str());

			frames = 0;
			elapsed_time += 1.0f;
		}
	}
};

