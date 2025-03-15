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

#pragma once

#include "Widget.h"
#include "base/compatiblerecursivemutex.h"
#include "modules/module.h"

namespace module::meet {

/**
 * 会议模块
 */
class Meet : public QObject, public Module {
    Q_OBJECT
public:
    explicit Meet();
    ~Meet() override;
    // 初始化
    void init(lib::session::Profile* p, QWidget* parent = nullptr) override;
    const QString& getName() const override;
    void start(lib::session::AuthSession* session) override;
    void stop() override;
    bool isStarted() override;
    void onSave(SavedInfo&) override;
    void cleanup() override;
    void activate() override;

    QWidget* widget() override {
        return m_widget;
    }
    void hide() override;
    void show() override;
private:
    Widget* m_widget;
    QString name;
    CompatibleRecursiveMutex mutex;
    bool started;
};

}  // namespace module::meet
