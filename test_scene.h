#pragma once
#include "test2_scene.h"

class test_scene : public test2_scene
{
public:
	test_scene(CONST HWND hwnd, render_data* renderdata);
	~test_scene() override;

protected:
	void initialize_character_models() override;
	void initialize_stage_models() override;
	const char* get_material_settings_path() const override;
};
