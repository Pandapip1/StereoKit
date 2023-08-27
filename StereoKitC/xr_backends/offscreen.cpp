// SPDX-License-Identifier: MIT
// The authors below grant copyright rights under the MIT license:
// Copyright (c) 2019-2023 Nick Klingensmith
// Copyright (c) 2023 Qualcomm Technologies, Inc.

#include "xr.h"

#include "../_stereokit.h"
#include "../sk_memory.h"
#include "../device.h"
#include "../platforms/platform.h"
#include "../systems/input.h"
#include "../systems/render.h"
#include "../libraries/stref.h"

///////////////////////////////////////////

using namespace sk;

struct offscreen_backend_state_t {
};
static offscreen_backend_state_t* local = {};

///////////////////////////////////////////

namespace sk {

///////////////////////////////////////////

bool offscreen_init() {
	const sk_settings_t* settings = sk_get_settings_ref();

	device_data.has_hand_tracking = false;
	device_data.has_eye_gaze      = false;
	device_data.tracking          = device_tracking_none;
	device_data.display_blend     = display_blend_opaque;
	device_data.display_type      = display_type_flatscreen;
	device_data.name              = string_copy("Window");

	local = sk_malloc_zero_t(offscreen_backend_state_t, 1);

	return true;
}

///////////////////////////////////////////

void offscreen_step_begin() {
	input_step();
}

///////////////////////////////////////////

void offscreen_step_end() {
	input_update_poses(true);

	skg_event_begin("Setup");
	{
		skg_draw_begin();
	}
	skg_event_end();
	skg_event_begin("Draw");
	{
		render_check_viewpoints();
		render_check_screenshots();
		render_clear();
	}
	skg_event_end();
}

///////////////////////////////////////////

void offscreen_shutdown() {
	*local = {};
	sk_free(local);
}

}