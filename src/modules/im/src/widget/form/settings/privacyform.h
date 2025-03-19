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

#ifndef PRIVACYFORM_H
#define PRIVACYFORM_H

#include "BaseSettingsForm.h"

namespace Ui {
class PrivacySettings;
}
namespace module::im {

class PrivacyForm : public BaseSettingsForm {
    Q_OBJECT
public:
    PrivacyForm();
    ~PrivacyForm() override;
    
    QString getFormName() override {
        return tr("Privacy");
    }

protected:
  void showEvent(QShowEvent*) override;

private:
    Ui::PrivacySettings* bodyUI;

signals:
    void clearAllReceipts();
};
}  // namespace module::im
#endif
