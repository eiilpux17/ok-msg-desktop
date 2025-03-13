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
#include <src/widget/form/GroupChatForm.h>
#include "src/nexus.h"
#include "src/persistence/profile.h"

namespace module::im {

SendWorker::SendWorker(const ContactId& cid) : contactId{cid} {
    qDebug() << __func__ << "ContactId:" << cid.toString();

    auto core = Core::getInstance();
    auto profile = Nexus::getProfile();
    auto history = profile->getHistory();
    auto settings = profile->getSettings();

    if(cid.getChatType() == lib::messenger::ChatType::Chat){
        messageDispatcher = std::make_unique<FriendMessageDispatcher>(FriendId(cid), sharedParams, *core, *core);
    }else{
        messageDispatcher = std::make_unique<GroupMessageDispatcher>(GroupId(cid), sharedParams, *core, *core,*settings);
    }

    chatHistory = std::make_unique<ChatHistory>(cid, history, *core, *settings, *messageDispatcher);

    // chatLog = std::make_unique<SessionChatLog>(*core);

    // connect(messageDispatcher.get(), &IMessageDispatcher::messageSent, chatLog.get(),
            // &SessionChatLog::onMessageSent);
    // connect(messageDispatcher.get(), &IMessageDispatcher::messageComplete, chatLog.get(),
            // &SessionChatLog::onMessageComplete);
    // connect(messageDispatcher.get(), &IMessageDispatcher::messageReceived, chatLog.get(),
            // &SessionChatLog::onMessageReceived);
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


}  // namespace module::im
