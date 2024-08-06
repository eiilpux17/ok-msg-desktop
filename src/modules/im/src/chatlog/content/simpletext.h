#ifndef SIMPLETEXT_H
#define SIMPLETEXT_H

#include "../chatlinecontent.h"
#include "src/lib/settings/style.h"

#include <QFont>

class SimpleText : public ChatLineContent {
    Q_OBJECT

public:
    SimpleText(const QString &txt = "", const QFont &font = QFont());
    ~SimpleText() {
    }
    
    void setText(const QString &txt);
    void setAlignment(Qt::Alignment align);
    QSizeF sizeHint() const;

    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;
    void setWidth(qreal width) override;
    void setHeight(qreal height);
    void setSize(QSizeF size);

    void setColor(Style::ColorPalette role);
    void setColor(const QColor& color);

    void setFont(const QFont &font);

private:
    void updateBoundingRect();
    void reloadTheme() override;

private:
    QString text;
    QString _elidedText;
    QSizeF boundSize;
    mutable QSizeF _sh;
    QFont defFont;
    QColor color;
    Style::ColorPalette colorRole = Style::MainText;
    bool customColor = false;
    qreal forceWidth = -1;
    qreal forceHeight = -1;
    Qt::Alignment alignment = Qt::AlignLeft | Qt::AlignVCenter;
};

#endif  // !SIMPLETEXT_H
