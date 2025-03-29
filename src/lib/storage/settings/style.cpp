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

#include "style.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFontInfo>
#include <QIcon>
#include <QMap>
#include <QPainter>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QStringBuilder>
#include <mutex>


#include "OkSettings.h"

/**
 * @enum Style::Font
 *
 * @var ExtraBig
 * @brief [SystemDefault + 2]px, bold
 *
 * @var Big
 * @brief [SystemDefault]px
 *
 * @var BigBold
 * @brief [SystemDefault]px, bold
 *
 * @var Medium
 * @brief [SystemDefault - 1]px
 *
 * @var MediumBold
 * @brief [SystemDefault - 1]px, bold
 *
 * @var Small
 * @brief [SystemDefault - 2]px
 *
 * @var SmallLight
 * @brief [SystemDefault - 2]px, light
 *
 * @var BuiltinThemePath
 * @brief Path to the theme built into the application binary
 */

namespace {
static std::mutex mutex;
const QLatin1String ThemeFolder{":themes/"};
const QLatin1String ThemeExt{".ini"};

}  // namespace

// helper functions
QFont appFont(int pixelSize, int weight) {
    QFont font;
    font.setPixelSize(pixelSize);
    font.setWeight(weight);
    return font;
}

QString qssifyFont(QFont font) {
    return QString("%1 %2px \"%3\"")
            .arg(font.weight() * 8)
            .arg(font.pixelSize())
            .arg(font.family());
}

namespace lib::settings {

static QMap<QString, QColor> palette;
static QMap<QString, QColor> extPalette;
static QMap<QString, QString> dictFont;
static QMap<QString, QString> dictTheme;
static QMap<QString, QString> dictExtColor;

Style::Style():QObject(), dir(ThemeFolder)
{
    auto num = OkSettings::getInstance()->getThemeColor();
    setTheme(num);
}



QStringList Style::getThemeColorNames() {
    QStringList l;

    static QList<ThemeNameColor> ThemeNameColors = {//
        {MainTheme::Light, QObject::tr("Light"), QColor("#FFFFFF")},//
        {MainTheme::Dark, QObject::tr("Dark"), QColor("#000000")},//
    };

    for (auto& t : ThemeNameColors) {
        l << t.name;
    }
    return l;
}


QString Style::getThemeFolder() {
    return dir.path();
}

void Style::setTheme(MainTheme theme_)
{
    switch (theme_) {
        case MainTheme::Light:
            theme="light";
            break;
        case MainTheme::Dark:
            theme = "dark";
            break;
        default:
            break;
    }
}

QMap<Style::ColorPalette, QString> Style::aliasColors = {{ColorPalette::TransferGood, "transferGood"},
                                                         {ColorPalette::TransferWait, "transferWait"},
                                                         {ColorPalette::TransferBad, "transferBad"},
                                                         {ColorPalette::TransferMiddle, "transferMiddle"},
                                                         {ColorPalette::MainText, "mainText"},
                                                         {ColorPalette::NameActive, "nameActive"},
                                                         {ColorPalette::StatusActive, "statusActive"},
                                                         {ColorPalette::GroundExtra, "groundExtra"},
                                                         {ColorPalette::GroundBase, "groundBase"},
                                                         {ColorPalette::Orange, "orange"},
                                                         {ColorPalette::ThemeDark, "themeDark"},
                                                         {ColorPalette::ThemeMediumDark, "themeMediumDark"},
                                                         {ColorPalette::ThemeMedium, "themeMedium"},
                                                         {ColorPalette::ThemeLight, "themeLight"},
                                                         {ColorPalette::ThemeHighlight, "themeHighlight"},
                                                         {ColorPalette::Action, "action"},
                                                         {ColorPalette::Link, "link"},
                                                         {ColorPalette::SearchHighlighted, "searchHighlighted"},
                                                         {ColorPalette::SelectText, "selectText"}};


const QString Style::getStylesheet(const QString& filename, const QFont& baseFont) {
    const QString fullPath = ":styles/" + filename;
    return resolve(fullPath, baseFont);
}

const QString Style::getModuleStylesheet(const QString &module,const QString &filename)
{
    const QString fullPath = ":"+module+"/styles/" + filename;
    return resolve(fullPath, QFont());
}

const QIcon Style::getModuleIcon(const QString &module, const QString &filename)
{
    return QIcon(":"+module+"/icons/"+filename);
}

QString Style::getImagePath(const QString& filename) {
    QString fullPath = ":icons/"+filename;

    if (QFileInfo::exists(fullPath)) {
        return fullPath;
    }

    fullPath = ":icons/" % theme % "/" % filename;
    if (QFileInfo::exists(fullPath)) {
        return fullPath;
    }

    qWarning() << "Failed to open default file:" << fullPath;
    return {};
}

QColor Style::getColor(Style::ColorPalette entry) {
    auto x = aliasColors[entry];
    return palette[x];
}

QColor Style::getExtColor(const QString& key) {
    return extPalette.value(key, QColor(0, 0, 0));
}

QFont Style::getFont(Style::Font font) {
    static int defSize = QFontInfo(QFont()).pixelSize();
    static QFont fonts[] = {
            appFont(defSize + 3, QFont::Bold),    // extra big
            appFont(defSize + 1, QFont::Normal),  // big
            appFont(defSize + 1, QFont::Bold),    // big bold
            appFont(defSize, QFont::Normal),      // medium
            appFont(defSize, QFont::Bold),        // medium bold
            appFont(defSize - 1, QFont::Normal),  // small
            appFont(defSize - 1, QFont::Light),   // small light
    };

    return fonts[(int)font];
}

const QString Style::resolve(const QString& fullPath, const QFont& baseFont) {
    qDebug() << __func__ <<"path:" << fullPath;
    QString qss;
    QFile file{fullPath};
    if (file.open(QFile::ReadOnly | QFile::Text)) {
        qss = file.readAll();
    }else{
        return {};
    }

    if (palette.isEmpty()) {
        initPalette(getThemeFile());
    }

    if (dictFont.isEmpty()) {
        dictFont = {{"@baseFont", QString::fromUtf8("'%1' %2px").arg(baseFont.family()).arg(QFontInfo(baseFont).pixelSize())},
                    {"@extraBig", qssifyFont(Style::getFont(Style::Font::ExtraBig))},
                    {"@big", qssifyFont(Style::getFont(Style::Font::Big))},
                    {"@bigBold", qssifyFont(Style::getFont(Style::Font::BigBold))},
                    {"@medium", qssifyFont(Style::getFont(Style::Font::Medium))},
                    {"@mediumBold", qssifyFont(Style::getFont(Style::Font::MediumBold))},
                    {"@small", qssifyFont(Style::getFont(Style::Font::Small))},
                    {"@smallLight", qssifyFont(Style::getFont(Style::Font::SmallLight))}};
    }

    if (dictExtColor.isEmpty() && !extPalette.isEmpty()) {
        auto it = extPalette.begin();
        while (it != extPalette.end()) {
            dictExtColor.insert("@" + it.key(), it.value().name());
            it++;
        }
    }

    QRegularExpression anchorReg(R"(@([a-zA-z0-9\.]+))");
    int from = 0;
    int index = qss.indexOf('@');
    while (index >= 0) {
        QRegularExpressionMatch match = anchorReg.match(qss, from);
        if (match.hasMatch()) {
            QString key = match.captured(0);
            // c++17
            if (auto it = palette.find(key); it != palette.end())
                qss.replace(key, it.value().name(QColor::HexArgb));
            else if (auto it = dictFont.find(key); it != dictFont.end())
                qss.replace(key, it.value());
            else if (auto it = dictTheme.find(key); it != dictTheme.end())
                qss.replace(key, it.value());
            else if (auto it = dictExtColor.find(key); it != dictExtColor.end())
                qss.replace(key, it.value());
        }
        from++;
        index = qss.indexOf('@', from);
    }

            // @getImagePath() function
    const QRegularExpression re{QStringLiteral(R"(@getImagePath\([^)\s]*\))")};
    QRegularExpressionMatchIterator i = re.globalMatch(qss);

    while (i.hasNext()) {
        QRegularExpressionMatch match = i.next();
        QString path = match.captured(0);
        const QString phrase = path;

        path.remove(QStringLiteral("@getImagePath("));
        path.chop(1);

        QString fullImagePath = getThemeFolder() + path;
        qss.replace(phrase, fullImagePath);
    }

    return qss;
}


/**
 * @brief Reloads some CCS
 */
void Style::applyTheme() {
    //    GUI::reloadTheme();
}

void Style::initPalette(const QString& path) {
    QSettings settings(path, QSettings::IniFormat);

    settings.beginGroup("colors");

    auto an = settings.value("nameActive").toString();


    QMap<Style::ColorPalette, QString> c;
    // auto keys = aliasColors.keys();
    for (auto k : settings.allKeys()) {
        // c[k] = settings.value(aliasColors[k], "#000").toString();
        palette["@"+k] = QColor(settings.value(k, "#000").toString());
    }
    auto p = palette;
    settings.endGroup();

    settings.beginGroup("extends-colors");
    for (auto k : settings.childKeys()) {
        QColor color(settings.value(k).toString());
        if (color.isValid()) extPalette.insert(k, color);
    }
    settings.endGroup();
}

void Style::initDictColor() {
    // dictColor = {{"@transferGood", Style::getColor(Style::ColorPalette::TransferGood).name()},
    //              {"@transferWait", Style::getColor(Style::ColorPalette::TransferWait).name()},
    //              {"@transferBad", Style::getColor(Style::ColorPalette::TransferBad).name()},
    //              {"@transferMiddle", Style::getColor(Style::ColorPalette::TransferMiddle).name()},
    //              {"@mainText", Style::getColor(Style::ColorPalette::MainText).name()},
    //              {"@nameActive", Style::getColor(Style::ColorPalette::NameActive).name()},
    //              {"@statusActive", Style::getColor(Style::ColorPalette::StatusActive).name()},
    //              {"@groundExtra", Style::getColor(Style::ColorPalette::GroundExtra).name()},
    //              {"@groundBase", Style::getColor(Style::ColorPalette::GroundBase).name()},
    //              {"@orange", Style::getColor(Style::ColorPalette::Orange).name()},
    //              {"@action", Style::getColor(Style::ColorPalette::Action).name()},
    //              {"@link", Style::getColor(Style::ColorPalette::Link).name()},
    //              {"@searchHighlighted", Style::getColor(Style::ColorPalette::SearchHighlighted).name()},
    //              {"@selectText", Style::getColor(Style::ColorPalette::SelectText).name()},
    //              {"@themeMedium", Style::getColor(Style::ColorPalette::ThemeMedium).name()},
    //              {"@themeMediumDark", Style::getColor(Style::ColorPalette::ThemeMediumDark).name()},
    //              {"@themeDark", Style::getColor(Style::ColorPalette::ThemeDark).name()},
    //              {"@themeLight", Style::getColor(Style::ColorPalette::ThemeLight).name()},
    //              {"@themeHighlight", Style::getColor(Style::ColorPalette::ThemeHighlight).name()},
    //              };
}

QString Style::getThemePath() {
    return dir.path();
}

QString Style::getThemeFile()
{
    return dir.filePath(theme+ThemeExt);
}


Style* Style::getInstance()
{
    std::lock_guard<std::mutex> lock(mutex);
    static Style* s = nullptr;
    if(!s){
        s = new Style;
    }
    return s;
}

}  // namespace lib::settings
