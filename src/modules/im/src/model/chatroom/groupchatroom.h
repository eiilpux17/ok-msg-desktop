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

#ifndef GROUP_CHATROOM_H
#define GROUP_CHATROOM_H

#include "chatroom.h"

#include <QObject>

#include "src/model/GroupId.h"
namespace module::im {

class IDialogsManager;
class Group;
class Friend;
class FriendId;
class Profile;

class GroupChatroom : public Chatroom {
    Q_OBJECT
public:
    GroupChatroom(const GroupId* groupId, IDialogsManager* dialogsManager);
    ~GroupChatroom();

    virtual const ContactId& getContactId() override;

    bool hasNewMessage() const;
    void resetEventFlags();

    std::optional<Friend*> findFriend(const FriendId& pk);
    bool friendExists(const FriendId& pk);
    void inviteFriend(const FriendId& pk);

private:
    const GroupId* groupId{nullptr};
    Profile* mProfile;
    IDialogsManager* dialogsManager{nullptr};
};
}  // namespace module::im
#endif /* GROUP_CHATROOM_H */
