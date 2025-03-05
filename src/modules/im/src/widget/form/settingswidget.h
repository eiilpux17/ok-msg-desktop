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


class QLabel;
class QTabWidget;


namespace module::im {
class GenericForm;
class GeneralForm;
class IAudioControl;
class AVForm;
class StorageSettingsForm;

class ContentLayout;
class Widget;

/**
 * 设置界面
 */
class SettingsWidget : public QWidget {
    Q_OBJECT
public:
    SettingsWidget(QWidget* parent = nullptr);
    ~SettingsWidget();

    bool isShown() const;
    void show(ContentLayout* contentLayout);
    void setBodyHeadStyle(QString style);

    void showAbout();

public slots:
    void onUpdateAvailable(void);

private slots:
    void onTabChanged(int);

private:
    void retranslateUi();

private:
    QVBoxLayout* bodyLayout;
    QTabWidget* tab;

    GeneralForm* gfrm;
    AVForm* rawAvfrm;
    StorageSettingsForm* uifrm;

    std::vector<GenericForm*> cfgForms;
    int currentIndex;
};
}  // namespace module::im
#endif  // SETTINGSWIDGET_H
