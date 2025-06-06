﻿/*
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

#include <QMap>
#include <QObject>
#include <memory>

#include "base/Page.h"
#include "modules/module.h"
#include "src/UI/main/src/MainWindow.h"

namespace UI {

class WindowManager : public QObject {
    Q_OBJECT
public:
    WindowManager(QObject* parent = nullptr);
    ~WindowManager();

    static WindowManager* Instance();

    void startMainUI();
    void stopMainUI();

    void putPage(SystemMenu menu, QFrame* p);

    QFrame* getPage(SystemMenu menu);

    inline UI::MainWindow* window() {
        return m_mainWindow.get();
    }

    QWidget* getContainer(SystemMenu menu);

    OMainMenu* getMainMenu();

private:
    std::unique_ptr<UI::MainWindow> m_mainWindow;

signals:
    void menuPushed(SystemMenu menu, bool checked);
    void mainClose(SavedInfo savedInfo);
};
}  // namespace UI
