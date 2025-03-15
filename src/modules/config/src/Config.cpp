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
// Created by gaojie on 25-1-16.
//

#include "Config.h"

namespace module::config {

Config::Config() : m_widget(nullptr), m_name(OK_Config_MODULE) {
    OK_RESOURCE_INIT(Config);
}

Config::~Config() {}

void Config::init(lib::session::Profile* p, QWidget* parent) {
    m_widget = new ConfigWindow();
}

const QString& Config::getName() const {
    return m_name;
}

void Config::start(lib::session::AuthSession* session) {
    QMutexLocker locker(&mutex);

    if (started) {
        qWarning("This module is already started.");
        return;
    }
}

bool Config::isStarted() {
    QMutexLocker locker(&mutex);
    return started;
}

void Config::stop() {
    QMutexLocker locker(&mutex);
    if(!started)
        return;

    started = false;
}

void Config::show()
{
    m_widget->show();
}

void Config::hide() {
    m_widget->hide();
}

void Config::onSave(SavedInfo&) {

}

void Config::cleanup() {
    m_widget->deleteLater();
}

QWidget* Config::widget() {
    return m_widget;
}

}  // namespace module::config
