#include <obs-module.h>
#include "plugin-support.h"

struct AvatarSourceContext {
	obs_source_t *source;
	uint32_t width = 320;
	uint32_t height = 240;
	gs_texture_t *placeholder_tex = nullptr;
};

static const char *avatar_get_name(void * /* type_data */)
{
	return obs_module_text("AnimatedAvatar.SourceName");
}

static void *avatar_create(obs_data_t * /* settings */, obs_source_t *source)
{
	auto *ctx = new AvatarSourceContext;
	ctx->source = source;

	// Create 1x1 purple RGBA texture -- MUST be inside the graphics lock
	obs_enter_graphics();
	uint8_t purple[4] = {128, 0, 128, 255}; // R G B A -- straight alpha
	const uint8_t *ptr = purple;
	ctx->placeholder_tex = gs_texture_create(1, 1, GS_RGBA, 1, &ptr, 0);
	obs_leave_graphics();

	obs_log(LOG_INFO, "[init] avatar source created");
	return ctx;
}

static void avatar_destroy(void *data)
{
	auto *ctx = static_cast<AvatarSourceContext *>(data);

	// GPU resource destruction MUST also be inside the graphics lock
	obs_enter_graphics();
	gs_texture_destroy(ctx->placeholder_tex);
	obs_leave_graphics();

	delete ctx;
}

static uint32_t avatar_get_width(void *data)
{
	return static_cast<AvatarSourceContext *>(data)->width;
}

static uint32_t avatar_get_height(void *data)
{
	return static_cast<AvatarSourceContext *>(data)->height;
}

static void avatar_render(void *data, gs_effect_t * /* effect */)
{
	auto *ctx = static_cast<AvatarSourceContext *>(data);
	if (!ctx->placeholder_tex)
		return;

	gs_effect_t *eff = obs_get_base_effect(OBS_EFFECT_DEFAULT);
	gs_eparam_t *param = gs_effect_get_param_by_name(eff, "image");
	gs_effect_set_texture(param, ctx->placeholder_tex);

	while (gs_effect_loop(eff, "Draw")) {
		gs_draw_sprite(ctx->placeholder_tex, 0, ctx->width, ctx->height);
	}
}

static obs_properties_t *avatar_get_properties(void * /* data */)
{
	obs_properties_t *props = obs_properties_create();
	// Phase 1: empty properties panel -- proves the OBS properties call works
	return props;
}

static void avatar_get_defaults(obs_data_t * /* settings */)
{
	// Phase 1: no settings yet
}

// *** obs_source_info field order MUST match the declaration order in
// libobs/obs-source.h for C++20 designated initializers. Unused optional
// callbacks are simply omitted (they default to NULL). ***
static struct obs_source_info avatar_source_info = {
	.id = "animated_avatar_source", // D-01: ONE-WAY DOOR -- never rename
	.type = OBS_SOURCE_TYPE_INPUT,
	.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW,
	.get_name = avatar_get_name,
	.create = avatar_create,
	.destroy = avatar_destroy,
	.get_width = avatar_get_width,
	.get_height = avatar_get_height,
	.get_defaults = avatar_get_defaults,
	.get_properties = avatar_get_properties,
	.video_render = avatar_render,
};

void register_avatar_source()
{
	obs_register_source(&avatar_source_info);
}
