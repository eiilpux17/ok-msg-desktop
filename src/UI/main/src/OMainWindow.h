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

#pragma once

#include <QBoxLayout>
#include <QMainWindow>
#include <QMap>
#include <QStackedWidget>
#include <QSystemTrayIcon>

#include "OMainMenu.h"
#include "lib/session/AuthSession.h"
#include "lib/ui/OWindow.h"

namespace Ui {
class OMainWindow;
}

namespace UI {

class OMenuWidget;

/**
 * 主窗口
 */
class OMainWindow : public lib::ui::OWindow {
    Q_OBJECT
public:
    explicit OMainWindow(std::shared_ptr<lib::session::AuthSession> session,
                        QWidget* parent = nullptr);
    ~OMainWindow();

    static OMainWindow* getInstance();

    void stop();

    void init();

    OMenuWidget* getMenuWindow(SystemMenu menu);
    OMenuWidget* initMenuWindow(SystemMenu menu);

    inline OMainMenu* menu() {
        return m_menu;
    }
    QWidget* getContainer(SystemMenu menu);

protected:
    void showEvent(QShowEvent* event) override;
    void closeEvent(QCloseEvent* event) override;
    void updateIcons();
    void retranslateUi();

private:
    std::shared_ptr<lib::session::AuthSession> session;

    std::shared_ptr<::base::DelayedCallTimer> delayCaller;

    Ui::OMainWindow* ui;
    OMainMenu* m_menu;
    QMap<SystemMenu, OMenuWidget*> menuMap;

    QSystemTrayIcon* sysTrayIcon;
    QMenu* trayMenu;
    QAction* actionQuit;
    QAction* actionShow;
    bool wasMaximized = false;

    //  bool autoAwayActive = false;
    void saveWindowGeometry();
    void createSystemTrayIcon();

    static inline QIcon prepareIcon(QString path, int w = 0, int h = 0);

signals:
    void toClose();

private slots:
    void onSwitchPage(SystemMenu menu, bool checked);

    void onIconClick(QSystemTrayIcon::ActivationReason);

    void onSetShowSystemTray(bool newValue);

    void forceShow();
};

}  // namespace UI

