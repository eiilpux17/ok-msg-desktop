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

#include "MessageSessionWidget.h"

#include "ContactListWidget.h"

#include "src/lib/ui/widget/tools/RoundedPixmapLabel.h"


#include "src/core/core.h"
#include "src/model/GroupMessageDispatcher.h"
#include "src/model/aboutfriend.h"
#include "src/model/Friend.h"
#include "src/model/FriendList.h"
#include "src/model/Group.h"
#include "src/model/Status.h"
#include "src/nexus.h"
#include "src/persistence/settings.h"
#include "src/widget/contentlayout.h"
#include "src/widget/form/aboutfriendform.h"
#include "src/widget/form/FriendChatForm.h"
#include "src/widget/tool/callconfirmwidget.h"
#include "src/widget/widget.h"

#include <QApplication>
#include <QBitmap>
#include <QContextMenuEvent>
#include <QDebug>
#include <QDrag>
#include <QFileDialog>
#include <QInputDialog>
#include <QMenu>
#include <QMimeData>
#include <QScrollBar>
#include <QStyleOption>
#include <application.h>

#include <cassert>

#include "form/FriendChatForm.h"
#include "src/chatlog/ChatLogView.h"
#include "src/core/coreav.h"
#include "src/nexus.h"
#include "src/widget/chatformheader.h"


namespace module::im {

/**
 * @class MessageSessionWidget
 *
 * Widget, which displays brief information about friend.
 * For example, used on friend list.
 * When you click should open the chat with friend. Widget has a context menu.
 */
MessageSessionWidget::MessageSessionWidget(ContentLayout* layout, const ContactId& cId,
                                           lib::messenger::ChatType chatType)
        : GenericChatroomWidget(chatType, cId)
        , contentLayout(layout)
        , sendWorker{nullptr}
        , contactId(cId) {
    qDebug() << __func__ << "contactId:" << cId.toString();
    setCursor(Qt::PointingHandCursor);
    setHidden(true);
    setAutoFillBackground(false);

    auto profile = Nexus::getProfile();
    auto core = Core::getInstance();

    if (chatType == lib::messenger::ChatType::Chat) {
        friendId = FriendId(contactId);

        sendWorker = std::make_unique<SendWorker>(friendId);
        connect(sendWorker->dispacher(), &IMessageDispatcher::messageSent, this,
                &MessageSessionWidget::onMessageSent);
        contentWidget = std::make_unique<ContentWidget>(sendWorker.get(), this);

        auto f = core->getFriend(friendId);
        if(f.has_value())
            setFriend(f.value());

    } else if (chatType == lib::messenger::ChatType::GroupChat) {
        auto nick = core->getNick();
        groupId = GroupId(contactId);
        groupId.nick = nick;

        sendWorker = std::make_unique<SendWorker>(groupId);

        connect(sendWorker->dispacher(), &IMessageDispatcher::messageSent, this,
                &MessageSessionWidget::onMessageSent);

        contentWidget = std::make_unique<ContentWidget>(sendWorker.get(), this);

        auto& gl = core->getGroupList();
        auto* g = gl.findGroup(groupId);
        setGroup(g);
        core->requestRoomInfo(contactId.toString());
    }



    auto chatForm = contentWidget->getChatForm();
    connect(chatForm->getChatLog(), &ChatLogView::readAll, this, [&]() {
        if (contentWidget->isVisible()) {
            // 已经阅读完消息，信号灯还原
            updateStatusLight(Core::getInstance()->getFriendStatus(contactId.toString()), false);
        }
    });

    connect(this, &MessageSessionWidget::chatroomWidgetClicked,
            [=, this](GenericChatroomWidget* w) {
                Q_UNUSED(w);
                do_widgetClicked();
                emit widgetClicked(this);
            });

    if (getContactId() == core->getSelfId()) {
        // 自己
        connect(profile, &Profile::selfAvatarChanged, this, &MessageSessionWidget::setAvatar);
    }
}

MessageSessionWidget::~MessageSessionWidget() {
    qDebug() << __func__ << contactId;
}

void MessageSessionWidget::doDelete()
{
    qDebug() << __func__ << contactId;
    //清空记录
    sendWorker->clearHistory();
}

void MessageSessionWidget::do_widgetClicked() {
    auto w = contentWidget.get();
    if(!contentLayout->existWidget(w)){
        contentLayout->addWidget(w);
    }
    contentWidget->showTo(contentLayout);
}

void MessageSessionWidget::showEvent(QShowEvent* e) {
    auto core = Nexus::getCore();

    if (isGroup()) {
        auto group = core->getGroup(contactId);
        if (group.has_value()) {
            setContact(*group.value());

            contentWidget->getChatForm()->setContact(contact);
            core->requestRoomInfo(contactId.toString());
        }
    } else {
        auto f = core->getFriend(contactId);
        if (f.has_value()) {
            setContact(*f.value());
            contentWidget->getChatForm()->setContact(contact);
        }

        updateStatusLight(core->getFriendStatus(contactId.toString()), false);

        auto msgs = sendWorker->getLastTextMessage();
        for (auto& m : msgs) {
            updateLastMessage(m);
            break;
        }
    }

    GenericChatroomWidget::showEvent(e);
}

/**
 * @brief MessageSessionWidget::contextMenuEvent
 * @param event Describe a context menu event
 *
 * Default context menu event handler.
 * Redirect all event information to the signal.
 */
void MessageSessionWidget::contextMenuEvent(QContextMenuEvent* event) {
    onContextMenuCalled(event);
    emit contextMenuCalled(event);
}

/**
 * @brief MessageSessionWidget::onContextMenuCalled
 * @param event Redirected from native contextMenuEvent
 *
 * Context menu handler. Always should be called to MessageSessionWidget from FriendList
 */
void MessageSessionWidget::onContextMenuCalled(QContextMenuEvent* event) {


    installEventFilter(this);  // Disable leave event.

    auto chatRoom = sendWorker->getChatroom();

    QMenu menu;
    //  menu.addSeparator();
    auto deleteAct = menu.addAction(tr("Delete the session"));
    //  inviteMenu->setEnabled(chatRoom->canBeInvited());

    connect(deleteAct, &QAction::triggered, this, &MessageSessionWidget::removeChat);
    //  inviteMenu->addSeparator();



    //  const auto setAlias = menu.addAction(tr("Set alias..."));
    //  connect(setAlias, &QAction::triggered, nameLabel, &CroppingLabel::editBegin);

    //  自动接收文件
    //  menu.addSeparator();
    //  auto autoAccept = menu.addAction(
    //      tr("Auto accept files from this friend", "context menu entry"));
    //  autoAccept->setCheckable(true);
    //  autoAccept->setChecked(!chatRoom->autoAcceptEnabled());
    //  connect(autoAccept, &QAction::triggered, this,
    //  &MessageSessionWidget::changeAutoAccept);

    //  menu.addSeparator();

    //  auto fnd = chatRoom->getFriend();
    //  if (chatRoom->friendCanBeRemoved()) {
    //    const auto friendPk = fnd->getPublicKey();
    //    const auto removeAction = menu.addAction(
    //        tr("Remove friend", "Menu to remove the friend from our friendlist"));
    //    connect(
    //        removeAction, &QAction::triggered, this,
    //        [=, this]() { emit removeFriend(friendPk); }, Qt::QueuedConnection);
    //  }

    //  menu.addSeparator();

    //  if (!fnd->isFriend()) {
    //    const auto friendPk = fnd->getPublicKey();
    //    const auto addAction = menu.addAction("添加好友");
    //    connect(
    //        addAction, &QAction::triggered, this,
    //        [=, this]() { emit addFriend(friendPk); }, Qt::QueuedConnection);
    //  }

    //    menu.addSeparator();
    const auto aboutWindow = menu.addAction(tr("Show details"));
    connect(aboutWindow, &QAction::triggered, this, &MessageSessionWidget::showDetails);

    const auto pos = event->globalPos();
    menu.exec(pos);

    removeEventFilter(this);

}

void MessageSessionWidget::removeChat() {
    emit deleteSession(contactId.toString());
}

// namespace {

// std::tuple<CircleWidget *, FriendListWidget *>
// getCircleAndFriendList(const IMFriend *frnd, MessageSessionWidget *fw) {
//   const auto pk = frnd->getPublicKey();
//   const auto circleId = Nexus::getProfile()->getSettings()->getFriendCircleID(pk);
//   auto circleWidget = CircleWidget::getFromID(circleId);
//   auto w = circleWidget ? static_cast<QWidget *>(circleWidget)
//                         : static_cast<QWidget *>(fw);
//   auto friendList = qobject_cast<FriendListWidget *>(w->parentWidget());
//   return std::make_tuple(circleWidget, friendList);
// }

//} // namespace

void MessageSessionWidget::moveToNewCircle() {
    //  const auto frnd = chatRoom->getFriend();
    //  CircleWidget *circleWidget;
    //  FriendListWidget *friendList;
    //  std::tie(circleWidget, friendList) = getCircleAndFriendList(frnd, this);

    //  if (circleWidget != nullptr) {
    //    circleWidget->updateStatus();
    //  }

    //  if (friendList != nullptr) {
    //    friendList->addCircleWidget(this);
    //  } else {
    //    const auto pk = frnd->getPublicKey();
    //    auto &s = Nexus::getProfile()->getSettings();
    //    auto circleId = s.addCircle();
    //    s.setFriendCircleID(pk, circleId);
    //  }
}

void MessageSessionWidget::removeFromCircle() {
    //  const auto frnd = chatRoom->getFriend();
    //  CircleWidget *circleWidget;
    //  FriendListWidget *friendList;
    //  std::tie(circleWidget, friendList) = getCircleAndFriendList(frnd, this);

    //  if (friendList != nullptr) {
    //    friendList->moveWidget(this, frnd->getStatus(), true);
    //  } else {
    //    const auto pk = frnd->getPublicKey();
    //    auto &s = Nexus::getProfile()->getSettings();
    //    s.setFriendCircleID(pk, -1);
    //  }

    //  if (circleWidget != nullptr) {
    //    circleWidget->updateStatus();
    //    emit searchCircle(*circleWidget);
    //  }
}

void MessageSessionWidget::moveToCircle(int newCircleId) {
    //  const auto frnd = getFriend();
    //  const auto pk = frnd->getPublicKey();
    //  const auto oldCircleId = Nexus::getProfile()->getSettings()->getFriendCircleID(pk);
    //  auto &s = Nexus::getProfile()->getSettings();
    //  auto oldCircleWidget = CircleWidget::getFromID(oldCircleId);
    //  auto newCircleWidget = CircleWidget::getFromID(newCircleId);

    //  if (newCircleWidget) {
    //    newCircleWidget->addFriendWidget(this, frnd->getStatus());
    //    newCircleWidget->setExpanded(true);
    //    emit searchCircle(*newCircleWidget);
    //    s.savePersonal();
    //  } else {
    //    s.setFriendCircleID(pk, newCircleId);
    //  }

    //  if (oldCircleWidget) {
    //    oldCircleWidget->updateStatus();
    //    emit searchCircle(*oldCircleWidget);
    //  }
}

void MessageSessionWidget::changeAutoAccept(bool enable) {
    //    auto chatRoom = sendWorker->getChatroom();
    //    if (enable) {
    //    const auto oldDir = chatRoom->getAutoAcceptDir();
    //    const auto newDir = QFileDialog::getExistingDirectory(
    //        Q_NULLPTR, tr("Choose an auto accept directory", "popup title"),
    //        oldDir);
    //    chatRoom->setAutoAcceptDir(newDir);
    //  } else {
    //    chatRoom->disableAutoAccept();
    //  }
}

void MessageSessionWidget::showDetails() {
    auto w = Widget::getInstance();
    if (w) {
        emit w->toShowDetails(getContactId());
    }
}

void MessageSessionWidget::onMessageSent(DispatchedMessageId id, const Message& message) {
    updateLastMessage(message);
}

void MessageSessionWidget::setFriend(const Friend* f) {
    if (!f) {
        return;
    }

    qDebug() << __func__ << f->toString();

    connect(f, &Friend::displayedNameChanged, this, [&](const QString& name) { setName(name); });
    connect(f, &Friend::avatarChanged, this, [this](const QPixmap& avatar) { setAvatar(avatar); });
    connect(f, &Friend::statusChanged, this,
            [this](Status status, bool event) { setStatus(status, event); });

    setContact(*f);

    contentWidget->getChatForm()->setContact(f);

}

void MessageSessionWidget::removeFriend() {
    removeContact();

    contentWidget->getChatForm()->removeContact();

}


void MessageSessionWidget::setGroup(const Group* g) {
    qDebug() << __func__ << g;
    if (!g) {
        return;
    }

    connect(g, &Group::displayedNameChanged, this, [&](const QString& name) { setName(name); });
    connect(g, &Group::avatarChanged, this, [this](const QPixmap& avatar) { setAvatar(avatar); });

    setContact(*g);

    contentWidget->getChatForm()->setContact(g);
}

void MessageSessionWidget::removeGroup() {
    contentWidget->getChatForm()->removeContact();
}

void MessageSessionWidget::clearReceipts() {
    sendWorker->dispacher()->clearOutgoingMessages();
}

void MessageSessionWidget::doForwardMessage(const ContactId& cid, const MsgId& msgId) {
    auto profile = Nexus::getProfile();
    auto history = profile->getHistory();
    auto msgs = history->getMessageById(msgId);

    if (msgs.empty()) {
        return;
    }

    auto& msg = msgs.at(0);
    sendWorker->dispacher()->sendMessage(false, msg.asMessage(), false);
}

void MessageSessionWidget::setAsActiveChatroom() {
    setActive(true);
}

void MessageSessionWidget::setAsInactiveChatroom() {
    setActive(false);
}

void MessageSessionWidget::onActiveSet(bool active) {
    setBackgroundRole(QPalette::Window);
}

void MessageSessionWidget::paintEvent(QPaintEvent* e) {
    QPainter painter(this);
    QStyleOptionFrame opt;
    initStyleOption(&opt);
    if (active) {
        opt.state |= QStyle::State_Selected;
    }
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &painter, this);
}

QString MessageSessionWidget::getStatusString() const {
    qDebug() << __func__;
    //  auto contactId = sendWorker->getChatroom()->getContactId();
    //  auto frnd = Nexus::getCore()->getFriendList().findFriend(ToxPk(contact));

    //  const int status = static_cast<int>(frnd->getStatus());
    //  const bool event = frnd->getEventFlag();

    //  static const QVector<QString> names = {
    //      tr("Online"),
    //      tr("Away"),
    //      tr("Busy"),
    //      tr("Offline"),
    //  };

    //  return event ? tr("New message") : names.value(status);

    return {};
}

// IMFriend *MessageSessionWidget::getFriend() const {
//     auto contact = sendWorker->getChatroom()->getContact();
//     auto cid = contact->getId();

//    auto frnd = Nexus::getCore()->getFriendList().findFriend(ToxPk(cid));
//    return frnd;
//}

// const ContactId& MessageSessionWidget::getContactId() const { return contactId; }

void MessageSessionWidget::search(const QString& searchString, bool hide) {
    //  const auto frnd = chatRoom->getFriend();
    //  searchName(searchString, hide);
    //  const Settings &s = Nexus::getProfile()->getSettings();
    //  const uint32_t circleId = s.getFriendCircleID(frnd->getPublicKey());
    //  CircleWidget *circleWidget = CircleWidget::getFromID(circleId);
    //  if (circleWidget) {
    //    circleWidget->search(searchString);
    //  }
}

void MessageSessionWidget::resetEventFlags() {
    //    getFriend()->setEventFlag(false);
}

void MessageSessionWidget::onAvatarSet(const FriendId& friendPk, const QPixmap& pic) {
    qDebug() << __func__ << "onAvatarSet:" << friendPk.toString() << "pic:" << pic.size();
    if (!pic.isNull()) {
        setAvatar(pic);
    }
}

void MessageSessionWidget::onAvatarRemoved(const FriendId& fid) {
    qDebug() << __func__ << fid.toString();
}

void MessageSessionWidget::mousePressEvent(QMouseEvent* ev) {
    if (ev->button() == Qt::LeftButton) {
        dragStartPos = ev->pos();
    }
    GenericChatroomWidget::mousePressEvent(ev);
}

void MessageSessionWidget::mouseMoveEvent(QMouseEvent* ev) {
    if (!(ev->buttons() & Qt::LeftButton)) {
        return;
    }

    const int distance = (dragStartPos - ev->pos()).manhattanLength();
    if (distance > QApplication::startDragDistance()) {
        QMimeData* mdata = new QMimeData;
        //    const IMFriend *frnd = getFriend();
        //    mdata->setText(frnd->getDisplayedName());
        //    mdata->setData("toxPk", frnd->getPublicKey().getByteArray());

        QDrag* drag = new QDrag(this);
        drag->setMimeData(mdata);
        drag->setPixmap(avatar->getPixmap());
        drag->exec(Qt::CopyAction | Qt::MoveAction);
    }
}

void MessageSessionWidget::setRecvMessage(const FriendMessage& msg, bool isAction) {
    FriendMessage m = msg;
    m.from = ContactId(m.from, lib::messenger::ChatType::Chat).toString();

    auto f = Nexus::getCore()->getFriendList().findFriend(contactId);
    if (f.has_value()) {
        m.displayName = f.value()->getDisplayedName();
    }

    auto md = (FriendMessageDispatcher*)sendWorker->dispacher();
    md->onMessageReceived(m);

    updateLastMessage(m);

    auto vis = contentWidget->isVisible();
    if (!vis) {
        // 更新状态信号灯
        auto status = Core::getInstance()->getFriendStatus(contactId.toString());
        updateStatusLight(status, true);
        // 聊天界面不显示，消息提示。
        Widget::getInstance()->newGroupMessageAlert(GroupId(contactId), FriendId(msg.from),
                                                    msg.content, true);
    }
}

void MessageSessionWidget::setMessageReceipt(const MsgId& msgId) {
    auto md = (FriendMessageDispatcher*)sendWorker->dispacher();
    md->onReceiptReceived(msgId);
}

void MessageSessionWidget::setRecvGroupMessage(const GroupMessage& msg) {
    GroupMessage m = msg;
    m.from = ContactId(m.from, lib::messenger::ChatType::GroupChat).toString();
    auto frd = Nexus::getCore()->getFriendList().findFriend(contactId);
    if (frd.has_value()) {
        m.displayName = frd.value()->getDisplayedName();
    } else {
        auto g = Core::getInstance()->getGroupList().findGroup(GroupId(msg.from));
        if (g) m.displayName = g->getPeerDisplayName(PeerId(msg.from).getResource());
    }

    auto md = (GroupMessageDispatcher*)sendWorker->dispacher();
    md->onMessageReceived(m);

    updateLastMessage(m);

    auto vis = contentWidget->isVisible();
    if (!vis) {
        // 更新状态信号灯
        updateStatusLight(Status::Online, true);
        // 聊天界面不显示，消息提示。
        Widget::getInstance()->newGroupMessageAlert(GroupId(contactId),
                                                    FriendId(msg.from),
                                                    msg.content, true);
    }
}

void MessageSessionWidget::setFileReceived(const File& file) {
    qDebug() << __func__ << file.toString();
    auto md = (FriendMessageDispatcher*)sendWorker->dispacher();
    if (md) md->onFileReceived(file);
}

void MessageSessionWidget::setFileCancelled(const QString& fileId) {
    qDebug() << __func__ << fileId;
    auto md = (FriendMessageDispatcher*)sendWorker->dispacher();
    if (md) md->onFileCancelled(fileId);
}

void MessageSessionWidget::clearHistory() {
    sendWorker->clearHistory();
}

void MessageSessionWidget::setStatus(Status status, bool event) {
    updateStatusLight(status, event);
    auto f = Nexus::getCore()->getFriendList().findFriend(contactId);
    if (!f.has_value()) {
        qWarning() << "friend is no existing.";
        return;
    }
    f.value()->setStatus(status);
}

void MessageSessionWidget::setStatusMsg(const QString& msg) {
    //  m_friend->setStatusMessage(msg);
    GenericChatroomWidget::setStatusMsg(msg);
}

void MessageSessionWidget::setTyping(bool typing) {
    qDebug() << __func__ << typing;
    auto chatForm = contentWidget->getChatForm();
    if (chatForm) chatForm->setFriendTyping(typing, "");
}

void MessageSessionWidget::setName(const QString& name) {
    GenericChatroomWidget::setName(name);
}

void MessageSessionWidget::setAvatar(const QPixmap& avatar) {
    GenericChatroomWidget::setAvatar(avatar);
}



}  // namespace module::im
