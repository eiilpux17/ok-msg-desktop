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

#ifndef MESSAGE_SESSION_LIST_WIDGET_H
#define MESSAGE_SESSION_LIST_WIDGET_H

#include <QWidget>
#include "MessageSessionWidget.h"
#include "genericchatitemlayout.h"
#include "lib/ui/widget/OWidget.h"
#include "src/core/core.h"
#include "src/model/FriendMessageDispatcher.h"
#include "src/model/Message.h"
#include "src/model/Status.h"
#include "src/persistence/settings.h"

class QVBoxLayout;
class QGridLayout;
class QPixmap;

namespace module::im {

class Widget;
class FriendWidget;
class GroupWidget;
// class CircleWidget;
// class CategoryWidget;
class ContactListLayout;
class GenericChatroomWidget;
class Friend;
class ContentLayout;
class MainLayout;

class MessageSessionListWidget  final : public lib::ui::OWidget {
    Q_OBJECT
public:
    using SortingMode = Settings::FriendListSortingMode;
    explicit MessageSessionListWidget(MainLayout* parent,
                                      ContentLayout* contentBox,
                                      bool groupsOnTop = true);
    ~MessageSessionListWidget();
    void setMode(SortingMode mode);
    SortingMode getMode() const;
    void reloadTheme();

    MessageSessionWidget* createMessageSession(const ContactId& cId,
                                               const QString& sid,
                                               lib::messenger::ChatType type);

    MessageSessionWidget* getMessageSession(const QString& contactId);

    void removeSessionWidget(MessageSessionWidget* w);

    void setFriend(const Friend* f);
    void removeFriend(const Friend* f);

    void setFriendStatus(const FriendId& friendPk, Status status);
    void setFriendStatusMsg(const FriendId& friendPk, const QString& statusMsg);
    void setFriendName(const FriendId& fId, const QString& name);
    void setFriendAvatar(const FriendId& fId, const QByteArray& avatar);
    void setFriendTyping(const ContactId& cId, bool typing);
    void setFriendFileReceived(const ContactId& cId, const File& file);
    void setFriendFileCancelled(const ContactId& cId, const QString& fileId);

    void search(const QString& searchString);

    void cycleMessageSession(bool forward);

    void updateActivityTime(const QDateTime& date);
    void reDraw();

    void setRecvGroupMessage(const GroupId& groupId, const GroupMessage& msg);

    void setRecvFriendMessage(FriendId friendnumber,         //
                              const FriendMessage& message,  //
                              bool isAction);

    void setFriendMessageReceipt(const FriendId& friendId, const MsgId& receipt);

    //  CircleWidget *createCircleWidget(int id = -1);

    void toSendMessage(const ContactId& cid, bool isGroup);
    void toForwardMessage(const ContactId& cid, const MsgId& id);

    // av
    void setFriendAvInvite(const ContactId& cid, bool video);
    void setFriendAvCreating(const FriendId& friendId, bool video);
    void setFriendAvStart(const FriendId& friendId, bool video);
    void setFriendAvPeerConnectedState(const FriendId& friendId,
                                       lib::ortc::PeerConnectionState state);

    void setFriendAvEnd(const FriendId& friendId, bool error);

    void addGroup(const Group* f);
    void removeGroup(const Group* f);

    void clearAllReceipts();

signals:
    void sessionAdded(MessageSessionWidget* widget);
    void onCompactChanged(bool compact);
    //  void connectCircleWidget(CircleWidget &circleWidget);
    //  void searchCircle(CircleWidget &circleWidget);
public slots:
    //  void renameGroupWidget(GroupWidget *groupWidget, const QString &newName);

    //  void renameCircleWidget(CircleWidget *circleWidget, const QString &newName);
    void onFriendWidgetRenamed(FriendWidget* friendWidget);

    void slot_sessionClicked(MessageSessionWidget* w);
    void do_deleteSession(const QString& contactId);
    void do_clearHistory(const QString& contactId);

    void moveWidget(MessageSessionWidget* w, Status s, bool add = false);

    void dayTimeout();

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void showEvent(QShowEvent* event) override;

private:
    QLayout* nextLayout(QLayout* layout, bool forward) const;
    void moveFriends(QLayout* layout);
    //  CategoryWidget *getTimeCategoryWidget(const IMFriend *frd) const;
    void sortByMode(SortingMode mode);
    void connectSessionWidget(MessageSessionWidget& sw);
    void updateFriendActivity(const Friend& frnd);

    SortingMode mode;

    bool groupsOnTop;
    ContactListLayout* listLayout;
    //  GenericChatItemLayout *circleLayout = nullptr;
    QVBoxLayout* activityLayout = nullptr;
    QTimer* dayTimer;

    ContentLayout* m_contentLayout;

    QMap<QString, MessageSessionWidget*> sessionWidgets;
};
}  // namespace module::im
#endif  // FRIENDLISTWIDGET_H
