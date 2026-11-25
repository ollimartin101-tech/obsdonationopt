/*
 * Donation Options for OBS Studio
 * Preset donation amounts shown on stream in a dark, liquid-glass gold overlay.
 */

#include "config-store.hpp"
#include "donation-dock.hpp"
#include "donation-source.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>
#include <util/bmem.h>

#include <QMainWindow>

#include <algorithm>
#include <string>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("obs-donation-options", "en-US")

MODULE_EXPORT const char *obs_module_description(void)
{
	return "Preset donation amounts with a rotating liquid-glass overlay.";
}

namespace {

obs_hotkey_id hkToggle = OBS_INVALID_HOTKEY_ID;
obs_hotkey_id hkShowNow = OBS_INVALID_HOTKEY_ID;
obs_hotkey_id hkNext = OBS_INVALID_HOTKEY_ID;

std::string hotkeysPath()
{
	char *p = obs_module_config_path("hotkeys.json");
	std::string s = p ? p : "";
	bfree(p);
	return s;
}

void onToggle(void *, obs_hotkey_id, obs_hotkey_t *, bool pressed)
{
	if (pressed)
		ConfigStore::instance().update([](DonationConfig &c) { c.visible = !c.visible; });
}

void onShowNow(void *, obs_hotkey_id, obs_hotkey_t *, bool pressed)
{
	if (pressed)
		ConfigStore::instance().update([](DonationConfig &c) {
			c.triggerUntil = ConfigStore::nowMs() + (int64_t)std::max(5, c.visibility.showSeconds) * 1000;
		});
}

void onNext(void *, obs_hotkey_id, obs_hotkey_t *, bool pressed)
{
	if (pressed)
		ConfigStore::instance().update([](DonationConfig &c) { c.nextSeq++; });
}

void loadHotkey(obs_data_t *d, const char *key, obs_hotkey_id id)
{
	obs_data_array_t *arr = obs_data_get_array(d, key);
	if (arr) {
		obs_hotkey_load(id, arr);
		obs_data_array_release(arr);
	}
}

void saveHotkeys()
{
	std::string path = hotkeysPath();
	if (path.empty())
		return;
	obs_data_t *d = obs_data_create();
	const std::pair<const char *, obs_hotkey_id> keys[] = {
		{"toggle", hkToggle}, {"show_now", hkShowNow}, {"next", hkNext}};
	for (const auto &k : keys) {
		if (k.second == OBS_INVALID_HOTKEY_ID)
			continue;
		obs_data_array_t *arr = obs_hotkey_save(k.second);
		obs_data_set_array(d, k.first, arr);
		obs_data_array_release(arr);
	}
	obs_data_save_json_safe(d, path.c_str(), "tmp", "bak");
	obs_data_release(d);
}

void registerHotkeys()
{
	hkToggle = obs_hotkey_register_frontend("DonationOptions.Toggle",
						obs_module_text("DonationOptions.Hotkey.Toggle"), onToggle, nullptr);
	hkShowNow = obs_hotkey_register_frontend("DonationOptions.ShowNow",
						 obs_module_text("DonationOptions.Hotkey.ShowNow"), onShowNow, nullptr);
	hkNext = obs_hotkey_register_frontend("DonationOptions.Next", obs_module_text("DonationOptions.Hotkey.Next"),
					      onNext, nullptr);

	obs_data_t *d = obs_data_create_from_json_file_safe(hotkeysPath().c_str(), "bak");
	if (d) {
		loadHotkey(d, "toggle", hkToggle);
		loadHotkey(d, "show_now", hkShowNow);
		loadHotkey(d, "next", hkNext);
		obs_data_release(d);
	}
}

void onFrontendEvent(enum obs_frontend_event event, void *)
{
	if (event == OBS_FRONTEND_EVENT_EXIT)
		saveHotkeys();
}

} // namespace

bool obs_module_load(void)
{
	ConfigStore::instance().load();
	donation_source_register();

	auto *mainWindow = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	obs_frontend_push_ui_translation(obs_module_get_string);
	auto *dock = new DonationDock(mainWindow);
	obs_frontend_add_dock_by_id("DonationOptionsDock", obs_module_text("DonationOptions.Dock"), dock);
	obs_frontend_pop_ui_translation();

	registerHotkeys();
	obs_frontend_add_event_callback(onFrontendEvent, nullptr);

	blog(LOG_INFO, "[donation-options] loaded (version %s)", PLUGIN_VERSION);
	return true;
}

void obs_module_unload(void)
{
	obs_frontend_remove_event_callback(onFrontendEvent, nullptr);
	blog(LOG_INFO, "[donation-options] unloaded");
}
