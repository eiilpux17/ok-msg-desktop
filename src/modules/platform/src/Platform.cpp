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

//
// Created by gaojie on 24-7-31.
//

#include "Platform.h"
#include "AppCenterWidget.h"
#include "platformpagecontainer.h"

namespace module::platform {

Platform::Platform() : name{OK_Platform_MODULE}, m_widget{nullptr} {



}

Platform::~Platform() {
    // 目前PlatformPage指针绑定到了Widget内部的tab页上
    // 当Widget释放时，会自动释放PlatformPage
    // todo: 是否考虑要调整PlatformPage的所有权
    if (pageContainer) {
        delete pageContainer;
        pageContainer = nullptr;
    }
}

void Platform::init(lib::session::Profile* p, QWidget* parent) {
    m_widget = new Widget(parent);
    pageContainer = new PlatformPageContainer(this, m_widget);
    auto* page = new AppCenterPage(pageContainer);
    page->createContent(m_widget);
    pageContainer->addPage(page);
}

const QString& Platform::getName() const {
    return name;
}

void Platform::start(lib::session::AuthSession* session) {
    QMutexLocker locker(&mutex);

    if (started) {
        qWarning("This module is already started.");
        return;
    }
    m_widget->start();
    started = true;
    qDebug() <<__func__<< "Starting up";
}

void Platform::stop() {
    QMutexLocker locker(&mutex);
    if(!started)
        return;
    started = false;
};

bool Platform::isStarted() {
    QMutexLocker locker(&mutex);
    return started;
}
void Platform::onSave(SavedInfo&) {}

void Platform::cleanup() {
    m_widget->deleteLater();
}

PlatformPageContainer* Platform::getPageContainer() {
    return pageContainer;
}

void Platform::hide() {
    m_widget->hide();
}

void Platform::show()
{
    m_widget->show();
}
}  // namespace module::platform
