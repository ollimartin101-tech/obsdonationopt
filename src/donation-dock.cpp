#include "donation-dock.hpp"
#include "amount-dialog.hpp"
#include "donation-source.hpp"
#include "theme.hpp"
#include "toggle-switch.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>

#include <QButtonGroup>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QFrame>
#include <QGraphicsOpacityEffect>
#include <QGridLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMetaObject>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <cstring>
#include <sstream>

namespace {

QPointer<DonationDock> g_dock;

const QString DOT = QString::fromUtf8(" \xC2\xB7 ");

QLabel *sectionLabel(const QString &text)
{
	auto *l = new QLabel(text.toUpper());
	l->setProperty("role", "section");
	return l;
}

QLabel *hintLabel(const QString &text)
{
	auto *l = new QLabel(text);
	l->setProperty("role", "hint");
	l->setWordWrap(true);
	return l;
}

QFrame *cardFrame()
{
	auto *f = new QFrame();
	f->setProperty("card", true);
	return f;
}

QFrame *segGroup(QButtonGroup *group, const QStringList &labels)
{
	auto *frame = new QFrame();
	frame->setObjectName("SegGroup");
	auto *row = new QHBoxLayout(frame);
	row->setContentsMargins(3, 3, 3, 3);
	row->setSpacing(3);
	group->setExclusive(true);
	for (int i = 0; i < labels.size(); i++) {
		auto *b = new QPushButton(labels[i]);
		b->setCheckable(true);
		b->setProperty("seg", true);
		b->setCursor(Qt::PointingHandCursor);
		group->addButton(b, i);
		row->addWidget(b, 1);
	}
	return frame;
}

void repolish(QWidget *w)
{
	w->style()->unpolish(w);
	w->style()->polish(w);
	w->update();
}

void setTextIfIdle(QLineEdit *edit, const std::string &value)
{
	QString v = QString::fromStdString(value);
	if (!edit->hasFocus() && edit->text() != v)
		edit->setText(v);
}

std::string tiersSignature(const DonationConfig &c)
{
	std::ostringstream ss;
	ss << c.currency << '|';
	for (const auto &t : c.tiers)
		ss << t.amount << ';' << t.label << ';' << t.link << ';' << t.icon << ';' << t.show << ';'
		   << t.popular << '|';
	return ss.str();
}

bool findSourceCb(void *param, obs_source_t *s)
{
	const char *id = obs_source_get_unversioned_id(s);
	if (id && strcmp(id, DONATION_SOURCE_ID) == 0) {
		*static_cast<obs_source_t **>(param) = obs_source_get_ref(s);
		return false;
	}
	return true;
}

obs_source_t *findDonationSource()
{
	obs_source_t *found = nullptr;
	obs_enum_sources(findSourceCb, &found);
	return found;
}

const int ROW_HEIGHT = 48;

} // namespace

extern "C" void donation_dock_show(void)
{
	DonationDock *d = g_dock.data();
	if (!d)
		return;
	if (QWidget *dock = d->parentWidget()) {
		dock->show();
		dock->raise();
	}
	d->show();
}

DonationDock *DonationDock::current()
{
	return g_dock.data();
}

DonationDock::DonationDock(QWidget *parent) : QWidget(parent)
{
	g_dock = this;
	setObjectName("DonationDockRoot");
	setAttribute(Qt::WA_StyledBackground, true);
	setStyleSheet(DONATION_STYLESHEET);
	setMinimumWidth(300);

	textDebounce_ = new QTimer(this);
	textDebounce_->setSingleShot(true);
	textDebounce_->setInterval(400);
	connect(textDebounce_, &QTimer::timeout, this, [this]() { commitTexts(); });

	spotTimer_ = new QTimer(this);
	spotTimer_->setSingleShot(true);
	connect(spotTimer_, &QTimer::timeout, this, [this]() { refreshFromStore(); });

	buildUi();

	QPointer<DonationDock> self(this);
	listenerId_ = ConfigStore::instance().addListener([self]() {
		if (!self)
			return;
		QMetaObject::invokeMethod(
			self.data(),
			[self]() {
				if (self)
					self->refreshFromStore();
			},
			Qt::QueuedConnection);
	});

	refreshFromStore();
}

DonationDock::~DonationDock()
{
	ConfigStore::instance().removeListener(listenerId_);
}

void DonationDock::buildUi()
{
	auto *outer = new QVBoxLayout(this);
	outer->setContentsMargins(0, 0, 0, 0);
	outer->setSpacing(0);

	auto *scroll = new QScrollArea();
	scroll->setWidgetResizable(true);
	scroll->setFrameShape(QFrame::NoFrame);
	scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	outer->addWidget(scroll, 1);

	auto *content = new QWidget();
	content->setObjectName("DonationContent");
	scroll->setWidget(content);
	auto *v = new QVBoxLayout(content);
	v->setContentsMargins(14, 14, 14, 14);
	v->setSpacing(16);

	// --- Live card -------------------------------------------------------
	liveCard_ = cardFrame();
	{
		auto *h = new QHBoxLayout(liveCard_);
		h->setContentsMargins(14, 12, 14, 12);
		auto *col = new QVBoxLayout();
		col->setSpacing(2);
		liveTitle_ = new QLabel();
		liveTitle_->setProperty("role", "title");
		liveSub_ = hintLabel(QString());
		col->addWidget(liveTitle_);
		col->addWidget(liveSub_);
		h->addLayout(col, 1);
		liveSwitch_ = new ToggleSwitch(nullptr, 48, 26);
		liveSwitch_->setToolTip(tr("Show the donation overlay on stream"));
		h->addWidget(liveSwitch_, 0, Qt::AlignVCenter);
	}
	v->addWidget(liveCard_);
	connect(liveSwitch_, &QAbstractButton::toggled, this,
		[](bool on) { ConfigStore::instance().update([on](DonationConfig &c) { c.visible = on; }); });

	// --- Donation link -----------------------------------------------------
	{
		auto *box = new QVBoxLayout();
		box->setSpacing(6);
		box->addWidget(sectionLabel(tr("Donation link")));
		auto *row = new QHBoxLayout();
		row->setSpacing(6);
		link_ = new QLineEdit();
		link_->setPlaceholderText(tr("e.g. ko-fi.com/yourname"));
		auto *copy = new QPushButton(tr("Copy"));
		copy->setToolTip(tr("Copy link"));
		row->addWidget(link_, 1);
		row->addWidget(copy);
		box->addLayout(row);
		box->addWidget(hintLabel(tr("Any page that takes payments: Ko-fi, PayPal.me, a Stripe link, etc.")));
		v->addLayout(box);
		connect(copy, &QPushButton::clicked, this,
			[this]() { QGuiApplication::clipboard()->setText(link_->text()); });
	}

	// --- Overlay text --------------------------------------------------------
	{
		auto *box = new QVBoxLayout();
		box->setSpacing(6);
		box->addWidget(sectionLabel(tr("Overlay text")));
		box->addWidget(new QLabel(tr("Heading")));
		heading_ = new QLineEdit();
		heading_->setPlaceholderText(tr("Support the stream"));
		box->addWidget(heading_);
		box->addWidget(new QLabel(tr("Subtext (Bar layout, optional)")));
		subtext_ = new QLineEdit();
		subtext_->setPlaceholderText(tr("Every bit keeps us live"));
		box->addWidget(subtext_);
		v->addLayout(box);
	}
	for (QLineEdit *e : {link_, heading_, subtext_}) {
		connect(e, &QLineEdit::textEdited, this, [this](const QString &) { textDebounce_->start(); });
		connect(e, &QLineEdit::editingFinished, this, [this]() { commitTexts(); });
	}

	// --- Preset amounts -----------------------------------------------------
	{
		auto *box = new QVBoxLayout();
		box->setSpacing(6);
		auto *head = new QHBoxLayout();
		head->addWidget(sectionLabel(tr("Preset amounts")));
		head->addStretch(1);
		head->addWidget(hintLabel(tr("Currency")));
		currency_ = new QComboBox();
		currency_->setEditable(true);
		currency_->addItems({QString::fromUtf8("\xC2\xA3"), QStringLiteral("$"), QString::fromUtf8("\xE2\x82\xAC"),
				     QString::fromUtf8("\xC2\xA5"), QString::fromUtf8("\xE2\x82\xB9"), QStringLiteral("R$"),
				     QStringLiteral("kr ")});
		currency_->setFixedWidth(70);
		head->addWidget(currency_);
		box->addLayout(head);

		auto *listCard = cardFrame();
		auto *lc = new QVBoxLayout(listCard);
		lc->setContentsMargins(4, 4, 4, 4);
		tiers_ = new QListWidget();
		tiers_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		tiers_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		tiers_->setSelectionMode(QAbstractItemView::SingleSelection);
		lc->addWidget(tiers_);
		box->addWidget(listCard);

		auto *btns = new QHBoxLayout();
		btns->setSpacing(6);
		auto *add = new QPushButton(tr("+ Add amount"));
		add->setProperty("primary", true);
		editBtn_ = new QPushButton(tr("Edit"));
		upBtn_ = new QPushButton(QString::fromUtf8("\xE2\x96\xB2"));
		downBtn_ = new QPushButton(QString::fromUtf8("\xE2\x96\xBC"));
		removeBtn_ = new QPushButton(tr("Remove"));
		upBtn_->setToolTip(tr("Move up"));
		downBtn_->setToolTip(tr("Move down"));
		upBtn_->setFixedWidth(34);
		downBtn_->setFixedWidth(34);
		btns->addWidget(add, 1);
		btns->addWidget(editBtn_);
		btns->addWidget(upBtn_);
		btns->addWidget(downBtn_);
		btns->addWidget(removeBtn_);
		box->addLayout(btns);

		showAny_ = new QCheckBox(tr("Also show \"Any amount\""));
		box->addWidget(showAny_);
		v->addLayout(box);

		connect(add, &QPushButton::clicked, this, [this]() { addTier(); });
		connect(editBtn_, &QPushButton::clicked, this, [this]() { editTier(tiers_->currentRow()); });
		connect(removeBtn_, &QPushButton::clicked, this, [this]() { removeSelected(); });
		connect(upBtn_, &QPushButton::clicked, this, [this]() { moveSelected(-1); });
		connect(downBtn_, &QPushButton::clicked, this, [this]() { moveSelected(1); });
		connect(tiers_, &QListWidget::itemDoubleClicked, this,
			[this](QListWidgetItem *item) { editTier(tiers_->row(item)); });
		connect(tiers_, &QListWidget::currentRowChanged, this, [this](int row) {
			bool has = row >= 0;
			editBtn_->setEnabled(has);
			removeBtn_->setEnabled(has);
			upBtn_->setEnabled(has && row > 0);
			downBtn_->setEnabled(has && row < tiers_->count() - 1);
		});
		connect(currency_, &QComboBox::currentTextChanged, this, [](const QString &text) {
			std::string cur = text.toStdString();
			ConfigStore::instance().update([cur](DonationConfig &c) { c.currency = cur; });
		});
		connect(showAny_, &QCheckBox::toggled, this,
			[](bool on) { ConfigStore::instance().update([on](DonationConfig &c) { c.showAny = on; }); });
	}

	// --- Rotation ------------------------------------------------------------
	{
		auto *card = cardFrame();
		auto *cv = new QVBoxLayout(card);
		cv->setContentsMargins(14, 12, 14, 12);
		cv->setSpacing(10);
		auto *head = new QHBoxLayout();
		auto *col = new QVBoxLayout();
		col->setSpacing(2);
		auto *title = new QLabel(tr("Rotate amounts"));
		title->setProperty("role", "title");
		rotateSub_ = hintLabel(QString());
		col->addWidget(title);
		col->addWidget(rotateSub_);
		head->addLayout(col, 1);
		rotateSwitch_ = new ToggleSwitch(nullptr, 44, 24);
		head->addWidget(rotateSwitch_, 0, Qt::AlignVCenter);
		cv->addLayout(head);

		rotateBody_ = new QWidget();
		auto *rb = new QVBoxLayout(rotateBody_);
		rb->setContentsMargins(0, 0, 0, 0);
		rb->setSpacing(8);
		auto *grid = new QGridLayout();
		grid->setHorizontalSpacing(8);
		grid->setVerticalSpacing(4);
		grid->addWidget(hintLabel(tr("Show at a time")), 0, 0);
		grid->addWidget(hintLabel(tr("Change every")), 0, 1);
		perPage_ = new QSpinBox();
		perPage_->setRange(1, 4);
		perPage_->setSuffix(tr(" amounts"));
		seconds_ = new QSpinBox();
		seconds_->setRange(2, 3600);
		seconds_->setSuffix(tr(" sec"));
		grid->addWidget(perPage_, 1, 0);
		grid->addWidget(seconds_, 1, 1);
		rb->addLayout(grid);
		order_ = new QButtonGroup(this);
		rb->addWidget(segGroup(order_, {tr("List order"), tr("Shuffle")}));
		keepPopular_ = new QCheckBox(tr("Keep \"Popular\" amount in every set"));
		rb->addWidget(keepPopular_);
		auto *next = new QPushButton(tr("Next set now"));
		rb->addWidget(next);
		cv->addWidget(rotateBody_);
		v->addWidget(card);

		connect(rotateSwitch_, &QAbstractButton::toggled, this, [](bool on) {
			ConfigStore::instance().update([on](DonationConfig &c) { c.rotate.enabled = on; });
		});
		connect(perPage_, &QSpinBox::valueChanged, this, [](int n) {
			ConfigStore::instance().update([n](DonationConfig &c) { c.rotate.perPage = n; });
		});
		connect(seconds_, &QSpinBox::valueChanged, this, [](int n) {
			ConfigStore::instance().update([n](DonationConfig &c) { c.rotate.seconds = n; });
		});
		connect(order_, &QButtonGroup::idClicked, this, [](int id) {
			ConfigStore::instance().update(
				[id](DonationConfig &c) { c.rotate.order = id == 1 ? "shuffle" : "list"; });
		});
		connect(keepPopular_, &QCheckBox::toggled, this, [](bool on) {
			ConfigStore::instance().update([on](DonationConfig &c) { c.rotate.keepPopular = on; });
		});
		connect(next, &QPushButton::clicked, this,
			[]() { ConfigStore::instance().update([](DonationConfig &c) { c.nextSeq++; }); });
	}

	// --- Layout --------------------------------------------------------------
	{
		auto *box = new QVBoxLayout();
		box->setSpacing(6);
		box->addWidget(sectionLabel(tr("Layout on stream")));
		layout_ = new QButtonGroup(this);
		box->addWidget(segGroup(layout_, {tr("Card"), tr("Bar"), tr("Minimal")}));
		v->addLayout(box);
		connect(layout_, &QButtonGroup::idClicked, this, [](int id) {
			const char *names[] = {"card", "bar", "minimal"};
			std::string l = (id >= 0 && id < 3) ? names[id] : "card";
			ConfigStore::instance().update([l](DonationConfig &c) { c.layout = l; });
		});
	}

	// --- Spotlight -----------------------------------------------------------
	{
		auto *box = new QVBoxLayout();
		box->setSpacing(6);
		box->addWidget(sectionLabel(tr("Spotlight")));
		box->addWidget(hintLabel(tr("Pause rotation and enlarge one amount, e.g. when you mention it.")));
		auto *row = new QHBoxLayout();
		row->setSpacing(6);
		spotAmount_ = new QComboBox();
		spotDuration_ = new QComboBox();
		for (int s : {10, 20, 30, 60, 120})
			spotDuration_->addItem(tr("%1 s").arg(s), s);
		spotGo_ = new QPushButton(tr("Go"));
		spotGo_->setProperty("primary", true);
		row->addWidget(spotAmount_, 1);
		row->addWidget(spotDuration_);
		row->addWidget(spotGo_);
		box->addLayout(row);
		v->addLayout(box);
		connect(spotGo_, &QPushButton::clicked, this, [this]() {
			DonationConfig c = ConfigStore::instance().get();
			bool active = c.spotlightIndex >= 0 && c.spotlightUntil > ConfigStore::nowMs();
			if (active)
				ConfigStore::instance().update([](DonationConfig &cfg) {
					cfg.spotlightIndex = -1;
					cfg.spotlightUntil = 0;
				});
			else
				startSpotlight();
		});
	}

	v->addStretch(1);

	// --- Footer --------------------------------------------------------------
	auto *footer = new QFrame();
	footer->setStyleSheet("QFrame { background: rgba(255,255,255,8); border-top: 1px solid rgba(255,255,255,22); }");
	auto *fh = new QHBoxLayout(footer);
	fh->setContentsMargins(10, 8, 10, 8);
	fh->setSpacing(6);
	auto *showNow = new QPushButton(tr("Show now"));
	showNow->setToolTip(tr("Show the overlay now (for Cycle / Only-when-triggered visibility)"));
	auto *addScene = new QPushButton(tr("Add to scene"));
	addScene->setToolTip(tr("Add a Donation Options source to the current scene"));
	auto *style = new QPushButton(tr("Style..."));
	style->setProperty("ghost", true);
	fh->addWidget(showNow);
	fh->addWidget(addScene);
	fh->addStretch(1);
	fh->addWidget(style);
	outer->addWidget(footer);

	connect(showNow, &QPushButton::clicked, this, []() {
		ConfigStore::instance().update([](DonationConfig &c) {
			c.triggerUntil = ConfigStore::nowMs() + (int64_t)std::max(5, c.visibility.showSeconds) * 1000;
		});
	});
	connect(addScene, &QPushButton::clicked, this, [this]() { addSourceToScene(); });
	connect(style, &QPushButton::clicked, this, [this]() { openStyle(); });
}

void DonationDock::refreshFromStore()
{
	DonationConfig c = ConfigStore::instance().get();

	{
		QSignalBlocker b1(liveSwitch_), b2(currency_), b3(showAny_), b4(rotateSwitch_), b5(perPage_),
			b6(seconds_), b7(keepPopular_);
		liveSwitch_->setChecked(c.visible);
		setTextIfIdle(link_, c.link);
		setTextIfIdle(heading_, c.heading);
		setTextIfIdle(subtext_, c.subtext);
		QString cur = QString::fromStdString(c.currency);
		if (currency_->currentText() != cur && !currency_->hasFocus())
			currency_->setCurrentText(cur);
		showAny_->setChecked(c.showAny);
		rotateSwitch_->setChecked(c.rotate.enabled);
		perPage_->setValue(c.rotate.perPage);
		seconds_->setValue(c.rotate.seconds);
		keepPopular_->setChecked(c.rotate.keepPopular);
	}
	if (QAbstractButton *b = order_->button(c.rotate.order == "shuffle" ? 1 : 0))
		b->setChecked(true);
	int layoutId = c.layout == "bar" ? 1 : c.layout == "minimal" ? 2 : 0;
	if (QAbstractButton *b = layout_->button(layoutId))
		b->setChecked(true);

	rotateBody_->setVisible(c.rotate.enabled);
	int visible = (int)std::count_if(c.tiers.begin(), c.tiers.end(), [](const DonationTier &t) { return t.show; });
	int per = std::max(1, c.rotate.perPage);
	int sets = (visible + per - 1) / per;
	rotateSub_->setText(c.rotate.enabled ? tr("%1 amounts in %2 sets, every %3 s").arg(visible).arg(sets).arg(c.rotate.seconds)
					     : tr("Shows the first %1 amounts only").arg(per));

	std::string sig = tiersSignature(c);
	if (sig != tiersSignature_ || tiers_->count() != (int)c.tiers.size()) {
		tiersSignature_ = sig;
		rebuildTierList(c);
	}
	updateLiveCard(c);
	updateSpotlightUi(c);
}

void DonationDock::rebuildTierList(const DonationConfig &c)
{
	int keepRow = pendingRow_ >= 0 ? pendingRow_ : tiers_->currentRow();
	pendingRow_ = -1;
	int keepSpot = spotAmount_->currentData().isValid() ? spotAmount_->currentData().toInt() : -1;

	QSignalBlocker block(tiers_);
	tiers_->clear();
	QSignalBlocker blockSpot(spotAmount_);
	spotAmount_->clear();

	for (int i = 0; i < (int)c.tiers.size(); i++) {
		const DonationTier &t = c.tiers[i];
		QString amount = QString::fromStdString(ConfigStore::formatAmount(c, t.amount));
		QString label = QString::fromStdString(t.label);

		auto *row = new QWidget();
		row->setAttribute(Qt::WA_TranslucentBackground);
		auto *h = new QHBoxLayout(row);
		h->setContentsMargins(8, 4, 8, 4);
		h->setSpacing(8);
		auto *amt = new QLabel(amount);
		amt->setProperty("role", "amount");
		amt->setMinimumWidth(52);
		auto *col = new QVBoxLayout();
		col->setSpacing(0);
		auto *name = new QLabel(label.isEmpty() ? tr("(no label)") : label);
		QStringList notes;
		if (t.popular)
			notes << tr("Popular");
		if (!t.link.empty())
			notes << tr("Custom link");
		if (!t.show)
			notes << tr("Hidden on stream");
		auto *note = hintLabel(notes.join(DOT));
		note->setVisible(!notes.isEmpty());
		col->addWidget(name);
		col->addWidget(note);
		auto *sw = new ToggleSwitch(nullptr, 36, 20);
		sw->setChecked(t.show);
		sw->setToolTip(tr("Show this amount on stream"));
		h->addWidget(amt);
		h->addLayout(col, 1);
		h->addWidget(sw, 0, Qt::AlignVCenter);
		if (!t.show) {
			auto *fx = new QGraphicsOpacityEffect(row);
			fx->setOpacity(0.55);
			row->setGraphicsEffect(fx);
		}
		connect(sw, &QAbstractButton::toggled, this, [i](bool on) {
			ConfigStore::instance().update([i, on](DonationConfig &cfg) {
				if (i < (int)cfg.tiers.size())
					cfg.tiers[i].show = on;
			});
		});

		auto *item = new QListWidgetItem(tiers_);
		item->setSizeHint(QSize(0, ROW_HEIGHT));
		tiers_->setItemWidget(item, row);

		if (t.show)
			spotAmount_->addItem(amount + (label.isEmpty() ? QString() : DOT + label), i);
	}
	tiers_->setFixedHeight(std::max(1, (int)c.tiers.size()) * ROW_HEIGHT + 4);

	int sel = std::min(keepRow, tiers_->count() - 1);
	tiers_->setCurrentRow(sel);
	bool has = sel >= 0;
	editBtn_->setEnabled(has);
	removeBtn_->setEnabled(has);
	upBtn_->setEnabled(has && sel > 0);
	downBtn_->setEnabled(has && sel < tiers_->count() - 1);

	int spotIdx = spotAmount_->findData(keepSpot);
	if (spotIdx >= 0)
		spotAmount_->setCurrentIndex(spotIdx);
}

void DonationDock::updateLiveCard(const DonationConfig &c)
{
	liveCard_->setProperty("live", c.visible);
	repolish(liveCard_);
	int visible = (int)std::count_if(c.tiers.begin(), c.tiers.end(), [](const DonationTier &t) { return t.show; });
	QString layoutName = c.layout == "bar" ? tr("Bar") : c.layout == "minimal" ? tr("Minimal") : tr("Card");
	if (c.visible) {
		liveTitle_->setText(tr("Showing on stream"));
		QString what = c.rotate.enabled ? tr("Rotating %1 amounts").arg(visible) : tr("%1 amounts").arg(visible);
		QString mode;
		if (c.visibility.mode == "cycle")
			mode = DOT + tr("cycles");
		else if (c.visibility.mode == "triggered")
			mode = DOT + tr("on trigger");
		liveSub_->setText(what + DOT + tr("%1 layout").arg(layoutName) + mode);
	} else {
		liveTitle_->setText(tr("Hidden from stream"));
		liveSub_->setText(tr("Viewers can't see the overlay"));
	}
}

void DonationDock::updateSpotlightUi(const DonationConfig &c)
{
	int64_t now = ConfigStore::nowMs();
	bool active = c.spotlightIndex >= 0 && c.spotlightUntil > now;
	spotGo_->setText(active ? tr("Stop") : tr("Go"));
	spotAmount_->setEnabled(!active);
	spotDuration_->setEnabled(!active);
	spotGo_->setEnabled(active || spotAmount_->count() > 0);
	if (active)
		spotTimer_->start((int)std::min<int64_t>(c.spotlightUntil - now + 100, 3600 * 1000));
	else
		spotTimer_->stop();
}

void DonationDock::commitTexts()
{
	textDebounce_->stop();
	std::string link = link_->text().trimmed().toStdString();
	std::string heading = heading_->text().toStdString();
	std::string subtext = subtext_->text().toStdString();
	DonationConfig cur = ConfigStore::instance().get();
	if (cur.link == link && cur.heading == heading && cur.subtext == subtext)
		return;
	ConfigStore::instance().update([&](DonationConfig &c) {
		c.link = link;
		c.heading = heading;
		c.subtext = subtext;
	});
}

void DonationDock::addTier()
{
	DonationConfig c = ConfigStore::instance().get();
	AmountDialog dlg(this, QString::fromStdString(c.currency), nullptr);
	if (dlg.exec() != QDialog::Accepted)
		return;
	DonationTier t = dlg.tier();
	pendingRow_ = (int)c.tiers.size();
	ConfigStore::instance().update([&t](DonationConfig &cfg) {
		if (t.popular)
			for (auto &o : cfg.tiers)
				o.popular = false;
		cfg.tiers.push_back(t);
	});
}

void DonationDock::editTier(int index)
{
	DonationConfig c = ConfigStore::instance().get();
	if (index < 0 || index >= (int)c.tiers.size())
		return;
	AmountDialog dlg(this, QString::fromStdString(c.currency), &c.tiers[index]);
	if (dlg.exec() != QDialog::Accepted)
		return;
	DonationTier t = dlg.tier();
	pendingRow_ = index;
	ConfigStore::instance().update([&t, index](DonationConfig &cfg) {
		if (index >= (int)cfg.tiers.size())
			return;
		if (t.popular)
			for (auto &o : cfg.tiers)
				o.popular = false;
		cfg.tiers[index] = t;
	});
}

void DonationDock::removeSelected()
{
	int row = tiers_->currentRow();
	if (row < 0)
		return;
	pendingRow_ = std::max(0, row - 1);
	ConfigStore::instance().update([row](DonationConfig &cfg) {
		if (row >= (int)cfg.tiers.size())
			return;
		cfg.tiers.erase(cfg.tiers.begin() + row);
		if (cfg.spotlightIndex == row) {
			cfg.spotlightIndex = -1;
			cfg.spotlightUntil = 0;
		} else if (cfg.spotlightIndex > row) {
			cfg.spotlightIndex--;
		}
	});
}

void DonationDock::moveSelected(int delta)
{
	int row = tiers_->currentRow();
	int target = row + delta;
	DonationConfig c = ConfigStore::instance().get();
	if (row < 0 || target < 0 || target >= (int)c.tiers.size())
		return;
	pendingRow_ = target;
	ConfigStore::instance().update([row, target](DonationConfig &cfg) {
		if (row >= (int)cfg.tiers.size() || target >= (int)cfg.tiers.size())
			return;
		std::swap(cfg.tiers[row], cfg.tiers[target]);
		if (cfg.spotlightIndex == row)
			cfg.spotlightIndex = target;
		else if (cfg.spotlightIndex == target)
			cfg.spotlightIndex = row;
	});
}

void DonationDock::startSpotlight()
{
	if (spotAmount_->count() == 0)
		return;
	int index = spotAmount_->currentData().toInt();
	int secs = spotDuration_->currentData().toInt();
	ConfigStore::instance().update([index, secs](DonationConfig &c) {
		c.spotlightIndex = index;
		c.spotlightUntil = ConfigStore::nowMs() + (int64_t)secs * 1000;
	});
}

void DonationDock::addSourceToScene()
{
	obs_source_t *sceneSource = obs_frontend_get_current_scene();
	if (!sceneSource)
		return;
	obs_scene_t *scene = obs_scene_from_source(sceneSource);

	std::string base = obs_module_text("DonationOptions.Source");
	std::string name = base;
	for (int n = 2; n < 100; n++) {
		obs_source_t *existing = obs_get_source_by_name(name.c_str());
		if (!existing)
			break;
		obs_source_release(existing);
		name = base + " " + std::to_string(n);
	}

	obs_source_t *src = obs_source_create(DONATION_SOURCE_ID, name.c_str(), nullptr, nullptr);
	if (src && scene)
		obs_scene_add(scene, src);
	if (src)
		obs_source_release(src);
	obs_source_release(sceneSource);
}

void DonationDock::openStyle()
{
	obs_source_t *src = findDonationSource();
	if (!src) {
		addSourceToScene();
		src = findDonationSource();
	}
	if (src) {
		obs_frontend_open_source_properties(src);
		obs_source_release(src);
	}
}
