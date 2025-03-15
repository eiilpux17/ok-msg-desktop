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
// Created by gaojie on 25-1-14.
//

#include "Document.h"

namespace module::doc {

Document::Document() : name{OK_Document_MODULE}, m_widget(nullptr) {}

Document::~Document() {}

void Document::init(lib::session::Profile* p, QWidget *parent) {
    m_widget = new Widget(parent);
}

QWidget* Document::widget() {
    return m_widget;
}

const QString& Document::getName() const {
    return name;
}

void Document::start(lib::session::AuthSession* session) {
    QMutexLocker locker(&mutex);

    if (started) {
        qWarning("This module is already started.");
        return;
    }
    started = true;
    qDebug() <<__func__<< "Starting up";
}

void Document::stop() {
    QMutexLocker locker(&mutex);
    if(!started)
        return;
    started = false;
}

bool Document::isStarted() {
    QMutexLocker locker(&mutex);
    return true;
}

void Document::onSave(SavedInfo&) {}

void Document::cleanup() {
    m_widget->deleteLater();
}

void Document::show()
{
    m_widget->show();
}

void Document::hide() {
    m_widget->hide();
}

}  // namespace module::doc
