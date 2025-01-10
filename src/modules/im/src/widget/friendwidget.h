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

#include "ContentWidget.h"
#include "genericchatroomwidget.h"
#include "src/model/FriendId.h"
#include "src/model/chatroom/groupchatroom.h"
#include "src/model/friendmessagedispatcher.h"
#include "src/model/message.h"
#include "src/model/sessionchatlog.h"

#include <memory>

class FriendChatroom;
class QPixmap;
class MaskablePixmapWidget;
class CircleWidget;
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

    void setStatus(Status::Status status, bool event);
    void setStatusMsg(const QString& msg);
    void setTyping(bool typing);
    void setName(const QString& name);

    void resetEventFlags() override final;
    QString getStatusString() const override final;

    const Friend* getFriend() const;
    const Contact* getContact() const;

    void search(const QString& searchString, bool hide = false);
    void setRecvMessage(const FriendMessage& message, bool isAction);

protected:
    void contextMenuEvent(QContextMenuEvent* event) override final;
    void mousePressEvent(QMouseEvent* ev) override;
    void mouseMoveEvent(QMouseEvent* ev) override;
    void paintEvent(QPaintEvent* e) override;
    void onActiveSet(bool active) override;

private:

    AboutFriendForm* about;

    Friend* m_friend;

    ContentDialog* createContentDialog() const;
    ContentDialog* addFriendDialog(const Friend* frnd);

public slots:
    void onContextMenuCalled(QContextMenuEvent* event);
    void do_widgetClicked(GenericChatroomWidget* w);

    void changeAutoAccept(bool enable);
    void inviteToNewGroup();

signals:
    void friendClicked(FriendWidget* widget);

    void addFriend(const FriendId& friendPk);
    void copyFriendIdToClipboard(const FriendId& friendPk);
    void contextMenuCalled(QContextMenuEvent* event);
    void friendHistoryRemoved();
    void friendWidgetRenamed(FriendWidget* friendWidget);
    void searchCircle(CircleWidget& circleWidget);
    void updateFriendActivity(const Friend& frnd);
    //    void setActive(bool active);
};

#endif  // FRIENDWIDGET_H
