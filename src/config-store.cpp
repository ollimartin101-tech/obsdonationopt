#include "config-store.hpp"

#include <obs-module.h>
#include <util/bmem.h>
#include <util/platform.h>

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace {

std::string getStr(obs_data_t *d, const char *key, const std::string &def)
{
	if (!obs_data_has_user_value(d, key))
		return def;
	const char *v = obs_data_get_string(d, key);
	return v ? std::string(v) : def;
}

bool getBool(obs_data_t *d, const char *key, bool def)
{
	return obs_data_has_user_value(d, key) ? obs_data_get_bool(d, key) : def;
}

int getInt(obs_data_t *d, const char *key, int def)
{
	return obs_data_has_user_value(d, key) ? (int)obs_data_get_int(d, key) : def;
}

double getDouble(obs_data_t *d, const char *key, double def)
{
	return obs_data_has_user_value(d, key) ? obs_data_get_double(d, key) : def;
}

int64_t getInt64(obs_data_t *d, const char *key, int64_t def)
{
	return obs_data_has_user_value(d, key) ? (int64_t)obs_data_get_int(d, key) : def;
}

obs_data_t *toData(const DonationConfig &c)
{
	obs_data_t *d = obs_data_create();
	obs_data_set_int(d, "version", 1);
	obs_data_set_bool(d, "visible", c.visible);
	obs_data_set_string(d, "link", c.link.c_str());
	obs_data_set_string(d, "heading", c.heading.c_str());
	obs_data_set_string(d, "subtext", c.subtext.c_str());
	obs_data_set_string(d, "currency", c.currency.c_str());
	obs_data_set_bool(d, "showAny", c.showAny);
	obs_data_set_string(d, "layout", c.layout.c_str());

	obs_data_array_t *arr = obs_data_array_create();
	for (const auto &t : c.tiers) {
		obs_data_t *td = obs_data_create();
		obs_data_set_double(td, "amount", t.amount);
		obs_data_set_string(td, "label", t.label.c_str());
		obs_data_set_string(td, "link", t.link.c_str());
		obs_data_set_string(td, "icon", t.icon.c_str());
		obs_data_set_bool(td, "show", t.show);
		obs_data_set_bool(td, "popular", t.popular);
		obs_data_array_push_back(arr, td);
		obs_data_release(td);
	}
	obs_data_set_array(d, "tiers", arr);
	obs_data_array_release(arr);

	obs_data_t *r = obs_data_create();
	obs_data_set_bool(r, "enabled", c.rotate.enabled);
	obs_data_set_int(r, "perPage", c.rotate.perPage);
	obs_data_set_int(r, "seconds", c.rotate.seconds);
	obs_data_set_string(r, "order", c.rotate.order.c_str());
	obs_data_set_bool(r, "keepPopular", c.rotate.keepPopular);
	obs_data_set_obj(d, "rotate", r);
	obs_data_release(r);

	obs_data_t *s = obs_data_create();
	obs_data_set_string(s, "accent", c.style.accent.c_str());
	obs_data_set_int(s, "blur", c.style.blur);
	obs_data_set_int(s, "tint", c.style.tint);
	obs_data_set_string(s, "font", c.style.font.c_str());
	obs_data_set_bool(s, "showLink", c.style.showLink);
	obs_data_set_string(s, "linkSize", c.style.linkSize.c_str());
	obs_data_set_string(s, "entrance", c.style.entrance.c_str());
	obs_data_set_string(s, "anchor", c.style.anchor.c_str());
	obs_data_set_int(s, "margin", c.style.margin);
	obs_data_set_double(s, "scale", c.style.scale);
	obs_data_set_obj(d, "style", s);
	obs_data_release(s);

	obs_data_t *v = obs_data_create();
	obs_data_set_string(v, "mode", c.visibility.mode.c_str());
	obs_data_set_int(v, "showSeconds", c.visibility.showSeconds);
	obs_data_set_int(v, "everyMinutes", c.visibility.everyMinutes);
	obs_data_set_obj(d, "visibility", v);
	obs_data_release(v);

	obs_data_t *sp = obs_data_create();
	obs_data_set_int(sp, "index", c.spotlightIndex);
	obs_data_set_int(sp, "until", c.spotlightUntil);
	obs_data_set_obj(d, "spotlight", sp);
	obs_data_release(sp);

	obs_data_t *tr = obs_data_create();
	obs_data_set_int(tr, "until", c.triggerUntil);
	obs_data_set_obj(d, "trigger", tr);
	obs_data_release(tr);

	obs_data_set_int(d, "nextSeq", c.nextSeq);
	return d;
}

DonationConfig fromData(obs_data_t *d, const DonationConfig &def)
{
	DonationConfig c = def;
	c.visible = getBool(d, "visible", def.visible);
	c.link = getStr(d, "link", def.link);
	c.heading = getStr(d, "heading", def.heading);
	c.subtext = getStr(d, "subtext", def.subtext);
	c.currency = getStr(d, "currency", def.currency);
	c.showAny = getBool(d, "showAny", def.showAny);
	c.layout = getStr(d, "layout", def.layout);

	obs_data_array_t *arr = obs_data_get_array(d, "tiers");
	if (arr) {
		c.tiers.clear();
		size_t n = obs_data_array_count(arr);
		for (size_t i = 0; i < n; i++) {
			obs_data_t *td = obs_data_array_item(arr, i);
			DonationTier t;
			t.amount = getDouble(td, "amount", 0.0);
			t.label = getStr(td, "label", "");
			t.link = getStr(td, "link", "");
			t.icon = getStr(td, "icon", "none");
			t.show = getBool(td, "show", true);
			t.popular = getBool(td, "popular", false);
			c.tiers.push_back(t);
			obs_data_release(td);
		}
		obs_data_array_release(arr);
	}

	if (obs_data_t *r = obs_data_get_obj(d, "rotate")) {
		c.rotate.enabled = getBool(r, "enabled", def.rotate.enabled);
		c.rotate.perPage = getInt(r, "perPage", def.rotate.perPage);
		c.rotate.seconds = getInt(r, "seconds", def.rotate.seconds);
		c.rotate.order = getStr(r, "order", def.rotate.order);
		c.rotate.keepPopular = getBool(r, "keepPopular", def.rotate.keepPopular);
		obs_data_release(r);
	}
	if (obs_data_t *s = obs_data_get_obj(d, "style")) {
		c.style.accent = getStr(s, "accent", def.style.accent);
		c.style.blur = getInt(s, "blur", def.style.blur);
		c.style.tint = getInt(s, "tint", def.style.tint);
		c.style.font = getStr(s, "font", def.style.font);
		c.style.showLink = getBool(s, "showLink", def.style.showLink);
		c.style.linkSize = getStr(s, "linkSize", def.style.linkSize);
		c.style.entrance = getStr(s, "entrance", def.style.entrance);
		c.style.anchor = getStr(s, "anchor", def.style.anchor);
		c.style.margin = getInt(s, "margin", def.style.margin);
		c.style.scale = getDouble(s, "scale", def.style.scale);
		obs_data_release(s);
	}
	if (obs_data_t *v = obs_data_get_obj(d, "visibility")) {
		c.visibility.mode = getStr(v, "mode", def.visibility.mode);
		c.visibility.showSeconds = getInt(v, "showSeconds", def.visibility.showSeconds);
		c.visibility.everyMinutes = getInt(v, "everyMinutes", def.visibility.everyMinutes);
		obs_data_release(v);
	}
	// Runtime commands are intentionally not restored: a spotlight or
	// trigger from a previous session should not fire again on startup.
	c.nextSeq = getInt64(d, "nextSeq", 0);
	return c;
}

std::string takeBstr(char *s)
{
	std::string out = s ? s : "";
	bfree(s);
	return out;
}

} // namespace

ConfigStore &ConfigStore::instance()
{
	static ConfigStore store;
	return store;
}

int64_t ConfigStore::nowMs()
{
	using namespace std::chrono;
	return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

std::string ConfigStore::formatAmount(const DonationConfig &cfg, double amount)
{
	char buf[64];
	if (std::fabs(amount - std::round(amount)) < 0.005)
		snprintf(buf, sizeof(buf), "%.0f", std::round(amount));
	else
		snprintf(buf, sizeof(buf), "%.2f", amount);
	return cfg.currency + buf;
}

DonationConfig ConfigStore::defaults()
{
	DonationConfig c;
	c.tiers = {
		{3.0, "Say hi", "", "none", true, false},
		{5.0, "Buy me a coffee", "", "cup", true, true},
		{10.0, "Pizza slice", "", "none", true, false},
		{15.0, "Lunch on me", "", "none", true, false},
		{25.0, "Fund new gear", "", "none", true, false},
		{50.0, "Big hype", "", "star", true, false},
	};
	return c;
}

void ConfigStore::installOverlay()
{
	// Copy the bundled overlay page next to config.json so the page can
	// fetch() its settings from the same folder.
	std::string src = takeBstr(obs_module_file("overlay/overlay.html"));
	if (src.empty()) {
		blog(LOG_ERROR, "[donation-options] overlay/overlay.html missing from plugin data folder");
		return;
	}
	char *html = os_quick_read_utf8_file(src.c_str());
	if (!html) {
		blog(LOG_ERROR, "[donation-options] could not read %s", src.c_str());
		return;
	}
	if (!os_quick_write_utf8_file(htmlPath.c_str(), html, strlen(html), false))
		blog(LOG_ERROR, "[donation-options] could not write %s", htmlPath.c_str());
	bfree(html);
}

void ConfigStore::load()
{
	std::lock_guard<std::mutex> lock(mtx);

	dir = takeBstr(obs_module_config_path("overlay"));
	os_mkdirs(dir.c_str());
	jsonPath = dir + "/config.json";
	htmlPath = dir + "/overlay.html";

	installOverlay();

	DonationConfig def = defaults();
	obs_data_t *d = obs_data_create_from_json_file_safe(jsonPath.c_str(), "bak");
	if (d) {
		cfg = fromData(d, def);
		obs_data_release(d);
	} else {
		cfg = def;
	}
	cfg.spotlightIndex = -1;
	cfg.spotlightUntil = 0;
	cfg.triggerUntil = 0;
	writeLocked();

	blog(LOG_INFO, "[donation-options] overlay at %s", htmlPath.c_str());
}

DonationConfig ConfigStore::get() const
{
	std::lock_guard<std::mutex> lock(mtx);
	return cfg;
}

void ConfigStore::writeLocked()
{
	if (jsonPath.empty())
		return;
	obs_data_t *d = toData(cfg);
	if (!obs_data_save_json_safe(d, jsonPath.c_str(), "tmp", "bak"))
		blog(LOG_WARNING, "[donation-options] failed to save %s", jsonPath.c_str());
	obs_data_release(d);
}

void ConfigStore::update(const std::function<void(DonationConfig &)> &fn)
{
	{
		std::lock_guard<std::mutex> lock(mtx);
		fn(cfg);
		writeLocked();
	}

	std::vector<Listener> copy;
	{
		std::lock_guard<std::mutex> lock(listenerMtx);
		for (auto &l : listeners)
			copy.push_back(l.second);
	}
	for (auto &l : copy)
		l();
}

int ConfigStore::addListener(Listener listener)
{
	std::lock_guard<std::mutex> lock(listenerMtx);
	int id = nextListenerId++;
	listeners.emplace_back(id, std::move(listener));
	return id;
}

void ConfigStore::removeListener(int id)
{
	std::lock_guard<std::mutex> lock(listenerMtx);
	for (auto it = listeners.begin(); it != listeners.end(); ++it) {
		if (it->first == id) {
			listeners.erase(it);
			return;
		}
	}
}
