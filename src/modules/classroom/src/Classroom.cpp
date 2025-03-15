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
// Created by gaojie on 23-10-7.
//

#include "Classroom.h"
#include "Widget.h"

#include <QWriteLocker>

namespace module::classroom {

static Classroom* Instance;

Classroom::Classroom() :
        m_widget(nullptr),
        m_name(OK_Classroom_MODULE) {}

Classroom::~Classroom() {
    qDebug() << __func__;
}

const QString& Classroom::getName() const {
    return m_name;
}

void Classroom::init(lib::session::Profile* p, QWidget* parent) {
    m_widget = new Widget(parent);
}

void Classroom::start(lib::session::AuthSession* session) {
    QMutexLocker locker(&mutex);
    if (started) {
        qWarning("This module is already started.");
        return;
    }

    started = true;
}


bool Classroom::isStarted() {
    QMutexLocker locker(&mutex);
    return started;
}

void Classroom::stop() {
    QMutexLocker locker(&mutex);
    if(!started)
        return;
    started = false;
}

void Classroom::show()
{
    m_widget->show();
}

void Classroom::hide() {
    m_widget->hide();
}

void Classroom::cleanup() {
    m_widget->deleteLater();
}


void Classroom::onSave(SavedInfo&) {

}

QWidget* Classroom::widget() {
    return m_widget;
}

}  // namespace module::classroom
