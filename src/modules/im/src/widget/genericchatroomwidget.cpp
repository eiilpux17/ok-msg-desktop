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

#include "genericchatroomwidget.h"
#include <QBoxLayout>
#include <QMouseEvent>
#include "lib/storage/settings/style.h"
#include "lib/ui/widget/tools/CroppingLabel.h"
#include "lib/ui/widget/tools/RoundedPixmapLabel.h"
namespace module::im {

GenericChatroomWidget::GenericChatroomWidget(lib::messenger::ChatType type, const ContactId& cid, QWidget* parent)
        : GenericChatItemWidget(type, cid, parent), contactId(cid) {
    // setAutoFillBackground(true);


    mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(10);


    textLayout = new QVBoxLayout(this);
    textLayout->setSpacing(0);
    textLayout->setMargin(0);
    textLayout->addStretch();
    textLayout->addWidget(nameLabel);
    textLayout->addWidget(lastMessageLabel);
    textLayout->addStretch();

    avatar->setContentsSize(QSize(40, 40));
    mainLayout->addWidget(avatar);

    mainLayout->addLayout(textLayout);

    if (statusPic.has_value()) {
        mainLayout->addWidget(statusPic.value());
    }


    mainLayout->activate();
    setLayout(mainLayout);
    reloadTheme();
}

GenericChatroomWidget::~GenericChatroomWidget() {}

bool GenericChatroomWidget::eventFilter(QObject*, QEvent*) {
    return true;  // Disable all events.
}

void GenericChatroomWidget::setName(const QString& name) {
    GenericChatItemWidget::setName(name);
    nameLabel->setText(name);
}

void GenericChatroomWidget::setStatusMsg(const QString& status) {
    //    statusMessageLabel->setText(status);
}

QString GenericChatroomWidget::getStatusMsg() const {
    //    return statusMessageLabel->text();
    return QString{};
}

QString GenericChatroomWidget::getTitle() const {
    QString title = getName();

    if (!getStatusString().isNull())
        title += QStringLiteral(" (") + getStatusString() + QStringLiteral(")");

    return title;
}

void GenericChatroomWidget::reloadTheme() {
    // QPalette p;

            //    p = statusMessageLabel->palette();
            //    p.setColor(QPalette::WindowText, Style::getColor(Style::GroundExtra));       // Base color
            //    p.setColor(QPalette::HighlightedText, Style::getColor(Style::StatusActive)); // Color when
            //    active statusMessageLabel->setPalette(p);

            // p = nameLabel->palette();
            // p.setColor(QPalette::WindowText,
            //            lib::settings::Style::getColor(
            //                    lib::settings::Style::ColorPalette::MainText));  // Base color
            // p.setColor(QPalette::HighlightedText,
            //            lib::settings::Style::getColor(
            //                    lib::settings::Style::ColorPalette::NameActive));  // Color when active
            // nameLabel->setPalette(p);

            // p = palette();
            // p.setColor(QPalette::Window,
            //            lib::settings::Style::getColor(
            //                    lib::settings::Style::ColorPalette::ThemeMedium));  // Base background color
            // p.setColor(QPalette::Highlight,
            //            lib::settings::Style::getColor(
            //                    lib::settings::Style::ColorPalette::ThemeHighlight));  // On mouse over
            // p.setColor(QPalette::Light,
            //            lib::settings::Style::getColor(
            //                    lib::settings::Style::ColorPalette::ThemeLight));  // When active
            // setPalette(p);
}

void GenericChatroomWidget::activate() {
    emit chatroomWidgetClicked(this);
}

void GenericChatroomWidget::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        emit chatroomWidgetClicked(this);
    } else if (event->button() == Qt::MiddleButton) {
        emit middleMouseClicked();
    } else {
        event->ignore();
    }
}

void GenericChatroomWidget::enterEvent(QEvent* event) {
    if (!active) setBackgroundRole(QPalette::Light);
    QWidget::enterEvent(event);
}

void GenericChatroomWidget::leaveEvent(QEvent* event) {
    if (!active) setBackgroundRole(QPalette::Window);
    QWidget::leaveEvent(event);
}
}  // namespace module::im
