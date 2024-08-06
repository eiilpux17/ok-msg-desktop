#include "simpletext.h"
#include <QPainter>
#include <QtMath>

static constexpr int text_frame = 1;

SimpleText::SimpleText(const QString& txt, const QFont& font) : text(txt), defFont(font) {
    color = Style::getColor(colorRole);
    updateBoundingRect();
}

void SimpleText::setText(const QString& txt) {
    if (text != txt) {
        text = txt;
        _sh = QSizeF();
        updateBoundingRect();
    }
}

void SimpleText::setAlignment(Qt::Alignment align) {
    if (alignment != align) {
        alignment = align;
        update();
    }
}

QSizeF SimpleText::sizeHint() const {
    QFontMetricsF fm(defFont);
    if (_sh.isEmpty()) {
        // 细调一下
        qreal h_margins = text_frame * 2 + 2;
        if (!text.isEmpty())
            _sh = fm.boundingRect(text).size();
        else
            _sh = QSizeF(fm.height(), fm.height());
        _sh.rwidth() = qCeil(_sh.width() + h_margins);
    }
    return _sh;
}

QRectF SimpleText::boundingRect() const { return QRectF(QPointF(0, 0), boundSize); }

void SimpleText::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) {
    const QString& paint_text = _elidedText.isEmpty() ? text : _elidedText;
    if (!paint_text.isEmpty()) {
        int y_offset = QFontMetricsF(defFont).leading() / 2.0;
        QRectF br = boundingRect();
        br.adjust(text_frame + 1, y_offset, -text_frame, y_offset);
        painter->save();
        painter->setFont(defFont);
        painter->setPen(QPen(color));
        painter->drawText(br, alignment, paint_text);
        painter->restore();
    }
}

void SimpleText::setWidth(qreal width) {
    if ((width <= 0 && forceWidth > 0) || (width >= 0 && forceWidth != width)) {
        forceWidth = width <= 0 ? -1 : width;
        updateBoundingRect();
    }
}

void SimpleText::setHeight(qreal height) {
    if ((height <= 0 && forceHeight > 0) || (height >= 0 && forceHeight != height)) {
        forceHeight = height <= 0 ? -1 : height;
        updateBoundingRect();
    }
}

void SimpleText::setSize(QSizeF size) {
    bool update_rect = false;
    if ((size.height() <= 0 && forceHeight > 0) ||
        (size.height() >= 0 && forceHeight != size.height())) {
        forceHeight = size.height() <= 0 ? -1 : size.height();
        update_rect = true;
    }

    if ((size.width() <= 0 && forceWidth > 0) ||
        (size.width() >= 0 && forceWidth != size.width())) {
        forceWidth = size.width() <= 0 ? -1 : size.width();
        update_rect = true;
    }
    if (update_rect) {
        updateBoundingRect();
    }
}

void SimpleText::setColor(Style::ColorPalette role) {
    customColor = false;
    colorRole = role;
    color = Style::getColor(colorRole);
}

void SimpleText::setColor(const QColor& color) {
    customColor = true;
    this->color = color;
}

void SimpleText::setFont(const QFont& font) {
    if (defFont != font) {
        defFont = font;
        _sh = QSizeF();
        updateBoundingRect();
    }
}

void SimpleText::updateBoundingRect() {
    prepareGeometryChange();
    boundSize = sizeHint();
    if (forceHeight > 0) {
        boundSize.rheight() = forceHeight;
    }
    if (forceWidth > 0) {
        boundSize.rwidth() = forceWidth;
        qreal h_margins = text_frame * 2;
        QFontMetricsF fm(defFont);
        if (fm.size(0, text).width() > forceWidth - h_margins) {
            _elidedText = fm.elidedText(text, Qt::ElideRight, forceWidth - h_margins);
            return;
        }
    }
    _elidedText = QString();
}

void SimpleText::reloadTheme() {
    if (!customColor) {
        color = Style::getColor(colorRole);
    }
}
