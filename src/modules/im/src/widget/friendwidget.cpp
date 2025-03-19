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

#include "friendwidget.h"

#include <QApplication>
#include <QBitmap>
#include <QContextMenuEvent>
#include <QDebug>
#include <QDrag>
#include <QFileDialog>
#include <QGraphicsOpacityEffect>
#include <QInputDialog>
#include <QMenu>
#include <QMimeData>
#include <QPainter>
#include <QStyleOption>

#include "ContactListWidget.h"
#include "base/MessageBox.h"
#include "contentdialogmanager.h"
#include "form/FriendChatForm.h"
#include "lib/ui/gui.h"
#include "lib/ui/widget/tools/CroppingLabel.h"
#include "src/core/core.h"
#include "src/lib/storage/settings/style.h"
#include "src/lib/ui/widget/tools/RoundedPixmapLabel.h"
#include "src/model/aboutfriend.h"
#include "src/model/chatroom/friendchatroom.h"
#include "src/model/Friend.h"
#include "src/model/FriendList.h"
#include "src/model/Status.h"
#include "src/nexus.h"
#include "src/persistence/profile.h"
#include "src/persistence/settings.h"
#include "src/widget/form/GroupCreateForm.h"
#include "src/widget/form/aboutfriendform.h"
#include "src/widget/form/FriendChatForm.h"
#include "src/widget/widget.h"

namespace module::im {

FriendWidget::FriendWidget(Friend* f, QWidget* parent)
        : GenericChatroomWidget(lib::messenger::ChatType::Chat, f->getId(), parent)
        , about{nullptr}, m_friend{f} {
    setObjectName("friendWidget");
    setHidden(true);
    setCursor(Qt::PointingHandCursor);

    // update alias when edited
    nameLabel->setText(m_friend->getDisplayedName());
    connect(nameLabel, &lib::ui::CroppingLabel::editFinished, m_friend, [&](const QString& txt){
        f->setAlias(txt, true);
    });

    connect(m_friend, &Friend::avatarChanged, [&](const QPixmap& pixmap) { setAvatar(pixmap); });
    connect(m_friend, &Friend::displayedNameChanged, nameLabel, &lib::ui::CroppingLabel::setText);
    connect(m_friend, &Friend::displayedNameChanged, this, [this](const QString& newName) {
        Q_UNUSED(newName);
        emit friendWidgetRenamed(this);
    });

    connect(f, &Friend::avInvite, this, &FriendWidget::setAvInvite);
    connect(f, &Friend::avEnd, this, &FriendWidget::setAvEnd);


    connect(this, &FriendWidget::chatroomWidgetClicked, [=, this](GenericChatroomWidget* w) {
        Q_UNUSED(w);
        do_widgetClicked(this);
        emit friendClicked(this);
    });

}

FriendWidget::~FriendWidget() {
    // qDebug() << __func__;
}

void FriendWidget::do_widgetClicked(GenericChatroomWidget* w) {
    qDebug() << __func__ << m_friend->getId().toString();
}

ContentDialog* FriendWidget::addFriendDialog(const Friend* frnd) {
    QString friendId = frnd->getId().toString();
    qDebug() << __func__ << friendId;

    const FriendId& friendPk = frnd->getPublicKey();
    qDebug() << "friendPk" << friendPk.toString();

    ContentDialog* dialog = ContentDialogManager::getInstance()->getFriendDialog(friendPk);
    qDebug() << "Find contentDialog:" << dialog;
    if (!dialog) {
        dialog = createContentDialog();
    }

            //    auto* settings = Nexus::getProfile()->getSettings();

            //  FriendWidget *widget = friendWidgets[friendPk];
            //  bool isCurrent = activeChatroomWidget == widget;
            //  if (!contentDialog && !isSeparate && isCurrent) {
            //    onAddClicked();
            //  }

            //  ContentDialogManager::getInstance()->addFriendToDialog(
            //      friendPk, dialog, chatRoom.get(), chatForm.get());

            //  friendWidget->setStatusMsg(widget->getStatusMsg());

            // #if (QT_VERSION >= QT_VERSION_CHECK(5, 7, 0))
            //   auto widgetRemoveFriend = QOverload<const ToxPk
            //   &>::of(&Widget::removeFriend);
            // #else
            //   auto widgetRemoveFriend =
            //       static_cast<void (Widget::*)(const ToxPk &)>(&Widget::removeFriend);
            // #endif
            //   connect(friendWidget, &FriendWidget::removeFriend, this,
            //   widgetRemoveFriend); connect(friendWidget, &FriendWidget::addFriend,
            //   this, &Widget::addFriend0);

            // FIXME: emit should be removed
            //  emit friendWidget->chatroomWidgetClicked(friendWidget);

            //  Profile *profile = Nexus::getProfile();
            //  connect(profile, &Profile::friendAvatarSet, this, &FriendWidget::onAvatarSet);
            //  connect(profile, &Profile::friendAvatarRemoved, this,
            //          &FriendWidget::onAvatarRemoved);

    auto profile = Nexus::getProfile();
    if (profile) {
        QPixmap avatar = profile->loadAvatar(frnd->getPublicKey());
        if (!avatar.isNull()) {
            setAvatar(avatar);
        }

                //        connect(profile, &Profile::friendAvatarChanged,
                //                [&](const FriendId& friendPk, const QPixmap& pixmap){
                //
                //                    setAvatar(pixmap);
                //        });
    }

    return dialog;
}

/**
 * @brief FriendWidget::contextMenuEvent
 * @param event Describe a context menu event
 *
 * Default context menu event handler.
 * Redirect all event information to the signal.
 */
void FriendWidget::contextMenuEvent(QContextMenuEvent* event) {
    onContextMenuCalled(event);
    //  emit contextMenuCalled(event);
}

/**
 * @brief FriendWidget::onContextMenuCalled
 * @param event Redirected from native contextMenuEvent
 *
 * Context menu handler. Always should be called to FriendWidget from FriendList
 */
void FriendWidget::onContextMenuCalled(QContextMenuEvent* event) {
    if (!showContextMenu) {
        return;
    }

    installEventFilter(this);  // Disable leave event.

            //  const auto newGroupAction = inviteMenu->addAction(tr("To new group"));
            //  connect(newGroupAction, &QAction::triggered, chatRoom.get(),
            //          &FriendChatroom::inviteToNewGroup);
            //  inviteMenu->addSeparator();

            //  for (const auto &group : chatRoom->getGroups()) {
            //    const auto groupAction =
            //        inviteMenu->addAction(tr("Invite to group '%1'").arg(group.name));
            //    connect(groupAction, &QAction::triggered,
            //            [=, this]() { chatRoom->inviteFriend(group.group); });
            //  }
            //

            //  const auto setAlias = menu.addAction(tr("Set alias..."));
            //  connect(setAlias, &QAction::triggered, nameLabel, &CroppingLabel::editBegin);

            //  自动接收文件
            //  menu.addSeparator();
            //  auto autoAccept = menu.addAction(
            //      tr("Auto accept files from this friend", "context menu entry"));
            //  autoAccept->setCheckable(true);
            //  autoAccept->setChecked(!chatRoom->autoAcceptEnabled());
            //  connect(autoAccept, &QAction::triggered, this,
            //  &FriendWidget::changeAutoAccept);

            //  menu.addSeparator();
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

            //  menu.addSeparator();
            //  const auto aboutWindow = menu.addAction(tr("Show details"));
            //  connect(aboutWindow, &QAction::triggered, this,
            //  &FriendWidget::showDetails);

    const auto pos = event->globalPos();

    QMenu menu(this);
    auto inviteToGrp = menu.addAction(tr("Invite to group"));
    auto newGroupAction = menu.addAction(tr("To new group"));

    connect(newGroupAction, &QAction::triggered, this, &FriendWidget::inviteToNewGroup);

            //  inviteMenu->addSeparator();

    menu.addSeparator();
    auto removeAct = menu.addAction(tr("Remove friend"));

    auto selected = menu.exec(pos);
    qDebug() << "selected" << selected;

    removeEventFilter(this);

    if (selected == removeAct) {
        const bool yes = lib::ui::GUI::askQuestion(tr("Confirmation"),
                                                   tr("Are you sure to remove %1 ?").arg(getName()),
                                                   false,
                                                   true,
                                                   true);
        if (!yes) {
            return;
        }

        auto removed = Core::getInstance()->removeFriend(m_friend->getId().toString());
        if (!removed) {
            ok::base::MessageBox::warning(this, tr("Warning"), tr("Can not remove the friend!"));
            return;
        }

        auto w = Widget::getInstance();
        emit w->friendRemoved(m_friend);

    } else if (selected == inviteToGrp) {
    }
}

void FriendWidget::changeAutoAccept(bool enable) {
    //  if (enable) {
    //    const auto oldDir = chatRoom->getAutoAcceptDir();
    //    const auto newDir = QFileDialog::getExistingDirectory(
    //        Q_NULLPTR, tr("Choose an auto accept directory", "popup title"),
    //        oldDir);
    //    chatRoom->setAutoAcceptDir(newDir);
    //  } else {
    //    chatRoom->disableAutoAccept();
    //  }
}

void FriendWidget::inviteToNewGroup() {
    auto groupCreate = new GroupCreateForm();
    connect(groupCreate, &GroupCreateForm::confirmed, [&, groupCreate](const QString name) {
        auto core = Core::getInstance();
        auto groupId = core->createGroup(name);
        qDebug() << "Create group successful:" << groupId;
        if (groupId.isValid()) {
            core->inviteToGroup(m_friend->getId(), groupId);
            groupCreate->close();
        }
    });
    groupCreate->show();
}

void FriendWidget::setAsActiveChatroom() {
    setActive(true);
}

void FriendWidget::setAsInactiveChatroom() {
    setActive(false);
}

void FriendWidget::onActiveSet(bool active) {

}

void FriendWidget::enterEvent(QEvent *e)
{
   // auto shadowEffect = new QGraphicsDropShadowEffect(this);
   // shadowEffect->setOffset(1, 1);
   // shadowEffect->setColor(lib::settings::Style::getInstance()->getColor(lib::settings::Style::ColorPalette::ThemeLight));
   // shadowEffect->setBlurRadius(6);
   // this->setGraphicsEffect(shadowEffect);
}

void FriendWidget::leaveEvent(QEvent *e)
{
   // setGraphicsEffect(nullptr);
}

QString FriendWidget::getStatusString() const {
    const int status = static_cast<int>(m_friend->getStatus());
    const bool event = m_friend->getEventFlag();

    static const QVector<QString> names = {
            tr("Online"),
            tr("Away"),
            tr("Busy"),
            tr("Offline"),
    };

    return event ? tr("New message") : names.value(status);
}

Friend* FriendWidget::getFriend(){
    return m_friend;
}

Contact* FriendWidget::getContact(){
    return getFriend();
}

void FriendWidget::search(const QString& searchString, bool hide) {
    //  const auto frnd = chatRoom->getFriend();
    //  searchName(searchString, hide);
    //  const Settings &s = Nexus::getProfile()->getSettings();
    //  const uint32_t circleId = s.getFriendCircleID(frnd->getPublicKey());
    //  CircleWidget *circleWidget = CircleWidget::getFromID(circleId);
    //  if (circleWidget) {
    //    circleWidget->search(searchString);
    //  }
}

void FriendWidget::resetEventFlags() {
    // resetEventFlags();
}

void FriendWidget::mousePressEvent(QMouseEvent* ev) {
    if (ev->button() == Qt::LeftButton) {
        dragStartPos = ev->pos();
    }

    GenericChatroomWidget::mousePressEvent(ev);
}

void FriendWidget::mouseMoveEvent(QMouseEvent* ev) {
    if (!(ev->buttons() & Qt::LeftButton)) {
        return;
    }

    const int distance = (dragStartPos - ev->pos()).manhattanLength();
    if (distance > QApplication::startDragDistance()) {
        QMimeData* mdata = new QMimeData;
        const Friend* frnd = getFriend();
        mdata->setText(frnd->getDisplayedName());
        mdata->setData("toxPk", frnd->getPublicKey().getByteArray());

        QDrag* drag = new QDrag(this);
        drag->setMimeData(mdata);
        drag->setPixmap(avatar->getPixmap());
        drag->exec(Qt::CopyAction | Qt::MoveAction);
    }
}

void FriendWidget::paintEvent(QPaintEvent* e) {
    QPainter painter(this);
    QStyleOptionFrame opt;
    initStyleOption(&opt);
    if (active) {
        opt.state |= QStyle::State_Selected;
    }
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &painter, this);
}

ContentDialog* FriendWidget::createContentDialog() const {
    qDebug() << __func__;

    ContentDialog* contentDialog = new ContentDialog();
    //  connect(contentDialog, &ContentDialog::friendDialogShown, this,
    //          &Widget::onFriendDialogShown);
    //  connect(contentDialog, &ContentDialog::groupDialogShown, this,
    //          &Widget::onGroupDialogShown);

    auto core = Core::getInstance();

    connect(core, &Core::usernameSet, contentDialog, &ContentDialog::setUsername);

            //  connect(&settings, &Settings::groupchatPositionChanged, &contentDialog,
            //          &ContentDialog::reorderLayouts);
            //  connect(&contentDialog, &ContentDialog::addFriendDialog, this,
            //          &Widget::addFriendDialog);
            //  connect(&contentDialog, &ContentDialog::addGroupDialog, this,
            //          &Widget::addGroupDialog);
            //  connect(&contentDialog, &ContentDialog::connectFriendWidget, this,
            //          &Widget::connectFriendWidget);

            // #ifdef Q_OS_MAC
            //   Nexus &n = Nexus::getInstance();
            //   connect(&contentDialog, &ContentDialog::destroyed, &n,
            //           &Nexus::updateWindowsClosed);
            //   connect(&contentDialog, &ContentDialog::windowStateChanged, &n,
            //           &Nexus::onWindowStateChanged);
            //   connect(contentDialog.windowHandle(), &QWindow::windowTitleChanged, &n,
            //           &Nexus::updateWindows);
            //   n.updateWindows();
            // #endif

    ContentDialogManager::getInstance()->addContentDialog(*contentDialog);
    return contentDialog;
}

void FriendWidget::setStatus(Status status, bool event) {
    updateStatusLight(status, event);
}

void FriendWidget::setStatusMsg(const QString& msg) {
    m_friend->setStatusMessage(msg);
    GenericChatroomWidget::setStatusMsg(msg);
}
void FriendWidget::setTyping(bool typing) {}

void FriendWidget::setName(const QString& name) {
    GenericChatroomWidget::setName(name);
    if (about) {
        about->setName(name);
    }
}


CallConfirmWidget* FriendWidget::createCallConfirm(const PeerId& peer,
                                                           bool video,
                                                           QString& displayedName) {
    qDebug() << __func__ << "peer:" << peer.toString() << "video?" << video;
    callConfirm = std::make_unique<CallConfirmWidget>(peer, video, this);
    connect(callConfirm.get(), &CallConfirmWidget::accepted, this, [&, peer]() {
        doAcceptCall(peer, video);
    });
    connect(callConfirm.get(), &CallConfirmWidget::rejected, this, [&, peer]() {
        doRejectCall(peer);
    });
    return callConfirm.get();
}

void FriendWidget::showCallConfirm() {
    callConfirm->show();
}

void FriendWidget::removeCallConfirm() {
    callConfirm.reset();
}

void FriendWidget::setAvInvite(const PeerId& peerId, bool video) {
    qDebug() << __func__ << "video:" << video;


    auto displayedName = m_friend->getDisplayedName();

    // 显示呼叫请求框
    createCallConfirm(peerId, video, displayedName);
    showCallConfirm();

    // 发送来电声音
    auto nexus = Nexus::getInstance();
    nexus->incomingNotification(peerId.toFriendId().toString());
}

void FriendWidget::setAvCreating(const FriendId& friendId, bool video) {
    qDebug() << __func__ << "friendId:" << friendId.toString() << "video:" << video;
    auto peerId = PeerId(friendId.toString());

    auto f = Nexus::getCore()->getFriend(peerId);
    if(!f.has_value())
        return;

    auto displayedName = f.value()->getDisplayedName();

    // 显示呼叫请求框
    auto callConfirm = createCallConfirm(peerId, video, displayedName);
    callConfirm->setCallLabel(tr("Calling..."));
    showCallConfirm();
}

void FriendWidget::setAvStart(bool video) {
    qDebug() << __func__;

    // 移除确认框
    removeCallConfirm();

    // 显示呼叫请求框
    createCallDuration(video);
}


void FriendWidget::setAvEnd(bool error) {
    qDebug() << __func__ << "error:" << error;

    // 移除确认框
    removeCallConfirm();
    // 关计时器
    destroyCallDuration(error);

}

void FriendWidget::setAvPeerConnectedState(lib::ortc::PeerConnectionState state) {
    auto dur = getCallDuration();
    if (dur) {
        switch (state) {
            case lib::ortc::PeerConnectionState::Connected: {
                dur->startCounter();
                break;
            }
            case lib::ortc::PeerConnectionState::Disconnected:
                dur->stopCounter();
                break;
            default:
                break;
        }
    };
}



CallDurationForm* FriendWidget::createCallDuration(bool video) {
    qDebug() << __func__ << "video:" << video;

    if (!callDuration) {
        callDuration = std::make_unique<CallDurationForm>(this);
        callDuration->setContact(getContact());

        connect(callDuration.get(), &CallDurationForm::endCall, [&]() {
            endCall();
        });

        connect(callDuration.get(), &CallDurationForm::muteSpeaker,
                [&](bool mute) { doSilenceSpeaker(mute); });

        connect(callDuration.get(), &CallDurationForm::muteMicrophone,
                [&](bool mute) { doMuteMicrophone(mute); });
    }

    callDuration->show();
    if (video) {
        callDuration->showNetcam();
    } else {
        callDuration->showAvatar();
    }

    return callDuration.get();
}

void FriendWidget::destroyCallDuration(bool error) {
    qDebug() << __func__;
    callDuration.reset();
}

void FriendWidget::doMuteMicrophone(bool mute) {
    auto fId = contactId.getId();
    qDebug() << __func__ << fId;
    auto av = CoreAV::getInstance();
    if (av->isCallStarted(contactId)) {
        av->muteCallOutput(contactId, mute);
    }
}

void FriendWidget::doSilenceSpeaker(bool mute) {
    auto fId = contactId.getId();
    qDebug() << __func__ << fId;
    auto av = CoreAV::getInstance();
    if (av->isCallStarted(contactId)) {
        av->muteCallSpeaker(contactId, mute);
    }
}

void FriendWidget::endCall() {

    qDebug() << __func__ << contactId;
    auto av = CoreAV::getInstance();
    if (av->isCallStarted(contactId)) {
        av->cancelCall(contactId);
    }
}



void FriendWidget::doAcceptCall(const PeerId& p, bool video) {
    qDebug() << __func__ << p.toString();

    // 关闭确认窗
    removeCallConfirm();

    // 发送接收应答
    CoreAV* coreav = CoreAV::getInstance();
    coreav->answerCall(p, video);
}

void FriendWidget::doRejectCall(const PeerId& p) {
    qDebug() << __func__ << p.toString();

    // 关闭确认窗
    removeCallConfirm();

    // 发送拒绝应答
    CoreAV* coreav = CoreAV::getInstance();
    coreav->rejectOrCancelCall(p);
}

/**
 * 执行呼叫
 */
void FriendWidget::doCall() {

    qDebug() << __func__ << contactId;

    auto av = CoreAV::getInstance();
    if (av->isCallStarted(contactId)) {
        av->cancelCall(contactId);
        return;
    }

    auto started = av->createCall(contactId, false);
    if (!started) {
        // 返回失败对方可能不在线，免费版本不支持离线呼叫！
        lib::ui::GUI::showWarning(tr("The feature unsupported in the open-source version"),
                                  tr("The call cannot be made due participant is offline!"));
        return;
    }

            // 播放外呼声音
    auto nexus = Nexus::getInstance();
    nexus->outgoingNotification();
}

void FriendWidget::doVideoCall() {

    qDebug() << __func__ << contactId;

    auto av = CoreAV::getInstance();
    if (av->isCallStarted(contactId)) {
        if (av->isCallVideoEnabled(contactId)) {
            av->cancelCall(contactId);
        }
    }

    auto started = av->createCall(contactId, true);
    if (!started) {
        // 返回失败对方可能不在线，免费版本不支持离线呼叫！
        lib::ui::GUI::showWarning(tr("The feature unsupported in the open-source version"),
                                  tr("The call cannot be made due participant is offline!"));
        return;
    }
}



}  // namespace module::im
