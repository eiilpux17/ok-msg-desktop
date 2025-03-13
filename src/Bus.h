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
// Created by gaojie on 24-8-1.
//

#pragma once

#include <QObject>

class Module;
class QMouseEvent;

namespace module ::im {
class Profile;
class Core;
class CoreFile;
class CoreAV;
class Group;
class Friend;
}  // namespace module::im

namespace ok {

/**
 * 全局系统总线，负责多模块之间事件交互
 */
class Bus : public QObject {
    Q_OBJECT
public:
    explicit Bus(QObject* parent = nullptr);
    ~Bus() override;

signals:
    void languageChanged(const QString& locale);
    void moduleCreated(Module* module);

    void profileChanged(module::im::Profile* profile);
    void friendChanged(const module::im::Friend* f);
    void groupChanged(const module::im::Group* g);
    void coreChanged(module::im::Core* core);
    void coreAvChanged(module::im::CoreAV* coreAv);
    void coreFileChanged(module::im::CoreFile* coreFile);
    void themeColorChanged(int idx, const QString& color);
    //    void fontChanged(const QFont& font);
    //    void fontSizeChanged(int size);

    void avatarClicked(QMouseEvent* e);
};

}  // namespace ok
