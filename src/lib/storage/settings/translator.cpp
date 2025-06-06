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
#include <algorithm>

namespace settings {

// module, QTranslator
static QMap<QString, QTranslator*> m_translatorMap{};
static QVector<Callback> callbacks{};
// static QMutex Translator::lock;
static bool m_loadedQtTranslations{false};

/**
 * @brief Loads the translations according to the settings or locale.
 */
void Translator::translate(const QString& moduleName, const QString& localeName) {
    qDebug() << "translate module:" << moduleName << "locale:" << localeName;
    QMutexLocker locker{&lock};

    auto* translator = m_translatorMap.value(moduleName);
    if (translator) {
        QCoreApplication::removeTranslator(translator);
        m_translatorMap.remove(moduleName);
        delete translator;
    }

    translator = new QTranslator();

    // Load translations
    QString locale =
            localeName.isEmpty() ? QLocale::system().name().section('_', 0, 0) : localeName;
    //    qDebug() << "Loaded locale" << locale << "=>" << moduleName;
    if (locale != "en") {
        if (!m_loadedQtTranslations) {
            QTranslator* qtTranslator = new QTranslator();
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

        // 增加 {translations}/{module}
        QString path = ":translations/" + moduleName;
        qDebug() << "Loading translation path" << path << "locale" << locale;
        if (translator->load(locale + ".qm", path)) {
            qDebug() << "Loaded translation successful locale" << locale << "for" << moduleName;
            bool installed = QCoreApplication::installTranslator(translator);
            qDebug() << "Installed translator locale" << locale << "for" << moduleName << "=>"
                     << installed;

            m_translatorMap.insert(moduleName, translator);
        } else {
            delete translator;
            qWarning() << "Error loading translation" << locale << "for" << moduleName;
            return;
        }
    }

    // TODO RTL
    // After the language is changed from RTL to LTR, the layout direction isn't
    // always restored
    // const QString direction = QCoreApplication::tr("LTR",
                                               // "Translate this string to the string 'RTL' in"
                                               // " right-to-left languages (for example Hebrew and"
                                               // " Arabic) to get proper widget layout");

    // QCoreApplication::setLayoutDirection(direction == "RTL" ? Qt::RightToLeft : Qt::LeftToRight);

    for (auto& pair : callbacks) {
        pair.second();
    }
}

}  // namespace settings
