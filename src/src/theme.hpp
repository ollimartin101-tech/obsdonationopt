#pragma once

// Dark "liquid glass" + gold style for the dock and dialogs.
// Qt widgets can't blur what's behind them, so the glass is suggested with
// translucent gradients, light top edges and gold glows.
static const char *DONATION_STYLESHEET = R"QSS(
QWidget#DonationDockRoot, QDialog#DonationDialog {
	background: qlineargradient(x1:0, y1:0, x2:0.6, y2:1, stop:0 #1A160D, stop:0.35 #0E0D10, stop:1 #0A0A0C);
	color: #F4EFE3;
	font-size: 10pt;
}
QWidget#DonationContent { background: transparent; }
QScrollArea { background: transparent; border: none; }
QScrollArea > QWidget > QWidget { background: transparent; }

QLabel { color: #F4EFE3; background: transparent; }
QLabel[role="section"] { color: #CDB57A; font-size: 8pt; font-weight: 700; }
QLabel[role="hint"] { color: #B5AE9E; font-size: 9pt; }
QLabel[role="title"] { font-size: 11pt; font-weight: 700; }
QLabel[role="amount"] { color: #F2C75C; font-size: 11pt; font-weight: 800; }

QFrame[card="true"] {
	background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 rgba(255,255,255,26), stop:1 rgba(255,255,255,8));
	border: 1px solid rgba(255,255,255,32);
	border-top-color: rgba(255,255,255,60);
	border-radius: 14px;
}
QFrame[card="true"][live="true"] {
	border: 1px solid rgba(233,185,73,150);
	background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 rgba(233,185,73,40), stop:1 rgba(255,255,255,8));
}

QLineEdit, QComboBox, QSpinBox, QDoubleSpinBox {
	background: rgba(0,0,0,95);
	border: 1px solid rgba(255,255,255,32);
	border-radius: 8px;
	padding: 5px 8px;
	color: #F4EFE3;
	selection-background-color: #E9B949;
	selection-color: #1C1404;
	min-height: 20px;
}
QLineEdit:focus, QComboBox:focus, QSpinBox:focus, QDoubleSpinBox:focus { border: 1px solid #E9B949; }
QLineEdit:disabled { color: #7A7466; }
QComboBox::drop-down { border: none; width: 18px; }
QComboBox QAbstractItemView {
	background: #16140F; color: #F4EFE3; border: 1px solid rgba(233,185,73,90);
	selection-background-color: #E9B949; selection-color: #1C1404; outline: none;
}
QSpinBox::up-button, QSpinBox::down-button, QDoubleSpinBox::up-button, QDoubleSpinBox::down-button { width: 14px; border: none; }

QPushButton {
	background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 rgba(255,255,255,34), stop:1 rgba(255,255,255,10));
	border: 1px solid rgba(255,255,255,40);
	border-top-color: rgba(255,255,255,70);
	border-radius: 8px;
	padding: 6px 12px;
	color: #F4EFE3;
}
QPushButton:hover { border-color: rgba(233,185,73,170); }
QPushButton:pressed { background: rgba(255,255,255,12); }
QPushButton:disabled { color: #6C665A; border-color: rgba(255,255,255,20); }
QPushButton[primary="true"] {
	background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #FBE08A, stop:0.5 #EDBE4C, stop:1 #D49C22);
	color: #1C1404;
	border: 1px solid rgba(255,240,190,190);
	font-weight: 700;
}
QPushButton[primary="true"]:hover { background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #FFE9A3, stop:1 #DDA62E); }
QPushButton[ghost="true"] { background: transparent; border: none; color: #F2C75C; font-weight: 600; }
QPushButton[ghost="true"]:hover { color: #FADF94; }

QFrame#SegGroup { background: rgba(0,0,0,95); border: 1px solid rgba(255,255,255,26); border-radius: 10px; }
QPushButton[seg="true"] { background: transparent; border: none; color: #B5AE9E; font-weight: 600; padding: 6px 4px; border-radius: 7px; }
QPushButton[seg="true"]:hover { color: #F4EFE3; }
QPushButton[seg="true"]:checked {
	background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #FBE08A, stop:1 #D49C22);
	color: #1C1404;
}

QCheckBox { color: #E2DCCD; spacing: 8px; background: transparent; }
QCheckBox::indicator { width: 16px; height: 16px; border-radius: 5px; border: 1px solid rgba(255,255,255,70); background: rgba(0,0,0,95); }
QCheckBox::indicator:checked { background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #FBE08A, stop:1 #D49C22); border: 1px solid #FBE08A; }

QListWidget { background: transparent; border: none; outline: none; }
QListWidget::item { border-bottom: 1px solid rgba(255,255,255,18); }
QListWidget::item:selected { background: rgba(233,185,73,38); }

QScrollBar:vertical { background: transparent; width: 8px; margin: 2px; }
QScrollBar::handle:vertical { background: rgba(233,185,73,90); border-radius: 3px; min-height: 30px; }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }
QToolTip { background: #16140F; color: #F4EFE3; border: 1px solid rgba(233,185,73,120); }
)QSS";
