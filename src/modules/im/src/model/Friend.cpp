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

#include "Friend.h"
#include "src/model/Status.h"

#include <src/core/core.h>
#include <src/core/coreav.h>
#include <src/core/corefile.h>

namespace module::im {

Friend::Friend(const FriendId& friendPk,
               const QString& name,
               const QString& alias,
               bool isFriend ,
               bool is_online,
               const QStringList& groups)   //
        : Contact(friendPk, name, alias, false)
        , id{friendPk}
        , hasNewEvents{false}
        , friendStatus{Status::None}
        , mRelationStatus{RelationStatus::none} {
    auto core = Core::getInstance();
    friendStatus = core->getFriendStatus(friendPk.toString());
}

Friend::~Friend() = default;

QString Friend::toString() const {
    return getId().toString();
}

void Friend::setStatusMessage(const QString& message) {
    if (statusMessage != message) {
        statusMessage = message;
        emit statusMessageChanged(message);
    }
}

QString Friend::getStatusMessage() const {
    return statusMessage;
}

void Friend::setEventFlag(bool flag) {
    hasNewEvents = flag;
}

bool Friend::getEventFlag() const {
    return hasNewEvents;
}

void Friend::setStatus(Status s) {
    if (friendStatus != s) {
        auto oldStatus = friendStatus;
        friendStatus = s;
        emit statusChanged(friendStatus);
        if (!isOnline(oldStatus) && isOnline(friendStatus)) {
            emit onlineOfflineChanged(true);
        } else if (isOnline(oldStatus) && !isOnline(friendStatus)) {
            emit onlineOfflineChanged(false);
        }
    }
}

Status Friend::getStatus() const {
    return friendStatus;
}

bool Friend::startCall(bool video)
{
    auto av = CoreAV::getInstance();
    if (av->isCallStarted(id)) {
        av->cancelCall(id);
        return false;
    }

    return av->createCall(id, false);
}

bool Friend::sendFile(const QFile &file)
{
    auto cf = CoreFile::getInstance();
    return cf->sendFile(id.getId(), file);
}

void Friend::removeRelation(RelationStatus status)
{

    //朋友关系指向对方
    if(mRelationStatus != status){
        return;
    }

    switch (status) {
        case RelationStatus::to:
            //删除指向对方的关系(即保留对方指向自己的关系)
            mRelationStatus = RelationStatus::from;
            Core::getInstance()->removeFriend(id.getId());
            break;
        case RelationStatus::from:
            //删除指向自己的关系（即保留自己指向对方的关系）
            mRelationStatus = RelationStatus::to;
            break;
        default:
            break;
    }

    emit removed();
}


}  // namespace module::im
