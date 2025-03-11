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

#include "aboutfriendform.h"
#include "lib/ui/widget/tools/RoundedPixmapLabel.h"
#include "ui_aboutfriendform.h"

#include <QFileDialog>

#include <QAbstractButton>
#include <QCheckBox>
#include <QPainter>
#include <QPushButton>
#include <QStyledItemDelegate>
#include <QSvgRenderer>


#include "lib/ui/gui.h"
#include "src/core/core.h"
#include "src/lib/storage/settings/style.h"
#include "src/nexus.h"
#include "src/persistence/settings.h"
#include "src/widget/widget.h"
#include "src/persistence/profile.h"
#include "src/model/Friend.h"


namespace module::im {

AboutFriendForm::AboutFriendForm(const Friend* fw, QWidget* parent)
        : QWidget(parent)
        , ui(new Ui::AboutFriendForm)
        , m_friend{fw}
        , widget{Widget::getInstance()}
        , profile{Nexus::getInstance()->getProfile()} {
    ui->setupUi(this);

    setAttribute(Qt::WA_StyledBackground);

    const auto af = new AboutFriend(m_friend, profile->getSettings());
    about = std::unique_ptr<IAboutFriend>(af);

    connect(ui->sendMessage, &QPushButton::clicked, this,
            &AboutFriendForm::onSendMessageClicked);

    connect(ui->removeHistory, &QPushButton::clicked, this,
            &AboutFriendForm::onRemoveHistoryClicked);

    ui->id->setText(about->getPublicKey().toString());
    ui->statusMessage->setText(about->getStatusMessage());

    realAvatar = new lib::ui::RoundedPixmapLabel(this);
    realAvatar->setFixedSize(QSize(60, 60));
    realAvatar->setPixmap(about->getAvatar());
    ui->avatarLayout->replaceWidget(ui->avatar, realAvatar);
    ui->avatar->deleteLater();
    ui->avatar = nullptr;

    auto f = about->getFriend();
    connect(f, &Friend::avatarChanged, this,
            [&](const QPixmap& pixmap) { ui->avatar->setPixmap(pixmap); });

    ui->userName->setText(about->getName());
    connect(f, &Friend::nameChanged, this, [&](auto name) {
        ui->userName->setText(name);
        ui->alias->setPlaceholderText(name);
    });

    ui->alias->setText(about->getAlias());
    ui->alias->setPlaceholderText(about->getName());
    connect(f, &Contact::aliasChanged, this,
            [&](const auto& alias) { ui->alias->setText(alias); });

    connect(ui->alias, &QLineEdit::textChanged, this, &AboutFriendForm::onAliasChanged);

    connect(&lib::ui::GUI::getInstance(), &lib::ui::GUI::themeApplyRequest, this,
            &AboutFriendForm::reloadTheme);

    reloadTheme();
}

/**
 * @brief Called when user clicks the bottom OK button, save all settings
 */
void AboutFriendForm::onSendMessageClicked() {
    auto w = Widget::getInstance();
    emit w->toSendMessage(ui->id->text());
}

void AboutFriendForm::onRemoveHistoryClicked() {
    const bool retYes = lib::ui::GUI::askQuestion(
            tr("Confirmation"), tr("Are you sure to remove %1 chat history?").arg(about->getName()),
            /* defaultAns = */ false, /* warning = */ true, /* yesno = */ true);
    if (!retYes) {
        return;
    }

    auto w = Widget::getInstance();
    emit w->toClearHistory(ui->id->text());

    //   const bool result = about->clearHistory();
    //    if (!result) {
    //        GUI::showWarning(tr("History removed"),
    //                         tr("Failed to remove chat history with
    //                         %1!").arg(about->getName()).toHtmlEscaped());
    //        return;
    //    }
    //    emit histroyRemoved();
    //    ui->removeHistory->setEnabled(false); // For know clearly to has removed the history
}

AboutFriendForm::~AboutFriendForm() {
    qDebug() << __func__;
    delete ui;
}

void AboutFriendForm::setName(const QString& name) {
    ui->userName->setText(name);
}

void AboutFriendForm::reloadTheme() {
    setStyleSheet(lib::settings::Style::getStylesheet("window/aboutFriend.css"));
}

void AboutFriendForm::onAliasChanged(const QString& text) {
    auto fid = ui->id->text();

    auto f = Nexus::getCore()->getFriendList().findFriend(ContactId(fid, lib::messenger::ChatType::Chat));
    if(!f.has_value()){
        return;
    }

    f.value()->setAlias(text);
    Core::getInstance()->setFriendAlias(fid, text);
}

const ContactId& AboutFriendForm::getId() {
    return m_friend->getId();
}
}  // namespace module::im
