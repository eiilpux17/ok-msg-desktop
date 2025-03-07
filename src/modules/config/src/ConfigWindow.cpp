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
#include "ConfigWindow.h"
#include "Bus.h"
#include "ui_ConfigWindow.h"

#include <QWidget>

#include "lib/storage/settings/OkSettings.h"
#include "lib/storage/settings/style.h"
#include "lib/storage/settings/translator.h"

#include "about/src/aboutform.h"
#include "plugin/src/PluginManagerForm.h"
#include "settings/src/GeneralForm.h"
#include "application.h"


namespace module::config {

ConfigWindow::ConfigWindow(QWidget* parent) : lib::ui::OFrame(parent), ui(new Ui::ConfigWindow) {
    ui->setupUi(this);

    ui->tabWidget->setObjectName("mainTab");

    // 设置
    auto sw = new GeneralForm(this);
    connect(sw, &GeneralForm::onLanguageChanged, [](const QString& locale) {
        settings::Translator::translate(OK_Config_MODULE, locale);
    });
    ui->tabWidget->addTab(sw, tr("System settings"));

#if OK_PLUGIN
    // 插件管理
    ui->tabWidget->addTab(new PluginManagerForm(this), tr("Management"));
#endif

    // 关于
    ui->tabWidget->addTab(new AboutForm(this), tr("About"));

    ui->tabWidget->tabBar()->setCursor(Qt::PointingHandCursor);
    reloadTheme();

    QString locale = lib::settings::OkSettings().getTranslation();
    settings::Translator::translate(OK_Config_MODULE, locale);

    retranslateUi();
    auto a = ok::Application::Instance();
    connect(a->bus(), &ok::Bus::languageChanged,this,
            [&](const QString& locale0) {
                retranslateUi();
            });
}

ConfigWindow::~ConfigWindow() {
    delete ui;
}

void ConfigWindow::reloadTheme() {
    auto& style = lib::settings::Style::getStylesheet("general.css");
    setStyleSheet(style);
}

void ConfigWindow::retranslateUi() {
    ui->retranslateUi(this);

#if OK_PLUGIN
    ui->tabWidget->setTabText(0, tr("System settings"));
    ui->tabWidget->setTabText(1, tr("Management"));
    ui->tabWidget->setTabText(2, tr("About"));
#else
    ui->tabWidget->setTabText(0, tr("System settings"));
    ui->tabWidget->setTabText(1, tr("About"));
#endif

    for (int i = 0; i < ui->tabWidget->count(); i++) {
        auto gf = static_cast<lib::ui::GenericForm*>(ui->tabWidget->widget(i));
        gf->retranslateUi();
    }
}

}  // namespace module::config
