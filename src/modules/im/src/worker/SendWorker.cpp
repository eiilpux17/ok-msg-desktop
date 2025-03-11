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
// Created by gaojie on 24-5-14.
//

#include "SendWorker.h"

#include <src/model/chatroom/groupchatroom.h>
#include <src/model/FriendMessageDispatcher.h>
#include <src/model/GroupMessageDispatcher.h>
#include <src/persistence/settings.h>
#include <src/widget/chatformheader.h>
#include <src/widget/contentdialogmanager.h>
#include <src/widget/form/groupchatform.h>
#include "src/nexus.h"
#include "src/persistence/profile.h"

namespace module::im {
SendWorker::SendWorker(const FriendId& friendId) : contactId{friendId} {
    qDebug() << __func__ << "friend:" << friendId.toString();

    auto core = Core::getInstance();
    auto settings = Nexus::getProfile()->getSettings();
    auto profile = Nexus::getProfile();
    auto history = profile->getHistory();

    // initChatHeader(friendId);

    messageDispatcher =
            std::make_unique<FriendMessageDispatcher>(friendId, sharedParams, *core, *core);

    chatHistory = std::make_unique<ChatHistory>(friendId, history, *core, *settings,
                                                *messageDispatcher.get());

    chatForm = std::make_unique<ChatForm>(&friendId, *chatHistory.get(), *messageDispatcher.get());

    chatRoom = std::make_unique<FriendChatroom>(&friendId, ContentDialogManager::getInstance());

    // createCallDuration(true);
}

SendWorker::SendWorker(const GroupId& groupId) : contactId{groupId} {
    qDebug() << __func__ << "group:" << groupId.toString();

    auto profile = Nexus::getProfile();
    auto core = Core::getInstance();
    auto settings = Nexus::getProfile()->getSettings();
    auto history = profile->getHistory();

    initChatHeader(groupId);

    messageDispatcher = std::make_unique<GroupMessageDispatcher>(groupId, sharedParams, *core,
                                                                 *core, *settings);

    chatHistory = std::make_unique<ChatHistory>(groupId, history, *core, *settings,
                                                *messageDispatcher.get());

    chatLog = std::make_unique<SessionChatLog>(*core);
    connect(messageDispatcher.get(), &IMessageDispatcher::messageSent, chatLog.get(),
            &SessionChatLog::onMessageSent);
    connect(messageDispatcher.get(), &IMessageDispatcher::messageComplete, chatLog.get(),
            &SessionChatLog::onMessageComplete);
    connect(messageDispatcher.get(), &IMessageDispatcher::messageReceived, chatLog.get(),
            &SessionChatLog::onMessageReceived);

    chatForm = std::make_unique<GroupChatForm>(&groupId, *chatLog.get(), *messageDispatcher.get(),
                                               *settings);

    chatRoom = std::make_unique<GroupChatroom>(&groupId, ContentDialogManager::getInstance());
}

SendWorker::~SendWorker() {
    qDebug() << __func__;
}

void SendWorker::clearHistory() {
    qDebug() << __func__;
    auto profile = Nexus::getProfile();

    //数据库
    auto history = profile->getHistory();
    history->removeFriendHistory(contactId.toString());

    //TODO 存储
}

std::unique_ptr<SendWorker> SendWorker::forFriend(const FriendId& friend_) {
    return std::make_unique<SendWorker>(friend_);
}

std::unique_ptr<SendWorker> SendWorker::forGroup(const GroupId& group) {
    return std::make_unique<SendWorker>(group);
}

void SendWorker::initChatHeader(const ContactId& contactId) {

}


}  // namespace module::im
