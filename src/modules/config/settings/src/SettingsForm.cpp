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

#include <QLabel>
#include <QTabWidget>
#include <QWidget>
#include <QWindow>

#include "Bus.h"
#include "ConnectForm.h"
#include "GlobalGeneralForm.h"
#include "SettingsForm.h"
#include "application.h"
#include "lib/storage/settings/style.h"

namespace module::config {

SettingsForm::SettingsForm(QWidget* parent)
        : GenericForm(QPixmap(":/img/settings/general.png"), parent) {
    layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    tab = new QTabWidget(this);
    tab->setTabPosition(QTabWidget::North);
    tab->setObjectName("subTab");
    cfgForms = {
            new GeneralForm(this),
            new ConnectForm(this)
    };
    for (auto& cfgForm : cfgForms)
        tab->addTab(cfgForm, cfgForm->getFormName());


    layout->addWidget(tab);
    setLayout(layout);

    connect(tab, &QTabWidget::currentChanged, this,
            &SettingsForm::onTabChanged);

    auto& style = lib::settings::Style::getStylesheet("general.css");
    setStyleSheet(style);

    retranslateUi();

    auto a = ok::Application::Instance();
    connect(a->bus(), &ok::Bus::languageChanged,this,
            [&](const QString& locale0) {
                retranslateUi();
            });
}

void SettingsForm::setBodyHeadStyle(QString style) {
    tab->setStyle(QStyleFactory::create(style));
}

void SettingsForm::showAbout() {
    onTabChanged(tab->count() - 1);
}

bool SettingsForm::isShown() const {
    if (tab->isVisible()) {
        tab->window()->windowHandle()->alert(0);
        return true;
    }

    return false;
}

void SettingsForm::onTabChanged(int index) {
    tab->setCurrentIndex(index);
}

void SettingsForm::onUpdateAvailable(void) {
    //    settingsWidgets->tabBar()->setProperty("update-available", true);
    //    settingsWidgets->tabBar()->style()->unpolish(settingsWidgets->tabBar());
    //    settingsWidgets->tabBar()->style()->polish(settingsWidgets->tabBar());
}

void SettingsForm::retranslateUi() {
    for (int i = 0; i < cfgForms.size(); ++i) {
        tab->setTabText(i, cfgForms.at(i)->getFormName());
        cfgForms.at(i)->retranslateUi();
    }
}

}  // namespace UI
