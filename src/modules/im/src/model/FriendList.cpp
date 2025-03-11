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

#include "FriendList.h"
#include <QHash>
#include <QMenu>
#include <QDebug>
#include "src/model/FriendId.h"
#include "src/model/Friend.h"

namespace module::im {

FriendList::FriendList(QObject* parent) : QObject(parent) {}

FriendList::~FriendList() {
    clear();
}

Friend* FriendList::addFriend(Friend* newfriend) {
    auto frnd = findFriend(newfriend->getId());
    if (frnd.has_value()) {
        qWarning() << "friend is existing";
        return frnd.value();
    }    
    friendMap[newfriend->getId().toString()] = newfriend;
    emit friendAdded(newfriend);
    return newfriend;
}

std::optional<Friend*> FriendList::findFriend(const ContactId& cId) const {
    auto f = friendMap.value(cId.getId());
    return f ? std::make_optional(f) : std::nullopt;
}

void FriendList::removeFriend(const FriendId& friendPk, bool fake) {
    auto f = findFriend(friendPk);
    if (f.has_value()) {
        friendMap.remove(((ContactId&)friendPk).toString());
        f.value()->deleteLater();
    }
}

void FriendList::clear() {
    for (auto friendptr : friendMap) delete friendptr;
    friendMap.clear();
}

QList<Friend*> FriendList::getAllFriends() const {
    return friendMap.values();
}

QString FriendList::decideNickname(const FriendId& fid, const QString& origName) {
    auto f = FriendList::findFriend(fid);
    if (f.has_value()) {
        return f.value()->getDisplayedName();
    } else if (!origName.isEmpty()) {
        return origName;
    } else {
        return fid.toString();
    }
}
}  // namespace module::im
