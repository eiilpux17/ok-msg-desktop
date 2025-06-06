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

#include "pluginhost.h"

#include "accountinfoaccessor.h"
#include "activetabaccessor.h"
#include "applicationinfoaccessor.h"
#include "lib/storage/settings/applicationinfo.h"

#include "chattabaccessor.h"

#include "contactinfoaccessor.h"
#include "contactstateaccessor.h"
#include "eventcreator.h"
#include "eventfilter.h"
#include "gctoolbariconaccessor.h"

#include "OkOptions.h"
#include "base/system/sys_info.h"
#include "iconfactoryaccessor.h"
#include "iqfilter.h"
#include "iqnamespacefilter.h"
#include "menuaccessor.h"
#include "okplugin.h"
#include "optionaccessor.h"
#include "pluginaccessor.h"
#include "plugininfoprovider.h"
#include "pluginmanager.h"
#include "popupaccessor.h"
#include "psiaccountcontroller.h"
#include "psimediaaccessor.h"
#include "psimediaprovider.h"
#include "shortcutaccessor.h"
#include "soundaccessor.h"
#include "stanzafilter.h"
#include "stanzasender.h"

#include "toolbariconaccessor.h"

#include "iconset/iconset.h"
#include "textutil.h"
#include "webkitaccessor.h"

#include <QAction>
#include <QByteArray>
#include <QDomElement>
#include <QKeySequence>
#include <QObject>
#include <QPluginLoader>
#include <QRegExp>
#include <QSplitter>
#include <QString>
#include <QStringList>
#include <QTextEdit>
#include <QWidget>

namespace ok {
namespace plugin {

/**
 * \brief Constructs a host/wrapper for a plugin.
 *
 * PluginHost object manages one plugin. It can load/unload, enable/disable
 * a plugin. It also provides host-side services to the plugin.
 *
 * Constructor attempts to load the plugin, check if it is a valid psi plugin,
 * and cache its name, shortname and version.
 *
 * \param manager PluginManager instance that manages all plugins
 * \param pluginFile path to plugin file
 */
PluginHost::PluginHost(PluginManager* manager, const QString& pluginFile)
        : manager_(manager)
        , plugin_(nullptr)
        , file_(pluginFile)
        , priority_(OkPlugin::PriorityNormal)
        , loader_(nullptr)
        , valid_(false)
        , connected_(false)
        , enabled_(false)
        , hasInfo_(false)
        , infoString_(QString()) {
    updateMetadata();
}

/**
 * \brief Destroys plugin host.
 *
 * Plugin is disabled and unloaded if needed.
 */
PluginHost::~PluginHost() {
    disable();
    unload();
}

/**
 * \brief Returns true if wrapped file is a valid Psi plugin.
 *
 * Check is done once, in PluginHost constructor.
 */
bool PluginHost::isValid() const { return valid_; }

/**
 * \brief Returns full path to plugin file.
 */
const QString& PluginHost::path() const { return file_; }

/**
 * \brief Returns plugin short name.
 *
 * Data is available also when plugin is not loaded.
 */
const QString& PluginHost::shortName() const { return shortName_; }

/**
 * \brief Returns plugin version string.
 *
 * Data is available also when plugin is not loaded.
 */
const QString& PluginHost::version() const { return version_; }

/**
 * \brief Returns plugin vendor string.
 *
 * Data is available also when plugin is not loaded.
 */
const QString& PluginHost::vendor() const { return vendor_; }

/**
 * \brief Returns plugin description string.
 *
 * Data is available also when plugin is not loaded.
 */
const QString& PluginHost::description() const { return description_; }

/**
 * \brief Returns plugin priority.
 *
 * Data is available also when plugin is not loaded.
 */
int PluginHost::priority() const { return priority_; }

/**
 * \brief Returns plugin icon.
 *
 * Data is available also when plugin is not loaded.
 */
const QIcon& PluginHost::icon() const { return icon_; }

QStringList PluginHost::pluginFeatures() const {
    if (plugin_) {
        return qobject_cast<OkPlugin*>(plugin_)->pluginFeatures();
    }
    return QStringList();
}

/**
 * \brief Returns plugin options widget.
 *
 * Always returns null if plugin is not currently loaded.
 */
QWidget* PluginHost::optionsWidget() const {
    QWidget* widget = nullptr;
    if (plugin_) {
        widget = qobject_cast<OkPlugin*>(plugin_)->options();
    }
    return widget;
}

//-- loading and enabling -------------------------------------------
void PluginHost::updateMetadata() {
    if (plugin_) {
        return;
    }
    qDebug() << "Plugin file:" << file_;
    QPluginLoader loader(file_);
    if (!loader.load()) {
        qWarning() << "Can not load the plugin:" << file_ << loader.errorString();
        return;
    }
    auto md = loader.metaData();
    if (!md.isEmpty()) md = md.value("MetaData").toObject();

    // TODO: TranslationManager::instance()->currentLanguage();
    QString curLangFull = ":zh_CN";
    QString curLang = curLangFull.section('_', 0, 0);

    name_ = md.value(QLatin1String("name:") + curLangFull).toString();
    if (name_.isEmpty()) name_ = md.value(QLatin1String("name:") + curLang).toString();
    if (name_.isEmpty()) name_ = md.value(QLatin1String("name")).toString();

    shortName_ = md.value(QLatin1String("shortname")).toString();
    vendor_ = md.value(QLatin1String("vendor")).toString();
    version_ = md.value(QLatin1String("version")).toString();
    priority_ = md.value(QLatin1String("priority")).toInt(2);

    description_ = md.value(QLatin1String("description") + curLangFull).toString();
    if (description_.isEmpty())
        description_ = md.value(QLatin1String("description") + curLang).toString();
    if (description_.isEmpty()) description_ = md.value(QLatin1String("description")).toString();
    //    description_ = TextUtil::plain2rich(description_);

    QString data = md.value(QLatin1String("icon")).toString();
    if (data.startsWith("base64:")) {
        rawIcon_ = QByteArray::fromBase64(data.midRef(6).toLatin1());
        QPixmap pix;
        pix.loadFromData(rawIcon_);
        icon_ = QIcon(pix);
    } else if (data.startsWith("iconset:")) {
        //        icon_ = IconsetFactory::icon(data.mid(8)).icon();
    } else
        icon_ = QIcon(data);  // assuming file name or resource

    qDebug("Loaded plugin name is:%s shortName is:%s",  //
           qPrintable(name_), qPrintable(shortName_));

    qDebug("Loaded metadata for plugin %s", qPrintable(file_));
    valid_ = true;
    loader.unload();
}

/**
 * \brief Loads the plugin.
 *
 * Plugin is loaded but not enabled.
 * Does nothing if plugin was already loaded.
 *
 * Will fail if the plugin is not suitable (wrong Qt version,
 * wrong debug setting, wrong Psi plugin interface version etc).
 *
 * \return true if plugin is successfully loaded or if it was already loaded
 * before.
 */
bool PluginHost::load() {
    qDebug() << "Loading plugin" << file_;
    if (plugin_) {
        qWarning() << QString("Plugin %1 was already loaded.").arg(file_);
        return true;
    }

    loader_ = new QPluginLoader(file_, this);
    loader_->setLoadHints(QLibrary::ResolveAllSymbolsHint);
    bool loaded = loader_->load();
    qDebug() << "The plugin is loaded?=>" << loaded;
    if (!loaded) {
        delete loader_;
        loader_.clear();
        qWarning() << "Can not to load plugin:" << name();
        return false;
    }

    QObject* plugin = loader_->instance();
    qDebug() << "Plugin address is:" << plugin;
    OkPlugin* pPlugin = qobject_cast<OkPlugin*>(plugin);
    if (!pPlugin) {
        qWarning("File is a plugin but a valid OkPlugin");
        unload();
        return false;
    }

    plugin_ = plugin;
    valid_ = true;
    name_ = pPlugin->name();
    qDebug() << "Loaded the plugin sortName is" << shortName_;

    // shortName_ = psiPlugin->shortName();
    // version_   = psiPlugin->version();
    // priority_  = psiPlugin->priority();
    // icon_      = QIcon(psiPlugin->icon());

    plugin->setProperty("metadata", selfMetadata());

    hasToolBarButton_ = qobject_cast<ToolbarIconAccessor*>(plugin_) ? true : false;
    hasGCToolBarButton_ = qobject_cast<GCToolbarIconAccessor*>(plugin_) ? true : false;

    PluginInfoProvider* pip = qobject_cast<PluginInfoProvider*>(plugin_);
    if (pip) {
        hasInfo_ = true;
        infoString_ = pip->pluginInfo();
        qDebug() << "The plugin Info is:" << infoString_;
    }

    qDebug("Plugin loaded: %s", qPrintable(name_));
    return plugin_ != nullptr;
}

/**
 * \brief Unloads the plugin.
 *
 * Plugin is disabled (if needed) and unloaded.
 * Does nothing if plugin was not loaded yet.
 *
 * If plugin is successfully disabled but unloading fails, it is not enabled
 * again.
 *
 * \return true if plugin is successfully unloaded or if it was not loaded at
 * all.
 */
bool PluginHost::unload() {
    qDebug() << "Unloading plugin" << shortName();
    if (!plugin_) {
        qWarning() << QString("Plugin %1 was already unloaded.").arg(shortName());
        return true;
    }

    qDebug() << "Plugin address is:" << plugin_.data();
    disable();

    if (!loader_) {
        qWarning() << "The plugin was unloaded.";
        return false;
    }

    if (loader_->isLoaded()) {
        bool unloaded = loader_->unload();
        qDebug() << "Unload plugin" << name_ << "?=>" << unloaded;
        loader_->deleteLater();
    }

    // loader will delete it automatically
    delete plugin_;
    plugin_.clear();

    //            delete iconset_;
    //            iconset_   = nullptr;
    delete loader_;
    loader_.clear();

    connected_ = false;

    qDebug() << "Plugin unloaded:" << name_;
    return plugin_ == nullptr;
}

/**
 * \brief Returns true if plugin is currently loaded.
 */
bool PluginHost::isLoaded() const { return plugin_ != nullptr; }

/**
 * \brief Enabled the plugin
 *
 * Plugin is loaded (if needed) and enabled.
 * Does nothing if plugin was already enabled.
 *
 * Before plugin is enabled for the first time,
 * all appropriate set...Host() methods are called.
 *
 * Will fail if the plugin cannot be loaded or if plugin's enable() method
 * fails.
 *
 * \return true if plugin is successfully enabled or if it was already enabled
 * before.
 */
bool PluginHost::enable() {
    qDebug() << "To enabling plugin" << file_;

    QMutexLocker locker(&mutex_);

    if (!enabled_ && load()) {
        if (!connected_) {
            qDebug() << "connecting plugin " << name_;

            StanzaSender* s = qobject_cast<StanzaSender*>(plugin_);
            if (s) {
                qDebug("connecting stanza sender");
                s->setStanzaSendingHost(this);
            }

            IqFilter* f = qobject_cast<IqFilter*>(plugin_);
            if (f) {
                qDebug("connecting iq filter");
                f->setIqFilteringHost(this);
            }

            OptionAccessor* o = qobject_cast<OptionAccessor*>(plugin_);
            if (o) {
                qDebug("connecting option accessor");
                o->setOptionAccessingHost(this);
            }

            ShortcutAccessor* sa = qobject_cast<ShortcutAccessor*>(plugin_);
            if (sa) {
                qDebug("connecting shortcut accessor");
                sa->setShortcutAccessingHost(this);
            }
            PopupAccessor* pa = qobject_cast<PopupAccessor*>(plugin_);
            if (pa) {
                qDebug("connecting popup accessor");
                pa->setPopupAccessingHost(this);
            }

            IconFactoryAccessor* ia = qobject_cast<IconFactoryAccessor*>(plugin_);
            if (ia) {
                qDebug("connecting iconfactory accessor");
                ia->setIconFactoryAccessingHost(this);
            }
            ActiveTabAccessor* ta = qobject_cast<ActiveTabAccessor*>(plugin_);
            if (ta) {
                qDebug("connecting activetab accessor");
                ta->setActiveTabAccessingHost(this);
            }
            ApplicationInfoAccessor* aia = qobject_cast<ApplicationInfoAccessor*>(plugin_);
            if (aia) {
                qDebug("connecting applicationinfo accessor");
                aia->setApplicationInfoAccessingHost(this);
            }
            AccountInfoAccessor* ai = qobject_cast<AccountInfoAccessor*>(plugin_);
            if (ai) {
                qDebug("connecting accountinfo accessor");
                ai->setAccountInfoAccessingHost(this);
            }
            ToolbarIconAccessor* tia = qobject_cast<ToolbarIconAccessor*>(plugin_);
            if (tia) {
                qDebug("load toolbaricon param");
                buttons_ = tia->getButtonParam();
            }
            GCToolbarIconAccessor* gtia = qobject_cast<GCToolbarIconAccessor*>(plugin_);
            if (gtia) {
                qDebug("load gctoolbaricon param");
                gcbuttons_ = gtia->getGCButtonParam();
            }
            MenuAccessor* ma = qobject_cast<MenuAccessor*>(plugin_);
            if (ma) {
                qDebug("load menu actions param");
                accMenu_ = ma->getAccountMenuParam();
                contactMenu_ = ma->getContactMenuParam();
            }
            ContactStateAccessor* csa = qobject_cast<ContactStateAccessor*>(plugin_);
            if (csa) {
                qDebug("connecting contactstate accessor");
                csa->setContactStateAccessingHost(this);
            }
            PsiAccountController* pac = qobject_cast<PsiAccountController*>(plugin_);
            if (pac) {
                qDebug("connectint psiaccount controller");
                pac->setPsiAccountControllingHost(this);
            }
            EventCreator* ecr = qobject_cast<EventCreator*>(plugin_);
            if (ecr) {
                qDebug("connectint event creator");
                ecr->setEventCreatingHost(this);
            }
            ContactInfoAccessor* cia = qobject_cast<ContactInfoAccessor*>(plugin_);
            if (cia) {
                qDebug("connecting contactinfo accessor");
                cia->setContactInfoAccessingHost(this);
            }
            SoundAccessor* soa = qobject_cast<SoundAccessor*>(plugin_);
            if (soa) {
                qDebug("connecting sound accessor");
                soa->setSoundAccessingHost(this);
            }
            PluginAccessor* pla = qobject_cast<PluginAccessor*>(plugin_);
            if (pla) {
                qDebug("connecting plugin accessor");
                pla->setPluginAccessingHost(this);
            }
            auto wka = qobject_cast<WebkitAccessor*>(plugin_);
            if (wka) {
                wka->setWebkitAccessingHost(this);
            }
            auto pma = qobject_cast<PsiMediaAccessor*>(plugin_);
            if (pma) {
                pma->setPsiMediaHost(this);
            }

            connected_ = true;
        }

        enabled_ = qobject_cast<OkPlugin*>(plugin_)->enable();
        if (enabled_) {
            enableHandler = new QObject(this);
            emit enabled();
            qDebug() << "Enable plugin" << shortName() << "successfully.";
        }
    }

    return enabled_;
}

/**
 * \brief Disabled the plugin.
 *
 * Plugin is disabled but not unloaded.
 * Does nothing if plugin was not enabled yet.
 *
 * \return true if plugin is successfully disabled or if it was not enabled at
 * all.
 */
bool PluginHost::disable() {
    QMutexLocker locker(&mutex_);
    qDebug() << "Disable plugin" << shortName();
    if (!enabled_) {
        qWarning() << "The plugin" << shortName() << "is disabled already.";
        return true;
    }

    if (plugin_) {
        enabled_ = !qobject_cast<OkPlugin*>(plugin_)->disable();
        if (!enabled_) {
            delete enableHandler;
            emit disabled();
            qDebug() << "Disable plugin" << shortName() << "successfully.";
        }
    }
    return true;
}

/**
 * \brief Returns true if plugin is currently enabled.
 */
bool PluginHost::isEnabled() const { return enabled_; }

//-- for StanzaFilter and IqNamespaceFilter -------------------------

/**
 * \brief Give plugin the opportunity to process incoming xml
 *
 * If plugin implements incoming XML filters, they are called in turn.
 * Any handler may then modify the XML and may cause the stanza to be
 * silently discarded.
 *
 * \param account Identifier of the PsiAccount responsible
 * \param xml Incoming XML (may be modified)
 * \return Continue processing the XML stanza; true if the stanza should be
 * silently discarded.
 */
bool PluginHost::incomingXml(int account, const QDomElement& e) {
    QMutexLocker locker(&mutex_);
    if (!plugin_) {
        qWarning() << "The plugin has be unloaded.";
        return false;
    }
    bool handled = false;
    // try stanza filter first
    StanzaFilter* sf = qobject_cast<StanzaFilter*>(plugin_);
    if (sf && sf->incomingStanza(account, e)) {
        handled = true;
    }
    // try iq filters
    else if (e.tagName() == "iq") {
        // get iq namespace
        QString ns;
        for (QDomNode n = e.firstChild(); !n.isNull(); n = n.nextSibling()) {
            QDomElement i = n.toElement();
            if (!i.isNull() && !i.namespaceURI().isNull()) {
                ns = i.namespaceURI();
                break;
            }
        }

        // choose handler function depending on iq type
        bool (IqNamespaceFilter::*handler)(int account, const QDomElement& xml) = nullptr;
        const QString type = e.attribute("type");
        if (type == "get") {
            handler = &IqNamespaceFilter::iqGet;
        } else if (type == "set") {
            handler = &IqNamespaceFilter::iqSet;
        } else if (type == "result") {
            handler = &IqNamespaceFilter::iqResult;
        } else if (type == "error") {
            handler = &IqNamespaceFilter::iqError;
        }

        if (handler) {
            // normal filters
            const auto& items = iqNsFilters_.values(ns);
            for (IqNamespaceFilter* f : items) {
                if ((f->*handler)(account, e)) {
                    handled = true;
                    break;
                }
            }

            // regex filters
            QMapIterator<QRegExp, IqNamespaceFilter*> i(iqNsxFilters_);
            while (!handled && i.hasNext()) {
                if (i.key().indexIn(ns) >= 0 && (i.value()->*handler)(account, e)) {
                    handled = true;
                }
            }
        }
    }

    return handled;
}

bool PluginHost::outgoingXml(int account, QDomElement& e) {
    QMutexLocker locker(&mutex_);

    bool handled = false;
    StanzaFilter* ef = qobject_cast<StanzaFilter*>(plugin_);
    if (ef && ef->outgoingStanza(account, e)) {
        handled = true;
    }
    return handled;
}

//-- for EventFilter ------------------------------------------------

/**
 * \brief Give plugin the opportunity to process incoming event.
 *
 * If plugin implements EventFilter interface,
 * this will call its processEvent() handler.
 * Handler may then modify the event and may cause the event to be
 * silently discarded.
 *
 * \param account Identifier of the PsiAccount responsible
 * \param e Event XML
 * \return Continue processing the event; true if the stanza should be silently
 * discarded.
 */
bool PluginHost::processEvent(int account, QDomElement& e) {
    bool handled = false;
    EventFilter* ef = qobject_cast<EventFilter*>(plugin_);
    if (ef && ef->processEvent(account, e)) {
        handled = true;
    }
    return handled;
}

/**
 * \brief Give plugin the opportunity to process incoming message event.
 *
 * If plugin implements EventFilter interface,
 * this will call its processMessage() handler.
 * Handler may then modify the event and may cause the event to be
 * silently discarded.
 * TODO: modification doesn't work
 *
 * \param account Identifier of the PsiAccount responsible
 * \param jidFrom Jid of message sender
 * \param body Message body
 * \param subject Message subject
 * \return Continue processing the event; true if the stanza should be silently
 * discarded.
 */
bool PluginHost::processMessage(int account, const QString& jidFrom, const QString& body,
                                const QString& subject) {
    bool handled = false;
    EventFilter* ef = qobject_cast<EventFilter*>(plugin_);
    if (ef && ef->processMessage(account, jidFrom, body, subject)) {
        handled = true;
    }
    return handled;
}

bool PluginHost::processOutgoingMessage(int account, const QString& jidTo, QString& body,
                                        const QString& type, QString& subject) {
    bool handled = false;
    EventFilter* ef = qobject_cast<EventFilter*>(plugin_);
    if (ef && ef->processOutgoingMessage(account, jidTo, body, type, subject)) {
        handled = true;
    }
    return handled;
}

void PluginHost::logout(int account) {
    EventFilter* ef = qobject_cast<EventFilter*>(plugin_);
    if (ef) {
        ef->logout(account);
    }
}

//-- StanzaSender ---------------------------------------------------

/**
 * \brief Sends a stanza from the specified account.
 *
 * \param account Identifier of the PsiAccount responsible
 * \param stanza The stanza to be sent
 */
void PluginHost::sendStanza(int account, const QDomElement& stanza) {
    QTextStream stream;
    stream.setString(new QString());
    stanza.save(stream, QDomElement::EncodingFromDocument);
    manager_->sendXml(account, *stream.string());
}

/**
 * \brief Sends a stanza from the specified account.
 *
 * \param account Identifier of the PsiAccount responsible
 * \param stanza The stanza to be sent.
 */
void PluginHost::sendStanza(int account, const QString& stanza) {
    manager_->sendXml(account, stanza);
}

/**
 * \brief Sends a message from the specified account.
 *
 * \param account Identifier of the PsiAccount responsible
 * \param to Jid of message addressee
 * \param body Message body
 * \param subject Message type
 * \param type Message type (XMPP message type)
 * \param stanza The stanza to be sent.
 */
void PluginHost::sendMessage(int account,             //
                             const QString& to,       //
                             const QString& body,     //
                             const QString& subject,  //
                             const QString& type) {
    // XMPP::Message m;
    // m.setTo(to);
    // m.setBody(body);
    // m.setSubject(subject);
    // if (type =="chat" || type == "error" || type == "groupchat" || type ==
    // "headline" || type == "normal") {
    //    m.setType(type);
    //}
    // manager_->sendXml(account, m.toStanza(...).toString());

    // TODO(mck): yeah, that's sick..
    //    manager_->sendXml(account,
    //                      QString("<message to='%1'
    //                      type='%4'><subject>%3</subject><body>%2</body></message>")
    //                          .arg(escape(to), escape(body), escape(subject),
    //                          escape(type)));
}

/**
 * \brief Returns a unique stanza id in given account XMPP stream.
 *
 * \param account Identifier of the PsiAccount responsible
 * \return Unique stanza id, or empty string if account id is invalid.
 */
QString PluginHost::uniqueId(int account) { return manager_->uniqueId(account); }

QString PluginHost::escape(const QString& str) { return TextUtil::escape(str); }

//-- IqFilter -------------------------------------------------------

/**
 * \brief Registers an Iq handler for given namespace.
 *
 * One plugin may register multiple handlers, even for the same namespace.
 * The same handler may be registered for more than one namespace.
 * Attempts to register the same handler for the same namespace will be blocked.
 *
 * When Iq stanza arrives, it is passed in turn to matching handlers.
 * Handler may then modify the event and may cause the event to be
 * silently discarded.
 *
 * Note that iq-result may contain no namespaced element in some protocols,
 * and connection made by this method will not work in such case.
 *
 * \param ns Iq namespace
 * \param filter Filter to be registered
 */
void PluginHost::addIqNamespaceFilter(const QString& ns, IqNamespaceFilter* filter) {
    if (iqNsFilters_.values(ns).contains(filter)) {
#ifndef PLUGINS_NO_DEBUG
        qDebug("pluginmanager: blocked attempt to register the same filter again");
#endif
    } else {
        iqNsFilters_.insert(ns, filter);
    }
}

/**
 * \brief Registers an Iq handler for given namespace.
 *
 * One plugin may register multiple handlers, even for the same namespace.
 * The same handler may be registered for more than one namespace.
 * Attempts to register the same handler for the same namespace will be blocked.
 *
 * When Iq stanza arrives, it is passed in turn to matching handlers.
 * Handler may then modify the event and may cause the event to be
 * silently discarded.
 *
 * Note that iq-result may contain no namespaced element in some protocols,
 * and connection made by this method will not work in such case.
 *
 * \param ns Iq namespace defined by a regular expression
 * \param filter Filter to be registered
 */
void PluginHost::addIqNamespaceFilter(const QRegExp& ns, IqNamespaceFilter* filter) {
    // #ifndef PLUGINS_NO_DEBUG
    //     qDebug("add nsx");
    // #endif
    //     if (iqNsxFilters_.values(ns).contains(filter)) {
    // #ifndef PLUGINS_NO_DEBUG
    //         qDebug("pluginmanager: blocked attempt to register the same filter
    //         again");
    // #endif
    //     } else {
    //         iqNsxFilters_.insert(ns, filter);
    //     }
}

/**
 * \brief Unregisters namespace handler.
 *
 * Breaks connection made by addIqNamespaceFilter().
 * Note that \a filter object is never deleted by this function.
 */
void PluginHost::removeIqNamespaceFilter(const QString& ns, IqNamespaceFilter* filter) {
    iqNsFilters_.remove(ns, filter);
}

/**
 * \brief Unregisters namespace handler.
 *
 * Breaks connection made by addIqNamespaceFilter().
 * Note that \a filter object is never deleted by this function.
 */
void PluginHost::removeIqNamespaceFilter(const QRegExp& ns, IqNamespaceFilter* filter) {
    //    iqNsxFilters_.remove(ns, filter);
}

//-- OptionAccessor -------------------------------------------------

/**
 * \brief Sets an option (local to the plugin)
 * The options will be automatically prefixed by the plugin manager, so
 * there is no need to uniquely name the options. In the same way as the
 * main options system, a hierachy is available by dot-delimiting the
 * levels ( e.g. "emoticons.show"). Use this and not setGlobalOption
 * in almost every case.
 * \param  option Option to set
 * \param value New option value
 */
void PluginHost::setPluginOption(const QString& option, const QVariant& value) {
    // TODO(mck)

    // PsiPlugin* plugin=nullptr;
    //
    // if (!plugin)
    //    return;
    QString optionKey =
            QString("%1.%2.%3").arg(PluginManager::pluginOptionPrefix, shortName(), option);
    OkOptions::instance()->setOption(optionKey, value);
}

/**
 * \brief Gets an option (local to the plugin)
 * The options will be automatically prefixed by the plugin manager, so
 * there is no need to uniquely name the options. In the same way as the
 * main options system, a hierachy is available by dot-delimiting the
 * levels ( e.g. "emoticons.show"). Use this and not getGlobalOption
 * in almost every case.
 * \param  option Option to set
 * \param value Return value
 */
QVariant PluginHost::getPluginOption(const QString& option, const QVariant& defValue) {
    QString pluginName = name();
    QString optionKey =
            QString("%1.%2.%3").arg(PluginManager::pluginOptionPrefix, shortName(), option);
    return OkOptions::instance()->getOption(optionKey, defValue);
}

/**
 * \brief Sets a global option (not local to the plugin)
 * The options will be passed unaltered by the plugin manager, so
 * the options are presented as they are stored in the main option
 * system. Use setPluginOption instead of this in almost every case.
 * \param  option Option to set
 * \param value New option value
 */
void PluginHost::setGlobalOption(const QString& option, const QVariant& value) {
    OkOptions::instance()->setOption(option, value);
}

void PluginHost::addSettingPage(OAH_PluginOptionsTab* tab) {
    //    manager_->addSettingPage(tab);
}

void PluginHost::removeSettingPage(OAH_PluginOptionsTab* tab) {
    //    manager_->removeSettingPage(tab);
}

/**
 * \brief Gets a global option (not local to the plugin)
 * The options will be passed unaltered by the plugin manager, so
 * the options are presented as they are stored in the main option
 * system. Use getPluginOption instead of this in almost every case.
 * \param  option Option to set
 * \param value Return value
 */
QVariant PluginHost::getGlobalOption(const QString& option) {
    return OkOptions::instance()->getOption(option);
}

void PluginHost::optionChanged(const QString& option) {
    OptionAccessor* oa = qobject_cast<OptionAccessor*>(plugin_);
    if (oa) oa->optionChanged(option);
}

void PluginHost::applyOptions() {
    OkPlugin* pp = qobject_cast<OkPlugin*>(plugin_);
    if (pp) pp->applyOptions();
}

void PluginHost::restoreOptions() {
    OkPlugin* pp = qobject_cast<OkPlugin*>(plugin_);
    if (pp) pp->restoreOptions();
}

/**
 * Shortcut accessing host
 */
void PluginHost::setShortcuts() {
    ShortcutAccessor* sa = qobject_cast<ShortcutAccessor*>(plugin_);
    if (sa) {
        sa->setShortcuts();
    }
}

void PluginHost::connectShortcut(const QKeySequence& shortcut, QObject* receiver,
                                 const char* slot) {
    //    GlobalShortcutManager::instance()->connect(shortcut, receiver, slot);
}

void PluginHost::disconnectShortcut(const QKeySequence& shortcut, QObject* receiver,
                                    const char* slot) {
    //    GlobalShortcutManager::instance()->disconnect(shortcut, receiver, slot);
}

void PluginHost::requestNewShortcut(QObject* receiver, const char* slot) {
    //    GrepShortcutKeyDialog *grep = new GrepShortcutKeyDialog();
    //    connect(grep, SIGNAL(newShortcutKey(QKeySequence)), receiver, slot);
    //    grep->show();
}

/**
 * IconFactory accessing host
 */
QIcon PluginHost::getIcon(const QString& name) { return IconsetFactory::icon(name).icon(); }

void PluginHost::addIcon(const QString& name, const QByteArray& ba) {
    QPixmap pm;
    pm.loadFromData(ba);
    //    PsiIcon icon;
    //    icon.setImpix(pm);
    //    icon.setName(name);
    //    if (!iconset_) {
    //        iconset_ = new Iconset();
    //    }
    //    iconset_->setIcon(name, icon);
    //    iconset_->addToFactory();
}

QTextEdit* PluginHost::getEditBox() {
    QTextEdit* ed = nullptr;
    //    TabbableWidget *tw = findActiveTab();
    //    if (tw) {
    //        QWidget *chatEditProxy = tw->findChild<QWidget *>("mle");
    //        if (chatEditProxy) {
    //            ed = static_cast<QTextEdit *>(chatEditProxy->children().at(1));
    //        }
    //    }

    return ed;
}

QString PluginHost::getJid() {
    QString jid;
    //    TabbableWidget *tw = findActiveTab();
    //    if (tw) {
    //        jid = tw->jid().full();
    //    }

    return jid;
}

QString PluginHost::getYourJid() {
    QString jid;
    //    TabbableWidget *tw = findActiveTab();
    //    if (tw) {
    //        jid = tw->account()->jid().full();
    //    }

    return jid;
}

/**
 * ApplicationInfo accessing host
 */
Proxy PluginHost::getProxyFor(const QString& obj) {
    Proxy prx;
    //    ProxyItem it = ProxyManager::instance()->getItemForObject(obj);
    //    prx.type     = it.type;
    //    prx.host     = it.settings.host;
    //    prx.port     = it.settings.port;
    //    prx.user     = it.settings.user;
    //    prx.pass     = it.settings.pass;
    return prx;
}

QString PluginHost::appName() { return ApplicationInfo::name(); }

QString PluginHost::appVersion() { return ApplicationInfo::version(); }

QString PluginHost::appCapsNode() { return ApplicationInfo::capsNode(); }

QString PluginHost::appCapsVersion() {  // this stuff is incompatible with new caps 1.5
    return QString();                   // return ApplicationInfo::capsVersion();
}

QString PluginHost::appOsName() { return ApplicationInfo::osName(); }

QString PluginHost::appOsVersion() { return ok::base::SystemInfo::instance()->osVersion(); }

QString PluginHost::appHomeDir(ApplicationInfoAccessingHost::HomedirType type) {
    return ApplicationInfo::homeDir(ApplicationInfo::HomedirType(type));
}

QString PluginHost::appResourcesDir() { return ApplicationInfo::resourcesDir(); }

QString PluginHost::appLibDir() { return ApplicationInfo::libDir(); }

QString PluginHost::appProfilesDir(ApplicationInfoAccessingHost::HomedirType type) {
    return ApplicationInfo::profilesDir(ApplicationInfo::HomedirType(type));
}

QString PluginHost::appHistoryDir() { return ApplicationInfo::historyDir(); }

QString PluginHost::appCurrentProfileDir(ApplicationInfoAccessingHost::HomedirType type) {
    return ApplicationInfo::currentProfileDir(ApplicationInfo::HomedirType(type));
}

QString PluginHost::appVCardDir() { return ApplicationInfo::vCardDir(); }

// AccountInfoAcsessingHost
QString PluginHost::getStatus(int account) {
    // return manager_->getStatus(account);
    return {};
}

QString PluginHost::getStatusMessage(int account) {
    //    return manager_->getStatusMessage(account);
    return {};
}

QString PluginHost::proxyHost(int account) { return manager_->proxyHost(account); }

int PluginHost::proxyPort(int account) { return manager_->proxyPort(account); }

QString PluginHost::proxyUser(int account) { return manager_->proxyUser(account); }

QString PluginHost::proxyPassword(int account) { return manager_->proxyPassword(account); }

QStringList PluginHost::getRoster(int account) { return manager_->getRoster(account); }

QString PluginHost::getJid(int account) { return manager_->getJid(account); }

QString PluginHost::getId(int account) { return manager_->getId(account); }

QString PluginHost::getName(int account) { return manager_->getName(account); }

int PluginHost::findOnlineAccountForContact(const QString& jid) const {
    return manager_->findOnlineAccountForContact(jid);
}

QString PluginHost::getPgpKey(int account) { return manager_->getPgpKey(account); }

QMap<QString, QString> PluginHost::getKnownPgpKeys(int account) {
    return manager_->getKnownPgpKeys(account);
}

void PluginHost::subscribeBeforeLogin(QObject* context, std::function<void(int)> callback) {
    connect(manager_, &PluginManager::accountBeforeLogin, context, callback);
}

void PluginHost::subscribeLogout(QObject* context, std::function<void(int account)> callback) {
    connect(manager_, &PluginManager::accountLoggedOut, context, callback);
}

void PluginHost::setPgpKey(int account, const QString& keyId) {
    manager_->setPgpKey(account, keyId);
}

void PluginHost::removeKnownPgpKey(int account, const QString& jid) {
    manager_->removeKnownPgpKey(account, jid);
}

void PluginHost::setClientVersionInfo(int account, const QVariantMap& info) {
    manager_->setClientVersionInfo(account, info);
}

bool PluginHost::setActivity(int account, const QString& Jid, QDomElement xml) {
    return manager_->setActivity(account, Jid, xml);
}

bool PluginHost::setMood(int account, const QString& Jid, QDomElement xml) {
    return manager_->setMood(account, Jid, xml);
}
bool PluginHost::setTune(int account, const QString& Jid, QString tune) {
    return manager_->setTune(account, Jid, tune);
}

void PluginHost::addToolBarButton(QObject* parent, QWidget* toolbar, int account,
                                  const QString& contact) {
    ToolbarIconAccessor* ta = qobject_cast<ToolbarIconAccessor*>(plugin_);
    if (ta) {
        if (!buttons_.isEmpty()) {
            for (int i = 0; i < buttons_.size(); ++i) {
                QVariantHash param = buttons_.at(i);
                QString th = param.value("tooltip").value<QString>();
                //           TODO:插件头像
                //                IconAction * button = new IconAction(th,
                //                param.value("icon").value<QString>(), th, 0, parent);
                //                connect(button, SIGNAL(triggered()),
                //                param.value("reciver").value<QObject *>(),
                //                        param.value("slot").value<QString>().toLatin1());
                //                connect(enableHandler, &QObject::destroyed, button,
                //                &QObject::deleteLater); toolbar->addAction(button);
            }
        }
        QAction* act = ta->getAction(parent, account, contact);
        if (act) {
            act->setObjectName(shortName_);
            connect(enableHandler, &QObject::destroyed, act, &QObject::deleteLater);
            toolbar->addAction(act);
        }
    }
}

bool PluginHost::hasToolBarButton() { return hasToolBarButton_; }

void PluginHost::addGCToolBarButton(QObject* parent, QWidget* toolbar, int account,
                                    const QString& contact) {
    GCToolbarIconAccessor* ta = qobject_cast<GCToolbarIconAccessor*>(plugin_);
    if (ta) {
        if (!gcbuttons_.isEmpty()) {
            for (int i = 0; i < gcbuttons_.size(); ++i) {
                QVariantHash param = gcbuttons_.at(i);
                QString th = param.value("tooltip").value<QString>();
                //                IconAction * button = new IconAction(th,
                //                param.value("icon").value<QString>(), th, 0, parent);
                //                connect(button, SIGNAL(triggered()),
                //                param.value("reciver").value<QObject *>(),
                //                        param.value("slot").value<QString>().toLatin1());
                //                connect(enableHandler, &QObject::destroyed, button,
                //                &QObject::deleteLater); toolbar->addAction(button);
            }
        }
        QAction* act = ta->getGCAction(parent, account, contact);
        if (act) {
            act->setObjectName(shortName_);
            connect(enableHandler, &QObject::destroyed, act, &QObject::deleteLater);
            toolbar->addAction(act);
        }
    }
}

bool PluginHost::hasGCToolBarButton() { return hasGCToolBarButton_; }

void PluginHost::initPopup(const QString& text, const QString& title, const QString& icon,
                           int type) {
    manager_->initPopup(text, title, icon, type);
}

void PluginHost::initPopupForJid(int account, const QString& jid, const QString& text,
                                 const QString& title, const QString& icon, int type) {
    manager_->initPopupForJid(account, jid, text, title, icon, type);
}

int PluginHost::registerOption(const QString& name, int initValue, const QString& path) {
    return manager_->registerOption(name, initValue, path);
}

void PluginHost::unregisterOption(const QString& name) { manager_->unregisterOption(name); }

int PluginHost::popupDuration(const QString& name) { return manager_->popupDuration(name); }

void PluginHost::setPopupDuration(const QString& name, int value) {
    manager_->setPopupDuration(name, value);
}

void PluginHost::addAccountMenu(QMenu* menu, int account) {
    MenuAccessor* ma = qobject_cast<MenuAccessor*>(plugin_);
    if (ma) {
        if (!accMenu_.isEmpty()) {
            for (int i = 0; i < accMenu_.size(); ++i) {
                QVariantHash param = accMenu_.at(i);
                //                IconAction * act
                //                    = new
                //                    IconAction(param.value("name").value<QString>(),
                //                    menu, param.value("icon").value<QString>());
                //                act->setProperty("account", QVariant(account));
                //                connect(act, SIGNAL(triggered()),
                //                param.value("reciver").value<QObject *>(),
                //                        param.value("slot").value<QString>().toLatin1());
                //                menu->addAction(act);
            }
        }
        QAction* act = ma->getAccountAction(menu, account);
        if (act) menu->addAction(act);
    }
}

void PluginHost::addContactMenu(QMenu* menu, int account, const QString& jid) {
    MenuAccessor* ma = qobject_cast<MenuAccessor*>(plugin_);
    if (ma) {
        if (!contactMenu_.isEmpty()) {
            for (int i = 0; i < contactMenu_.size(); ++i) {
                QVariantHash param = contactMenu_.at(i);
                //                IconAction * act
                //                    = new
                //                    IconAction(param.value("name").value<QString>(),
                //                    menu, param.value("icon").value<QString>());
                //                act->setProperty("account", QVariant(account));
                //                act->setProperty("jid", QVariant(jid));
                //                connect(act, SIGNAL(triggered()),
                //                param.value("reciver").value<QObject *>(),
                //                        param.value("slot").value<QString>().toLatin1());
                //                menu->addAction(act);
            }
        }
        QAction* act = ma->getContactAction(menu, account, jid);
        if (act) menu->addAction(act);
    }
}

void PluginHost::setupChatTab(QWidget* tab, int account, const QString& contact) {
    ChatTabAccessor* cta = qobject_cast<ChatTabAccessor*>(plugin_);
    if (cta) {
        cta->setupChatTab(tab, account, contact);
    }
}

void PluginHost::setupGCTab(QWidget* tab, int account, const QString& contact) {
    ChatTabAccessor* cta = qobject_cast<ChatTabAccessor*>(plugin_);
    if (cta) {
        cta->setupGCTab(tab, account, contact);
    }
}

bool PluginHost::appendingChatMessage(int account, const QString& contact, QString& body,
                                      QDomElement& html, bool local) {
    ChatTabAccessor* cta = qobject_cast<ChatTabAccessor*>(plugin_);
    if (cta) {
        return cta->appendingChatMessage(account, contact, body, html, local);
    }
    return false;
}

bool PluginHost::isSelf(int account, const QString& jid) { return manager_->isSelf(account, jid); }

bool PluginHost::isAgent(int account, const QString& jid) {
    return manager_->isAgent(account, jid);
}

bool PluginHost::inList(int account, const QString& jid) { return manager_->inList(account, jid); }

bool PluginHost::isPrivate(int account, const QString& jid) {
    return manager_->isPrivate(account, jid);
}

bool PluginHost::isConference(int account, const QString& jid) {
    return manager_->isConference(account, jid);
}

QString PluginHost::name(int account, const QString& jid) { return manager_->name(account, jid); }

QString PluginHost::status(int account, const QString& jid) {
    return manager_->status(account, jid);
}

QString PluginHost::statusMessage(int account, const QString& jid) {
    return manager_->statusMessage(account, jid);
}

QStringList PluginHost::resources(int account, const QString& jid) {
    return manager_->resources(account, jid);
}

QString PluginHost::realJid(int account, const QString& jid) {
    return manager_->realJid(account, jid);
}

QString PluginHost::mucNick(int account, const QString& mucJid) {
    return manager_->mucNick(account, mucJid);
}

QStringList PluginHost::mucNicks(int account, const QString& mucJid) {
    return manager_->mucNicks(account, mucJid);
}

bool PluginHost::hasCaps(int account, const QString& jid, const QStringList& caps) {
    return manager_->hasCaps(account, jid, caps);
}

bool PluginHost::hasInfoProvider() { return hasInfo_; }

QString PluginHost::pluginInfo() { return infoString_; }

void PluginHost::setStatus(int account, const QString& status, const QString& statusMessage) {
    manager_->setStatus(account, status, statusMessage);
}

bool PluginHost::appendSysMsg(int account, const QString& jid, const QString& message) {
    return manager_->appendSysMsg(account, jid, message);
}

bool PluginHost::appendSysHtmlMsg(int account, const QString& jid, const QString& message) {
    return manager_->appendSysHtmlMsg(account, jid, message);
}

void PluginHost::createNewEvent(int account, const QString& jid, const QString& descr,
                                QObject* receiver, const char* slot) {
    manager_->createNewEvent(account, jid, descr, receiver, slot);
}

void PluginHost::createNewMessageEvent(int account, QDomElement const& element) {
    manager_->createNewMessageEvent(account, element);
}

void PluginHost::playSound(const QString& fileName) {
    //    soundPlay(fileName);
}

/**
 * EncryptionSupport
 */

bool PluginHost::decryptMessageElement(int account, QDomElement& message) {
    QMutexLocker locker(&mutex_);

    qDebug() << "decryptMessageElement account:" << account << "msg:" << &message;
    auto es = qobject_cast<ok::plugin::EncryptionSupport*>(plugin_);
    bool decrypted = es && es->decryptMessageElement(account, message);
    qDebug() << "decryptMessageElement account:" << account << decrypted;
    return decrypted;
}

bool PluginHost::encryptMessageElement(int account, QDomElement& message) {
    QMutexLocker locker(&mutex_);

    if (plugin_.isNull()) {
        qWarning() << "Unable find plugin";
        return false;
    }

    auto es = qobject_cast<ok::plugin::EncryptionSupport*>(plugin_);
    if (!es) {
        qWarning() << "Unable find plugin for EncryptionSupport";
        return false;
    }

    qDebug() << "encryptMessageElement account:" << account
             << "msg:" << message.ownerDocument().toString();
    auto encrypted = es->encryptMessageElement(account, message);
    qDebug() << "encryptMessageElement account:" << account << encrypted;
    return true;
}

/**
 * PluginAccessingHost
 */

QObject* PluginHost::getPlugin(const QString& shortName) {
    for (PluginHost* plugin : qAsConst(manager_->pluginsByPriority_)) {
        if (plugin->shortName() == shortName || plugin->name() == shortName) {
            return plugin->plugin_;
        }
    }
    return nullptr;
}

QVariantMap PluginHost::selfMetadata() const {
    QVariantMap md;
    md.insert(QLatin1String("name"), name_);
    md.insert(QLatin1String("shortname"), shortName_);
    md.insert(QLatin1String("version"), version_);
    md.insert(QLatin1String("priority"), priority_);
    md.insert(QLatin1String("icon"), icon_);
    md.insert(QLatin1String("rawIcon"), rawIcon_);
    md.insert(QLatin1String("description"), description_);
    return md;
}

WebkitAccessingHost::RenderType PluginHost::chatLogRenderType() const {
#ifdef WEBKIT
#ifdef WEBENGINE
    return WebkitAccessingHost::RT_WebEngine;
#else
    return WebkitAccessingHost::RT_WebKit;
#endif
#else
    return WebkitAccessingHost::RT_TextEdit;
#endif
}

QString PluginHost::installChatLogJSDataFilter(const QString& js, OkPlugin::Priority priority) {
    return {};  // manager_->installChatLogJSDataFilter(js, priority);
}

// void PluginHost::uninstallChatLogJSDataFilter(const QString &id) {
//   manager_->uninstallChatLogJSDataFilter(id);
// }

void PluginHost::executeChatLogJavaScript(QWidget* log, const QString& js) {
#ifdef WEBKIT
    auto cv = qobject_cast<ChatView*>(log);
    if (!cv) {
        cv = log->findChild<ChatView*>("log");
    }
    if (cv) {
        cv->sendJsCode(js);
    }
#else
    Q_UNUSED(log);
    Q_UNUSED(js)
#endif
}

void PluginHost::selectMediaDevices(const QString& audioInput,
                                    const QString& audioOutput,
                                    const QString& videoInput) {
    qDebug() << "selectMediaDevices";
    //    MediaDeviceWatcher::instance()->selectDevices(audioInput, audioOutput,
    //    videoInput);
}

void PluginHost::setMediaProvider(PsiMedia::Provider* provider) {
    //    PsiMedia::setProvider(provider);
    //    MediaDeviceWatcher::instance()->setup();
}

//-- helpers --------------------------------------------------------

}  // namespace plugin
}  // namespace ok
