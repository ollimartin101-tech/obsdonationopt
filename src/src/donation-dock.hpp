#pragma once

#include "dock-api.h"
#include "config-store.hpp"

#include <QWidget>
#include <QPointer>
#include <string>

class QLabel;
class QLineEdit;
class QComboBox;
class QSpinBox;
class QCheckBox;
class QPushButton;
class QListWidget;
class QButtonGroup;
class QFrame;
class QTimer;
class ToggleSwitch;

class DonationDock : public QWidget {
public:
	explicit DonationDock(QWidget *parent = nullptr);
	~DonationDock() override;

	static DonationDock *current();

private:
	void buildUi();
	void refreshFromStore();
	void rebuildTierList(const DonationConfig &c);
	void updateLiveCard(const DonationConfig &c);
	void updateSpotlightUi(const DonationConfig &c);

	void commitTexts();
	void addTier();
	void editTier(int index);
	void removeSelected();
	void moveSelected(int delta);
	void startSpotlight();
	void addSourceToScene();
	void openStyle();

	int listenerId_ = 0;
	int pendingRow_ = -1;
	std::string tiersSignature_;

	ToggleSwitch *liveSwitch_ = nullptr;
	QFrame *liveCard_ = nullptr;
	QLabel *liveTitle_ = nullptr;
	QLabel *liveSub_ = nullptr;

	QLineEdit *link_ = nullptr;
	QLineEdit *heading_ = nullptr;
	QLineEdit *subtext_ = nullptr;
	QComboBox *currency_ = nullptr;
	QListWidget *tiers_ = nullptr;
	QPushButton *editBtn_ = nullptr;
	QPushButton *removeBtn_ = nullptr;
	QPushButton *upBtn_ = nullptr;
	QPushButton *downBtn_ = nullptr;
	QCheckBox *showAny_ = nullptr;

	ToggleSwitch *rotateSwitch_ = nullptr;
	QLabel *rotateSub_ = nullptr;
	QWidget *rotateBody_ = nullptr;
	QSpinBox *perPage_ = nullptr;
	QSpinBox *seconds_ = nullptr;
	QButtonGroup *order_ = nullptr;
	QCheckBox *keepPopular_ = nullptr;

	QButtonGroup *layout_ = nullptr;

	QComboBox *spotAmount_ = nullptr;
	QComboBox *spotDuration_ = nullptr;
	QPushButton *spotGo_ = nullptr;
	QTimer *spotTimer_ = nullptr;

	QTimer *textDebounce_ = nullptr;
};
