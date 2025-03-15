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

#pragma once

#include "ConfigWindow.h"
#include "base/compatiblerecursivemutex.h"
#include "base/resources.h"
#include "modules/module.h"

OK_RESOURCE_LOADER(Config)

namespace lib::session {
class Profile;
}

namespace module::config {

class Config : public Module {
public:
    explicit Config();
    ~Config();

    void init(lib::session::Profile* p, QWidget* parent=nullptr) override;
    const QString& getName() const override;
    void start(lib::session::AuthSession* session) override;
    [[nodiscard]] bool isStarted() override;
    void stop() override;
    void show() override;
    void hide() override;

    void onSave(SavedInfo&) override;
    void cleanup() override;
    QWidget* widget() override;

private:
    OK_RESOURCE_PTR(Config);

    QString m_name;
    ConfigWindow* m_widget;

    bool started;
    CompatibleRecursiveMutex mutex;

};

}  // namespace module::config
