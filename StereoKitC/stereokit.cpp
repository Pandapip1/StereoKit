// SPDX-License-Identifier: MIT
// The authors below grant copyright rights under the MIT license:
// Copyright (c) 2019-2023 Nick Klingensmith
// Copyright (c) 2023 Qualcomm Technologies, Inc.

#include "stereokit.h"
#include "_stereokit.h"
#include "_stereokit_ui.h"
#include "log.h"

#include "libraries/sokol_time.h"
#include "libraries/tinycthread.h"

#include "systems/render.h"
#include "systems/input.h"
#include "systems/physics.h"
#include "systems/system.h"
#include "systems/text.h"
#include "systems/audio.h"
#include "systems/sprite_drawer.h"
#include "systems/line_drawer.h"
#include "systems/world.h"
#include "systems/defaults.h"
#include "asset_types/animation.h"
#include "platforms/win32.h"
#include "platforms/uwp.h"
#include "platforms/android.h"
#include "platforms/web.h"
#include "platforms/platform.h"
#include "platforms/platform_utils.h"
#include "xr_backends/openxr.h"

///////////////////////////////////////////

using namespace sk;

struct sk_state_t {
	void        (*app_step_func)(void);
	sk_settings_t settings;
	system_info_t info;
	app_focus_    focus;
	bool32_t      running;
	bool32_t      stepping;
	bool32_t      initialized;
	bool32_t      first_step ;
	thrd_id_t     init_thread;

	double   timev_scale;
	float    timevf;
	double   timev;
	float    timevf_us;
	double   timev_us;
	double   time_start;
	double   timev_step;
	float    timev_stepf;
	double   timev_step_us;
	float    timev_stepf_us;
	uint64_t timev_raw;

	uint64_t  app_init_time;
	system_t *app_system;
};
static sk_state_t local;

///////////////////////////////////////////

namespace sk {

///////////////////////////////////////////

void sk_step_timer();
void sk_app_step();

///////////////////////////////////////////

bool32_t sk_init(sk_settings_t settings) {
	local = {};
	local.timev_scale = 1;

	local.settings    = settings;
	local.init_thread = thrd_id_current();
	if (local.settings.log_filter != log_none)
		log_set_filter(local.settings.log_filter);

	// Set some default values
	if (local.settings.app_name           == nullptr) local.settings.app_name           = "StereoKit App";
	if (local.settings.flatscreen_width   == 0      ) local.settings.flatscreen_width   = 1280;
	if (local.settings.flatscreen_height  == 0      ) local.settings.flatscreen_height  = 720;
	if (local.settings.render_scaling     == 0      ) local.settings.render_scaling     = 1;
	if (local.settings.render_multisample == 0      ) local.settings.render_multisample = 1;

	// display_preference is obsolete, so we'll fill in `mode` based on it, if
	// mode hasn't been specified.
	if (local.settings.mode == app_mode_none) {
		if      (local.settings.display_preference == display_mode_mixedreality) local.settings.mode = app_mode_xr;
		else if (local.settings.display_preference == display_mode_none        ) local.settings.mode = app_mode_window;
		else if (local.settings.display_preference == display_mode_flatscreen  )
			local.settings.mode = local.settings.disable_flatscreen_mr_sim
				? app_mode_window
				: app_mode_simulator;
	}

#if defined(SK_OS_ANDROID)
	// don't allow flatscreen fallback on Android
	local.settings.no_flatscreen_fallback = true;
#endif

	render_set_scaling    (local.settings.render_scaling);
	render_set_multisample(local.settings.render_multisample);

	log_diagf("Initializing StereoKit v%s...", sk_version_name());

	stm_setup();
	sk_step_timer();

	// Platform related systems
	system_t sys_platform         = { "Platform"    };
	system_t sys_platform_begin   = { "FrameBegin"  };
	system_t sys_platform_render  = { "FrameRender" };

	system_set_step_deps(sys_platform_render, "App", "Text", "Sprites", "Lines", "World", "UILate", "Animation");

	sys_platform        .func_initialize       = platform_init;
	sys_platform        .func_shutdown         = platform_shutdown;
	sys_platform_begin  .func_step             = platform_step_begin;
	sys_platform_render .func_step             = platform_step_end;

	systems_add(&sys_platform);
	systems_add(&sys_platform_begin);
	systems_add(&sys_platform_render);

	// Rest of the systems
	system_t sys_defaults = { "Defaults" };
	system_set_initialize_deps(sys_defaults, "Platform", "Assets");
	sys_defaults.func_initialize = defaults_init;
	sys_defaults.func_shutdown   = defaults_shutdown;
	systems_add(&sys_defaults);

	system_t sys_ui = { "UI" };
	system_set_initialize_deps(sys_ui, "Defaults");
	system_set_step_deps      (sys_ui, "Input", "FrameBegin");
	sys_ui.func_initialize = ui_init;
	sys_ui.func_step       = ui_step;
	sys_ui.func_shutdown   = ui_shutdown;
	systems_add(&sys_ui);

	system_t sys_ui_late = { "UILate" };
	system_set_step_deps(sys_ui_late, "App");
	sys_ui_late.func_step = ui_step_late;
	systems_add(&sys_ui_late);

	system_t sys_physics = { "Physics" };
	system_set_initialize_deps(sys_physics, "Defaults");
	system_set_step_deps      (sys_physics, "Input", "FrameBegin");
	sys_physics.func_initialize = physics_init;
	sys_physics.func_step       = physics_step;
	sys_physics.func_shutdown   = physics_shutdown;
	systems_add(&sys_physics);

	system_t sys_renderer = { "Renderer" };
	system_set_initialize_deps(sys_renderer, "Platform", "Defaults");
	system_set_step_deps      (sys_renderer, "Physics", "FrameBegin");
	sys_renderer.func_initialize = render_init;
	sys_renderer.func_step       = render_step;
	sys_renderer.func_shutdown   = render_shutdown;
	systems_add(&sys_renderer);

	system_t sys_assets = { "Assets" };
	system_set_initialize_deps(sys_assets, "Platform");
	system_set_step_deps      (sys_assets, "FrameRender");
	sys_assets.func_initialize       = assets_init;
	sys_assets.func_step             = assets_step;
	sys_assets.func_shutdown         = assets_shutdown;
	systems_add(&sys_assets);

	system_t sys_audio = { "Audio" };
	system_set_initialize_deps(sys_audio, "Platform");
	system_set_step_deps      (sys_audio, "Platform");
	sys_audio.func_initialize = audio_init;
	sys_audio.func_step       = audio_step;
	sys_audio.func_shutdown   = audio_shutdown;
	systems_add(&sys_audio);

	system_t sys_input = { "Input" };
	system_set_initialize_deps(sys_input, "Platform", "Defaults");
	system_set_step_deps      (sys_input, "FrameBegin");
	sys_input.func_initialize = input_init;
	//sys_input.func_step       = input_step; // Handled by the platform, not my fav solution
	sys_input.func_shutdown   = input_shutdown;
	systems_add(&sys_input);

	system_t sys_text = { "Text" };
	system_set_initialize_deps(sys_text, "Defaults");
	system_set_step_deps      (sys_text, "App");
	sys_text.func_step     = text_step;
	sys_text.func_shutdown = text_shutdown;
	systems_add(&sys_text);

	system_t sys_sprite = { "Sprites" };
	system_set_initialize_deps(sys_sprite, "Defaults");
	system_set_step_deps      (sys_sprite, "App");
	sys_sprite.func_initialize = sprite_drawer_init;
	sys_sprite.func_step       = sprite_drawer_step;
	sys_sprite.func_shutdown   = sprite_drawer_shutdown;
	systems_add(&sys_sprite);

	system_t sys_lines = { "Lines" };
	system_set_initialize_deps(sys_lines, "Defaults");
	system_set_step_deps      (sys_lines, "App");
	sys_lines.func_initialize = line_drawer_init;
	sys_lines.func_step       = line_drawer_step;
	sys_lines.func_shutdown   = line_drawer_shutdown;
	systems_add(&sys_lines);

	system_t sys_world = { "World" };
	system_set_initialize_deps(sys_world, "Platform", "Defaults", "Renderer");
	system_set_step_deps      (sys_world, "Platform", "App");
	sys_world.func_initialize = world_init;
	sys_world.func_step       = world_step;
	sys_world.func_shutdown   = world_shutdown;
	systems_add(&sys_world);

	system_t sys_anim = { "Animation" };
	system_set_step_deps(sys_anim, "App");
	sys_anim.func_step     = anim_step;
	sys_anim.func_shutdown = anim_shutdown;
	systems_add(&sys_anim);

	system_t sys_app = { "App" };
	system_set_step_deps(sys_app, "Input", "Defaults", "FrameBegin", "Platform", "Physics", "Renderer", "UI");
	sys_app.func_step = sk_app_step;
	systems_add(&sys_app);

	local.initialized = systems_initialize();
	if (!local.initialized) log_show_any_fail_reason();
	else                    log_clear_any_fail_reason();

	local.app_system    = systems_find("App");
	local.app_init_time = stm_now();
	local.running       = local.initialized;
	return local.initialized;
}

///////////////////////////////////////////

void sk_shutdown() {
	if (sk_is_stepping()) {
		log_err("sk_shutdown should only be called for cleanup, please use sk_quit to exit the app!");
		abort();
	}

	sk_shutdown_unsafe();
}

///////////////////////////////////////////

void sk_shutdown_unsafe(void) {
	log_show_any_fail_reason();

	systems_shutdown();
	sk_mem_log_allocations();

	local = {};
}

///////////////////////////////////////////

void sk_app_step() {
	if (local.app_step_func != nullptr)
		local.app_step_func();
}

///////////////////////////////////////////

void sk_quit() {
	local.running = false;
}

///////////////////////////////////////////

bool32_t sk_step(void (*app_step)(void)) {
	if (local.app_system->profile_start_duration == 0)
		local.app_system->profile_start_duration = stm_since(local.app_init_time);

	// TODO: remove this in v0.4 when sk_step is formally replaced by sk_run
	sk_assert_thread_valid();
	local.first_step = true;
	local.stepping   = true;
	
	local.app_step_func = app_step;
	sk_step_timer();

	systems_step();

	if (device_display_get_type() == display_type_flatscreen && local.focus != app_focus_active && !local.settings.disable_unfocused_sleep)
		platform_sleep(100);
	local.stepping = false;
	return local.running;
}

///////////////////////////////////////////

void sk_run(void (*app_update)(void), void (*app_shutdown)(void)) {
	sk_assert_thread_valid();
	local.first_step = true;
	
#if defined(SK_OS_WEB)
	web_start_main_loop(app_update, app_shutdown);
#else
	while (sk_step(app_update));

	if (app_shutdown != nullptr)
		app_shutdown();
	sk_shutdown();
#endif
}

///////////////////////////////////////////

void (*_sk_run_data_app_update)(void *);
void  *_sk_run_data_update_data;
void (*_sk_run_data_app_shutdown)(void *);
void  *_sk_run_data_shutdown_data;
void sk_run_data(void (*app_update)(void *update_data), void *update_data, void (*app_shutdown)(void *shutdown_data), void *shutdown_data) {
	_sk_run_data_app_update    = app_update;
	_sk_run_data_update_data   = update_data;
	_sk_run_data_app_shutdown  = app_shutdown;
	_sk_run_data_shutdown_data = shutdown_data;

	sk_assert_thread_valid();
	local.first_step = true;

#if defined(SK_OS_WEB)
	web_start_main_loop(
		[]() { if (_sk_run_data_app_update  ) _sk_run_data_app_update  (_sk_run_data_update_data  ); },
		[]() { if (_sk_run_data_app_shutdown) _sk_run_data_app_shutdown(_sk_run_data_shutdown_data); });
#else
	while (sk_step(
		[]() { if (_sk_run_data_app_update  ) _sk_run_data_app_update  (_sk_run_data_update_data  ); }));

	if (_sk_run_data_app_shutdown)
		_sk_run_data_app_shutdown(_sk_run_data_shutdown_data);

	sk_shutdown();
#endif
}

///////////////////////////////////////////

void sk_step_timer() {
	local.timev_raw = stm_now();
	double time_curr = stm_sec(local.timev_raw);

	if (local.time_start == 0)
		local.time_start = time_curr;
	double new_time = time_curr - local.time_start;
	local.timev_step_us  =  new_time - local.timev_us;
	local.timev_step     = (new_time - local.timev_us) * local.timev_scale;
	local.timev_us       = new_time;
	local.timev         += local.timev_step;
	local.timev_stepf_us = (float)local.timev_step_us;
	local.timev_stepf    = (float)local.timev_step;
	local.timevf_us      = (float)local.timev_us;
	local.timevf         = (float)local.timev;
}

///////////////////////////////////////////

void sk_assert_thread_valid() {
	// sk_init and sk_run/step need to happen on the same thread, but there's a
	// non-zero chance that some async code can inadvertently put execution
	// onto another thread without the dev's knowledge. This can trip up async
	// asset code and cause a blocking loop as the asset waits for the main
	// thread to step. This function is used to detect and warn of such a
	// situation.
	if (local.initialized && local.first_step)
		return;
	
	if (thrd_id_equal(local.init_thread, thrd_id_current()) == false) {
		const char* err = "SK.Run and pre-Run GPU asset creation currently must be called on the same thread as SK.Initialize! Has async code accidentally bumped you to another thread?";
		log_err(err);
		platform_msgbox_err(err, "Fatal Error");
		abort();
	}
}

///////////////////////////////////////////

void sk_set_window(void* window) {
	platform_set_window(window);
}

///////////////////////////////////////////

void sk_set_window_xam(void* window) {
	platform_set_window_xam(window);
}

///////////////////////////////////////////

const char *sk_version_name() {
	return SK_VERSION " "
#if defined(SK_OS_WEB)
		"Web"
#elif defined(SK_OS_ANDROID)
		"Android"
#elif defined(SK_OS_LINUX)
		"Linux"
#elif defined(SK_OS_WINDOWS)
		"Win32"
#elif defined(SK_OS_WINDOWS_UWP)
		"UWP"
#else
		"MysteryPlatform"
#endif
		
		" "
		
#if defined(__x86_64__) || defined(_M_X64)
		"x64"
#elif defined(__aarch64__) || defined(_M_ARM64)
		"ARM64"
#elif defined(_M_ARM)
		"ARM"
#elif defined(i386) || defined(__i386__) || defined(__i386) || defined(_M_IX86)
		"x86"
#else
		"MysteryArchitecture"
#endif
	;
}

///////////////////////////////////////////

uint64_t sk_version_id() { return SK_VERSION_ID; }

///////////////////////////////////////////

app_focus_ sk_app_focus() { return local.focus; }

///////////////////////////////////////////

void sk_set_app_focus(app_focus_ focus_state) { local.focus = focus_state; }

///////////////////////////////////////////

sk_settings_t sk_get_settings() { return local.settings; }

///////////////////////////////////////////

system_info_t sk_system_info() {
	system_info_t result = local.info;
	// Right now there's some major overlap in system_info_t and the device
	// APIs. Ultimately, system_info_t will become deprecated or significantly
	// changed, but until then, we'll fill in the data by treating the device
	// APIs as the source of truth.
	//
	// All references to the following system_info_t valuables have been
	// scrubbed out of the core code and should not be used!
	result.display_width        = device_display_get_width();
	result.display_height       = device_display_get_height();
	result.eye_tracking_present = device_has_eye_gaze();
	switch (device_display_get_blend()) {
		case display_blend_none:            result.display_type = display_none;            break;
		case display_blend_opaque:          result.display_type = display_opaque;          break;
		case display_blend_additive:        result.display_type = display_additive;        break;
		case display_blend_blend:           result.display_type = display_blend;           break;
		case display_blend_any_transparent: result.display_type = display_any_transparent; break;
		default: result.display_type = display_none; break;
	}
	return result;
}

///////////////////////////////////////////

const sk_settings_t* sk_get_settings_ref() { return &local.settings; }

///////////////////////////////////////////

system_info_t* sk_get_info_ref() { return &local.info; }

///////////////////////////////////////////

bool32_t sk_has_stepped() { return local.first_step; }

///////////////////////////////////////////

bool32_t sk_is_stepping() { return local.stepping; }

///////////////////////////////////////////

bool32_t sk_is_initialized() { return local.initialized; }

///////////////////////////////////////////

display_mode_ sk_active_display_mode() {
	switch (device_display_get_type()) {
	case display_type_flatscreen: return display_mode_flatscreen;
	case display_type_none:       return display_mode_none;
	case display_type_stereo:     return display_mode_mixedreality;
	default:                      return display_mode_none;
	}
}

///////////////////////////////////////////

double time_get_raw          (){ return time_total_raw      (); }
float  time_getf_unscaled    (){ return time_totalf_unscaled(); };
double time_get_unscaled     (){ return time_total_unscaled (); };
float  time_getf             (){ return time_totalf         (); };
double time_get              (){ return time_total          (); };
float  time_elapsedf_unscaled(){ return time_stepf_unscaled (); };
double time_elapsed_unscaled (){ return time_step_unscaled  (); };
float  time_elapsedf         (){ return time_stepf          (); };
double time_elapsed          (){ return time_step           (); };
double time_total_raw        (){ return stm_sec(stm_now()); }
float  time_totalf_unscaled  (){ return local.timevf_us;       };
double time_total_unscaled   (){ return local.timev_us;        };
float  time_totalf           (){ return local.timevf;          };
double time_total            (){ return local.timev;           };
float  time_stepf_unscaled   (){ return local.timev_stepf_us;  };
double time_step_unscaled    (){ return local.timev_step_us;   };
float  time_stepf            (){ return local.timev_stepf;     };
double time_step             (){ return local.timev_step;      };
void   time_scale(double scale) { local.timev_scale = scale; }

///////////////////////////////////////////

void time_set_time(double total_seconds, double frame_elapsed_seconds) {
	if (frame_elapsed_seconds < 0) {
		frame_elapsed_seconds = local.timev_step_us;
		if (frame_elapsed_seconds == 0)
			frame_elapsed_seconds = 1.f / 90.f;
	}
	total_seconds = fmax(total_seconds, 0);

	local.timev_raw  = stm_now();
	local.time_start = stm_sec(local.timev_raw) - total_seconds;

	local.timev_step_us  = frame_elapsed_seconds;
	local.timev_step     = frame_elapsed_seconds * local.timev_scale;
	local.timev_us       = total_seconds;
	local.timev          = total_seconds;
	local.timev_stepf_us = (float)local.timev_step_us;
	local.timev_stepf    = (float)local.timev_step;
	local.timevf_us      = (float)local.timev_us;
	local.timevf         = (float)local.timev;
	physics_sim_time  = local.timev;
}

} // namespace sk
