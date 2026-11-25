#pragma once

#include "config-store.hpp"

#include <QDialog>

class QDoubleSpinBox;
class QLineEdit;
class QComboBox;
class QCheckBox;
class QButtonGroup;
class QLabel;

// "Add amount" / "Edit amount" dialog.
class AmountDialog : public QDialog {
public:
	AmountDialog(QWidget *parent, const QString &currency, const DonationTier *existing);
	DonationTier tier() const;

private:
	void updatePreview();

	QString currency_;
	QDoubleSpinBox *amount_ = nullptr;
	QLineEdit *label_ = nullptr;
	QComboBox *linkMode_ = nullptr;
	QLineEdit *customLink_ = nullptr;
	QButtonGroup *icons_ = nullptr;
	QCheckBox *show_ = nullptr;
	QCheckBox *popular_ = nullptr;
	QLabel *previewAmount_ = nullptr;
	QLabel *previewLabel_ = nullptr;
};
