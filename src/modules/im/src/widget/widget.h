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

#ifndef WIDGET_H
#define WIDGET_H

#include "ui_mainwindow.h"

#include <QFileInfo>
#include <QMainWindow>
#include <QPointer>
#include <QSystemTrayIcon>


#include "src/core/core.h"
#include "src/core/toxfile.h"
#include "src/core/toxid.h"
#include "src/model/FriendId.h"
#include "src/model/groupid.h"
#include "ui_mainwindow.h"
#if DESKTOP_NOTIFICATIONS
#include "src/platform/desktop_notifications/desktopnotify.h"
#endif

#define PIXELS_TO_ACT 7

namespace Ui {
class IMMainWindow;
}

class AlSink;
class Camera;
class QActionGroup;
class QMenu;
class QPushButton;
class QSplitter;
class QTimer;



namespace module::im {

class CircleWidget;
class AddFriendForm;
class ContentDialog;
class ContentLayout;
class Core;
class FilesForm;
class Friend;
class FriendChatroom;
class ContactListWidget;
class GenericChatroomWidget;
class Group;
class GroupChatForm;
class GroupChatroom;
class GroupInvite;
class GroupInviteForm;
class GroupWidget;

class ProfileForm;
class ProfileInfo;

class SettingsWidget;
class SystemTrayIcon;
class VideoSurface;
class UpdateCheck;
class Settings;
class IChatLog;
class ChatHistory;
class ChatWidget;
class ContactWidget;
class ContactSelectDialog;

enum class DialogType { AddDialog, TransferDialog, SettingDialog, ProfileDialog, GroupDialog };

enum class ActiveToolMenuButton {

    AddButton,
    GroupButton,
    TransferButton,
    SettingButton,
    None,
};

class Widget final : public QFrame {
    Q_OBJECT
public:
    explicit Widget(QWidget* parent = nullptr);
    ~Widget() override;

    static Widget* getInstance();
    bool newMessageAlert(QWidget* currentWindow, bool isActive, bool sound = true,
                         bool notify = true);


    void setCentralWidget(QWidget* widget, const QString& widgetName);
    QString getUsername();
    Camera* getCamera();

    void showUpdateDownloadProgress();

    bool newFriendMessageAlert(const FriendId& friendId, const QString& text, bool sound = true,
                               bool file = false);
    bool newGroupMessageAlert(const GroupId& groupId, const FriendId& authorPk,
                              const QString& message, bool notify);
    bool getIsWindowMinimized();
    void updateIcons();

    static QString fromDialogType(DialogType type);
    ContentDialog* createContentDialog() const;
    ContentLayout* createContentDialog(DialogType type) const;

    static void confirmExecutableOpen(const QFileInfo& file);

    void clearAllReceipts();

    void reloadTheme();

    void resetIcon();

    ContactWidget* getContactWidget() const {
        return contactWidget;
    }

public slots:
    void setWindowTitle(const QString& title);
    void forceShow();
    void onConnected();
    void onDisconnected();
    void onStarted();
    void onStatusSet(Status status);
    void onFailedToStartCore();
    void onBadProxyCore();
    void onSelfAvatarLoaded(const QPixmap& pic);

    void setUsername(const QString& username);

    void setAvatar(QByteArray avatar);

    void addFriendFailed(const FriendId& userId, const QString& errorInfo = QString());

    void onFileReceiveRequested(const ToxFile& file);

    void titleChangedByUser(const QString& title);
    void onGroupPeerAudioPlaying(QString groupnumber, FriendId peerPk);

    void onFriendDialogShown(const Friend* f);
    void onGroupDialogShown(const Group* g);
    void toggleFullscreen();
    void onUpdateAvailable();
    void onCoreChanged(Core& core);

    void showForwardMessageDialog(const MsgId& msgId);
    void showAddMemberDialog(const ContactId& groupId);

signals:
    void friendAdded(const Friend* f);
    void friendRemoved(const Friend* f);
    void friendRequestAccepted(const FriendId& friendPk);
    void friendRequestRejected(const FriendId& friendPk);
    void friendRequested(const ToxId& friendAddress, const QString& nick, const QString& message);

    void groupAdded(const Group* g);
    void groupRemoved(const Group* g);

    void statusSet(Status status);
    void statusSelected(Status status);
    void usernameChanged(const QString& username);
    void avatarSet(const QPixmap& avt);

    void changeGroupTitle(QString groupnumber, const QString& title);
    void statusMessageChanged(const QString& statusMessage);
    void resized();
    void windowStateChanged(Qt::WindowStates states);
    void toSendMessage(const QString& to, bool isGroup = false);
    void toAddMember(const ContactId& to);
    void toShowDetails(const ContactId& to);
    void toDeleteChat(const QString& to);
    void toClearHistory(const QString& to);

    void toForwardMessage(const MsgId& msgId);
    void forwardMessage(const ContactId& id, const MsgId& msgId);
    void addMember(const ContactId& id, const ContactId& gId);

protected:
    void showEvent(QShowEvent* e) override;

private slots:

    void onTransferClicked();

    void openNewDialog(GenericChatroomWidget* widget);
    void onChatroomWidgetClicked(GenericChatroomWidget* widget);
    void onStatusMessageChanged(const QString& newStatusMessage);

    void copyFriendIdToClipboard(const FriendId& friendId);
    void onIconClick(QSystemTrayIcon::ActivationReason);
    void onUserAwayCheck();
    void onEventIconTick();

    void onSplitterMoved(int pos, int index);

    void onDialogShown(GenericChatroomWidget* widget);

    void registerContentDialog(ContentDialog& contentDialog) const;
    void friendRequestedTo(const ToxId& friendAddress, const QString& nick, const QString& message);

private:
    // QMainWindow overrides
    bool eventFilter(QObject* obj, QEvent* event) final override;
    bool event(QEvent* e) final override;
    void closeEvent(QCloseEvent* event) final override;
    void changeEvent(QEvent* event) final override;
    void resizeEvent(QResizeEvent* event) final override;
    void moveEvent(QMoveEvent* event) final override;

    void saveWindowGeometry();
    void saveSplitterGeometry();
    void cycleContacts(bool forward);

    void retranslateUi();

    void openDialog(GenericChatroomWidget* widget, bool newWindow);
    void connectToCore(Core& core);

private:
    QAction* statusOnline;
    QAction* statusAway;
    QAction* statusBusy;
    QAction* actionLogout;
    QAction* actionQuit;
    QAction* actionShow;
    void setStatusOnline();
    void setStatusAway();
    void setStatusBusy();

    Ui::IMMainWindow* ui;
    QSplitter* centralLayout;

    ChatWidget* chatWidget;
    ContactWidget* contactWidget;
    SettingsWidget* settingsWidget;

    Core* core;

    bool notify(QObject* receiver, QEvent* event);
    bool autoAwayActive = false;
    QTimer* timer;
    bool eventFlag;
    bool eventIcon;
    bool wasMaximized = false;

    int icon_size;

#if DESKTOP_NOTIFICATIONS
    DesktopNotify notifier;
#endif

#ifdef Q_OS_MAC
    QAction* fileMenu;
    QAction* editMenu;
    QAction* contactMenu;
    QMenu* changeStatusMenu;
    QAction* editProfileAction;
    QAction* logoutAction;
    QAction* addContactAction;
    QAction* nextConversationAction;
    QAction* previousConversationAction;
#endif

    std::shared_ptr<::base::DelayedCallTimer> delayCaller;
};

bool toxActivateEventHandler(const QByteArray& data);
}  // namespace module::im
#endif  // WIDGET_H
