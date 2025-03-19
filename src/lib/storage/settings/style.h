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

#ifndef STYLE_H
#define STYLE_H

#include <QColor>
#include <QDir>
#include <QFile>
#include <QFont>

class QString;
class QWidget;

namespace lib::settings {


enum class MainTheme { Light, Dark };

struct ThemeNameColor {
    MainTheme type;
    QString name;
    QColor color;
};

class Style : public QObject {
    Q_OBJECT
public:
    enum class Font { ExtraBig, Big, BigBold, Medium, MediumBold, Small, SmallLight };

    enum class ColorPalette {
        TransferGood,
        TransferWait,
        TransferBad,
        TransferMiddle,
        MainText,
        NameActive,
        StatusActive,
        GroundExtra,
        GroundBase,
        Orange,
        OrangeActive,
        ThemeDark,
        ThemeMediumDark,
        ThemeMedium,
        ThemeLight,
        ThemeHighlight,
        Action,
        Link,
        SearchHighlighted,
        SelectText
    };
    static QStringList getThemeColorNames();
    const QString getStylesheet(const QString& filename, const QFont& baseFont = QFont());
    const QString getModuleStylesheet( const QString& module, const QString& filename);
    const QIcon getModuleIcon(const QString& module, const QString& filename);
    QString getImagePath(const QString& filename);

    QString getThemeFolder();

    const QString& getTheme() const {
        return theme;
    };
    void setTheme(MainTheme theme);

    QColor getColor(ColorPalette entry);
    static QColor getExtColor(const QString& key);
    static QFont getFont(Font font);


    static void applyTheme();
    static void initPalette(const QString &path);
    static void initDictColor();

    QString getThemePath();
    QString getThemeFile();

    static Style* getInstance();

private:
    Style();
    const QString resolve(const QString& fullPath, const QFont& baseFont = QFont());

private:
    static QList<ThemeNameColor> themeNameColors;
    static std::map<std::pair<const QString, const QFont>, const QString> stylesheetsCache;
    static QMap<ColorPalette, QString> aliasColors;

    QString theme;
    QDir dir;

signals:
    void themeChanged();

};


}
#endif  // STYLE_H
