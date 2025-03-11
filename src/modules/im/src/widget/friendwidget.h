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

#ifndef FRIENDWIDGET_H
#define FRIENDWIDGET_H

#include "genericchatroomwidget.h"
#include "src/model/FriendId.h"
#include "src/model/chatroom/groupchatroom.h"
#include "src/model/Message.h"
#include "src/widget/form/CallDurationForm.h"
#include "src/widget/tool/callconfirmwidget.h"

class QPixmap;
class MaskablePixmapWidget;

namespace module::im {

class FriendChatroom;
class ContentDialog;
class ContentLayout;
class Widget;
class AboutFriendForm;

class FriendWidget : public GenericChatroomWidget {
    Q_OBJECT

public:
    explicit FriendWidget(Friend* f, QWidget* parent = nullptr);
    ~FriendWidget() override;
    void setAsActiveChatroom() override final;
    void setAsInactiveChatroom() override final;

    void setStatus(Status status, bool event);
    void setStatusMsg(const QString& msg);
    void setTyping(bool typing);
    void setName(const QString& name);

    void resetEventFlags() override final;
    QString getStatusString() const override final;

    Friend* getFriend() ;
    Contact* getContact() ;

    void search(const QString& searchString, bool hide = false);
    void setRecvMessage(const FriendMessage& message, bool isAction);

    CallConfirmWidget* createCallConfirm(const PeerId& pid, bool video, QString& displayedName);
    void showCallConfirm();
    void removeCallConfirm();
    CallConfirmWidget* getCallConfirm() const {
        return callConfirm.get();
    }


    CallDurationForm* createCallDuration(bool video = false);
    void destroyCallDuration(bool error = false);
    [[nodiscard]] CallDurationForm* getCallDuration() const {
        return callDuration.get();
    }

protected:
    void contextMenuEvent(QContextMenuEvent* event) override final;
    void mousePressEvent(QMouseEvent* ev) override;
    void mouseMoveEvent(QMouseEvent* ev) override;
    void paintEvent(QPaintEvent* e) override;
    void onActiveSet(bool active) override;
    void enterEvent(QEvent* e) override;
    void leaveEvent(QEvent* e) override;

private:
    AboutFriendForm* about;

    Friend* m_friend;

    ContentDialog* createContentDialog() const;
    ContentDialog* addFriendDialog(const Friend* frnd);
    //呼叫确定框
    std::unique_ptr<CallConfirmWidget> callConfirm;
    //呼叫进行中窗口
    std::unique_ptr<CallDurationForm> callDuration;
signals:
    void friendClicked(FriendWidget* widget);

    void addFriend(const FriendId& friendPk);
    void copyFriendIdToClipboard(const FriendId& friendPk);
    void contextMenuCalled(QContextMenuEvent* event);
    void friendHistoryRemoved();
    void friendWidgetRenamed(FriendWidget* friendWidget);

    void updateFriendActivity(const Friend& frnd);
    //    void setActive(bool active);
public slots:

    void onContextMenuCalled(QContextMenuEvent* event);
    void do_widgetClicked(GenericChatroomWidget* w);

    void changeAutoAccept(bool enable);
    void inviteToNewGroup();

    void doMuteMicrophone(bool mute);
    void doSilenceSpeaker(bool mute);
    void endCall();

    void doAcceptCall(const PeerId& p, bool video);
    void doRejectCall(const PeerId& p);
    void doCall();
    void doVideoCall();

    void setAvInvite(const PeerId& pid, bool video);
    void setAvCreating(const FriendId& fid, bool video);
    void setAvStart(bool video);
    void setAvPeerConnectedState(lib::ortc::PeerConnectionState state);
    void setAvEnd(bool error);
};

}  // namespace module::im
#endif  // FRIENDWIDGET_H
