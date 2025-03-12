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

#ifndef FRIEND_CHATROOM_H
#define FRIEND_CHATROOM_H

#include "chatroom.h"

#include <QLabel>
#include <QObject>
#include <QString>
#include <QVector>
#include "src/model/FriendId.h"
namespace module::im {

class IDialogsManager;
class Friend;
class Group;

struct GroupToDisplay {
    QString name;
    Group* group;
};

struct CircleToDisplay {
    QString name;
    int circleId;
};

class FriendChatroom : public Chatroom {
    Q_OBJECT
public:
    FriendChatroom(const FriendId* frnd, IDialogsManager* dialogsManager, QObject* parent=nullptr);
    ~FriendChatroom() override;

    virtual const ContactId& getContactId() override;

public slots:

    const FriendId* getFriend();

    bool canBeInvited() const;



    void inviteFriend(const Group* group);

    QVector<GroupToDisplay> getGroups() const;

    void resetEventFlags();

    bool possibleToOpenInNewWindow() const;
    bool canBeRemovedFromWindow() const;
    bool friendCanBeRemoved() const;
    void removeFriendFromDialogs();

private:
    bool active{false};
    const FriendId* frnd{nullptr};
    IDialogsManager* dialogsManager{nullptr};
};
}  // namespace module::im

#endif  // FRIEND_H
