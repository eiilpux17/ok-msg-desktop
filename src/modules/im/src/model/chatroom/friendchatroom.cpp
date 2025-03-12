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

#include "src/model/chatroom/friendchatroom.h"
#include "src/model/dialogs/idialogsmanager.h"
#include "src/model/Friend.h"
#include "src/model/Group.h"
#include "src/model/GroupList.h"
#include "src/model/Status.h"
#include "src/nexus.h"
#include "src/persistence/settings.h"
#include "src/widget/contentdialog.h"

#include <QCollator>
#include "src/nexus.h"

namespace {

QString getShortName(const QString& name) {
    constexpr auto MAX_NAME_LENGTH = 30;
    if (name.length() <= MAX_NAME_LENGTH) {
        return name;
    }

    return name.left(MAX_NAME_LENGTH).trimmed() + ("...");
}

}  // namespace
namespace module::im {

FriendChatroom::FriendChatroom(const FriendId* frnd, IDialogsManager* dialogsManager, QObject* parent)
        :Chatroom(parent), frnd{frnd}, dialogsManager{dialogsManager} {
    qDebug() << __func__ << "friend" << frnd->getId();
}

FriendChatroom::~FriendChatroom() {
    qDebug() << __func__;
}

const FriendId* FriendChatroom::getFriend() {
    return frnd;
}

const ContactId& FriendChatroom::getContactId() {
    return *frnd;
}

bool FriendChatroom::canBeInvited() const {
    return false;
    //    return Status::isOnline(frnd->getStatus());
}

void FriendChatroom::inviteFriend(const Group* group) {
    const auto friendId = frnd->getId();
    const auto groupId = group->getIdAsString();
    //    Core::getInstance()->groupInviteFriend(friendId, groupId);
}

QVector<GroupToDisplay> FriendChatroom::getGroups() const {
    QVector<GroupToDisplay> groups;
    auto& gl = Core::getInstance()->getGroupList();
    for (const auto group : gl.getAllGroups()) {
        const auto name = getShortName(group->getName());
        const GroupToDisplay groupToDisplay = {name, group};
        groups.push_back(groupToDisplay);
    }

    return groups;
}

void FriendChatroom::resetEventFlags() {
    //    frnd->setEventFlag(false);
}

bool FriendChatroom::possibleToOpenInNewWindow() const {
    //    const auto friendPk = frnd->getId();
    const auto dialogs = dialogsManager->getFriendDialogs(*frnd);
    return !dialogs || dialogs->chatroomCount() > 1;
}

bool FriendChatroom::canBeRemovedFromWindow() const {
    const auto friendPk = frnd;
    const auto dialogs = dialogsManager->getFriendDialogs(*friendPk);
    return dialogs && dialogs->hasContact(ContactId(frnd->toString(), lib::messenger::ChatType::Chat));
}

bool FriendChatroom::friendCanBeRemoved() const {
    const auto dialogs = dialogsManager->getFriendDialogs(*frnd);
    return !dialogs || !dialogs->hasContact(ContactId(frnd->toString(), lib::messenger::ChatType::Chat));
}

void FriendChatroom::removeFriendFromDialogs() {
    auto dialogs = dialogsManager->getFriendDialogs(*frnd);
    dialogs->removeFriend(*frnd);
}
}  // namespace module::im
