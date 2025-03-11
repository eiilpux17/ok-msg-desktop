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

#include "ContactListWidget.h"
#include "ContactListLayout.h"
#include "base/times.h"

#include "friendwidget.h"
#include "groupwidget.h"
#include "src/model/chathistory.h"
#include "src/model/chatroom/friendchatroom.h"
#include "src/model/Friend.h"
#include "src/model/FriendList.h"
#include "src/model/Group.h"
#include "src/model/Status.h"
#include "src/nexus.h"
#include "src/persistence/settings.h"

#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QGridLayout>
#include <QMimeData>
#include <QShortcut>
#include <QTimer>

#include "Bus.h"
#include "widget.h"
#include "src/application.h"


namespace module::im {

inline QDateTime getActiveTimeFriend(const Friend* contact) {
    return Nexus::getProfile()->getSettings()->getFriendActivity(contact->getPublicKey());
}

ContactListWidget::ContactListWidget(QWidget* parent, bool groupsOnTop)
        : QFrame(parent), groupsOnTop(groupsOnTop) {
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    setAttribute(Qt::WA_StyledBackground, true);
    //    groupLayout.getLayout()->setSpacing(0);
    //    groupLayout.getLayout()->setMargin(0);
    setObjectName("contactListWidget");

    // Prevent QLayout's add child warning before setting the mode.
    listLayout = new ContactListLayout(this);


    // setAcceptDrops(true);

    auto widget = Widget::getInstance();
    connect(widget, &Widget::toShowDetails, this, &ContactListWidget::do_toShowDetails);
    connect(widget, &Widget::friendRemoved, this, [&](const Friend* f) { removeFriend(f); });

    new QShortcut(QKeySequence(Qt::Key_Up), this, [this]() { cycleContacts(true); });
    new QShortcut(QKeySequence(Qt::Key_Down), this, [this]() { cycleContacts(false); });

    // auto css = lib::settings::Style::getStylesheet("contact/ContactWidget.css");
    // qDebug() << css;

    // setStyleSheet(css);

    ok::Bus* bus = ok::Application::Instance()->bus();
    connect(bus, &ok::Bus::coreChanged, this, [&](Core* core) {
        connect(core, &Core::started, this, &ContactListWidget::init);
    });


    setLayout(listLayout);
}

ContactListWidget::~ContactListWidget() {
    //    if (activityLayout != nullptr) {
    //        QLayoutItem* item;
    //        while ((item = activityLayout->takeAt(0)) != nullptr) {
    //            delete item->widget();
    //            delete item;
    //        }
    //        delete activityLayout;
    //    }
}

FriendWidget* ContactListWidget::addFriend(const FriendId& friendId) {
    qDebug() << __func__ << friendId;
    auto exist = getFriend(friendId);
    if (exist) {
        qWarning() << "Exist friend" << friendId;
        return exist;
    }

    Core* core = Nexus::getCore();
    auto f = core->getFriend(friendId);
    if(!f.has_value()){
        return exist;
    }

    // auto f = new Friend(friendId);

    auto fw = new FriendWidget(f.value(), this);
    connect(fw, &FriendWidget::updateFriendActivity, this,
            &ContactListWidget::updateFriendActivity);
    connect(fw, &FriendWidget::friendClicked, this, &ContactListWidget::slot_friendClicked);

    connect(fw->getFriend(), &Friend::displayedNameChanged, fw, [&, fw](){
        listLayout->sortFriendWidget(fw);
    });

    friendWidgets.insert(friendId.toString(), fw);
    listLayout->addWidget(fw);

    auto status = core->getFriendStatus(friendId.toString());
    setFriendStatus(friendId, status);

    // emit Widget::getInstance() -> friendAdded(f.value());
    return fw;
}

void ContactListWidget::removeFriend(const Friend* f) {
    qDebug() << __func__ << f;
    auto fw = friendWidgets.value(f->getId().toString());
    if (!fw) {
        qWarning() << "Unable to find friendWidget";
        return;
    }

    listLayout->removeFriendWidget(fw);
    friendWidgets.remove(f->getId().toString());
    disconnect(fw);
    fw->deleteLater();
}

FriendWidget* ContactListWidget::getFriend(const ContactId& friendPk) {
    return friendWidgets.value(friendPk.toString());
}

void ContactListWidget::updateFriendActivity(const Friend& frnd) {
    const FriendId& pk = frnd.getPublicKey();
    auto settings = Nexus::getProfile()->getSettings();
    const auto oldTime = settings->getFriendActivity(pk);
    const auto newTime = QDateTime::currentDateTime();
    settings->setFriendActivity(pk, newTime);
    FriendWidget* widget = getFriend(frnd.getPublicKey());
    moveWidget(widget, frnd.getStatus());
    updateActivityTime(oldTime);  // update old category widget
}


GroupWidget* ContactListWidget::addGroup(const GroupId& groupId, const QString& groupName) {
    qDebug() << __func__ << groupId.toString();
    auto* p = Nexus::getProfile();
    auto* core = p->getCore();


    auto* gw = getGroup(groupId);
    if (gw) {
        qWarning() << "Group already exist.";
        return groupWidgets.value(groupId.toString());
    }

    // g = new Group(p, groupId, groupName);
    // core->addGroup(g);


    auto settings = p->getSettings();

    //  const bool enabled = core->getGroupAvEnabled(groupId.toString());

    gw = new GroupWidget(groupId, groupName);
    connect(gw, &GroupWidget::chatroomWidgetClicked, this, &ContactListWidget::slot_groupClicked);
    connect(gw, &GroupWidget::removeGroup, this, &ContactListWidget::do_groupDeleted);
    connect(gw->getGroup(), &Group::displayedNameChanged, gw, [&, gw](){
        listLayout->sortFriendWidget(gw);
    });

    groupWidgets[groupId.toString()] = gw;
    //    groupLayout.addSortedWidget(gw);
    listLayout->addWidget(gw);

    auto& gl = core->getGroupList();
    auto* g = gl.findGroup(groupId);
    connect(g, &Group::subjectChanged, [=, this](const QString& author, const QString& name) {
        Q_UNUSED(author);
        renameGroupWidget(gw, name);
    });


    return gw;
}

void ContactListWidget::do_groupDeleted(const ContactId& cid) {
    qDebug() << __func__ << cid.toString();
    removeGroup(GroupId(cid));
}

void ContactListWidget::init()
{
    auto* p = Nexus::getProfile();
    auto* core = p->getCore();

    auto& gl = core->getGroupList();
    for(auto* g: gl.getAllGroups()){
        addGroup(g->getId(), g->getName());
    }
}

void ContactListWidget::cycleContacts(bool forward) {
    if (friendWidgets.empty() && groupWidgets.empty()) {
        return;
    }

    GenericChatroomWidget* activeWidget = nullptr;

    for (auto fw : friendWidgets) {
        if (fw->isActive()) {
            activeWidget = fw;
            break;
        }
    }

    if (activeWidget == nullptr) {
        for (auto gw : groupWidgets) {
            if (gw->isActive()) {
                activeWidget = gw;
                break;
            }
        }
    }

    int curActiveWidgetIdx = -1;
    int nextActiveWidgetIdx = -1;

    if (activeWidget != nullptr) {
        curActiveWidgetIdx = listLayout->indexOfFriendWidget(activeWidget, true);
    }

    if (curActiveWidgetIdx != -1) {
        if (forward && curActiveWidgetIdx != 0) {
            nextActiveWidgetIdx = curActiveWidgetIdx - 1;
        } else if (!forward && curActiveWidgetIdx < listLayout->friendTotalCount() - 1) {
            nextActiveWidgetIdx = curActiveWidgetIdx + 1;
        }
    } else {
        nextActiveWidgetIdx = 0;
    }

    if (nextActiveWidgetIdx != -1) {
        QWidget* widget = listLayout->getLayoutOnline()->itemAt(nextActiveWidgetIdx)->widget();
        GenericChatroomWidget* chatWidget = qobject_cast<GenericChatroomWidget*>(widget);
        if (chatWidget) {
            emit chatWidget->chatroomWidgetClicked(chatWidget);
        }
    }
}

void ContactListWidget::removeGroup(const GroupId& cid) {
    qDebug() << __func__ << cid.toString();
    auto gw = groupWidgets.value(cid.toString());
    gw->deleteLater();
    groupWidgets.remove(cid.getId());
    //    groupLayout.removeSortedWidget(gw);
}

void ContactListWidget::setGroupTitle(const GroupId& groupId,
                                      const QString& author,
                                      const QString& title) {
    auto g = Core::getInstance()->getGroupList().findGroup(groupId);
    if (!g) {
        qWarning() << "group is no existing." << groupId.toString();
        return;
    }
    g->setSubject(author, title);
}

void ContactListWidget::setGroupInfo(const GroupId& groupId, const GroupInfo& info) {
    auto g = Core::getInstance()->getGroupList().findGroup(groupId);
    if (!g) {
        qWarning() << "group is no existing." << groupId.toString();
        return;
    }

    g->setName(info.name);
    g->setDesc(info.description);
    g->setSubject("", info.subject);
    g->setPeerCount(info.occupants);
}

void ContactListWidget::search(const QString& searchString, bool hideOnline, bool hideOffline,
                               bool hideGroups) {
    listLayout->search(searchString);

    //
    //  if (circleLayout != nullptr) {
    //    for (int i = 0; i != circleLayout->getLayout()->count(); ++i) {
    //      CircleWidget *circleWidget = static_cast<CircleWidget *>(
    //          circleLayout->getLayout()->itemAt(i)->widget());
    //      circleWidget->search(searchString, true, hideOnline, hideOffline);
    //    }
    //  } else if (activityLayout != nullptr) {
    //    for (int i = 0; i != activityLayout->count(); ++i) {
    //      CategoryWidget *categoryWidget =
    //          static_cast<CategoryWidget
    //          *>(activityLayout->itemAt(i)->widget());
    //      categoryWidget->search(searchString, true, hideOnline, hideOffline);
    //      categoryWidget->setVisible(categoryWidget->hasChatrooms());
    //    }
    //  }
}

void ContactListWidget::renameGroupWidget(GroupWidget* groupWidget, const QString& newName) {
    //  groupLayout.removeSortedWidget(groupWidget);
    //  groupLayout.addSortedWidget(groupWidget);

    groupWidget->setName(newName);
}

void ContactListWidget::dragEnterEvent(QDragEnterEvent* event) {
    if (!event->mimeData()->hasFormat("toxPk")) {
        return;
    }
    FriendId toxPk(event->mimeData()->data("toxPk"));
    auto frnd = Nexus::getCore()->getFriendList().findFriend(toxPk);
    if (frnd.has_value())
        event->acceptProposedAction();
}

void ContactListWidget::dropEvent(QDropEvent* event) {
    // Check, that the element is dropped from OkMsg
    QObject* o = event->source();
    FriendWidget* widget = qobject_cast<FriendWidget*>(o);
    if (!widget) return;

    // Check, that the user has a friend with the same ToxPk
    assert(event->mimeData()->hasFormat("toxPk"));
    const FriendId toxPk{event->mimeData()->data("toxPk")};
    auto f = Nexus::getCore()->getFriendList().findFriend(toxPk);
    if (!f.has_value()) return;

    moveWidget(widget, f.value()->getStatus(), true);
}

void ContactListWidget::showEvent(QShowEvent* event) {}

void ContactListWidget::moveWidget(FriendWidget* widget, Status s, bool add) {

}

void ContactListWidget::updateActivityTime(const QDateTime& time) {


    // int timeIndex = static_cast<int>(ok::base::getTimeBucket(time));
    //  QWidget *widget = activityLayout->itemAt(timeIndex)->widget();
    //  CategoryWidget *categoryWidget = static_cast<CategoryWidget *>(widget);
    //  categoryWidget->updateStatus();
    //  categoryWidget->setVisible(categoryWidget->hasChatrooms());
}

GroupWidget* ContactListWidget::getGroup(const GroupId& id) {
    return groupWidgets.value(id.toString());
}

void ContactListWidget::slot_groupClicked(GenericChatroomWidget* actived) {
    for (auto fw : friendWidgets) {
        fw->setActive(false);
    }
    for (auto gw : groupWidgets) {
        if (gw != actived) {
            gw->setActive(false);
        } else {
            gw->setActive(true);
            emit groupClicked(gw);
        }
    }
}

void ContactListWidget::slot_friendClicked(GenericChatroomWidget* actived) {
    for (auto gw : groupWidgets) {
        if (gw != actived) {
            gw->setActive(false);
        } else {
            gw->setActive(true);
            emit groupClicked(gw);
        }
    }

    for (auto fw : friendWidgets) {
        if (fw != actived) {
            fw->setActive(false);
        } else {
            fw->setActive(true);
            emit friendClicked(fw);
        }
    }
}
void ContactListWidget::setRecvGroupMessage(const GroupMessage& msg) {
    //  const GroupId &groupId = msg.groupId;
    //  auto gw = getGroup(groupId);
    //  if (!gw) {
    //      qWarning() <<"group is no existing";
    //      return;
    //  }
    //    gw->setRecvMessage(msg);
}

void ContactListWidget::setFriendStatus(const ContactId& friendPk, Status status) {
    auto fw = getFriend(friendPk);
    if (!fw) {
        // qWarning() << "friend" << friendPk.toString() << "widget is no existing.";
        return;
    }
    fw->setStatus(status, false);
}

void ContactListWidget::setFriendStatusMsg(const FriendId& friendPk, const QString& statusMsg) {
    auto fw = getFriend(friendPk);
    if (!fw) {
        qWarning() << "friend widget no exist.";
        return;
    }

    fw->setStatusMsg(statusMsg);
}

void ContactListWidget::setFriendName(const FriendId& friendPk, const QString& name) {
    auto f = Nexus::getCore()->getFriendList().findFriend(friendPk);
    if (!f.has_value()) {
        qWarning() << "friend is no existing.";
        return;
    }
    f.value()->setName(name);
}

void ContactListWidget::setFriendAlias(const FriendId& friendPk, const QString& alias) {
    qDebug() << __func__ << friendPk.toString() << alias;
    auto f = Nexus::getCore()->getFriendList().findFriend(friendPk);
    if (!f.has_value()) {
        qWarning() << "friend is no existing.";
        return;
    }
    f.value()->setAlias(alias);
}

void ContactListWidget::setFriendAvatar(const FriendId& friendPk, const QByteArray& avatar) {
    auto fw = getFriend(friendPk);
    if (!fw) {
        qWarning() << "friend is no exist.";
        return;
    }

    QPixmap p;
    p.loadFromData(avatar);
    fw->setAvatar(p);
}

void ContactListWidget::setFriendTyping(const FriendId& friendId, bool isTyping) {
    auto fw = getFriend(friendId);
    if (fw) fw->setTyping(isTyping);
}

void ContactListWidget::reloadTheme() {

    for (auto gw : groupWidgets) {
        gw->reloadTheme();
    }

    for (auto fw : friendWidgets) {
        fw->reloadTheme();
    }
}

void ContactListWidget::do_toShowDetails(const ContactId& cid) {
    qDebug() << __func__ << cid.toString();
    for (auto fw : friendWidgets) {
        if (fw->getContactId() == cid) {
            emit fw->chatroomWidgetClicked(fw);
            break;
        }
    }

    for (auto gw : groupWidgets) {
        if (gw->getContactId() == cid) {
            emit gw->chatroomWidgetClicked(gw);
            break;
        }
    }
}
}  // namespace module::im
