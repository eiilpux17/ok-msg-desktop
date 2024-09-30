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

#ifndef CHATMESSAGEITEM_H
#define CHATMESSAGEITEM_H

#include <QMenu>
#include <QObject>
#include "chatline.h"

class ContactAvatar;
class Text;
class SimpleText;
class NotificationIcon;

class ChatMessageBox : public IChatItem {
public:
    ChatMessageBox(const QPixmap& avatar, const QString& contactName, const QString& message,
                   bool isSelf = false);

    ChatMessageBox(const QPixmap& avatar, const QString& contactName, ChatLineContent* messageItem,
                   bool isSelf = false);

    void setMessageState(MessageState state);

    void layout(qreal width, QPointF scenePos) override;
    QRectF sceneBoundingRect() const override;
    void setVisible(bool visible) override;

    void markAsDelivered(const QDateTime& time) override;

    ChatLineContent* contentAtPos(QPointF scenePos) const override;
    ChatLineContent* centerContent() const override;

    // set before add to scene
    void setLayoutDirection(Qt::LayoutDirection direction);
    void setShowNickname(bool show);
    bool selectable() const override { return true; }
    ContactAvatar* getAvatarItem() { return avatarItem; }

    SimpleText* nickname() { return nicknameItem; }

protected:
    QList<ChatLineContent*> contents() override;
    void reloadTheme() override;

private:
    inline QPointF mapToLayout(const QPointF& pos, qreal width, qreal offset) {
        if (layoutDirection == Qt::RightToLeft) {
            return QPointF(width - pos.x() - offset, pos.y());
        }
        return pos;
    }
    QFont nicknameFont(const QFont& baseFont);
    void updateTextTheme();
    int itemType() override;

private:
    ContactAvatar* avatarItem = nullptr;
    SimpleText* nicknameItem = nullptr;
    ChatLineContent* messageItem = nullptr;
    ChatLineContent* stateItem = nullptr;
    MessageState msgState = MessageState::complete;
    QPointF scenePos;
    Qt::LayoutDirection layoutDirection = Qt::LeftToRight;
    bool showNickname = true;
    bool customMsg = false;
    bool _IsSelf = false;
};

class ChatNotificationBox : public IChatItem {
public:
    ChatNotificationBox(const QString& message, const QFont& font);

    void setIcon(ChatLineContent* item);

    void layout(qreal width, QPointF scenePos) override;
    QRectF sceneBoundingRect() const override;

    ChatLineContent* contentAtPos(QPointF scenePos) const override;
    ChatLineContent* centerContent() const override;
    int itemType() override;

    bool selectable() const override { return false; }

protected:
    QList<ChatLineContent*> contents() override;

private:
    ChatLineContent* iconItem = nullptr;
    Text* textItem = nullptr;
};

#endif  // !CHATMESSAGEITEM_H
