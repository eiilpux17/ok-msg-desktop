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

#include <QMainWindow>
#include <memory>

#include "base/resources.h"
#include "lib/session/AuthSession.h"

// 初始化资源加载器
OK_RESOURCE_LOADER(UILoginWindow)

namespace Ui {
class LoginWindow;
}  // namespace Ui

namespace UI {

class LoginWidget;
class BannerWidget;

/**
 * 登录窗口
 */
class LoginWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit LoginWindow(std::shared_ptr<lib::session::AuthSession> session,  //
                         bool bootstrap);
    ~LoginWindow();

    [[nodiscard]] LoginWidget* widget() { return loginWidget; }

private:
    Ui::LoginWindow* ui;
    LoginWidget* loginWidget;
    BannerWidget* bannerWidget;

    // 资源指针申明
    OK_RESOURCE_PTR(UILoginWindow);

public slots:
    void onProfileLoadFailed(QString msg);
};

}  // namespace UI
