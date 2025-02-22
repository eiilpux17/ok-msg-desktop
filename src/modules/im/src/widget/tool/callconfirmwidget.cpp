/*
 * Copyright (c) 2022 船山信息 chuanshaninfo.com
 * The project is licensed under Mulan PubL v2.
 * You can use this software according to the terms and conditions of the Mulan
 * PubL v2. You may obtain a copy of Mulan PubL v2 at:
 *          http://license.coscl.org.cn/MulanPubL-2.0
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PubL v2 for more details.
 */

#include "callconfirmwidget.h"
#include <assert.h>
#include <QDialogButtonBox>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPaintEvent>
#include <QPainter>
#include <QPalette>
#include <QPushButton>
#include <QRect>
#include <QVBoxLayout>
#include "base/widgets.h"
#include "src/lib/storage/settings/style.h"
#include "src/model/friendlist.h"
#include "src/widget/widget.h"

/**
 * @class CallConfirmWidget
 * @brief This is a widget with dialog buttons to accept/reject a call
 *
 * It tracks the position of another widget called the anchor
 * and looks like a bubble at the bottom of that widget.
 *
 * @var const QWidget* CallConfirmWidget::anchor
 * @brief The widget we're going to be tracking
 *
 * @var const int CallConfirmWidget::roundedFactor
 * @brief By how much are the corners of the main rect rounded
 *
 * @var const qreal CallConfirmWidget::rectRatio
 * @brief Used to correct the rounding factors on non-square rects
 */

CallConfirmWidget::CallConfirmWidget(const ToxPeer& from, bool video, QWidget* parent)
        : QWidget(parent)
        , from(from)
        , isVideo(video)
        , rectW{320}
        , rectH{182}
        , spikeW{30}
        , spikeH{15}
        , roundedFactor{20}
        , rectRatio(static_cast<qreal>(rectH) / static_cast<qreal>(rectW)) {
    setWindowFlags(Qt::ToolTip | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_DeleteOnClose);

    QVBoxLayout* layout = new QVBoxLayout(this);

    auto frd = Core::getInstance()->getFriendList().findFriend(from);
    auto avt = new QLabel(this);
    avt->setPixmap(frd->getAvatar());
    avt->setFixedSize(120, 120);
    avt->setAlignment(Qt::AlignHCenter);
    layout->addWidget(avt);

    QLabel* callLabel = new QLabel(QObject::tr("Incoming call..."), this);
    callLabel->setAlignment(Qt::AlignHCenter);
    // callLabel->setToolTip(callLabel->text());

    // Note: At the moment this may not work properly. For languages written
    // from right to left, there is no translation for the phrase "Incoming call...".
    // In this situation, the phrase "Incoming call..." looks as "...oming call..."
    Qt::TextElideMode elideMode = (QGuiApplication::layoutDirection() == Qt::LeftToRight)
                                          ? Qt::ElideRight
                                          : Qt::ElideLeft;
    int marginSize = 12;
    QFontMetrics fontMetrics(callLabel->font());
    QString elidedText =
            fontMetrics.elidedText(callLabel->text(), elideMode, rectW - marginSize * 2 - 4);
    callLabel->setText(elidedText);

    QDialogButtonBox* buttonBox = new QDialogButtonBox(Qt::Horizontal, this);
    QPushButton *accept = new QPushButton(this), *reject = new QPushButton(this);
    accept->setFlat(true);
    reject->setFlat(true);
    accept->setStyleSheet("QPushButton{border:none;}");
    reject->setStyleSheet("QPushButton{border:none;}");
    accept->setIcon(QIcon(lib::settings::Style::getImagePath("acceptCall/acceptCall.svg")));
    reject->setIcon(QIcon(lib::settings::Style::getImagePath("rejectCall/rejectCall.svg")));
    accept->setIconSize(accept->size());
    reject->setIconSize(reject->size());

    buttonBox->addButton(accept, QDialogButtonBox::AcceptRole);
    buttonBox->addButton(reject, QDialogButtonBox::RejectRole);
    for (auto* b : buttonBox->buttons()) b->setCursor(Qt::PointingHandCursor);

    connect(buttonBox, &QDialogButtonBox::accepted, this, &CallConfirmWidget::accepted);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &CallConfirmWidget::rejected);

    layout->setMargin(marginSize);
    layout->addSpacing(spikeH);
    layout->addWidget(callLabel);

    auto ctrlLayout = new QHBoxLayout(this);
    ctrlLayout->addStretch(1);
    ctrlLayout->addWidget(buttonBox);
    ctrlLayout->addStretch(1);

    layout->addLayout(ctrlLayout);

    ok::base::Widgets::moveToScreenCenter(this);
    show();
}

void CallConfirmWidget::reposition() {
    update();
}

void CallConfirmWidget::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(brush);
    painter.setPen(Qt::NoPen);
    painter.drawRoundedRect(mainRect, roundedFactor * rectRatio, roundedFactor, Qt::RelativeSize);
    painter.drawPolygon(spikePoly);
}

void CallConfirmWidget::showEvent(QShowEvent*) {
    update();
}

void CallConfirmWidget::hideEvent(QHideEvent*) {
    if (parentWidget()) parentWidget()->removeEventFilter(this);
}

bool CallConfirmWidget::eventFilter(QObject*, QEvent* event) {
    if (event->type() == QEvent::Resize) reposition();

    return false;
}
