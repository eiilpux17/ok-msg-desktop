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

#include "translator.h"
#include <QCoreApplication>
#include <QDebug>
#include <QLibraryInfo>
#include <QLocale>
#include <QMutexLocker>
#include <QString>
#include <QTranslator>

namespace settings {

// module, QTranslator
static QMap<QString, QTranslator*> m_translatorMap{};
static bool m_loadedQtTranslations{false};

static QMutex mutex;

/**
 * @brief Loads the translations according to the settings or locale.
 */
void Translator::translate(const QString& moduleName, const QString& localeName) {
    qDebug() << __func__ << "module:" << moduleName << "locale:" << localeName;
    QMutexLocker locker{&mutex};

            // Load translations
    QString locale = localeName.isEmpty() ? QLocale::system().name().section('_', 0, 0) : localeName;


    if (!m_loadedQtTranslations) {
        auto* qtTranslator = new QTranslator();
        QString s_locale = "qt_" + locale;
        QString location = QLibraryInfo::location(QLibraryInfo::TranslationsPath);
        if (qtTranslator->load(s_locale, location)) {
            QCoreApplication::installTranslator(qtTranslator);
            qDebug() << "System translation loaded" << locale;
        } else {
            qDebug() << "System translation not loaded" << locale;
        }
        m_loadedQtTranslations = true;
    }


    //移除已存在的
    remove(moduleName);

    // 增加 {translations}/{module}
    auto translator = new QTranslator();
    auto path = ":translations/" + moduleName;
    qDebug() << "Loading translation path:" << path;
    if (!translator->load(locale + ".qm", path)) {
        qWarning() << "Error loading translation!";
        delete translator;
        return;
    }

    qDebug() << "Loaded translation locale is:" << locale;
    bool installed = QCoreApplication::installTranslator(translator);
    if(!installed){
        qWarning() << "Installed translator failed!";
        delete translator;
        return;
    }
    m_translatorMap.insert(moduleName, translator);
    qDebug() << "Install translation for module:" << moduleName <<" was successfull.";


            // TODO RTL
            // After the language is changed from RTL to LTR, the layout direction isn't
            // always restored
            // const QString direction = QCoreApplication::tr("LTR",
            // "Translate this string to the string 'RTL' in"
            // " right-to-left languages (for example Hebrew and"
            // " Arabic) to get proper widget layout");

            // QCoreApplication::setLayoutDirection(direction == "RTL" ? Qt::RightToLeft : Qt::LeftToRight);


}

void Translator::remove(const QString &moduleName)
{

    auto* translator = m_translatorMap.value(moduleName);
    if (translator) {
        qDebug() << __func__ << moduleName << translator;
        QCoreApplication::removeTranslator(translator);
        m_translatorMap.remove(moduleName);
        delete translator;
    }

}

}  // namespace settings
