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

#pragma once

#include <QHBoxLayout>
#include <QPushButton>
#include <QStyleFactory>

#include <QTabWidget>
#include "GlobalGeneralForm.h"
#include "lib/ui/widget/GenericForm.h"

namespace module::config {

class SettingsForm : public lib::ui::GenericForm {
    Q_OBJECT
public:
    explicit SettingsForm(QWidget* parent = nullptr);
    ~SettingsForm() override {}

    virtual QString getFormName() final override {
        return tr("Settings");
    }

    bool isShown() const;
    void setBodyHeadStyle(QString style);

    void showAbout();
    GeneralForm* general() {
        return static_cast<GeneralForm*>(cfgForms.at(0));
    }
    void retranslateUi() override;

public slots:
    void onUpdateAvailable(void);

private slots:
    void onTabChanged(int);

private:
    QVBoxLayout* layout;
    QTabWidget* tab;
    std::vector<GenericForm*> cfgForms;
    int currentIndex = 0;
};
}  // namespace module::config
