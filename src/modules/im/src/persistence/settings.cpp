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

#include "settings.h"

#include <QApplication>
#include <QCryptographicHash>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFont>
#include <QList>
#include <QMutexLocker>
#include <QNetworkProxy>
#include <QStandardPaths>
#include <QStyleFactory>
#include <QThread>
#include <QtCore/QCommandLineParser>

#include "base/autorun.h"
#include "src/core/core.h"
#include "src/core/corefile.h"
#include "src/ipc.h"
#include "src/lib/settings/style.h"
#include "src/nexus.h"
#include "src/persistence/profile.h"
#include "src/persistence/profilelocker.h"
#include "src/persistence/settingsserializer.h"
#include "src/persistence/smileypack.h"
#include "base/OkSettings.h"
#include "base/compatiblerecursivemutex.h"

/**
 * @var QHash<QString, QByteArray> Settings::widgetSettings
 * @brief Assume all widgets have unique names
 * @warning Don't use it to save every single thing you want to save, use it
 * for some general purpose widgets, such as MainWindows or Splitters,
 * which have widget->saveX() and widget->loadX() methods.
 */

const QString Settings::globalSettingsFile = APPLICATION_ALIAS "-" OK_IM_MODULE ".ini";

CompatibleRecursiveMutex Settings::bigLock;
QThread* Settings::settingsThread{nullptr};

Settings::Settings() : loaded(false), useCustomDhtList{false}, makeToxPortable{false} {
    settingsThread = new QThread();
    settingsThread->setObjectName(globalSettingsFile);
    settingsThread->start(QThread::LowPriority);
    moveToThread(settingsThread);
    loadGlobal();
}

Settings::~Settings() {
    sync();
    settingsThread->exit(0);
    settingsThread->wait();
    delete settingsThread;
}

/**
 * @brief Returns the singleton instance.
 */
Settings& Settings::getInstance() {
    static Settings* settings = nullptr;
    if (!settings) settings = new Settings();
    return *settings;
}

void Settings::destroyInstance() {
    //  delete settings;
    //  settings = nullptr;
}

// 国际化下拉框
// QStringList Settings::getLocales(){
//   return locales;
// }

void Settings::loadGlobal() {
    QMutexLocker locker{&bigLock};

    if (loaded) return;

    createSettingsDir();

    makeToxPortable = Settings::isToxPortable();

    QDir dir(getSettingsDirPath());
    QString filePath = dir.filePath(globalSettingsFile);

    // If no settings file exist -- use the default one
    if (!QFile(filePath).exists()) {
        qDebug() << "No settings file found, using defaults";
        filePath = ":/conf/" + globalSettingsFile;
    }

    qDebug() << "Loading settings from " + filePath;

    QSettings s(filePath, QSettings::IniFormat);
    s.setIniCodec("UTF-8");

    // 自动登录
    s.beginGroup("Login");
    { autoLogin = s.value("autoLogin", false).toBool(); }
    s.endGroup();
    // 语言
    s.beginGroup("General");
    {
        autoAwayTime = s.value("autoAwayTime", 10).toInt();
        checkUpdates = s.value("checkUpdates", true).toBool();
        // note: notifySound and busySound UI elements are now under UI settings
        // page, but kept under General in settings file to be backwards compatible
        notifySound = s.value("notifySound", true).toBool();
        notifyHide = s.value("notifyHide", false).toBool();
        busySound = s.value("busySound", false).toBool();
        globalAutoAcceptDir =
                s.value("globalAutoAcceptDir",
                        QStandardPaths::locate(QStandardPaths::HomeLocation, QString(),
                                               QStandardPaths::LocateDirectory))
                        .toString();
        autoAcceptMaxSize =
                static_cast<size_t>(s.value("autoAcceptMaxSize", 20 << 20 /*20 MB*/).toLongLong());
        stylePreference = static_cast<StyleType>(s.value("stylePreference", 1).toInt());
    }
    s.endGroup();

    s.beginGroup("Advanced");
    {
        makeToxPortable = s.value("makeToxPortable", false).toBool();
        enableIPv6 = s.value("enableIPv6", true).toBool();
        forceTCP = s.value("forceTCP", false).toBool();
        enableLanDiscovery = s.value("enableLanDiscovery", true).toBool();
    }
    s.endGroup();

    s.beginGroup("Widgets");
    {
        QList<QString> objectNames = s.childKeys();
        for (const QString& name : objectNames) widgetSettings[name] = s.value(name).toByteArray();
    }
    s.endGroup();

    s.beginGroup("GUI");
    {
        showWindow = s.value("showWindow", true).toBool();
        notify = s.value("notify", true).toBool();
        desktopNotify = s.value("desktopNotify", true).toBool();
        groupAlwaysNotify = s.value("groupAlwaysNotify", true).toBool();
        groupchatPosition = s.value("groupchatPosition", true).toBool();
        separateWindow = s.value("separateWindow", false).toBool();
        dontGroupWindows = s.value("dontGroupWindows", false).toBool();
        showIdenticons = s.value("showIdenticons", true).toBool();

        const QString DEFAULT_SMILEYS = ":/smileys/emojione/emoticons.xml";
        smileyPack = s.value("smileyPack", DEFAULT_SMILEYS).toString();
        if (!QFile::exists(smileyPack)) {
            smileyPack = DEFAULT_SMILEYS;
        }

        emojiFontPointSize = s.value("emojiFontPointSize", 24).toInt();
        firstColumnHandlePos = s.value("firstColumnHandlePos", 50).toInt();
        secondColumnHandlePosFromRight = s.value("secondColumnHandlePosFromRight", 50).toInt();
        timestampFormat = s.value("timestampFormat", "hh:mm:ss").toString();
        dateFormat = s.value("dateFormat", "yyyy-MM-dd").toString();
        lightTrayIcon = s.value("lightTrayIcon", false).toBool();
        useEmoticons = s.value("useEmoticons", true).toBool();
        statusChangeNotificationEnabled =
                s.value("statusChangeNotificationEnabled", false).toBool();
        showGroupJoinLeaveMessages = s.value("showGroupJoinLeaveMessages", false).toBool();
        spellCheckingEnabled = s.value("spellCheckingEnabled", true).toBool();
        themeColor = s.value("themeColor", 0).toInt();
        style = s.value("style", "").toString();
        if (style == "")  // Default to Fusion if available, otherwise no style
        {
            if (QStyleFactory::keys().contains("Fusion"))
                style = "Fusion";
            else
                style = "None";
        }
        nameColors = s.value("nameColors", false).toBool();
    }
    s.endGroup();

    s.beginGroup("Chat");
    { chatMessageFont = s.value("chatMessageFont", Style::getFont(Style::Big)).value<QFont>(); }
    s.endGroup();
    // 隐私
    s.beginGroup("State");
    {
        windowGeometry = s.value("windowGeometry", QByteArray()).toByteArray();
        windowState = s.value("windowState", QByteArray()).toByteArray();
        splitterState = s.value("splitterState", QByteArray()).toByteArray();
        dialogGeometry = s.value("dialogGeometry", QByteArray()).toByteArray();
        dialogSplitterState = s.value("dialogSplitterState", QByteArray()).toByteArray();
        dialogSettingsGeometry = s.value("dialogSettingsGeometry", QByteArray()).toByteArray();
    }
    s.endGroup();

    s.beginGroup("Audio");
    {
        inDev = s.value("inDev", "").toString();
        audioInDevEnabled = s.value("audioInDevEnabled", true).toBool();
        outDev = s.value("outDev", "").toString();
        audioOutDevEnabled = s.value("audioOutDevEnabled", true).toBool();
        audioInGainDecibel = s.value("inGain", 0).toReal();
        audioThreshold = s.value("audioThreshold", 0).toReal();
        outVolume = s.value("outVolume", 100).toInt();
        enableTestSound = s.value("enableTestSound", true).toBool();
        audioBitrate = s.value("audioBitrate", 64).toInt();
    }
    s.endGroup();

    s.beginGroup("Video");
    {
        videoDev = s.value("videoDev", "").toString();
        camVideoRes = s.value("camVideoRes", QRect()).toRect();
        screenRegion = s.value("screenRegion", QRect()).toRect();
        screenGrabbed = s.value("screenGrabbed", false).toBool();
        camVideoFPS = static_cast<quint16>(s.value("camVideoFPS", 0).toUInt());
    }
    s.endGroup();

    loaded = true;
}

bool Settings::isToxPortable() {
    QString localSettingsPath = qApp->applicationDirPath() + QDir::separator() + globalSettingsFile;
    if (!QFile(localSettingsPath).exists()) {
        return false;
    }
    QSettings ps(localSettingsPath, QSettings::IniFormat);
    ps.setIniCodec("UTF-8");
    ps.beginGroup("Advanced");
    bool result = ps.value("makeToxPortable", false).toBool();
    ps.endGroup();
    return result;
}

void Settings::updateProfileData(Profile* profile, const QCommandLineParser* parser) {
    QMutexLocker locker{&bigLock};

    if (profile == nullptr) {
        qWarning() << QString("Could not load new settings (profile change to nullptr)");
        return;
    }
    auto& ok = ok::base::OkSettings::getInstance();
    ok.setCurrentProfile(profile->getUsername());
    ok.saveGlobal();

    saveGlobal();

    loadPersonal(profile->getUsername(), profile->getPasskey());
    if (parser) {
        applyCommandLineOptions(*parser);
    }
}

/**
 * Verifies that commandline proxy settings are at least reasonable. Does not
 * verify provided IP or hostname addresses are valid. Code duplication with
 * Settings::applyCommandLineOptions, which also verifies arguments, should be
 * removed in a future refactor.
 * @param parser QCommandLineParser instance
 */
bool Settings::verifyProxySettings(const QCommandLineParser& parser) {
    QString IPv6SettingString = parser.value("I").toLower();
    QString LANSettingString = parser.value("L").toLower();
    QString UDPSettingString = parser.value("U").toLower();
    QString proxySettingString = parser.value("proxy").toLower();
    QStringList proxySettingStrings = proxySettingString.split(":");

    const QString SOCKS5 = QStringLiteral("socks5");
    const QString HTTP = QStringLiteral("http");
    const QString NONE = QStringLiteral("none");
    const QString ON = QStringLiteral("on");
    const QString OFF = QStringLiteral("off");

    // Check for incompatible settings
    bool activeProxyType = false;

    if (parser.isSet("P")) {
        activeProxyType = proxySettingStrings[0] == SOCKS5 || proxySettingStrings[0] == HTTP;
    }

    if (parser.isSet("I")) {
        if (!(IPv6SettingString == ON || IPv6SettingString == OFF)) {
            qCritical() << "Unable to parse IPv6 setting.";
            return false;
        }
    }

    if (parser.isSet("U")) {
        if (!(UDPSettingString == ON || UDPSettingString == OFF)) {
            qCritical() << "Unable to parse UDP setting.";
            return false;
        }
    }

    if (parser.isSet("L")) {
        if (!(LANSettingString == ON || LANSettingString == OFF)) {
            qCritical() << "Unable to parse LAN setting.";
            return false;
        }
    }
    if (activeProxyType && UDPSettingString == ON) {
        qCritical() << "Cannot set UDP on with proxy.";
        return false;
    }

    if (activeProxyType && LANSettingString == ON) {
        qCritical() << "Cannot set LAN discovery on with proxy.";
        return false;
    }

    if (LANSettingString == ON && UDPSettingString == OFF) {
        qCritical() << "Incompatible UDP/LAN settings.";
        return false;
    }

    if (parser.isSet("P")) {
        if (proxySettingStrings[0] == NONE) {
            // slightly lazy check here, accepting 'NONE[:.*]' is fine since no other
            // arguments will be investigated when proxy settings are applied.
            return true;
        }
        // Since the first argument isn't 'none', verify format of remaining
        // arguments
        if (proxySettingStrings.size() != 3) {
            qCritical() << "Invalid number of proxy arguments.";
            return false;
        }

        if (!(proxySettingStrings[0] == SOCKS5 || proxySettingStrings[0] == HTTP)) {
            qCritical() << "Unable to parse proxy type.";
            return false;
        }

        // TODO(Kriby): Sanity check IPv4/IPv6 addresses/hostnames?

        int portNumber = proxySettingStrings[2].toInt();
        if (!(portNumber > 0 && portNumber < 65536)) {
            qCritical() << "Invalid port number range.";
        }
    }
    return true;
}

/**
 * Applies command line options on top of loaded settings. Fails without changes
 * if attempting to apply contradicting settings.
 * @param parser QCommandLineParser instance
 * @return Success indicator (success = true)
 */
bool Settings::applyCommandLineOptions(const QCommandLineParser& parser) {
    if (!verifyProxySettings(parser)) {
        return false;
    };

    QString IPv6Setting = parser.value("I").toUpper();
    QString LANSetting = parser.value("L").toUpper();
    QString UDPSetting = parser.value("U").toUpper();
    QString proxySettingString = parser.value("proxy").toUpper();
    QStringList proxySettings = proxySettingString.split(":");

    const QString SOCKS5 = QStringLiteral("SOCKS5");
    const QString HTTP = QStringLiteral("HTTP");
    const QString NONE = QStringLiteral("NONE");
    const QString ON = QStringLiteral("ON");
    const QString OFF = QStringLiteral("OFF");

    if (parser.isSet("I")) {
        enableIPv6 = IPv6Setting == ON;
        qDebug() << QString("Setting IPv6 %1.").arg(IPv6Setting);
    }

    if (parser.isSet("P")) {
        qDebug() << QString("Setting proxy type to %1.").arg(proxySettings[0]);

        quint16 portNumber = 0;
        QString address = "";

        if (proxySettings[0] == NONE) {
            proxyType = ICoreSettings::ProxyType::ptNone;
        } else {
            if (proxySettings[0] == SOCKS5) {
                proxyType = ICoreSettings::ProxyType::ptSOCKS5;
            } else if (proxySettings[0] == HTTP) {
                proxyType = ICoreSettings::ProxyType::ptHTTP;
            } else {
                qCritical() << "Failed to set valid proxy type";
                assert(false);  // verifyProxySettings should've made this impossible
            }

            forceTCP = true;
            enableLanDiscovery = false;

            address = proxySettings[1];
            portNumber = static_cast<quint16>(proxySettings[2].toInt());
        }

        proxyAddr = address;
        qDebug() << QString("Setting proxy address to %1.").arg(address);
        proxyPort = portNumber;
        qDebug() << QString("Setting port number to %1.").arg(portNumber);
    }

    if (parser.isSet("U")) {
        bool shouldForceTCP = UDPSetting == OFF;
        if (!shouldForceTCP && proxyType != ICoreSettings::ProxyType::ptNone) {
            qDebug() << "Cannot use UDP with proxy; disable proxy explicitly with "
                        "'-P none'.";
        } else {
            forceTCP = shouldForceTCP;
            qDebug() << QString("Setting UDP %1.").arg(UDPSetting);
        }

        // LANSetting == ON is caught by verifyProxySettings, the OFF check removes
        // needless debug
        if (shouldForceTCP && !(LANSetting == OFF) && enableLanDiscovery) {
            qDebug() << "Cannot perform LAN discovery without UDP; disabling LAN "
                        "discovery.";
            enableLanDiscovery = false;
        }
    }

    if (parser.isSet("L")) {
        bool shouldEnableLAN = LANSetting == ON;

        if (shouldEnableLAN && proxyType != ICoreSettings::ProxyType::ptNone) {
            qDebug() << "Cannot use LAN discovery with proxy; disable proxy "
                        "explicitly with '-P none'.";
        } else if (shouldEnableLAN && forceTCP) {
            qDebug() << "Cannot use LAN discovery without UDP; enable UDP explicitly "
                        "with '-U on'.";
        } else {
            enableLanDiscovery = shouldEnableLAN;
            qDebug() << QString("Setting LAN Discovery %1.").arg(LANSetting);
        }
    }
    return true;
}

void Settings::resetToDefault() {
    // To stop saving
    loaded = false;

    // Remove file with profile settings
    QDir dir(getSettingsDirPath());
    Profile* profile = Nexus::getProfile();
    QString localPath = dir.filePath(profile->getUsername() + ".ini");
    QFile local(localPath);
    if (local.exists()) local.remove();
}

/**
 * @brief Asynchronous, saves the global settings.
 */
void Settings::saveGlobal() {
    if (QThread::currentThread() != settingsThread)
        return (void)QMetaObject::invokeMethod(&getInstance(), "saveGlobal");

    QMutexLocker locker{&bigLock};
    if (!loaded) return;

    QString path = getSettingsDirPath() + globalSettingsFile;
    qDebug() << "Saving global settings at " + path;

    QSettings s(path, QSettings::IniFormat);
    s.setIniCodec("UTF-8");

    s.clear();

    s.beginGroup("Login");
    { s.setValue("autoLogin", autoLogin); }
    s.endGroup();

    s.beginGroup("General");
    {
        s.setValue("autoAwayTime", autoAwayTime);
        s.setValue("checkUpdates", checkUpdates);
        s.setValue("notifySound", notifySound);
        s.setValue("notifyHide", notifyHide);
        s.setValue("busySound", busySound);
        s.setValue("autoAcceptMaxSize", static_cast<qlonglong>(autoAcceptMaxSize));
        s.setValue("globalAutoAcceptDir", globalAutoAcceptDir);
        s.setValue("stylePreference", static_cast<int>(stylePreference));
    }
    s.endGroup();

    s.beginGroup("Advanced");
    {
        s.setValue("makeToxPortable", makeToxPortable);
        s.setValue("enableIPv6", enableIPv6);
        s.setValue("forceTCP", forceTCP);
        s.setValue("enableLanDiscovery", enableLanDiscovery);
        s.setValue("dbSyncType", static_cast<int>(dbSyncType));
    }
    s.endGroup();

    s.beginGroup("Widgets");
    {
        const QList<QString> widgetNames = widgetSettings.keys();
        for (const QString& name : widgetNames) s.setValue(name, widgetSettings.value(name));
    }
    s.endGroup();

    s.beginGroup("GUI");
    {
        s.setValue("showWindow", showWindow);
        s.setValue("notify", notify);
        s.setValue("desktopNotify", desktopNotify);
        s.setValue("groupAlwaysNotify", groupAlwaysNotify);
        s.setValue("separateWindow", separateWindow);
        s.setValue("dontGroupWindows", dontGroupWindows);
        s.setValue("groupchatPosition", groupchatPosition);
        s.setValue("showIdenticons", showIdenticons);

        s.setValue("smileyPack", smileyPack);
        s.setValue("emojiFontPointSize", emojiFontPointSize);
        s.setValue("firstColumnHandlePos", firstColumnHandlePos);
        s.setValue("secondColumnHandlePosFromRight", secondColumnHandlePosFromRight);
        s.setValue("timestampFormat", timestampFormat);
        s.setValue("dateFormat", dateFormat);
        s.setValue("lightTrayIcon", lightTrayIcon);
        s.setValue("useEmoticons", useEmoticons);
        s.setValue("themeColor", themeColor);
        s.setValue("style", style);
        s.setValue("nameColors", nameColors);
        s.setValue("statusChangeNotificationEnabled", statusChangeNotificationEnabled);
        s.setValue("showGroupJoinLeaveMessages", showGroupJoinLeaveMessages);
        s.setValue("spellCheckingEnabled", spellCheckingEnabled);
    }
    s.endGroup();

    s.beginGroup("Chat");
    { s.setValue("chatMessageFont", chatMessageFont); }
    s.endGroup();

    s.beginGroup("State");
    {
        s.setValue("windowGeometry", windowGeometry);
        s.setValue("windowState", windowState);
        s.setValue("splitterState", splitterState);
        s.setValue("dialogGeometry", dialogGeometry);
        s.setValue("dialogSplitterState", dialogSplitterState);
        s.setValue("dialogSettingsGeometry", dialogSettingsGeometry);
    }
    s.endGroup();
    // 音频
    s.beginGroup("Audio");
    {
        s.setValue("inDev", inDev);
        s.setValue("audioInDevEnabled", audioInDevEnabled);
        s.setValue("outDev", outDev);
        s.setValue("audioOutDevEnabled", audioOutDevEnabled);
        s.setValue("inGain", audioInGainDecibel);
        s.setValue("audioThreshold", audioThreshold);
        s.setValue("outVolume", outVolume);
        s.setValue("enableTestSound", enableTestSound);
        s.setValue("audioBitrate", audioBitrate);
    }
    s.endGroup();

    s.beginGroup("Video");
    {
        s.setValue("videoDev", videoDev);
        s.setValue("camVideoRes", camVideoRes);
        s.setValue("camVideoFPS", camVideoFPS);
        s.setValue("screenRegion", screenRegion);
        s.setValue("screenGrabbed", screenGrabbed);
    }
    s.endGroup();
}

/**
 * @brief Write a default personal .ini settings file for a profile.
 * @param basename Filename without extension to save settings.
 *
 * @note If basename is "profile", settings will be saved in profile.ini
 */
void Settings::createPersonal(QString basename) {
    QMutexLocker locker{&bigLock};

    qDebug() << "Creating new profile for" << basename;
    QString path = getSettingsDirPath() + QDir::separator() + basename + ".ini";
    qDebug() << "Creating new profile settings in " << path;

    QSettings ps(path, QSettings::IniFormat);
    ps.setIniCodec("UTF-8");
    ps.beginGroup("Friends");
    //  ps.beginWriteArray("IMFriend", 0);
    //  ps.endArray();
    ps.endGroup();

    ps.beginGroup("Privacy");
    ps.endGroup();
}

void Settings::loadPersonal(QString profileName, const ToxEncrypt* passKey) {
    QMutexLocker locker{&bigLock};

    QDir dir(getSettingsDirPath());
    QString filePath = dir.filePath(globalSettingsFile);

    // load from a profile specific friend data list if possible
    QString tmp = dir.filePath(profileName + ".ini");
    if (QFile(tmp).exists())  // otherwise, filePath remains the global file
        filePath = tmp;

    qDebug() << "Loading personal settings from" << filePath;

    QSettings ps(filePath, QSettings::IniFormat);
    friendLst.clear();

    ps.beginGroup("Privacy");
    {
        typingNotification = ps.value("typingNotification", true).toBool();
        enableLogging = ps.value("enableLogging", true).toBool();
    }
    ps.endGroup();

    ps.beginGroup("Friends");
    {
        int size = ps.beginReadArray("IMFriend");
        friendLst.reserve(size);
        friendLst.clear();
        for (int i = 0; i < size; i++) {
            ps.setArrayIndex(i);
            friendProp fp{ps.value("addr").toString()};
            fp.alias = ps.value("alias").toString();
            fp.note = ps.value("note").toString();
            fp.autoAcceptDir = ps.value("autoAcceptDir").toString();

            if (fp.autoAcceptDir == "") fp.autoAcceptDir = ps.value("autoAccept").toString();

            fp.autoAcceptCall =
                    Settings::AutoAcceptCallFlags(QFlag(ps.value("autoAcceptCall", 0).toInt()));
            fp.autoGroupInvite = ps.value("autoGroupInvite").toBool();
            fp.circleID = ps.value("circle", -1).toInt();

            if (getEnableLogging()) fp.activity = ps.value("activity", QDateTime()).toDateTime();
            friendLst.insert(ToxId(fp.addr).getPublicKey().getByteArray(), fp);
        }
        ps.endArray();
    }
    ps.endGroup();

    ps.beginGroup("Requests");
    {
        int size = ps.beginReadArray("Request");
        friendRequests.clear();
        friendRequests.reserve(size);
        for (int i = 0; i < size; i++) {
            ps.setArrayIndex(i);
            Request request;
            request.address = ps.value("addr").toString();
            request.message = ps.value("message").toString();
            request.read = ps.value("read").toBool();
            friendRequests.push_back(request);
        }
        ps.endArray();
    }
    ps.endGroup();
    // 用戶
    ps.beginGroup("GUI");
    {
        compactLayout = ps.value("compactLayout", true).toBool();
        sortingMode = static_cast<FriendListSortingMode>(
                ps.value("friendSortingMethod", static_cast<int>(FriendListSortingMode::Name))
                        .toInt());
    }
    ps.endGroup();

    ps.beginGroup("Proxy");
    {
        proxyType = static_cast<ProxyType>(ps.value("proxyType", 0 /* ProxyType::None */).toInt());
        proxyType = fixInvalidProxyType(proxyType);
        proxyAddr = ps.value("proxyAddr", proxyAddr).toString();
        proxyPort = static_cast<quint16>(ps.value("proxyPort", proxyPort).toUInt());
    }
    ps.endGroup();

    ps.beginGroup("Circles");
    {
        int size = ps.beginReadArray("Circle");
        circleLst.clear();
        circleLst.reserve(size);
        for (int i = 0; i < size; i++) {
            ps.setArrayIndex(i);
            circleProp cp;
            cp.name = ps.value("name").toString();
            cp.expanded = ps.value("expanded", true).toBool();
            circleLst.push_back(cp);
        }
        ps.endArray();
    }
    ps.endGroup();
}

/**
 * @brief Asynchronous, saves the current profile.
 */
void Settings::savePersonal() { savePersonal(Nexus::getProfile()); }

/**
 * @brief Asynchronous, saves the profile.
 * @param profile Profile to save.
 */
void Settings::savePersonal(Profile* profile) {
    if (!profile) {
        qDebug() << "Could not save personal settings because there is no active "
                    "profile";
        return;
    }
    if (QThread::currentThread() != settingsThread)
        return (void)QMetaObject::invokeMethod(&getInstance(), "savePersonal",
                                               Q_ARG(Profile*, profile));
    savePersonal(profile->getUsername(), profile->getPasskey());
}

void Settings::savePersonal(QString profileName, const ToxEncrypt* passkey) {
    QMutexLocker locker{&bigLock};
    if (!loaded) return;

    QString path = getSettingsDirPath() + profileName + ".ini";

    qDebug() << "Saving personal settings at " << path;

    QSettings ps(path, QSettings::IniFormat);
    ps.beginGroup("Friends");
    {
        ps.beginWriteArray("IMFriend", friendLst.size());
        int index = 0;
        for (auto& frnd : friendLst) {
            ps.setArrayIndex(index);
            ps.setValue("addr", frnd.addr);
            ps.setValue("alias", frnd.alias);
            ps.setValue("note", frnd.note);
            ps.setValue("autoAcceptDir", frnd.autoAcceptDir);
            ps.setValue("autoAcceptCall", static_cast<int>(frnd.autoAcceptCall));
            ps.setValue("autoGroupInvite", frnd.autoGroupInvite);
            ps.setValue("circle", frnd.circleID);

            if (getEnableLogging()) ps.setValue("activity", frnd.activity);

            ++index;
        }
        ps.endArray();
    }
    ps.endGroup();

    ps.beginGroup("Requests");
    {
        ps.beginWriteArray("Request", friendRequests.size());
        int index = 0;
        for (auto& request : friendRequests) {
            ps.setArrayIndex(index);
            ps.setValue("addr", request.address);
            ps.setValue("message", request.message);
            ps.setValue("read", request.read);

            ++index;
        }
        ps.endArray();
    }
    ps.endGroup();

    ps.beginGroup("GUI");
    {
        ps.setValue("compactLayout", compactLayout);
        ps.setValue("friendSortingMethod", static_cast<int>(sortingMode));
    }
    ps.endGroup();

    ps.beginGroup("Proxy");
    {
        ps.setValue("proxyType", static_cast<int>(proxyType));
        ps.setValue("proxyAddr", proxyAddr);
        ps.setValue("proxyPort", proxyPort);
    }
    ps.endGroup();

    ps.beginGroup("Circles");
    {
        ps.beginWriteArray("Circle", circleLst.size());
        int index = 0;
        for (auto& circle : circleLst) {
            ps.setArrayIndex(index);
            ps.setValue("name", circle.name);
            ps.setValue("expanded", circle.expanded);
            ++index;
        }
        ps.endArray();
    }
    ps.endGroup();

    ps.beginGroup("Privacy");
    {
        ps.setValue("typingNotification", typingNotification);
        ps.setValue("enableLogging", enableLogging);
    }
    ps.endGroup();
    //  ps.save();
}

/**
 * @brief Get path to directory, where the settings files are stored.
 * @return Path to settings directory, ends with a directory separator.
 */
QString Settings::getSettingsDirPath() const {
    QMutexLocker locker{&bigLock};
    if (makeToxPortable) return qApp->applicationDirPath() + QDir::separator();

// workaround for https://bugreports.qt-project.org/browse/QTBUG-38845
#ifdef Q_OS_WIN
    return QDir::cleanPath(QStandardPaths::writableLocation(QStandardPaths::HomeLocation) +
                           QDir::separator() + "AppData" + QDir::separator() + "Roaming" +
                           QDir::separator() + APPLICATION_ID) +
           QDir::separator();
#elif defined(Q_OS_OSX)
    return QDir::cleanPath(QStandardPaths::writableLocation(QStandardPaths::HomeLocation) +
                           QDir::separator() + "Library" + QDir::separator() +
                           "Application Support" + QDir::separator() + APPLICATION_ID) +
           QDir::separator();
#else
    return QDir::cleanPath(QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) +
                           QDir::separator() + APPLICATION_ID) +
           QDir::separator();
#endif
}

/**
 * @brief Get path to directory, where the application data are stored.
 * @return Path to application data, ends with a directory separator.
 */
QString Settings::getAppDataDirPath() const {
    QMutexLocker locker{&bigLock};
    if (makeToxPortable) return qApp->applicationDirPath() + QDir::separator();

// workaround for https://bugreports.qt-project.org/browse/QTBUG-38845
#ifdef Q_OS_WIN
    return QDir::cleanPath(QStandardPaths::writableLocation(QStandardPaths::HomeLocation) +
                           QDir::separator() + "AppData" + QDir::separator() + "Roaming" +
                           QDir::separator() + "tox") +
           QDir::separator();
#elif defined(Q_OS_OSX)
    return QDir::cleanPath(QStandardPaths::writableLocation(QStandardPaths::HomeLocation) +
                           QDir::separator() + "Library" + QDir::separator() +
                           "Application Support" + QDir::separator() + "Tox") +
           QDir::separator();
#else
    /*
     * TODO: Change QStandardPaths::DataLocation to AppDataLocation when upgrate
     * Qt to 5.4+ For now we need support Qt 5.3, so we use deprecated
     * DataLocation BTW, it's not a big deal since for linux AppDataLocation and
     * DataLocation are equal
     */
    return QDir::cleanPath(QStandardPaths::writableLocation(QStandardPaths::DataLocation)) +
           QDir::separator();
#endif
}

bool Settings::getEnableTestSound() const {
    QMutexLocker locker{&bigLock};
    return enableTestSound;
}

void Settings::setEnableTestSound(bool newValue) {
    QMutexLocker locker{&bigLock};

    if (newValue != enableTestSound) {
        enableTestSound = newValue;
        emit enableTestSoundChanged(enableTestSound);
    }
}

bool Settings::getEnableIPv6() const {
    QMutexLocker locker{&bigLock};
    return enableIPv6;
}

void Settings::setEnableIPv6(bool enabled) {
    QMutexLocker locker{&bigLock};

    if (enabled != enableIPv6) {
        enableIPv6 = enabled;
        emit enableIPv6Changed(enableIPv6);
    }
}

bool Settings::getMakeToxPortable() const {
    QMutexLocker locker{&bigLock};
    return makeToxPortable;
}

void Settings::setMakeToxPortable(bool newValue) {
    QMutexLocker locker{&bigLock};

    if (newValue != makeToxPortable) {
        QFile(getSettingsDirPath() + globalSettingsFile).remove();
        makeToxPortable = newValue;
        saveGlobal();

        emit makeToxPortableChanged(makeToxPortable);
    }
}

QString Settings::getStyle() const {
    QMutexLocker locker{&bigLock};
    return style;
}

void Settings::setStyle(const QString& newStyle) {
    QMutexLocker locker{&bigLock};

    if (newStyle != style) {
        style = newStyle;
        emit styleChanged(style);
    }
}

void Settings::setUseEmoticons(bool newValue) {
    QMutexLocker locker{&bigLock};

    if (newValue != useEmoticons) {
        useEmoticons = newValue;
        emit useEmoticonsChanged(useEmoticons);
    }
}

bool Settings::getUseEmoticons() const {
    QMutexLocker locker{&bigLock};
    return useEmoticons;
}

bool Settings::getLightTrayIcon() const {
    QMutexLocker locker{&bigLock};
    return lightTrayIcon;
}

void Settings::setLightTrayIcon(bool newValue) {
    QMutexLocker locker{&bigLock};

    if (newValue != lightTrayIcon) {
        lightTrayIcon = newValue;
        emit lightTrayIconChanged(lightTrayIcon);
    }
}

bool Settings::getStatusChangeNotificationEnabled() const {
    QMutexLocker locker{&bigLock};
    return statusChangeNotificationEnabled;
}

void Settings::setStatusChangeNotificationEnabled(bool newValue) {
    QMutexLocker locker{&bigLock};

    if (newValue != statusChangeNotificationEnabled) {
        statusChangeNotificationEnabled = newValue;
        emit statusChangeNotificationEnabledChanged(statusChangeNotificationEnabled);
    }
}

bool Settings::getShowGroupJoinLeaveMessages() const {
    QMutexLocker locker{&bigLock};
    return showGroupJoinLeaveMessages;
}

void Settings::setShowGroupJoinLeaveMessages(bool newValue) {
    QMutexLocker locker{&bigLock};
    if (newValue != showGroupJoinLeaveMessages) {
        showGroupJoinLeaveMessages = newValue;
        emit showGroupJoinLeaveMessagesChanged(showGroupJoinLeaveMessages);
    }
}

bool Settings::getSpellCheckingEnabled() const {
    const QMutexLocker locker{&bigLock};
    return spellCheckingEnabled;
}

void Settings::setSpellCheckingEnabled(bool newValue) {
    QMutexLocker locker{&bigLock};

    if (newValue != spellCheckingEnabled) {
        spellCheckingEnabled = newValue;
        emit statusChangeNotificationEnabledChanged(statusChangeNotificationEnabled);
    }
}

bool Settings::getNotifySound() const {
    QMutexLocker locker{&bigLock};
    return notifySound;
}

void Settings::setNotifySound(bool newValue) {
    QMutexLocker locker{&bigLock};

    if (newValue != notifySound) {
        notifySound = newValue;
        emit notifySoundChanged(notifySound);
    }
}

bool Settings::getNotifyHide() const {
    QMutexLocker locker{&bigLock};
    return notifyHide;
}

void Settings::setNotifyHide(bool newValue) {
    QMutexLocker locker{&bigLock};

    if (newValue != notifyHide) {
        notifyHide = newValue;
        emit notifyHideChanged(notifyHide);
    }
}

bool Settings::getBusySound() const {
    QMutexLocker locker{&bigLock};
    return busySound;
}

void Settings::setBusySound(bool newValue) {
    QMutexLocker locker{&bigLock};

    if (newValue != busySound) {
        busySound = newValue;
        emit busySoundChanged(busySound);
    }
}

bool Settings::getGroupAlwaysNotify() const {
    QMutexLocker locker{&bigLock};
    return groupAlwaysNotify;
}

void Settings::setGroupAlwaysNotify(bool newValue) {
    QMutexLocker locker{&bigLock};

    if (newValue != groupAlwaysNotify) {
        groupAlwaysNotify = newValue;
        emit groupAlwaysNotifyChanged(groupAlwaysNotify);
    }
}

QString Settings::getTranslation() const {
    QMutexLocker locker{&bigLock};
    auto& s = ok::base::OkSettings::getInstance();
    return s.getTranslation();
}

void Settings::setTranslation(const QString& newValue) {
    QMutexLocker locker{&bigLock};
    auto& s = ok::base::OkSettings::getInstance();
    auto translation = s.getTranslation();
    if (newValue != translation) {
        translation = newValue;
        s.translationChanged(translation);
    }
}

bool Settings::getForceTCP() const {
    QMutexLocker locker{&bigLock};
    return forceTCP;
}

void Settings::setForceTCP(bool enabled) {
    QMutexLocker locker{&bigLock};

    if (enabled != forceTCP) {
        forceTCP = enabled;
        emit forceTCPChanged(forceTCP);
    }
}

bool Settings::getEnableLanDiscovery() const {
    QMutexLocker locker{&bigLock};
    return enableLanDiscovery;
}

void Settings::setEnableLanDiscovery(bool enabled) {
    QMutexLocker locker{&bigLock};

    if (enabled != enableLanDiscovery) {
        enableLanDiscovery = enabled;
        emit enableLanDiscoveryChanged(enableLanDiscovery);
    }
}

QNetworkProxy Settings::getProxy() const {
    QMutexLocker locker{&bigLock};

    QNetworkProxy proxy;
    switch (Settings::getProxyType()) {
        case ProxyType::ptNone:
            proxy.setType(QNetworkProxy::NoProxy);
            break;
        case ProxyType::ptSOCKS5:
            proxy.setType(QNetworkProxy::Socks5Proxy);
            break;
        case ProxyType::ptHTTP:
            proxy.setType(QNetworkProxy::HttpProxy);
            break;
        default:
            proxy.setType(QNetworkProxy::NoProxy);
            qWarning() << "Invalid Proxy type, setting to NoProxy";
            break;
    }

    proxy.setHostName(Settings::getProxyAddr());
    proxy.setPort(Settings::getProxyPort());
    return proxy;
}

Settings::ProxyType Settings::getProxyType() const {
    QMutexLocker locker{&bigLock};
    return proxyType;
}

void Settings::setProxyType(ProxyType newValue) {
    QMutexLocker locker{&bigLock};

    if (newValue != proxyType) {
        proxyType = newValue;
        emit proxyTypeChanged(proxyType);
    }
}

QString Settings::getProxyAddr() const {
    QMutexLocker locker{&bigLock};
    return proxyAddr;
}

void Settings::setProxyAddr(const QString& address) {
    QMutexLocker locker{&bigLock};

    if (address != proxyAddr) {
        proxyAddr = address;
        emit proxyAddressChanged(proxyAddr);
    }
}

quint16 Settings::getProxyPort() const {
    QMutexLocker locker{&bigLock};
    return proxyPort;
}

void Settings::setProxyPort(quint16 port) {
    QMutexLocker locker{&bigLock};

    if (port != proxyPort) {
        proxyPort = port;
        emit proxyPortChanged(proxyPort);
    }
}

bool Settings::getEnableLogging() const {
    QMutexLocker locker{&bigLock};
    return enableLogging;
}

void Settings::setEnableLogging(bool newValue) {
    QMutexLocker locker{&bigLock};

    if (newValue != enableLogging) {
        enableLogging = newValue;
        emit enableLoggingChanged(enableLogging);
    }
}

int Settings::getAutoAwayTime() const {
    QMutexLocker locker{&bigLock};
    return autoAwayTime;
}

/**
 * @brief Sets how long the user may stay idle, before online status is set to
 * "away".
 * @param[in] newValue  the user idle duration in minutes
 * @note Values < 0 default to 10 minutes.
 */
void Settings::setAutoAwayTime(int newValue) {
    QMutexLocker locker{&bigLock};

    if (newValue < 0) newValue = 10;

    if (newValue != autoAwayTime) {
        autoAwayTime = newValue;
        emit autoAwayTimeChanged(autoAwayTime);
    }
}

QString Settings::getAutoAcceptDir(const FriendId& id) const {
    QMutexLocker locker{&bigLock};

    auto it = friendLst.find(id.getByteArray());
    if (it != friendLst.end()) return it->autoAcceptDir;

    return QString();
}

void Settings::setAutoAcceptDir(const FriendId& id, const QString& dir) {
    QMutexLocker locker{&bigLock};

    auto& frnd = getOrInsertFriendPropRef(id);

    if (frnd.autoAcceptDir != dir) {
        frnd.autoAcceptDir = dir;
        emit autoAcceptDirChanged(id, dir);
    }
}

Settings::AutoAcceptCallFlags Settings::getAutoAcceptCall(const FriendId& id) const {
    QMutexLocker locker{&bigLock};

    auto it = friendLst.find(id.getByteArray());
    if (it != friendLst.end()) return it->autoAcceptCall;

    return Settings::AutoAcceptCallFlags();
}

void Settings::setAutoAcceptCall(const FriendId& id, AutoAcceptCallFlags accept) {
    QMutexLocker locker{&bigLock};

    auto& frnd = getOrInsertFriendPropRef(id);

    if (frnd.autoAcceptCall != accept) {
        frnd.autoAcceptCall = accept;
        emit autoAcceptCallChanged(id, accept);
    }
}

bool Settings::getAutoGroupInvite(const FriendId& id) const {
    QMutexLocker locker{&bigLock};

    auto it = friendLst.find(id.getByteArray());
    if (it != friendLst.end()) {
        return it->autoGroupInvite;
    }

    return false;
}

void Settings::setAutoGroupInvite(const FriendId& id, bool accept) {
    QMutexLocker locker{&bigLock};

    auto& frnd = getOrInsertFriendPropRef(id);

    if (frnd.autoGroupInvite != accept) {
        frnd.autoGroupInvite = accept;
        emit autoGroupInviteChanged(id, accept);
    }
}

QString Settings::getContactNote(const FriendId& id) const {
    QMutexLocker locker{&bigLock};

    auto it = friendLst.find(id.getByteArray());
    if (it != friendLst.end()) return it->note;

    return QString();
}

void Settings::setContactNote(const FriendId& id, const QString& note) {
    QMutexLocker locker{&bigLock};

    auto& frnd = getOrInsertFriendPropRef(id);

    if (frnd.note != note) {
        frnd.note = note;
        emit contactNoteChanged(id, note);
    }
}

QString Settings::getGlobalAutoAcceptDir() const {
    QMutexLocker locker{&bigLock};
    return globalAutoAcceptDir;
}

void Settings::setGlobalAutoAcceptDir(const QString& newValue) {
    QMutexLocker locker{&bigLock};

    if (newValue != globalAutoAcceptDir) {
        globalAutoAcceptDir = newValue;
        emit globalAutoAcceptDirChanged(globalAutoAcceptDir);
    }
}

size_t Settings::getMaxAutoAcceptSize() const {
    QMutexLocker locker{&bigLock};
    return autoAcceptMaxSize;
}

void Settings::setMaxAutoAcceptSize(size_t size) {
    QMutexLocker locker{&bigLock};

    if (size != autoAcceptMaxSize) {
        autoAcceptMaxSize = size;
        emit autoAcceptMaxSizeChanged(autoAcceptMaxSize);
    }
}

const QFont& Settings::getChatMessageFont() const {
    QMutexLocker locker(&bigLock);
    return chatMessageFont;
}

void Settings::setChatMessageFont(const QFont& font) {
    QMutexLocker locker(&bigLock);

    if (font != chatMessageFont) {
        chatMessageFont = font;
        emit chatMessageFontChanged(chatMessageFont);
    }
}

void Settings::setWidgetData(const QString& uniqueName, const QByteArray& data) {
    QMutexLocker locker{&bigLock};

    if (!widgetSettings.contains(uniqueName) || widgetSettings[uniqueName] != data) {
        widgetSettings[uniqueName] = data;
        emit widgetDataChanged(uniqueName);
    }
}

QByteArray Settings::getWidgetData(const QString& uniqueName) const {
    QMutexLocker locker{&bigLock};
    return widgetSettings.value(uniqueName);
}

QString Settings::getSmileyPack() const {
    QMutexLocker locker{&bigLock};
    return smileyPack;
}

void Settings::setSmileyPack(const QString& value) {
    QMutexLocker locker{&bigLock};

    if (value != smileyPack) {
        smileyPack = value;
        emit smileyPackChanged(smileyPack);
    }
}

int Settings::getEmojiFontPointSize() const {
    QMutexLocker locker{&bigLock};
    return emojiFontPointSize;
}

void Settings::setEmojiFontPointSize(int value) {
    QMutexLocker locker{&bigLock};

    if (value != emojiFontPointSize) {
        emojiFontPointSize = value;
        emit emojiFontPointSizeChanged(emojiFontPointSize);
    }
}

const QString& Settings::getTimestampFormat() const {
    QMutexLocker locker{&bigLock};
    return timestampFormat;
}

void Settings::setTimestampFormat(const QString& format) {
    QMutexLocker locker{&bigLock};

    if (format != timestampFormat) {
        timestampFormat = format;
        emit timestampFormatChanged(timestampFormat);
    }
}

const QString& Settings::getDateFormat() const {
    QMutexLocker locker{&bigLock};
    return dateFormat;
}

void Settings::setDateFormat(const QString& format) {
    QMutexLocker locker{&bigLock};

    if (format != dateFormat) {
        dateFormat = format;
        emit dateFormatChanged(dateFormat);
    }
}

Settings::StyleType Settings::getStylePreference() const {
    QMutexLocker locker{&bigLock};
    return stylePreference;
}

void Settings::setStylePreference(StyleType newValue) {
    QMutexLocker locker{&bigLock};

    if (newValue != stylePreference) {
        stylePreference = newValue;
        emit stylePreferenceChanged(stylePreference);
    }
}

QByteArray Settings::getWindowGeometry() const {
    QMutexLocker locker{&bigLock};
    return windowGeometry;
}

void Settings::setWindowGeometry(const QByteArray& value) {
    QMutexLocker locker{&bigLock};

    if (value != windowGeometry) {
        windowGeometry = value;
        emit windowGeometryChanged(windowGeometry);
    }
}

QByteArray Settings::getWindowState() const {
    QMutexLocker locker{&bigLock};
    return windowState;
}

void Settings::setWindowState(const QByteArray& value) {
    QMutexLocker locker{&bigLock};

    if (value != windowState) {
        windowState = value;
        emit windowStateChanged(windowState);
    }
}

bool Settings::getCheckUpdates() const {
    QMutexLocker locker{&bigLock};
    return checkUpdates;
}

void Settings::setCheckUpdates(bool newValue) {
    QMutexLocker locker{&bigLock};

    if (newValue != checkUpdates) {
        checkUpdates = newValue;
        emit checkUpdatesChanged(checkUpdates);
    }
}

bool Settings::getNotify() const {
    QMutexLocker locker{&bigLock};
    return notify;
}

void Settings::setNotify(bool newValue) {
    QMutexLocker locker{&bigLock};
    if (newValue != notify) {
        notify = newValue;
        emit notifyChanged(notify);
    }
}

bool Settings::getShowWindow() const {
    QMutexLocker locker{&bigLock};
    return showWindow;
}

void Settings::setShowWindow(bool newValue) {
    QMutexLocker locker{&bigLock};

    if (newValue != showWindow) {
        showWindow = newValue;
        emit showWindowChanged(showWindow);
    }
}

bool Settings::getDesktopNotify() const {
    QMutexLocker locker{&bigLock};
    return desktopNotify;
}

void Settings::setDesktopNotify(bool enabled) {
    QMutexLocker locker{&bigLock};

    if (enabled != desktopNotify) {
        desktopNotify = enabled;
        emit desktopNotifyChanged(desktopNotify);
    }
}

QByteArray Settings::getSplitterState() const {
    QMutexLocker locker{&bigLock};
    return splitterState;
}

void Settings::setSplitterState(const QByteArray& value) {
    QMutexLocker locker{&bigLock};

    if (value != splitterState) {
        splitterState = value;
        emit splitterStateChanged(splitterState);
    }
}

QByteArray Settings::getDialogGeometry() const {
    QMutexLocker locker{&bigLock};
    return dialogGeometry;
}

void Settings::setDialogGeometry(const QByteArray& value) {
    QMutexLocker locker{&bigLock};

    if (value != dialogGeometry) {
        dialogGeometry = value;
        emit dialogGeometryChanged(dialogGeometry);
    }
}

QByteArray Settings::getDialogSplitterState() const {
    QMutexLocker locker{&bigLock};
    return dialogSplitterState;
}

void Settings::setDialogSplitterState(const QByteArray& value) {
    QMutexLocker locker{&bigLock};

    if (value != dialogSplitterState) {
        dialogSplitterState = value;
        emit dialogSplitterStateChanged(dialogSplitterState);
    }
}

QByteArray Settings::getDialogSettingsGeometry() const {
    QMutexLocker locker{&bigLock};
    return dialogSettingsGeometry;
}

void Settings::setDialogSettingsGeometry(const QByteArray& value) {
    QMutexLocker locker{&bigLock};

    if (value != dialogSettingsGeometry) {
        dialogSettingsGeometry = value;
        emit dialogSettingsGeometryChanged(dialogSettingsGeometry);
    }
}

bool Settings::getTypingNotification() const {
    QMutexLocker locker{&bigLock};
    return typingNotification;
}

void Settings::setTypingNotification(bool enabled) {
    QMutexLocker locker{&bigLock};

    if (enabled != typingNotification) {
        typingNotification = enabled;
        emit typingNotificationChanged(typingNotification);
    }
}

QString Settings::getInDev() const {
    QMutexLocker locker{&bigLock};
    return inDev;
}

void Settings::setInDev(const QString& deviceSpecifier) {
    QMutexLocker locker{&bigLock};

    if (deviceSpecifier != inDev) {
        inDev = deviceSpecifier;
        emit inDevChanged(inDev);
    }
}

bool Settings::getAudioInDevEnabled() const {
    QMutexLocker locker(&bigLock);
    return audioInDevEnabled;
}

void Settings::setAudioInDevEnabled(bool enabled) {
    QMutexLocker locker(&bigLock);

    if (enabled != audioInDevEnabled) {
        audioInDevEnabled = enabled;
        emit audioInDevEnabledChanged(enabled);
    }
}

qreal Settings::getAudioInGainDecibel() const {
    QMutexLocker locker{&bigLock};
    return audioInGainDecibel;
}

void Settings::setAudioInGainDecibel(qreal dB) {
    QMutexLocker locker{&bigLock};

    if (dB < audioInGainDecibel || dB > audioInGainDecibel) {
        audioInGainDecibel = dB;
        emit audioInGainDecibelChanged(audioInGainDecibel);
    }
}

qreal Settings::getAudioThreshold() const {
    QMutexLocker locker{&bigLock};
    return audioThreshold;
}

void Settings::setAudioThreshold(qreal percent) {
    QMutexLocker locker{&bigLock};

    if (percent < audioThreshold || percent > audioThreshold) {
        audioThreshold = percent;
        emit audioThresholdChanged(audioThreshold);
    }
}

QString Settings::getVideoDev() const {
    QMutexLocker locker{&bigLock};
    return videoDev;
}

void Settings::setVideoDev(const QString& deviceSpecifier) {
    QMutexLocker locker{&bigLock};

    if (deviceSpecifier != videoDev) {
        videoDev = deviceSpecifier;
        emit videoDevChanged(videoDev);
    }
}

QString Settings::getOutDev() const {
    QMutexLocker locker{&bigLock};
    return outDev;
}

void Settings::setOutDev(const QString& deviceSpecifier) {
    QMutexLocker locker{&bigLock};

    if (deviceSpecifier != outDev) {
        outDev = deviceSpecifier;
        emit outDevChanged(outDev);
    }
}

bool Settings::getAudioOutDevEnabled() const {
    QMutexLocker locker(&bigLock);
    return audioOutDevEnabled;
}

void Settings::setAudioOutDevEnabled(bool enabled) {
    QMutexLocker locker(&bigLock);

    if (enabled != audioOutDevEnabled) {
        audioOutDevEnabled = enabled;
        emit audioOutDevEnabledChanged(audioOutDevEnabled);
    }
}

int Settings::getOutVolume() const {
    QMutexLocker locker{&bigLock};
    return outVolume;
}

void Settings::setOutVolume(int volume) {
    QMutexLocker locker{&bigLock};

    if (volume != outVolume) {
        outVolume = volume;
        emit outVolumeChanged(outVolume);
    }
}

int Settings::getAudioBitrate() const {
    const QMutexLocker locker{&bigLock};
    return audioBitrate;
}

void Settings::setAudioBitrate(int bitrate) {
    const QMutexLocker locker{&bigLock};

    if (bitrate != audioBitrate) {
        audioBitrate = bitrate;
        emit audioBitrateChanged(audioBitrate);
    }
}

QRect Settings::getScreenRegion() const {
    QMutexLocker locker(&bigLock);
    return screenRegion;
}

void Settings::setScreenRegion(const QRect& value) {
    QMutexLocker locker{&bigLock};

    if (value != screenRegion) {
        screenRegion = value;
        emit screenRegionChanged(screenRegion);
    }
}

bool Settings::getScreenGrabbed() const {
    QMutexLocker locker(&bigLock);
    return screenGrabbed;
}

void Settings::setScreenGrabbed(bool value) {
    QMutexLocker locker{&bigLock};

    if (value != screenGrabbed) {
        screenGrabbed = value;
        emit screenGrabbedChanged(screenGrabbed);
    }
}

QRect Settings::getCamVideoRes() const {
    QMutexLocker locker{&bigLock};
    return camVideoRes;
}

void Settings::setCamVideoRes(QRect newValue) {
    QMutexLocker locker{&bigLock};

    if (newValue != camVideoRes) {
        camVideoRes = newValue;
        emit camVideoResChanged(camVideoRes);
    }
}

float Settings::getCamVideoFPS() const {
    QMutexLocker locker{&bigLock};
    return camVideoFPS;
}

void Settings::setCamVideoFPS(float newValue) {
    QMutexLocker locker{&bigLock};

    if (newValue != camVideoFPS) {
        camVideoFPS = newValue;
        emit camVideoFPSChanged(camVideoFPS);
    }
}

QString Settings::getFriendAddress(const QString& publicKey) const {
    QMutexLocker locker{&bigLock};
    // TODO: using ToxId here is a hack
    QByteArray key = ToxId(publicKey).getPublicKey().getByteArray();
    auto it = friendLst.find(key);
    if (it != friendLst.end()) return it->addr;

    return QString();
}

void Settings::updateFriendAddress(const QString& newAddr) {
    QMutexLocker locker{&bigLock};
    // TODO: using ToxId here is a hack
    auto key = ToxId(newAddr).getPublicKey();
    auto& frnd = getOrInsertFriendPropRef(key);
    frnd.addr = newAddr;
}

QString Settings::getFriendAlias(const ContactId& id) const {
    QMutexLocker locker{&bigLock};
    if (friendLst.isEmpty()) return {};

    auto it = friendLst.find(id.getByteArray());
    if (it != friendLst.end()) return it->alias;

    return {};
}

void Settings::setFriendAlias(const FriendId& id, const QString& alias) {
    QMutexLocker locker{&bigLock};
    auto& frnd = getOrInsertFriendPropRef(id);
    frnd.alias = alias;
}

int Settings::getFriendCircleID(const FriendId& id) const {
    QMutexLocker locker{&bigLock};
    auto it = friendLst.find(id.getByteArray());
    if (it != friendLst.end()) return it->circleID;

    return -1;
}

void Settings::setFriendCircleID(const FriendId& id, int circleID) {
    QMutexLocker locker{&bigLock};
    auto& frnd = getOrInsertFriendPropRef(id);
    frnd.circleID = circleID;
}

QDateTime Settings::getFriendActivity(const FriendId& id) const {
    QMutexLocker locker{&bigLock};
    auto it = friendLst.find(id.getByteArray());
    if (it != friendLst.end()) return it->activity;

    return QDateTime();
}

void Settings::setFriendActivity(const FriendId& id, const QDateTime& activity) {
    QMutexLocker locker{&bigLock};
    auto& frnd = getOrInsertFriendPropRef(id);
    frnd.activity = activity;
}

void Settings::saveFriendSettings(const FriendId& id) {
    Q_UNUSED(id);
    savePersonal();
}

void Settings::removeFriendSettings(const FriendId& id) {
    QMutexLocker locker{&bigLock};
    friendLst.remove(id.getByteArray());
}

bool Settings::getCompactLayout() const {
    QMutexLocker locker{&bigLock};
    return compactLayout;
}

void Settings::setCompactLayout(bool value) {
    QMutexLocker locker{&bigLock};

    if (value != compactLayout) {
        compactLayout = value;
        emit compactLayoutChanged(value);
    }
}

Settings::FriendListSortingMode Settings::getFriendSortingMode() const {
    QMutexLocker locker{&bigLock};
    return sortingMode;
}

void Settings::setFriendSortingMode(FriendListSortingMode mode) {
    QMutexLocker locker{&bigLock};

    if (mode != sortingMode) {
        sortingMode = mode;
        emit sortingModeChanged(sortingMode);
    }
}

bool Settings::getSeparateWindow() const {
    QMutexLocker locker{&bigLock};
    return separateWindow;
}

void Settings::setSeparateWindow(bool value) {
    QMutexLocker locker{&bigLock};

    if (value != separateWindow) {
        separateWindow = value;
        emit separateWindowChanged(value);
    }
}

bool Settings::getDontGroupWindows() const {
    QMutexLocker locker{&bigLock};
    return dontGroupWindows;
}

void Settings::setDontGroupWindows(bool value) {
    QMutexLocker locker{&bigLock};

    if (value != dontGroupWindows) {
        dontGroupWindows = value;
        emit dontGroupWindowsChanged(dontGroupWindows);
    }
}

bool Settings::getGroupchatPosition() const {
    QMutexLocker locker{&bigLock};
    return groupchatPosition;
}

void Settings::setGroupchatPosition(bool value) {
    QMutexLocker locker{&bigLock};

    if (value != groupchatPosition) {
        groupchatPosition = value;
        emit groupchatPositionChanged(value);
    }
}

bool Settings::getShowIdenticons() const {
    const QMutexLocker locker{&bigLock};
    return showIdenticons;
}

void Settings::setShowIdenticons(bool value) {
    const QMutexLocker locker{&bigLock};

    if (value != showIdenticons) {
        showIdenticons = value;
        emit showIdenticonsChanged(value);
    }
}

int Settings::getCircleCount() const {
    QMutexLocker locker{&bigLock};
    return circleLst.size();
}

QString Settings::getCircleName(int id) const {
    QMutexLocker locker{&bigLock};
    return circleLst[id].name;
}

void Settings::setCircleName(int id, const QString& name) {
    QMutexLocker locker{&bigLock};
    circleLst[id].name = name;
    savePersonal();
}

int Settings::addCircle(const QString& name) {
    QMutexLocker locker{&bigLock};

    circleProp cp;
    cp.expanded = false;

    if (name.isEmpty())
        cp.name = tr("Circle #%1").arg(circleLst.count() + 1);
    else
        cp.name = name;

    circleLst.append(cp);
    savePersonal();
    return circleLst.count() - 1;
}

bool Settings::getCircleExpanded(int id) const {
    QMutexLocker locker{&bigLock};
    return circleLst[id].expanded;
}

void Settings::setCircleExpanded(int id, bool expanded) {
    QMutexLocker locker{&bigLock};
    circleLst[id].expanded = expanded;
}

bool Settings::addFriendRequest(const QString& friendAddress, const QString& message) {
    QMutexLocker locker{&bigLock};

    for (auto queued : friendRequests) {
        if (queued.address == friendAddress) {
            queued.message = message;
            queued.read = false;
            return false;
        }
    }

    Request request;
    request.address = friendAddress;
    request.message = message;
    request.read = false;
    friendRequests.push_back(request);
    return true;
}

unsigned int Settings::getUnreadFriendRequests() const {
    QMutexLocker locker{&bigLock};
    unsigned int unreadFriendRequests = 0;
    for (auto request : friendRequests)
        if (!request.read) ++unreadFriendRequests;

    return unreadFriendRequests;
}

Settings::Request Settings::getFriendRequest(int index) const {
    QMutexLocker locker{&bigLock};
    return friendRequests.at(index);
}

int Settings::getFriendRequestSize() const {
    QMutexLocker locker{&bigLock};
    return friendRequests.size();
}

void Settings::clearUnreadFriendRequests() {
    QMutexLocker locker{&bigLock};

    for (auto& request : friendRequests) request.read = true;
}

void Settings::removeFriendRequest(int index) {
    QMutexLocker locker{&bigLock};
    friendRequests.removeAt(index);
}

void Settings::readFriendRequest(int index) {
    QMutexLocker locker{&bigLock};
    friendRequests[index].read = true;
}

int Settings::removeCircle(int id) {
    // Replace index with last one and remove last one instead.
    // This gives you contiguous ids all the time.
    circleLst[id] = circleLst.last();
    circleLst.pop_back();
    savePersonal();
    return circleLst.count();
}

int Settings::getThemeColor() const {
    QMutexLocker locker{&bigLock};
    return themeColor;
}

void Settings::setThemeColor(int value) {
    QMutexLocker locker{&bigLock};

    if (value != themeColor) {
        themeColor = value;
        emit themeColorChanged(themeColor);
    }
}

bool Settings::getAutoLogin() const {
    QMutexLocker locker{&bigLock};
    return autoLogin;
}

void Settings::setAutoLogin(bool state) {
    QMutexLocker locker{&bigLock};

    if (state != autoLogin) {
        autoLogin = state;
        emit autoLoginChanged(autoLogin);
    }
}

void Settings::setEnableGroupChatsColor(bool state) {
    QMutexLocker locker{&bigLock};
    if (state != nameColors) {
        nameColors = state;
        emit nameColorsChanged(nameColors);
    }
}

bool Settings::getEnableGroupChatsColor() const { return nameColors; }

/**
 * @brief Creates a path to the settings dir, if it doesn't already exist
 */
void Settings::createSettingsDir() {
    QMutexLocker locker{&bigLock};

    QString dir = Settings::getSettingsDirPath();
    QDir directory(dir);
    if (!directory.exists() && !directory.mkpath(directory.absolutePath()))
        qCritical() << "Error while creating directory " << dir;
}

/**
 * @brief Waits for all asynchronous operations to complete
 */
void Settings::sync() {
    if (QThread::currentThread() != settingsThread) {
        QMetaObject::invokeMethod(&getInstance(), "sync", Qt::BlockingQueuedConnection);
        return;
    }

    QMutexLocker locker{&bigLock};
    qApp->processEvents();
}

Settings::friendProp& Settings::getOrInsertFriendPropRef(const FriendId& id) {
    // No mutex lock, this is a private fn that should only be called by other
    // public functions that already locked the mutex
    auto it = friendLst.find(id.getByteArray());
    if (it == friendLst.end()) {
        it = friendLst.insert(id.getByteArray(), friendProp{id.toString()});
    }

    return *it;
}

ICoreSettings::ProxyType Settings::fixInvalidProxyType(ICoreSettings::ProxyType proxyType) {
    // Repair uninitialized enum that was saved to settings due to bug
    // (https://github.com/qTox/qTox/issues/5311)
    switch (proxyType) {
        case ICoreSettings::ProxyType::ptNone:
        case ICoreSettings::ProxyType::ptSOCKS5:
        case ICoreSettings::ProxyType::ptHTTP:
            return proxyType;
        default:
            qWarning() << "Repairing invalid ProxyType, UDP will be enabled";
            return ICoreSettings::ProxyType::ptNone;
    }
}
