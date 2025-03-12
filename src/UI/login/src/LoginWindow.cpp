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

#include "LoginWindow.h"
#include "BannerWidget.h"

#include "LoginWidget.h"
#include "base/resources.h"
#include "ui_LoginWindow.h"
#include "lib/storage/settings/style.h"

namespace UI {

using namespace lib::session;

/* 登录主窗口 */
LoginWindow::LoginWindow(std::shared_ptr<lib::session::AuthSession> session, bool bootstrap)
        : ui(new Ui::LoginWindow) {
    ui->setupUi(this);

    OK_RESOURCE_INIT(UILoginWindow);

    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(APPLICATION_NAME);
    // 黄金分割比例 874/520 = 1.618
    setFixedSize(QSize(874, 520));

    // 样式
    // auto css = lib::settings::Style::getStylesheet("application.css");
    // qDebug() << css;
    // setStyleSheet(css);

    bannerWidget = new BannerWidget(this);
    bannerWidget->setFixedWidth(width() / 2);

    ui->hBoxLayout->addWidget(bannerWidget);

    loginWidget = new LoginWidget(session, bootstrap, this);
    ui->hBoxLayout->addWidget(loginWidget);


    // 设置样式
    auto qss = lib::settings::Style::getStylesheet("login.css");
    setStyleSheet(qss);
}

LoginWindow::~LoginWindow() { qDebug() << __func__; }

void LoginWindow::onProfileLoadFailed(QString msg) { loginWidget->onError(200, msg); }

}  // namespace UI
