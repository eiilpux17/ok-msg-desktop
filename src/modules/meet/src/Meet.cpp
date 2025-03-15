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

#include "Meet.h"

namespace module::meet {

Meet::Meet() : name(OK_Meet_MODULE), m_widget{nullptr} {}

Meet::~Meet() = default;

void Meet::init(lib::session::Profile* p, QWidget* parent) {
    m_widget = new Widget(parent);
}

const QString& Meet::getName() const {
    return name;
}

void Meet::start(lib::session::AuthSession* session) {

    QMutexLocker locker(&mutex);
    if(started)
          return;

    m_widget->start();
    started = true;
    qDebug() <<__func__<< "Starting up";
}

void Meet::stop() {

    QMutexLocker locker(&mutex);
    if(!started)
        return;
    started = false;
}

bool Meet::isStarted() {
    QMutexLocker locker(&mutex);
    return true;
}

void Meet::onSave(SavedInfo&) {}

void Meet::cleanup() {
    m_widget->deleteLater();
}

void Meet::activate() {
    m_widget->activate();
}

void Meet::hide() {
    m_widget->hide();
}

void Meet::show()
{
    m_widget->show();
}
}  // namespace module::meet
