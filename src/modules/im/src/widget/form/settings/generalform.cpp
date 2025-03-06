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

#include "generalform.h"
#include "ui_generalsettings.h"

#include <QDesktopWidget>
#include <QFileDialog>

#include "src/core/core.h"
#include "src/lib/storage/settings/style.h"
#include "src/persistence/settings.h"
#include "src/widget/form/settingswidget.h"
#include "src/Bus.h"
#include "src/application.h"
#include "src/nexus.h"
#include "src/widget/widget.h"

namespace module::im {

/**
 * @class GeneralForm
 *
 * This form contains all settings that are not suited to other forms
 */
GeneralForm::GeneralForm(SettingsWidget* parent)
        : GenericForm(QPixmap(":/img/settings/general.png"), parent)
        , bodyUI(new Ui::GeneralSettings) {

    setContentsMargins(0, 0, 0, 0);

    setAttribute(Qt::WA_TranslucentBackground, true);

    bodyUI->setupUi(this);

    // block all child signals during initialization
    // const ok::base::RecursiveSignalBlocker signalBlocker(this);

    // eventsInit();

    retranslateUi();

    auto bus = ok::Application::Instance()->bus();
    connect(bus, &ok::Bus::languageChanged,
            [&](const QString& locale0) {
                retranslateUi();
            });
    connect(bus, &ok::Bus::profileChanged, this, &GeneralForm::onProfileChanged);


    auto css = lib::settings::Style::getStylesheet("settings/mainHead.css");
    setStyleSheet(css);
    bodyUI->scrollArea->setStyleSheet(css);

}

GeneralForm::~GeneralForm() {
    
    delete bodyUI;
}

void GeneralForm::on_notify_stateChanged() {
    const bool notify = bodyUI->notify->isChecked();
    Nexus::getProfile()->getSettings()->setNotify(notify);
    bodyUI->groupOnlyNotfiyWhenMentioned->setEnabled(notify);
    bodyUI->notifySound->setEnabled(notify);
    bodyUI->busySound->setEnabled(notify && bodyUI->notifySound->isChecked());
    bodyUI->desktopNotify->setEnabled(notify);
}
/*
void GeneralForm::on_notifySound_stateChanged()
{
    const bool notify = bodyUI->notifySound->isChecked();
    Nexus::getProfile()->getSettings()->setNotifySound(notify);
    bodyUI->busySound->setEnabled(notify);
}
*/
void GeneralForm::on_desktopNotify_stateChanged() {
    const bool notify = bodyUI->desktopNotify->isChecked();
    Nexus::getProfile()->getSettings()->setDesktopNotify(notify);
}

void GeneralForm::on_busySound_stateChanged() {
    Nexus::getProfile()->getSettings()->setBusySound(bodyUI->busySound->isChecked());
}

void GeneralForm::on_groupOnlyNotfiyWhenMentioned_stateChanged() {
    // Note: UI is boolean inversed from settings to maintain setting file backwards compatibility
    Nexus::getProfile()->getSettings()->setGroupAlwaysNotify(
            !bodyUI->groupOnlyNotfiyWhenMentioned->isChecked());
}

/**
 * @brief Retranslate all elements in the form.
 */

void GeneralForm::retranslateUi() {
    bodyUI->retranslateUi(this);
}

void GeneralForm::onProfileChanged(Profile* profile) {
    auto s = profile->getSettings();

    const QFont chatBaseFont = s->getChatMessageFont();
    bodyUI->txtChatFontSize->setValue(QFontInfo(chatBaseFont).pixelSize());
    bodyUI->txtChatFont->setCurrentFont(chatBaseFont);
    int index = static_cast<int>(s->getStylePreference());
    bodyUI->textStyleComboBox->setCurrentIndex(index);
    bodyUI->useNameColors->setChecked(s->getEnableGroupChatsColor());

    bodyUI->notify->setChecked(s->getNotify());
    // Note: UI is boolean inversed from settings to maintain setting file backwards compatibility
    bodyUI->groupOnlyNotfiyWhenMentioned->setChecked(!s->getGroupAlwaysNotify());
    bodyUI->groupOnlyNotfiyWhenMentioned->setEnabled(s->getNotify());
    bodyUI->notifySound->setChecked(s->getNotifySound());
    bodyUI->notifyHide->setChecked(s->getNotifyHide());
    bodyUI->notifySound->setEnabled(s->getNotify());
    bodyUI->busySound->setChecked(s->getBusySound());
    bodyUI->busySound->setEnabled(s->getNotifySound() && s->getNotify());
#if DESKTOP_NOTIFICATIONS
    bodyUI->desktopNotify->setChecked(s->getDesktopNotify());
    bodyUI->desktopNotify->setEnabled(s->getNotify());
#else
    bodyUI->desktopNotify->hide();
#endif
}

void GeneralForm::on_txtChatFont_currentFontChanged(const QFont& f) {
    qDebug() << __func__ << f;
    QFont tmpFont = f;
    const int px = bodyUI->txtChatFontSize->value();

    if (QFontInfo(tmpFont).pixelSize() != px) tmpFont.setPixelSize(px);

    Nexus::getProfile()->getSettings()->setChatMessageFont(tmpFont);
}

void GeneralForm::on_txtChatFontSize_valueChanged(int px) {
    qDebug() << __func__ << px;

    auto s = Nexus::getProfile()->getSettings();
    QFont tmpFont = s->getChatMessageFont();
    const int fontSize = QFontInfo(tmpFont).pixelSize();

    if (px != fontSize) {
        tmpFont.setPixelSize(px);
        s->setChatMessageFont(tmpFont);
    }
}
}  // namespace module::im
