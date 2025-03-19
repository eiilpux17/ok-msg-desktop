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

#include "nexus.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QDesktopWidget>

#include <cassert>
#include "Bus.h"
#include "application.h"
#include "lib/audio/audio.h"
#include "lib/storage/settings/OkSettings.h"
#include "lib/storage/settings/translator.h"
#include "persistence/settings.h"
#include "src/core/core.h"
#include "src/core/coreav.h"
#include "src/model/groupinvite.h"
#include "src/model/Status.h"
#include "src/persistence/profile.h"
#include "src/widget/widget.h"


/**
 * @class Nexus
 *
 * This class is in charge of connecting various systems together
 * and forwarding signals appropriately to the right objects,
 * it is in charge of starting the GUI and the Core.
 */
namespace module::im {

static Nexus* m_self = nullptr;

Nexus::Nexus() : name{OK_IM_MODULE}, started(false), profile{nullptr}, m_widget{nullptr} {
    qDebug() << __func__;

    OK_RESOURCE_INIT(res);
    OK_RESOURCE_INIT(IM);
    OK_RESOURCE_INIT(emojione);
    OK_RESOURCE_INIT(smileys);

    auto imptr = _okIM_ptr.get();
    qDebug() << "IM Resource is:" << imptr;

    // Setup the environment
    qRegisterMetaType<Status>("Status");

    qRegisterMetaType<uint8_t>("uint8_t");
    qRegisterMetaType<uint16_t>("uint16_t");
    qRegisterMetaType<uint32_t>("uint32_t");
    qRegisterMetaType<const int16_t*>("const int16_t*");
    qRegisterMetaType<int32_t>("int32_t");
    qRegisterMetaType<int64_t>("int64_t");
    qRegisterMetaType<size_t>("size_t");
    qRegisterMetaType<QPixmap>("QPixmap");
    qRegisterMetaType<Profile*>("Profile*");

    qRegisterMetaType<FileDirection>("FileDirection");
    qRegisterMetaType<FileStatus>("FileStatus");

    qRegisterMetaType<FriendId>("ToxPk");
    qRegisterMetaType<ok::base::Jid>("ok::base::Jid");
    qRegisterMetaType<GroupId>("GroupId");
    qRegisterMetaType<ContactId>("ContactId");
    qRegisterMetaType<GroupInvite>("GroupInvite");
    qRegisterMetaType<MsgId>("MsgId");
    qRegisterMetaType<RowId>("RowId");

    //初始化翻译
    initTranslate();
}

Nexus::~Nexus() {
    qDebug() << __func__;
    emit saveGlobal();
}

void Nexus::init(lib::session::Profile* p, QWidget* parent) {
    assert(p);
    assert(parent);
    // Create GUI
    m_widget = new Widget(parent);
}

void Nexus::onSave(SavedInfo& savedInfo) {
    auto s = getProfile()->getSettings();
    s->setWindowGeometry(savedInfo.windowGeometry);
    s->saveGlobal();
    m_widget->close();
}

/**
 * @brief Sets up invariants and calls showLogin
 *
 * Hides the login screen and shows the GUI for the given profile.
 * Will delete the current GUI, if it exists.
 */
void Nexus::start(lib::session::AuthSession* session) {
    QMutexLocker locker(&mutex);

    if (started) {
        qWarning("This module is already started.");
        return;
    }

    auto& signInInfo = session->getSignInInfo();
    qDebug() << __func__ << "for user:" << signInInfo.username;

    profile = Profile::createProfile(signInInfo.host, signInInfo.username, signInInfo.password);
    if (!profile) {
        qWarning() << tr("Can not create profile!");
        emit createProfileFailed(tr("Can not create profile!"));
        return;
    }


    auto bus = ok::Application::Instance()->bus();

    // Connections
    connect(profile.get(), &Profile::selfAvatarChanged, m_widget, &Widget::onSelfAvatarLoaded);
    connect(profile.get(), &Profile::coreChanged,
            [&, bus](Core& core) {
                emit bus->coreChanged(&core);
            });

    connect(bus, &ok::Bus::coreAvChanged, this, [&](const CoreAV* av){
        connect(av, &CoreAV::avInvite, this, [&](const ContactId& cid, bool video){
            incomingNotification(cid.getId());
        });

        connect(av, &CoreAV::avCreate, this, [&](const ContactId& cid, bool video){
            outgoingNotification();
        });

        connect(av, &CoreAV::avStart, this, [&](const ContactId& cid, bool video){
            stopNotification();
        });

        connect(av, &CoreAV::avAccept, this, [&](const ContactId& cid){
            stopNotification();
        });

        connect(av, &CoreAV::avReject, this, [&](const ContactId& cid){
            stopNotification();
        });

        connect(av, &CoreAV::avRetract, this, [&](const ContactId& cid){
            stopNotification();
        });

        connect(av, &CoreAV::avCancel, this, [&](const ContactId& cid){
            stopNotification();
        });

        connect(av, &CoreAV::avEnd, this, [&](const ContactId& cid){
            stopNotification();
        });
    });
    profile->start();
    
    started = true;
    qDebug() <<__func__<< "Starting up";
}

void Nexus::stop() {
    QMutexLocker locker(&mutex);
    if(!started)
        return;
    profile->stop();
    started = false;
}

void Nexus::show()
{
    m_widget->show();
}

void Nexus::hide() {
    m_widget->hide();
}

const QString& Nexus::getName() const {
    return name;
}

void Nexus::do_logout(const QString& profileName) {
    //    Nexus::getProfile()->getSettings()->saveGlobal();
}

lib::audio::IAudioControl *Nexus::audio() const
{
    return ok::Application::Instance()->getAudioControl();
}

Module* Nexus::Create() {
    if(!m_self){
        // std::lock_guard<std::mutex> lock(mtx); // 加锁
        m_self = new Nexus();
    }
    return m_self;
}

Nexus* Nexus::getInstance() {
    assert(m_self);
    return m_self;
}

void Nexus::cleanup() {
    qDebug() << __func__;
    m_widget->deleteLater();
    profile->quit();
}

/**
 * @brief Get current user profile.
 * @return nullptr if not started, profile otherwise.
 * @deprecated
 */
Profile* Nexus::getProfile() {
    assert(m_self);
    return m_self->profile.get();
}

std::optional<Profile *> Nexus::getOptProfile()
{
    return std::make_optional(m_self->profile.get());
}

/**
 * @brief Get core instance.
 * @return nullptr if not started, core instance otherwise.
 */
Core* Nexus::getCore() {
    auto p = getProfile();
    assert(p);
    assert(p->getCore());
    return p->getCore();
}

/**
 * @brief Get desktop GUI widget.
 * @return nullptr if not started, desktop widget otherwise.
 */
Widget* Nexus::getDesktopGUI() {
    return dynamic_cast<Widget*>(m_self->widget());
}


void Nexus::incomingNotification(const QString& friendnumber) {
    qDebug() << __func__ << friendnumber;
    const auto& friendId = FriendId(friendnumber);
    m_widget->newFriendMessageAlert(friendId, {}, false);
    ok::Application::Instance()->playNotificationSound(lib::audio::IAudioSink::Sound::IncomingCall, true);
}

void Nexus::outgoingNotification() {
    ok::Application::Instance()->playNotificationSound(lib::audio::IAudioSink::Sound::OutgoingCall, true);
}

void Nexus::stopNotification()
{
    ok::Application::Instance()->cleanupNotificationSound();
}


QWidget* Nexus::widget() {
    return m_widget->getInstance();
}

void Nexus::initTranslate() {
    QString locale = lib::settings::OkSettings::getInstance()->getTranslation();
    settings::Translator::translate(OK_IM_MODULE, locale);

    connect(ok::Application::Instance()->bus(), &ok::Bus::languageChanged,
            [](const QString& locale0) {
                settings::Translator::translate(OK_IM_MODULE, locale0);
            });
}

// #ifdef Q_OS_MAC
// void Nexus::retranslateUi() {
//     viewMenu->menuAction()->setText(tr("View", "OS X Menu bar"));
//     windowMenu->menuAction()->setText(tr("Window", "OS X Menu bar"));
//     minimizeAction->setText(tr("Minimize", "OS X Menu bar"));
//     frontAction->setText((tr("Bring All to Front", "OS X Menu bar")));
// }

// void Nexus::onWindowStateChanged(Qt::WindowStates state) {
//     minimizeAction->setEnabled(QApplication::activeWindow() != nullptr);

//     if (QApplication::activeWindow() != nullptr && sender() == QApplication::activeWindow()) {
//         if (state & Qt::WindowFullScreen) minimizeAction->setEnabled(false);

//         if (state & Qt::WindowFullScreen)
//             fullscreenAction->setText(tr("Exit Fullscreen"));
//         else
//             fullscreenAction->setText(tr("Enter Fullscreen"));

//         updateWindows();
//     }

//     updateWindowsStates();
// }

// void Nexus::updateWindows() {
//     updateWindowsArg(nullptr);
// }

// void Nexus::updateWindowsArg(QWindow* closedWindow) {
//     QWindowList windowList = QApplication::topLevelWindows();
//     delete windowActions;
//     windowActions = new QActionGroup(this);

//     windowMenu->addSeparator();

//     QAction* dockLast;
//     if (!dockMenu->actions().isEmpty())
//         dockLast = dockMenu->actions().first();
//     else
//         dockLast = nullptr;

//     QWindow* activeWindow;

//     if (QApplication::activeWindow())
//         activeWindow = QApplication::activeWindow()->windowHandle();
//     else
//         activeWindow = nullptr;

//     for (int i = 0; i < windowList.size(); ++i) {
//         if (closedWindow == windowList[i]) continue;

//         QAction* action = windowActions->addAction(windowList[i]->title());
//         action->setCheckable(true);
//         action->setChecked(windowList[i] == activeWindow);
//         connect(action, &QAction::triggered, [&] { onOpenWindow(windowList[i]); });
//         windowMenu->addAction(action);
//         dockMenu->insertAction(dockLast, action);
//     }

//     if (dockLast && !dockLast->isSeparator()) dockMenu->insertSeparator(dockLast);
// }

// void Nexus::updateWindowsClosed() {
//     updateWindowsArg(static_cast<QWidget*>(sender())->windowHandle());
// }

// void Nexus::updateWindowsStates() {
//     bool exists = false;
//     QWindowList windowList = QApplication::topLevelWindows();

//     for (QWindow* window : windowList) {
//         if (!(window->windowState() & Qt::WindowMinimized)) {
//             exists = true;
//             break;
//         }
//     }
//     if (frontAction) {
//         frontAction->setEnabled(exists);
//     }
// }

// void Nexus::onOpenWindow(QObject* object) {
//     QWindow* window = static_cast<QWindow*>(object);

//     if (window->windowState() & QWindow::Minimized) window->showNormal();

//     window->raise();
//     window->requestActivate();
// }

// void Nexus::toggleFullscreen() {
//     QWidget* window = QApplication::activeWindow();

//     if (window->isFullScreen())
//         window->showNormal();
//     else
//         window->showFullScreen();
// }

// void Nexus::bringAllToFront() {
//     QWindowList windowList = QApplication::topLevelWindows();
//     QWindow* focused = QApplication::focusWindow();

//     for (QWindow* window : windowList) window->raise();

//     focused->raise();
// }
// #endif
}  // namespace module::im
