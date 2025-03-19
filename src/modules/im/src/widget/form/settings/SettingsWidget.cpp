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

#include "SettingsWidget.h"


#include <QLabel>
#include <QStyle>
#include <QTabBar>
#include <QTabWidget>
#include <QWindow>

#include "src/application.h"
#include "src/nexus.h"
#include "src/widget/contentlayout.h"
#include "StorageSettingsForm.h"
#include "avform.h"
#include "GeneralForm.h"
#include "src/widget/widget.h"
#include "lib/storage/settings/translator.h"

#include <lib/storage/settings/style.h>

namespace module::im {

/**
 * @brief IM Settings widget
 * @param parent
 */
SettingsWidget::SettingsWidget(QWidget* parent)
        : lib::ui::OFrame(parent) {

    setObjectName("settingsWidget");

    bodyLayout = new QVBoxLayout(this);
    bodyLayout->setContentsMargins(0, 0, 0, 0);

    tab =  new QTabWidget(this);
    tab->setObjectName("subTab");
    tab->setTabPosition(QTabWidget::North);
    connect(tab, &QTabWidget::currentChanged, this,
            &SettingsWidget::onTabChanged);

    // General settings
    gfrm = new GeneralForm(this);
    // Storage settings
    uifrm = new StorageSettingsForm(this);
    // AV settings
    rawAvfrm = new AVForm(this);


// #if UPDATE_CHECK_ENABLED
//     if (updateCheck != nullptr) {
//         connect(updateCheck, &UpdateCheck::updateAvailable, this,
//                 &SettingsWidget::onUpdateAvailable);
//     } else {
//         qWarning() << "SettingsWidget passed null UpdateCheck!";
//     }
// #endif

    cfgForms.push_back(gfrm);
    cfgForms.push_back(uifrm);
    cfgForms.push_back(rawAvfrm);

    for (auto& f : cfgForms){
        f->setStyleSheet("padding: 0 10px;");
        tab->addTab(f, f->getFormName());
    }

    bodyLayout->addWidget(tab);
    setLayout(bodyLayout);


    reloadTheme();

    retranslateUi();
    auto a = ok::Application::Instance();
    connect(a->bus(), &ok::Bus::languageChanged,this,
            [&](const QString& locale0) {
                retranslateUi();
            });

}

SettingsWidget::~SettingsWidget() {
    
}

void SettingsWidget::setBodyHeadStyle(QString style) {
    tab->setStyle(QStyleFactory::create(style));
}

void SettingsWidget::showAbout() {
    onTabChanged(tab->count() - 1);
}

void SettingsWidget::reloadTheme()
{
    auto css = lib::settings::Style::getInstance()->getStylesheet("settings.css");
    setStyleSheet(css);
}

bool SettingsWidget::isShown() const {
    if (tab->isVisible()) {
        tab->window()->windowHandle()->alert(0);
        return true;
    }
    return false;
}

void SettingsWidget::show(ContentLayout* contentLayout) {
    tab->show();
    onTabChanged(tab->currentIndex());
}

void SettingsWidget::onTabChanged(int index) {
    tab->setCurrentIndex(index);
}

void SettingsWidget::onUpdateAvailable() {
    tab->tabBar()->setProperty("update-available", true);
    tab->tabBar()->style()->unpolish(tab->tabBar());
    tab->tabBar()->style()->polish(tab->tabBar());
}

void SettingsWidget::retranslateUi() {
    for (int i = 0; i < cfgForms.size(); ++i){
        auto n = cfgForms[i]->getFormName();
        tab->setTabText(i, n);
    }
}
}  // namespace module::im
