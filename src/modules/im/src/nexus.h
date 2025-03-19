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

#ifndef NEXUS_H
#define NEXUS_H

#include <QObject>
#include <QPointer>

#include "base/resources.h"
#include "modules/module.h"
#include "src/base/compatiblerecursivemutex.h"

OK_RESOURCE_LOADER(res);
OK_RESOURCE_LOADER(IM);
OK_RESOURCE_LOADER(emojione);
OK_RESOURCE_LOADER(smileys);

class QMenuBar;
class QMenu;
class QAction;
class QWindow;
class QActionGroup;
class QSignalMapper;
class QCommandLineParser;

namespace lib::session {
class Profile;
}

namespace lib::audio{
class IAudioControl;
}

namespace module::im {
class Widget;
class Settings;
class Core;
class Profile;
class ProfileForm;
class Nexus;



/**
 * 聊天模块关系组织者，模块实现。
 */
class Nexus : public QObject, public Module {
    Q_OBJECT
public:

    ~Nexus() override;

    static Module* Create();

    static Nexus* getInstance();
    static Core* getCore();
    static Profile* getProfile();
    static std::optional<Profile*> getOptProfile();
    static Widget* getDesktopGUI();
    const QString& getName() const override;
    QWidget* widget() override;

    void init(lib::session::Profile* profile, QWidget* parent=nullptr) override;

    [[nodiscard]] lib::audio::IAudioControl* audio() const ;

    void incomingNotification(const QString& friendId);
    void outgoingNotification();
    void stopNotification();

protected:
    void start(lib::session::AuthSession* session) override;
    void stop() override;
    bool isStarted() override {
        return started;
    }
    void show() override;
    void hide() override;
    void onSave(SavedInfo&) override;
    void cleanup() override;


private:
    void setProfile(lib::session::Profile* p);
    explicit Nexus();
    void initTranslate();
    // static std::mutex mtx; // 互斥锁

    QString name;
    std::unique_ptr<Profile> profile;

    // 某些异常情况下widget会被提前释放
    QPointer<Widget> m_widget;

    bool started;
    CompatibleRecursiveMutex mutex;

    OK_RESOURCE_PTR(res);
    OK_RESOURCE_PTR(IM);
    OK_RESOURCE_PTR(emojione);
    OK_RESOURCE_PTR(smileys);

signals:
    void currentProfileChanged(lib::session::Profile* Profile);
    void profileLoaded();
    void profileLoadFailed();
    void coreChanged(Core&);
    void saveGlobal();
    void createProfileFailed(QString msg);
    void destroyProfile(const QString& profile);
    void exit(const QString& profile);

public slots:
    void do_logout(const QString& profile);
};
}  // namespace module::im
#endif  // NEXUS_H
