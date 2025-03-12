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

#ifndef FRIENDCHATFORM_H
#define FRIENDCHATFORM_H

#include <QElapsedTimer>
#include <QLabel>
#include <QSet>
#include <QTimer>

#include "CallDurationForm.h"
#include "GenericChatForm.h"
#include "src/core/core.h"
#include "src/model/ichatlog.h"
#include "src/model/IMessageDispatcher.h"
#include "src/model/Status.h"
#include "src/persistence/history.h"
#include "src/widget/tool/screenshotgrabber.h"

class QPixmap;
class QHideEvent;
class QMoveEvent;

namespace lib::ui {
class CroppingLabel;
}

namespace module::im {

class CallConfirmWidget;
class FileTransferInstance;
class Friend;
class History;
class OfflineMsgEngine;

/**
 * 朋友聊天框
 * @brief The FriendChatForm class
 */
class FriendChatForm : public GenericChatForm {
    Q_OBJECT
public:
    static const QString ACTION_PREFIX;

    FriendChatForm(const FriendId& contact, IChatLog& chatLog, IMessageDispatcher& messageDispatcher, QWidget* parent=nullptr);
    ~FriendChatForm() override;

    void setStatusMessage(const QString& newMessage);

    void setFriendTyping(bool isTyping);

    virtual void show(ContentLayout* contentLayout) final override;

    void reloadTheme() final ;

    void insertChatMessage(IChatItem::Ptr msg) final override;

signals:
    void incomingNotification(QString friendId);
    void outgoingNotification();
    void stopNotification();
    void endCallNotification();

    void updateFriendActivity(const FriendId& frnd);

public slots:

    void onFileNameChanged(const FriendId& friendPk);
    void clearChatArea();

private slots:
    void updateFriendActivityForFile(const File& file);
    //    void onAttachClicked() override;
    //    void onScreenshotClicked() override;

    //  void onCallTriggered();
    //  void onVideoCallTriggered();
    //  void onAcceptCallTriggered(const ToxPeer &peer, bool video);
    //  void onRejectCallTriggered(const ToxPeer &peer);
    //  void onMicMuteToggle();
    //  void onVolMuteToggle();

    void onFriendStatusChanged(const FriendId& friendId, Status status);
    void onFriendNameChanged(const QString& name);
    void onStatusMessage(const QString& message);

    void sendImage(const QPixmap& pixmap);

    void onCopyStatusMessage();
    void callUpdateFriendActivity();

protected:
    void dragEnterEvent(QDragEnterEvent* ev) final override;
    void dropEvent(QDropEvent* ev) final override;
    void hideEvent(QHideEvent* event) final override;
    void showEvent(QShowEvent* event) final override;

private:
    void retranslateUi();
    void showOutgoingCall(bool video);

    FriendId f;
    lib::ui::CroppingLabel* statusMessageLabel;
    QMenu statusMessageMenu;

    QAction* copyStatusAction;
};
}  // namespace module::im
#endif  // FRIENDCHATFORM_H
