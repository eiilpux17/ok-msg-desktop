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

#ifndef SETTINGSWIDGET_H
#define SETTINGSWIDGET_H

#include <QHBoxLayout>
#include <QPushButton>
#include <QStyleFactory>
#include "lib/ui/widget/OFrame.h"


class QLabel;
class QTabWidget;


namespace module::im {

class GeneralForm;
class BaseSettingsForm;
class IAudioControl;
class AVForm;
class StorageSettingsForm;

class ContentLayout;
class Widget;

/**
 * 设置界面
 */
class SettingsWidget : public lib::ui::OFrame {
    Q_OBJECT
public:
    explicit SettingsWidget(QWidget* parent = nullptr);
    ~SettingsWidget() override;

    bool isShown() const;
    void show(ContentLayout* contentLayout);
    void setBodyHeadStyle(QString style);

    void showAbout();

protected:
    void reloadTheme() override;

private:
    void retranslateUi();

    QVBoxLayout* bodyLayout;
    QTabWidget* tab;

    GeneralForm* gfrm;
    AVForm* rawAvfrm;
    StorageSettingsForm* uifrm;

    std::vector<BaseSettingsForm*> cfgForms;
    int currentIndex;

public slots:
    void onUpdateAvailable(void);

private slots:
    void onTabChanged(int);
};
}  // namespace module::im
#endif  // SETTINGSWIDGET_H
