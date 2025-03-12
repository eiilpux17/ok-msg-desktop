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

#ifndef GROUPCHATFORM_H
#define GROUPCHATFORM_H

#include <QMap>
#include "GenericChatForm.h"
#include "src/model/FriendId.h"

class QTimer;

namespace lib::ui {
class FlowLayout;
}

namespace module::im {

class Group;
class TabCompleter;

class GroupId;
class IMessageDispatcher;
class IGroupSettings;
struct Message;

class GroupChatForm : public GenericChatForm {
    Q_OBJECT
public:
    GroupChatForm(const GroupId& chatGroup,
                  IChatLog& chatLog,
                  IMessageDispatcher& messageDispatcher,
                  IGroupSettings& _settings,
                  QWidget* parent = nullptr);
    ~GroupChatForm() override;

    void peerAudioPlaying(QString peerPk);

private slots:

    void onMicMuteToggle();
    void onVolMuteToggle();
    void onCallClicked();
    void onUserJoined(const FriendId& user, const QString& name);
    void onUserLeft(const FriendId& user, const QString& name);
    void onPeerNameChanged(const FriendId& peer, const QString& oldName, const QString& newName);
    void onTitleChanged(const QString& author, const QString& title);
    void onLabelContextMenuRequested(const QPoint& localPos);

protected:
    virtual void keyPressEvent(QKeyEvent* ev) final override;
    virtual void keyReleaseEvent(QKeyEvent* ev) final override;
    // drag & drop
    virtual void dragEnterEvent(QDragEnterEvent* ev) final override;
    virtual void dropEvent(QDropEvent* ev) final override;

private:
    void retranslateUi();
    void updateUserCount(int numPeers);
    void updateUserNames();
    void joinGroupCall();
    void leaveGroupCall();

private:
    GroupId group;
    QMap<QString, QLabel*> peerLabels;
    QMap<QString, QTimer*> peerAudioTimers;
    lib::ui::FlowLayout* namesListLayout;
    QLabel* nusersLabel;
    TabCompleter* tabber;
    bool inCall;
    IGroupSettings& settings;
};
}  // namespace module::im
#endif  // GROUPCHATFORM_H
