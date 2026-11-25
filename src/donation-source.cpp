// "Donation Options" source.
//
// Wraps a private OBS browser source that renders the bundled overlay page,
// so the overlay gets real CSS glass (backdrop blur) and animations. The
// source's own properties hold the look (colour, glass, position, timing);
// the dock holds the content (link, amounts, rotation, layout).

#include "donation-source.hpp"
#include "config-store.hpp"
#include "dock-api.h"

#include <obs-module.h>

#include <cstdio>
#include <string>

namespace {

struct DonationSource {
	obs_source_t *self = nullptr;
	obs_source_t *browser = nullptr;
	uint32_t width = 1920;
	uint32_t height = 1080;
};

const char *TRANSPARENT_CSS = "body { background-color: rgba(0, 0, 0, 0); margin: 0px auto; overflow: hidden; }";

// OBS stores colours as 0xAABBGGRR.
std::string colorToHex(long long c)
{
	unsigned r = (unsigned)(c & 0xFF);
	unsigned g = (unsigned)((c >> 8) & 0xFF);
	unsigned b = (unsigned)((c >> 16) & 0xFF);
	char buf[8];
	snprintf(buf, sizeof(buf), "#%02X%02X%02X", r, g, b);
	return buf;
}

void updateBrowser(DonationSource *ds)
{
	obs_data_t *bs = obs_data_create();
	obs_data_set_bool(bs, "is_local_file", true);
	obs_data_set_string(bs, "local_file", ConfigStore::instance().overlayHtmlPath().c_str());
	obs_data_set_int(bs, "width", ds->width);
	obs_data_set_int(bs, "height", ds->height);
	obs_data_set_bool(bs, "fps_custom", false);
	obs_data_set_string(bs, "css", TRANSPARENT_CSS);
	obs_data_set_bool(bs, "shutdown", false); // keep running so rotation/cycle timing continues
	obs_data_set_bool(bs, "restart_when_active", false);
	obs_data_set_bool(bs, "reroute_audio", false);

	if (!ds->browser) {
		ds->browser = obs_source_create_private("browser_source", "Donation Options overlay", bs);
		if (!ds->browser)
			blog(LOG_ERROR, "[donation-options] could not create browser source (is obs-browser installed?)");
	} else {
		obs_source_update(ds->browser, bs);
	}
	obs_data_release(bs);
}

void applyStyle(obs_data_t *s)
{
	ConfigStore::instance().update([s](DonationConfig &c) {
		c.style.accent = colorToHex(obs_data_get_int(s, "accent"));
		c.style.blur = (int)obs_data_get_int(s, "blur");
		c.style.tint = (int)obs_data_get_int(s, "tint");
		c.style.font = obs_data_get_string(s, "font");
		c.style.showLink = obs_data_get_bool(s, "show_link");
		c.style.linkSize = obs_data_get_string(s, "link_size");
		c.style.entrance = obs_data_get_string(s, "entrance");
		c.style.anchor = obs_data_get_string(s, "anchor");
		c.style.margin = (int)obs_data_get_int(s, "margin");
		c.style.scale = obs_data_get_double(s, "scale");
		c.visibility.mode = obs_data_get_string(s, "vis_mode");
		c.visibility.showSeconds = (int)obs_data_get_int(s, "show_seconds");
		c.visibility.everyMinutes = (int)obs_data_get_int(s, "every_minutes");
	});
}

const char *ds_get_name(void *)
{
	return obs_module_text("DonationOptions.Source");
}

void ds_update(void *data, obs_data_t *settings)
{
	auto *ds = static_cast<DonationSource *>(data);
	uint32_t w = (uint32_t)obs_data_get_int(settings, "width");
	uint32_t h = (uint32_t)obs_data_get_int(settings, "height");
	ds->width = w ? w : 1920;
	ds->height = h ? h : 1080;
	updateBrowser(ds);
	applyStyle(settings);
}

void *ds_create(obs_data_t *settings, obs_source_t *source)
{
	auto *ds = new DonationSource();
	ds->self = source;
	ds_update(ds, settings);
	return ds;
}

void ds_destroy(void *data)
{
	auto *ds = static_cast<DonationSource *>(data);
	if (ds->browser)
		obs_source_release(ds->browser);
	delete ds;
}

void ds_get_defaults(obs_data_t *s)
{
	obs_video_info ovi = {};
	uint32_t w = 1920, h = 1080;
	if (obs_get_video_info(&ovi)) {
		w = ovi.base_width;
		h = ovi.base_height;
	}
	obs_data_set_default_int(s, "width", w);
	obs_data_set_default_int(s, "height", h);
	obs_data_set_default_string(s, "anchor", "bottom-right");
	obs_data_set_default_int(s, "margin", 32);
	obs_data_set_default_double(s, "scale", 1.0);
	obs_data_set_default_int(s, "accent", 0xFF49B9E9); // #E9B949
	obs_data_set_default_int(s, "blur", 24);
	obs_data_set_default_int(s, "tint", 35);
	obs_data_set_default_string(s, "font", "Sora");
	obs_data_set_default_bool(s, "show_link", true);
	obs_data_set_default_string(s, "link_size", "large");
	obs_data_set_default_string(s, "entrance", "rise");
	obs_data_set_default_string(s, "vis_mode", "always");
	obs_data_set_default_int(s, "show_seconds", 45);
	obs_data_set_default_int(s, "every_minutes", 10);
}

bool visModeChanged(obs_properties_t *props, obs_property_t *, obs_data_t *settings)
{
	std::string mode = obs_data_get_string(settings, "vis_mode");
	obs_property_set_visible(obs_properties_get(props, "show_seconds"), mode != "always");
	obs_property_set_visible(obs_properties_get(props, "every_minutes"), mode == "cycle");
	return true;
}

bool openDockClicked(obs_properties_t *, obs_property_t *, void *)
{
	donation_dock_show();
	return false;
}

obs_properties_t *ds_get_properties(void *)
{
	obs_properties_t *p = obs_properties_create();
	obs_property_t *prop;

	obs_properties_add_button(p, "open_dock", obs_module_text("DonationOptions.OpenDock"), openDockClicked);

	prop = obs_properties_add_list(p, "anchor", obs_module_text("DonationOptions.Position"), OBS_COMBO_TYPE_LIST,
				       OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(prop, obs_module_text("DonationOptions.Pos.BottomRight"), "bottom-right");
	obs_property_list_add_string(prop, obs_module_text("DonationOptions.Pos.BottomLeft"), "bottom-left");
	obs_property_list_add_string(prop, obs_module_text("DonationOptions.Pos.BottomCenter"), "bottom-center");
	obs_property_list_add_string(prop, obs_module_text("DonationOptions.Pos.TopRight"), "top-right");
	obs_property_list_add_string(prop, obs_module_text("DonationOptions.Pos.TopLeft"), "top-left");
	obs_property_list_add_string(prop, obs_module_text("DonationOptions.Pos.TopCenter"), "top-center");

	obs_properties_add_int_slider(p, "margin", obs_module_text("DonationOptions.Margin"), 0, 400, 1);
	obs_properties_add_float_slider(p, "scale", obs_module_text("DonationOptions.Scale"), 0.5, 2.5, 0.05);

	obs_properties_add_color(p, "accent", obs_module_text("DonationOptions.Accent"));
	obs_properties_add_int_slider(p, "blur", obs_module_text("DonationOptions.Blur"), 0, 60, 1);
	obs_properties_add_int_slider(p, "tint", obs_module_text("DonationOptions.Tint"), 0, 100, 1);

	prop = obs_properties_add_list(p, "font", obs_module_text("DonationOptions.Font"), OBS_COMBO_TYPE_LIST,
				       OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(prop, "Sora", "Sora");
	obs_property_list_add_string(prop, "Inter", "Inter");

	obs_properties_add_bool(p, "show_link", obs_module_text("DonationOptions.ShowLink"));
	prop = obs_properties_add_list(p, "link_size", obs_module_text("DonationOptions.LinkSize"),
				       OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(prop, obs_module_text("DonationOptions.Large"), "large");
	obs_property_list_add_string(prop, obs_module_text("DonationOptions.Medium"), "medium");

	prop = obs_properties_add_list(p, "entrance", obs_module_text("DonationOptions.Entrance"),
				       OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(prop, obs_module_text("DonationOptions.Entrance.Rise"), "rise");
	obs_property_list_add_string(prop, obs_module_text("DonationOptions.Entrance.Fade"), "fade");
	obs_property_list_add_string(prop, obs_module_text("DonationOptions.Entrance.None"), "none");

	prop = obs_properties_add_list(p, "vis_mode", obs_module_text("DonationOptions.Visibility"),
				       OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(prop, obs_module_text("DonationOptions.Vis.Always"), "always");
	obs_property_list_add_string(prop, obs_module_text("DonationOptions.Vis.Cycle"), "cycle");
	obs_property_list_add_string(prop, obs_module_text("DonationOptions.Vis.Triggered"), "triggered");
	obs_property_set_modified_callback(prop, visModeChanged);

	obs_property_t *secs =
		obs_properties_add_int(p, "show_seconds", obs_module_text("DonationOptions.ShowFor"), 5, 3600, 1);
	obs_property_int_set_suffix(secs, " s");
	obs_property_t *mins =
		obs_properties_add_int(p, "every_minutes", obs_module_text("DonationOptions.Every"), 1, 240, 1);
	obs_property_int_set_suffix(mins, " min");

	obs_properties_add_int(p, "width", obs_module_text("DonationOptions.Width"), 64, 7680, 1);
	obs_properties_add_int(p, "height", obs_module_text("DonationOptions.Height"), 64, 4320, 1);

	return p;
}

uint32_t ds_get_width(void *data)
{
	return static_cast<DonationSource *>(data)->width;
}

uint32_t ds_get_height(void *data)
{
	return static_cast<DonationSource *>(data)->height;
}

void ds_video_render(void *data, gs_effect_t *)
{
	auto *ds = static_cast<DonationSource *>(data);
	if (ds->browser)
		obs_source_video_render(ds->browser);
}

void ds_enum_sources(void *data, obs_source_enum_proc_t cb, void *param)
{
	auto *ds = static_cast<DonationSource *>(data);
	if (ds->browser)
		cb(ds->self, ds->browser, param);
}

} // namespace

extern "C" void donation_source_register(void)
{
	obs_source_info info = {};
	info.id = DONATION_SOURCE_ID;
	info.type = OBS_SOURCE_TYPE_INPUT;
	info.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW;
	info.icon_type = OBS_ICON_TYPE_BROWSER;
	info.get_name = ds_get_name;
	info.create = ds_create;
	info.destroy = ds_destroy;
	info.update = ds_update;
	info.get_defaults = ds_get_defaults;
	info.get_properties = ds_get_properties;
	info.get_width = ds_get_width;
	info.get_height = ds_get_height;
	info.video_render = ds_video_render;
	info.enum_active_sources = ds_enum_sources;
	info.enum_all_sources = ds_enum_sources;
	obs_register_source(&info);
}
