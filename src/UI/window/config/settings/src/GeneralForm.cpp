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

#include "GeneralForm.h"
#include <QFileDialog>
#include <QStyleFactory>
#include <cmath>
#include "Bus.h"
#include "application.h"
#include "lib/storage/settings/OkSettings.h"
#include "lib/storage/settings/style.h"
#include "lib/storage/settings/translator.h"
#include "src/base/RecursiveSignalBlocker.h"

namespace UI {
/**
 * @class GeneralForm
 *
 * This form contains all settings that are not suited to other forms
 */
GeneralForm::GeneralForm(SettingsWidget* myParent)
        : GenericForm(QPixmap(":/img/settings/general.png")), bodyUI(new Ui::GeneralForm) {
    parent = myParent;

    bodyUI->setupUi(this);

    // block all child signals during initialization
    const ok::base::RecursiveSignalBlocker signalBlocker(this);

    // 先获取当前语言
    auto& s = lib::settings::OkSettings::getInstance();

    QString locale0 = s.getTranslation();
    settings::Translator::translate(OK_UIWindowConfig_MODULE, locale0);
    settings::Translator::registerHandler([this] { retranslateUi(); }, this);
    retranslateUi();

#ifndef UPDATE_CHECK_ENABLED
    bodyUI->checkUpdates->setVisible(false);
#endif

#ifndef SPELL_CHECKING
    bodyUI->cbSpellChecking->setVisible(false);
#endif
    // 获取复选框状态
    //  bodyUI->checkUpdates->setChecked(s.getCheckUpdates());

    auto& okSettings = lib::settings::OkSettings::getInstance();

    for (int i = 0; i < okSettings.getLocales().size(); ++i) {
        QString langName;
        auto& locale = okSettings.getLocales().at(i);
        if (locale.startsWith(QLatin1String("eo")))  // QTBUG-57802
            langName = QLocale::languageToString(QLocale::Esperanto);
        else if (locale.startsWith(QLatin1String("jbo")))
            langName = QLatin1String("Lojban");
        else if (locale.startsWith(QLatin1String("pr")))
            langName = QLatin1String("Pirate");
        else if (locale == (QLatin1String("pt")))  // QTBUG-47891
            langName = QStringLiteral("português");
        else
            langName = QLocale(locale).nativeLanguageName();

        bodyUI->transComboBox->insertItem(i, langName);
    }
    // 当前语言下拉框状态
    bodyUI->transComboBox->setCurrentIndex(
            okSettings.getLocales().indexOf(okSettings.getTranslation()));

    // autorun
    bodyUI->cbAutorun->setChecked(okSettings.getAutorun());
    // 系统图标
    bodyUI->showSystemTray->setChecked(okSettings.getShowSystemTray());

    // 主题
    //    bodyUI->styleBrowser->addItem(tr("None"));
    //    bodyUI->styleBrowser->addItems(QStyleFactory::keys());
    //    QString style;
    //    if (QStyleFactory::keys().contains(s.getStyle()))
    //        style = s.getStyle();
    //    else
    //        style = tr("None");
    //    bodyUI->styleBrowser->setCurrentText(style);

    for (const QString& color : lib::settings::Style::getThemeColorNames())
        bodyUI->themeColorCBox->addItem(color);
    bodyUI->themeColorCBox->setCurrentIndex((int)s.getThemeColor());

    QLocale ql;
    QStringList timeFormats;
    timeFormats << ql.timeFormat(QLocale::ShortFormat) << ql.timeFormat(QLocale::LongFormat)
                << "hh:mm AP"
                << "hh:mm:ss AP"
                << "hh:mm:ss";
    timeFormats.removeDuplicates();
    bodyUI->timestamp->addItems(timeFormats);

    QRegularExpression re(QString("^[^\\n]{0,%0}$").arg(MAX_FORMAT_LENGTH));
    QRegularExpressionValidator* validator = new QRegularExpressionValidator(re, this);

    QString timeFormat = s.getTimestampFormat();

    if (!re.match(timeFormat).hasMatch()) timeFormat = timeFormats[0];

    bodyUI->timestamp->setCurrentText(timeFormat);
    bodyUI->timestamp->setValidator(validator);
    on_timestamp_editTextChanged(timeFormat);

    QStringList dateFormats;
    dateFormats << QStringLiteral("yyyy-MM-dd")  // ISO 8601
                                                 // format strings from system locale
                << ql.dateFormat(QLocale::LongFormat) << ql.dateFormat(QLocale::ShortFormat)
                << ql.dateFormat(QLocale::NarrowFormat) << "dd-MM-yyyy"
                << "d-MM-yyyy"
                << "dddd dd-MM-yyyy"
                << "dddd d-MM";

    dateFormats.removeDuplicates();
    bodyUI->dateFormats->addItems(dateFormats);

    QString dateFormat = s.getDateFormat();
    if (!re.match(dateFormat).hasMatch()) dateFormat = dateFormats[0];

    bodyUI->dateFormats->setCurrentText(dateFormat);
    bodyUI->dateFormats->setValidator(validator);
    on_dateFormats_editTextChanged(dateFormat);
}

GeneralForm::~GeneralForm() {
    settings::Translator::unregister(this);
    delete bodyUI;
}

void GeneralForm::on_transComboBox_currentIndexChanged(int index) {
    auto& s = lib::settings::OkSettings::getInstance();
    const QString& locale = s.getLocales().at(index);
    s.setTranslation(locale);
    s.saveGlobal();
    emit onLanguageChanged(locale);
    emit ok::Application::Instance() -> bus()->languageChanged(locale);
    settings::Translator::translate(OK_UIWindowConfig_MODULE, locale);
}

void GeneralForm::on_cbAutorun_stateChanged() {
    auto& s = lib::settings::OkSettings::getInstance();
    s.setAutorun(bodyUI->cbAutorun->isChecked());
    s.saveGlobal();
}

void GeneralForm::on_cbSpellChecking_stateChanged() {
    //  Nexus::getProfile()->getSettings()->setSpellCheckingEnabled(bodyUI->cbSpellChecking->isChecked());
}

void GeneralForm::on_showSystemTray_stateChanged() {
    auto& s = lib::settings::OkSettings::getInstance();
    s.setShowSystemTray(bodyUI->showSystemTray->isChecked());
    s.saveGlobal();
}

void GeneralForm::on_startInTray_stateChanged() {
    auto& s = lib::settings::OkSettings::getInstance();
    s.setAutostartInTray(bodyUI->startInTray->isChecked());
    s.saveGlobal();
}

void GeneralForm::on_closeToTray_stateChanged() {
    auto& s = lib::settings::OkSettings::getInstance();
    s.setCloseToTray(bodyUI->closeToTray->isChecked());
    s.saveGlobal();
}

void GeneralForm::on_minimizeToTray_stateChanged() {
    auto& s = lib::settings::OkSettings::getInstance();
    s.setMinimizeToTray(bodyUI->minimizeToTray->isChecked());
    s.saveGlobal();
}

void GeneralForm::on_checkUpdates_stateChanged() {
    //      Nexus::getProfile()->getSettings()->setCheckUpdates(bodyUI->checkUpdates->isChecked());
}

void GeneralForm::on_timestamp_editTextChanged(const QString& format) {
    QString timeExample = QTime::currentTime().toString(format);
    bodyUI->timeExample->setText(timeExample);

    //        Nexus::getProfile()->getSettings()->setTimestampFormat(format);
    //    QString locale = Nexus::getProfile()->getSettings()->getTranslation();
    //    settings::Translator::translate(OK_UIWindowConfig_MODULE, locale);
}

void GeneralForm::on_dateFormats_editTextChanged(const QString& format) {
    QString dateExample = QDate::currentDate().toString(format);
    bodyUI->dateExample->setText(dateExample);

    //    Nexus::getProfile()->getSettings()->setDateFormat(format);
    //    QString locale = Nexus::getProfile()->getSettings()->getTranslation();
    //    settings::Translator::translate(OK_UIWindowConfig_MODULE, locale);
}

void GeneralForm::on_themeColorCBox_currentIndexChanged(int) {
    int index = bodyUI->themeColorCBox->currentIndex();
    auto color = bodyUI->themeColorCBox->currentText();
    lib::settings::OkSettings().setThemeColor(static_cast<lib::settings::MainTheme>(index));
    emit ok::Application::Instance() -> bus()->themeColorChanged(index, color);
}

/**
 * @brief Retranslate all elements in the form.
 */
void GeneralForm::retranslateUi() {
    bodyUI->retranslateUi(this);
}

}  // namespace UI
