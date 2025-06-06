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
// Created by gaojie on 24-5-7.
//

#ifndef CHATWIDGET_H
#define CHATWIDGET_H

#include <QActionGroup>
#include <QPushButton>
#include <QWidget>
#include "MainLayout.h"
#include "MessageSessionListWidget.h"
#include "contentlayout.h"

namespace Ui {
class ChatWidget;
}

namespace module::im {

class GroupInviteForm;
class AddFriendForm;
class MessageSessionListWidget;
class ProfileForm;
class ProfileInfo;
class CoreAV;
class CoreFile;

/**
 * 消息界面
 * @brief The ChatWidget class
 */
class ChatWidget : public MainLayout {
    Q_OBJECT
public:
    explicit ChatWidget(QWidget* parent = nullptr);
    ~ChatWidget() override;

    [[nodiscard]] ContentLayout* getContentLayout() const override {
        assert(contentLayout);
        return contentLayout;
    }

    AddFriendForm* openFriendAddForm();
    void reloadTheme();
    void retranslateUi();
    void showProfile();
    void clearAllReceipts();

protected:
    void showEvent(QShowEvent*) override;

private:
    Ui::ChatWidget* ui;
    QWidget* contentWidget;
    ContentLayout* contentLayout;
    MessageSessionListWidget* sessionListWidget;


    QAction* statusOnline;
    QAction* statusAway;
    QAction* statusBusy;
    QAction* actionLogout;
    QAction* actionQuit;
    QAction* actionShow;
    void setStatusOnline();
    void setStatusAway();
    void setStatusBusy();

    GroupInviteForm* groupInviteForm;
    uint32_t unreadGroupInvites;
    QPushButton* friendRequestsButton;
    QPushButton* groupInvitesButton;

    std::unique_ptr<AddFriendForm> addFriendForm;

    ProfileInfo* profileInfo;
    ProfileForm* profileForm;

    void init();
    void deinit();
    void updateIcons();
    void setupSearch();
    void searchContacts();

    bool groupsVisible() const;

    void connectToCore(Core* core);
    void connectToCoreFile(CoreFile* coreFile);
    void connectToCoreAv(CoreAV* coreAv);

    void groupInvitesUpdate();
    void groupInvitesClear();

public slots:
    void on_nameClicked();
    void onProfileChanged(Profile* profile);

    void onConnecting();
    void onDisconnected(int err);
    void onConnected();

    void onStatusSet(Status status);
    void onNicknameSet(const QString& nickname);
    void onStatusMessageSet(const QString& statusMessage);

    void onFriendNickChanged(const FriendId& friendPk, const QString& nickname);
    void onFriendAvatarChanged(const FriendId& friendPk, const QByteArray& avatar);
    void onFriendAdded(const Friend* f);
    void onFriendRemoved(const Friend* f);
    void doSendMessage(const QString& to, bool isGroup);
    void doForwardMessage(const ContactId& cid, const MsgId& msgId);

    void onFriendStatusChanged(const FriendId& friendPk, Status status);
    void onFriendStatusMessageChanged(const FriendId& friendPk, const QString& message);

    void onMessageSessionReceived(const ContactId& contactId, const QString& sid);

    void onFriendMessageReceived(const FriendId friendId,
                                 const FriendMessage message,
                                 bool isAction);

    void onReceiptReceived(const FriendId& friendPk, MsgId receipt);

    void onFriendTypingChanged(const FriendId& friendnumber, bool isTyping);

    void onGroupAdded(const Group* g);
    void onGroupRemoved(const Group* g);

    void onGroupMessageReceived(GroupId groupId, GroupMessage msg);

    void onGroupInviteAccepted(const GroupInvite& inviteInfo);

    void onGroupPeerListChanged(QString groupnumber);

    void onGroupPeerSizeChanged(QString groupnumber, const uint size);

    void onGroupPeerNameChanged(QString groupnumber, const FriendId& peerPk,
                                const QString& newName);
    void onGroupTitleChanged(QString groupnumber, const QString& author, const QString& title);

    void onGroupPeerStatusChanged(const QString&, const GroupOccupant&);
    void onGroupClicked();

    void setupStatus();
    void cancelFile(const QString& friendId, const QString& fileId);

    void dispatchFile(ToxFile file);
    void dispatchFileWithBool(ToxFile file, bool);
    void dispatchFileSendFailed(QString friendId, const QString& fileName);

    void onAvInvite(const PeerId& peerId, bool video);
    void onAvCreating(const FriendId& friendId, bool video);
    void onAvStart(const FriendId& friendId, bool video);
    void onAvPeerConnectionState(const FriendId& friendId, lib::ortc::PeerConnectionState state);
    void onAvEnd(const FriendId& friendId, bool error);
};
}  // namespace module::im
#endif  // CHATWIDGET_H
