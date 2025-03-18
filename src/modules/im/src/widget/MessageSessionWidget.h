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

#ifndef OK_MESSAGE_SESSION_WIDGET_H
#define OK_MESSAGE_SESSION_WIDGET_H

#include "ContentWidget.h"
#include "genericchatroomwidget.h"
#include "src/model/FriendId.h"
#include "src/model/chatroom/groupchatroom.h"
#include "src/model/FriendMessageDispatcher.h"
#include "src/model/Message.h"
#include "src/model/sessionchatlog.h"

#include <memory>

#include <src/worker/SendWorker.h>

class QPixmap;
class MaskablePixmapWidget;

namespace module::im {
class FriendChatroom;
class CircleWidget;
class FriendChatForm;
class ChatHistory;
class ContentDialog;
class ContentLayout;
class FriendWidget;

class MessageSessionWidget : public GenericChatroomWidget {
    Q_OBJECT

public:
    explicit MessageSessionWidget(ContentLayout* layout, const ContactId& cid, lib::messenger::ChatType);

    ~MessageSessionWidget() override;

    void doDelete();

    void contextMenuEvent(QContextMenuEvent* event) override final;
    void setAsActiveChatroom() override final;
    void setAsInactiveChatroom() override final;
    void setAvatar(const QPixmap& avatar) override final;
    void setStatus(Status status);
    void setStatusMsg(const QString& msg);
    void setTyping(bool typing);
    void setName(const QString& name);

    void resetEventFlags() override final;
    QString getStatusString() const override final;

    void search(const QString& searchString, bool hide = false);

    void setRecvMessage(const FriendMessage& message, bool isAction);

    void setMessageReceipt(const MsgId& msgId);

    void setRecvGroupMessage(const GroupMessage& msg);

    void setFileReceived(const File& file);
    void setFileCancelled(const QString& fileId);

    void clearHistory();

    void connectFriend(const Friend* f);
    void connectGroup(const Group* g);


    void clearReceipts();

    void doForwardMessage(const ContactId& cid, const MsgId& msgId);
    void setFriendRemoved();

protected:
    void mousePressEvent(QMouseEvent* ev) override;
    void mouseMoveEvent(QMouseEvent* ev) override;
    void setFriendAlias();
    void onActiveSet(bool active) override;
    void paintEvent(QPaintEvent* e) override;
    void showEvent(QShowEvent*) override;


private:
    ContentLayout* contentLayout;

    std::unique_ptr<ContentWidget> contentWidget;
    std::unique_ptr<SendWorker> sendWorker;

    // 联系人ID(朋友ID、群聊ID共享)
    ContactId contactId;
    FriendId friendId;
    GroupId groupId;
    bool friendRemoved = false;


signals:

    void copyFriendIdToClipboard(const FriendId& friendPk);
    void contextMenuCalled(QContextMenuEvent* event);
    void friendHistoryRemoved();
    void widgetClicked(MessageSessionWidget* widget);
    void widgetRenamed(MessageSessionWidget* widget);
    void searchCircle(CircleWidget& circleWidget);
    void updateFriendActivity(Friend& frnd);
    void deleteSession(const QString& contactId);

public slots:
    void onAvatarSet(const FriendId& friendPk, const QPixmap& pic);
    void onAvatarRemoved(const FriendId& friendPk);
    void onContextMenuCalled(QContextMenuEvent* event);
    void do_widgetClicked();

private slots:
    void removeChat();
    void moveToNewCircle();
    void removeFromCircle();
    void moveToCircle(int circleId);
    void changeAutoAccept(bool enable);
    void showDetails();
    void onMessageSent(DispatchedMessageId id, const Message& message);
};
}  // namespace module::im
#endif  // FRIENDWIDGET_H
