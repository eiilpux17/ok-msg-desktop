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

#ifndef GENERALFORM_H
#define GENERALFORM_H

#include <QLabel>

#include "BaseSettingsForm.h"
#include "src/persistence/profile.h"


namespace Ui {
class GeneralForm;
}

namespace module::im {

class SettingsWidget;

/**
 * @brief The GeneralForm class
 */
class GeneralForm : public BaseSettingsForm {
    Q_OBJECT
public:
    explicit GeneralForm(SettingsWidget* parent);
    ~GeneralForm() override;
    QString getFormName() override {
        return tr("General");
    }

signals:
    void updateIcons();

private slots:

    void on_notify_stateChanged();
    void on_desktopNotify_stateChanged();

    void on_groupOnlyNotfiyWhenMentioned_stateChanged();

    void on_txtChatFont_currentFontChanged(const QFont& f);
    void on_txtChatFontSize_valueChanged(int px);

    // void on_useNameColors_stateChanged(int value);

private:
    void retranslateUi();
    void reloadSmileys();

private:
    Ui::GeneralForm* bodyUI;
    QList<QLabel*> smileLabels;
    QList<std::shared_ptr<QIcon>> emoticonsIcons;
    const int MAX_FORMAT_LENGTH = 128;

private slots:
    void onProfileChanged(Profile*);
};
}  // namespace module::im

#endif
