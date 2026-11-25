#pragma once

#include <QAbstractButton>
#include <QPainter>
#include <QLinearGradient>
#include <QPainterPath>

// Gold pill switch used throughout the dock.
class ToggleSwitch : public QAbstractButton {
public:
	explicit ToggleSwitch(QWidget *parent = nullptr, int w = 44, int h = 24) : QAbstractButton(parent), w_(w), h_(h)
	{
		setCheckable(true);
		setCursor(Qt::PointingHandCursor);
		setFixedSize(w_, h_);
		setFocusPolicy(Qt::StrongFocus);
	}

	QSize sizeHint() const override { return QSize(w_, h_); }

protected:
	void paintEvent(QPaintEvent *) override
	{
		QPainter p(this);
		p.setRenderHint(QPainter::Antialiasing);
		QRectF r(0.5, 0.5, width() - 1.0, height() - 1.0);
		qreal radius = r.height() / 2.0;

		if (isChecked()) {
			QLinearGradient g(r.topLeft(), r.bottomLeft());
			g.setColorAt(0, QColor("#FBE08A"));
			g.setColorAt(1, QColor("#D49C22"));
			p.setBrush(g);
			p.setPen(QPen(QColor(255, 240, 190, 170), 1));
		} else {
			p.setBrush(QColor(255, 255, 255, 26));
			p.setPen(QPen(QColor(255, 255, 255, 50), 1));
		}
		if (!isEnabled())
			p.setOpacity(0.45);
		p.drawRoundedRect(r, radius, radius);

		qreal pad = 3.0;
		qreal d = r.height() - pad * 2;
		qreal x = isChecked() ? r.right() - pad - d : r.left() + pad;
		QRectF knob(x, r.top() + pad, d, d);
		QLinearGradient kg(knob.topLeft(), knob.bottomLeft());
		kg.setColorAt(0, QColor("#FFFFFF"));
		kg.setColorAt(1, QColor("#E8E2D2"));
		p.setPen(Qt::NoPen);
		p.setBrush(QColor(0, 0, 0, 70));
		p.drawEllipse(knob.translated(0, 1));
		p.setBrush(kg);
		p.drawEllipse(knob);

		if (hasFocus()) {
			p.setBrush(Qt::NoBrush);
			p.setPen(QPen(QColor(233, 185, 73, 200), 1.5));
			p.drawRoundedRect(r.adjusted(-0.5, -0.5, 0.5, 0.5), radius, radius);
		}
	}

private:
	int w_, h_;
};
