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

//
// Created by gaojie on 24-5-5.
//

#include "ContentWidget.h"
#include "contentdialogmanager.h"
#include <QLabel>
#include <QStyleFactory>
#include "chatformheader.h"
#include "contentlayout.h"
#include "form/FriendChatForm.h"
#include "src/persistence/profile.h"
#include "src/persistence/settings.h"
#include "src/widget/form/GroupChatForm.h"
#include "src/worker/SendWorker.h"
#include <src/nexus.h>
#include <src/model/chatroom/groupchatroom.h>

namespace module::im {
static constexpr int HEADER_MARIGN = 8;
static constexpr int CONTENT_MARIGN = 8;
static constexpr int SEPERATOR_WIDTH = 2;

ContentWidget::ContentWidget(SendWorker* sendWorker, QWidget* parent) : QWidget(parent) {
    setLayout(new QVBoxLayout(this));

    layout()->setMargin(0);
    layout()->setSpacing(0);

    mainHead = new QWidget(this);
    mainHead->setLayout(new QVBoxLayout);
    mainHead->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
    // mainHead->layout()->setContentsMargins(HEADER_MARIGN, HEADER_MARIGN, HEADER_MARIGN, HEADER_MARIGN);

    auto& cid = sendWorker->getContactId();

    // 头部信息
    headWidget =  new ChatFormHeader(cid, this);
    mainHead->layout()->addWidget(headWidget);
    layout()->addWidget(mainHead);


    auto core = Core::getInstance();
    auto profile = Nexus::getProfile();
    auto history = profile->getHistory();
    auto settings = profile->getSettings();
    auto chatHistory = sendWorker->getChatHistory();


    if(cid.getChatType() == lib::messenger::ChatType::Chat){
        FriendId friendId = FriendId(cid);
        chatroom = new FriendChatroom(&friendId, ContentDialogManager::getInstance(), this);
        chatForm = new FriendChatForm(friendId, chatHistory->getChatLog(), *sendWorker->dispacher(), this);
    }else{
        GroupId groupId = GroupId(cid);
        chatroom = new GroupChatroom(&groupId, ContentDialogManager::getInstance(), this);
        chatForm = new GroupChatForm(groupId, chatHistory->getChatLog(), *sendWorker->dispacher(), *settings, this);
    }

    // 主体内容区
    mainContent = new QWidget(this);
    mainContent->setLayout(new QVBoxLayout);
    mainContent->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    mainContent->layout()->setContentsMargins(CONTENT_MARIGN, 0, CONTENT_MARIGN, CONTENT_MARIGN);
    mainContent->layout()->addWidget(chatForm);
    layout()->addWidget(mainContent);

    hide();
}

ContentWidget::~ContentWidget() {
    qDebug() << __func__;
}

void ContentWidget::init() {
    //  QPalette palette = mainHLine.palette();
    //  palette.setBrush(QPalette::WindowText, QBrush(QColor(193, 193, 193)));
    //  mainHLine.setPalette(palette);

    //  if (QStyleFactory::keys().contains(Nexus::getProfile()->getSettings()->getStyle())
    //      && Nexus::getProfile()->getSettings()->getStyle() != "None") {
    //    mainHead->setStyle(QStyleFactory::create(Nexus::getProfile()->getSettings()->getStyle()));
    //    mainContent->setStyle(QStyleFactory::create(Nexus::getProfile()->getSettings()->getStyle()));
    //  }

    //  reloadTheme();
}

void ContentWidget::showTo(ContentLayout* layout) {
    //  auto contentIndex = layout->indexOf(this);
    //    if(contentIndex < 0 ){
    //     contentIndex = layout->addWidget(this);
    //    }
    layout->setCurrentWidget(this);
    this->show();
}

void ContentWidget::showEvent(QShowEvent* event) {}

void ContentWidget::hideEvent(QHideEvent* event) {}
}  // namespace module::im
