#pragma once

#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

// One preset donation amount.
struct DonationTier {
	double amount = 0.0;
	std::string label;
	std::string link; // optional: overrides the main link (used while spotlighted)
	std::string icon = "none"; // none | heart | cup | star | gift
	bool show = true;
	bool popular = false;
};

struct DonationRotation {
	bool enabled = true;
	int perPage = 3;
	int seconds = 8;
	std::string order = "list"; // list | shuffle
	bool keepPopular = false;
};

struct DonationStyle {
	std::string accent = "#E9B949";
	int blur = 24;  // px
	int tint = 35;  // %
	std::string font = "Sora";
	bool showLink = true;
	std::string linkSize = "large"; // large | medium
	std::string entrance = "rise";  // rise | fade | none
	std::string anchor = "bottom-right";
	int margin = 32;
	double scale = 1.0;
};

struct DonationVisibility {
	std::string mode = "always"; // always | cycle | triggered
	int showSeconds = 45;
	int everyMinutes = 10;
};

struct DonationConfig {
	bool visible = true;
	std::string link;
	std::string heading = "Support the stream";
	std::string subtext = "Every bit keeps us live";
	std::string currency = "\xC2\xA3"; // £
	bool showAny = true;
	std::vector<DonationTier> tiers;
	DonationRotation rotate;
	std::string layout = "card"; // card | bar | minimal
	DonationStyle style;
	DonationVisibility visibility;

	// Runtime commands read by the overlay (epoch milliseconds).
	int spotlightIndex = -1;
	int64_t spotlightUntil = 0;
	int64_t triggerUntil = 0;
	int64_t nextSeq = 0;
};

// Single source of truth shared by the dock, the source and the hotkeys.
// Every change is written to <plugin config>/overlay/config.json, which the
// overlay page polls.
class ConfigStore {
public:
	using Listener = std::function<void()>;

	static ConfigStore &instance();

	void load();
	DonationConfig get() const;
	void update(const std::function<void(DonationConfig &)> &fn);

	int addListener(Listener listener);
	void removeListener(int id);

	const std::string &overlayHtmlPath() const { return htmlPath; }
	const std::string &configDir() const { return dir; }

	static int64_t nowMs();
	static std::string formatAmount(const DonationConfig &cfg, double amount);

private:
	ConfigStore() = default;
	void writeLocked();
	void installOverlay();
	static DonationConfig defaults();

	mutable std::mutex mtx;
	DonationConfig cfg;
	std::string dir;
	std::string jsonPath;
	std::string htmlPath;

	std::mutex listenerMtx;
	int nextListenerId = 1;
	std::vector<std::pair<int, Listener>> listeners;
};
