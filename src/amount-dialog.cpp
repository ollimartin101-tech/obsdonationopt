#include "amount-dialog.hpp"
#include "theme.hpp"

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace {
const char *ICON_IDS[] = {"none", "heart", "cup", "star", "gift"};
const char *ICON_NAMES[] = {"None", "Heart", "Cup", "Star", "Gift"};
constexpr int ICON_COUNT = 5;

QLabel *sectionLabel(const QString &text)
{
	auto *l = new QLabel(text.toUpper());
	l->setProperty("role", "section");
	return l;
}
} // namespace

AmountDialog::AmountDialog(QWidget *parent, const QString &currency, const DonationTier *existing)
	: QDialog(parent),
	  currency_(currency)
{
	setObjectName("DonationDialog");
	setStyleSheet(DONATION_STYLESHEET);
	setWindowTitle(existing ? tr("Edit amount") : tr("Add amount"));
	setMinimumWidth(440);

	auto *root = new QVBoxLayout(this);
	root->setContentsMargins(20, 18, 20, 18);
	root->setSpacing(14);

	auto *grid = new QGridLayout();
	grid->setHorizontalSpacing(12);
	grid->setVerticalSpacing(6);
	grid->addWidget(new QLabel(tr("Amount")), 0, 0);
	grid->addWidget(new QLabel(tr("Label (optional)")), 0, 1);
	amount_ = new QDoubleSpinBox();
	amount_->setRange(0.01, 1000000.0);
	amount_->setDecimals(2);
	amount_->setPrefix(currency_);
	amount_->setValue(existing ? existing->amount : 5.0);
	label_ = new QLineEdit(existing ? QString::fromStdString(existing->label) : QString());
	label_->setPlaceholderText(tr("e.g. Buy me a coffee"));
	grid->addWidget(amount_, 1, 0);
	grid->addWidget(label_, 1, 1);
	grid->setColumnStretch(1, 1);
	root->addLayout(grid);

	root->addWidget(new QLabel(tr("Link for this amount")));
	linkMode_ = new QComboBox();
	linkMode_->addItem(tr("Use main donation link"));
	linkMode_->addItem(tr("Custom link..."));
	customLink_ = new QLineEdit();
	customLink_->setPlaceholderText(tr("Link with this amount pre-filled"));
	bool hasCustom = existing && !existing->link.empty();
	linkMode_->setCurrentIndex(hasCustom ? 1 : 0);
	customLink_->setText(hasCustom ? QString::fromStdString(existing->link) : QString());
	customLink_->setVisible(hasCustom);
	root->addWidget(linkMode_);
	root->addWidget(customLink_);
	auto *hint = new QLabel(tr("Shown on stream while this amount is spotlighted."));
	hint->setProperty("role", "hint");
	root->addWidget(hint);
	connect(linkMode_, &QComboBox::currentIndexChanged, this,
		[this](int i) { customLink_->setVisible(i == 1); });

	root->addWidget(new QLabel(tr("Icon")));
	auto *iconFrame = new QFrame();
	iconFrame->setObjectName("SegGroup");
	auto *iconRow = new QHBoxLayout(iconFrame);
	iconRow->setContentsMargins(4, 4, 4, 4);
	iconRow->setSpacing(4);
	icons_ = new QButtonGroup(this);
	icons_->setExclusive(true);
	QString current = existing ? QString::fromStdString(existing->icon) : QStringLiteral("none");
	for (int i = 0; i < ICON_COUNT; i++) {
		auto *b = new QPushButton(tr(ICON_NAMES[i]));
		b->setCheckable(true);
		b->setProperty("seg", true);
		b->setChecked(current == ICON_IDS[i]);
		icons_->addButton(b, i);
		iconRow->addWidget(b);
	}
	if (!icons_->checkedButton())
		icons_->button(0)->setChecked(true);
	root->addWidget(iconFrame);

	show_ = new QCheckBox(tr("Show on stream"));
	show_->setChecked(existing ? existing->show : true);
	popular_ = new QCheckBox(tr("Mark as \"Popular\""));
	popular_->setChecked(existing ? existing->popular : false);
	root->addWidget(show_);
	root->addWidget(popular_);

	root->addWidget(sectionLabel(tr("Preview")));
	auto *preview = new QFrame();
	preview->setProperty("card", true);
	auto *pv = new QHBoxLayout(preview);
	pv->setContentsMargins(14, 10, 14, 10);
	pv->setSpacing(10);
	previewAmount_ = new QLabel();
	previewAmount_->setProperty("role", "amount");
	previewLabel_ = new QLabel();
	pv->addWidget(previewAmount_);
	pv->addWidget(previewLabel_, 1);
	root->addWidget(preview);

	auto *buttons = new QHBoxLayout();
	buttons->addStretch(1);
	auto *cancel = new QPushButton(tr("Cancel"));
	auto *save = new QPushButton(existing ? tr("Save") : tr("Add amount"));
	save->setProperty("primary", true);
	save->setDefault(true);
	buttons->addWidget(cancel);
	buttons->addWidget(save);
	root->addLayout(buttons);
	connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
	connect(save, &QPushButton::clicked, this, &QDialog::accept);

	connect(amount_, &QDoubleSpinBox::valueChanged, this, [this](double) { updatePreview(); });
	connect(label_, &QLineEdit::textChanged, this, [this](const QString &) { updatePreview(); });
	updatePreview();
}

void AmountDialog::updatePreview()
{
	DonationConfig tmp;
	tmp.currency = currency_.toStdString();
	previewAmount_->setText(QString::fromStdString(ConfigStore::formatAmount(tmp, amount_->value())));
	previewLabel_->setText(label_->text());
}

DonationTier AmountDialog::tier() const
{
	DonationTier t;
	t.amount = amount_->value();
	t.label = label_->text().trimmed().toStdString();
	t.link = linkMode_->currentIndex() == 1 ? customLink_->text().trimmed().toStdString() : std::string();
	int id = icons_->checkedId();
	t.icon = (id >= 0 && id < ICON_COUNT) ? ICON_IDS[id] : "none";
	t.show = show_->isChecked();
	t.popular = popular_->isChecked();
	return t;
}
