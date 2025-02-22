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
#include "ui_ConfigWindow.h"

#include <memory>

#include <QWidget>

#include "lib/storage/settings/OkSettings.h"
#include "lib/storage/settings/translator.h"
#include "src/base/basic_types.h"
#include "src/base/widgets.h"

#if OK_PLUGIN
#include "about/src/aboutform.h"
#include "lib/storage/settings/style.h"
#include "plugin/src/PluginManagerForm.h"
#include "settings/src/SettingsForm.h"

#include <settings/src/GeneralForm.h>
#endif

namespace UI {

ConfigWindow::ConfigWindow(QWidget* parent) : OMenuWidget(parent), ui(new Ui::ConfigWindow) {
    OK_RESOURCE_INIT(UIWindowConfig);
    OK_RESOURCE_INIT(UIWindowConfigRes);

    ui->setupUi(this);

    auto sw = new SettingsWidget(this);
    connect(sw->general(), &GeneralForm::onLanguageChanged, [](QString locale) {
        settings::Translator::translate(OK_UIWindowConfig_MODULE, locale);
    });
    ui->tabWidget->setObjectName("mainTab");
#if OK_PLUGIN
    ui->tabWidget->addTab(new ok::plugin::PluginManagerForm(this), tr("Plugin form"));
#endif
    ui->tabWidget->addTab(sw, tr("Settings form"));
    ui->tabWidget->addTab(new AboutForm(this), tr("About form"));
    ui->tabWidget->tabBar()->setCursor(Qt::PointingHandCursor);
    reloadTheme();

    QString locale = lib::settings::OkSettings().getTranslation();
    settings::Translator::translate(OK_UIWindowConfig_MODULE, locale);
    settings::Translator::registerHandler([this] { retranslateUi(); }, this);
    retranslateUi();
}

ConfigWindow::~ConfigWindow() {
    settings::Translator::unregister(this);
    delete ui;
}

void ConfigWindow::reloadTheme() {
    auto& style = lib::settings::Style::getStylesheet("general.css");
    setStyleSheet(style);
}

void ConfigWindow::retranslateUi() {
    ui->retranslateUi(this);
    ui->tabWidget->setTabText(0, tr("Plugin form"));
    ui->tabWidget->setTabText(1, tr("Settings form"));
    ui->tabWidget->setTabText(2, tr("About form"));

    for (int i = 0; i < ui->tabWidget->count(); i++) {
        auto gf = static_cast<GenericForm*>(ui->tabWidget->widget(i));
        gf->retranslateUi();
    }
}

}  // namespace UI
