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

#ifndef USERINTERFACEFORM_H
#define USERINTERFACEFORM_H

#include "BaseSettingsForm.h"
#include "src/persistence/profile.h"
#include "SettingsWidget.h"


namespace Ui {
class StorageSettingsForm;
}

namespace module::im {
/**
 * @brief The StorageSettingsForm class
 */
class StorageSettingsForm : public BaseSettingsForm {
    Q_OBJECT
public:
    explicit StorageSettingsForm(SettingsWidget* myParent);
    ~StorageSettingsForm();
    virtual QString getFormName() final override {
        return tr("Storage Settings");
    }

private slots:
    //    void on_statusChanges_stateChanged();
    //    void on_groupJoinLeaveMessages_stateChanged();
    //    void on_autoAwaySpinBox_editingFinished();
    void on_autoSaveFilesDir_clicked();
    void on_autoacceptFiles_stateChanged();
    void on_maxAutoAcceptSizeMB_editingFinished();

private:
    void retranslateUi();

private:
    SettingsWidget* parent;
    Ui::StorageSettingsForm* bodyUI;

private slots:
    void onProfileChanged(Profile* profile);
};
}  // namespace module::im
#endif
