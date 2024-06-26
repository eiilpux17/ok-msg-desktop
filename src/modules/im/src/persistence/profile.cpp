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

#include <QBuffer>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QObject>
#include <QSaveFile>
#include <QThread>

#include <cassert>
#include <sodium.h>

#include "base/files.h"
#include "profile.h"
#include "profilelocker.h"
#include "settings.h"
#include "src/core/core.h"
#include "src/core/coreav.h"
#include "src/core/corefile.h"
#include "src/net/avatarbroadcaster.h"
#include "src/nexus.h"
#include "src/widget/gui.h"
#include "src/widget/tool/identicon.h"
#include "src/widget/widget.h"
#include "base/images.h"
#include "base/OkSettings.h"

/**
 * @class Profile
 * @brief Manages user profiles.
 *
 * @var bool Profile::newProfile
 * @brief True if this is a newly created profile, with no .tox save file yet.
 *
 * @var bool Profile::isRemoved
 * @brief True if the profile has been removed by remove().
 */

QStringList Profile::profiles;

void Profile::initCore(const QByteArray &toxsave, ICoreSettings &s,
                       bool isNewProfile) {
  if (toxsave.isEmpty() && !isNewProfile) {
    qCritical() << "Existing toxsave is empty";
    emit failedToStart();
  }

  if (!toxsave.isEmpty() && isNewProfile) {
    qCritical() << "New profile has toxsave data";
    emit failedToStart();
  }

  Core::ToxCoreErrors err;
  core = Core::makeToxCore(toxsave, &s, &err);
  if (!core) {
    switch (err) {
    case Core::ToxCoreErrors::BAD_PROXY:
      emit badProxy();
      break;
    case Core::ToxCoreErrors::ERROR_ALLOC:
    case Core::ToxCoreErrors::FAILED_TO_START:
    case Core::ToxCoreErrors::INVALID_SAVE:
    default:
      emit failedToStart();
    }

    qDebug() << "failed to start ToxCore";
    return;
  }

  //  if (isNewProfile) {
  //    core->setStatusMessage(tr("Toxing on qTox"));
  //    core->setUsername(name);
  //  }

  // save tox file when Core requests it
  connect(core.get(), &Core::saveRequest, this, &Profile::onSaveToxSave);
  // react to avatar changes
  connect(core.get(), &Core::friendAvatarRemoved, this, &Profile::removeAvatar);
  connect(core.get(), &Core::friendAvatarChanged, this,
          &Profile::setFriendAvatar);
  connect(core.get(), &Core::fileAvatarOfferReceived, this,
          &Profile::onAvatarOfferReceived,
          Qt::ConnectionType::QueuedConnection);
}

Profile::Profile(QString name, const QString &password, bool isNewProfile,
                 const QByteArray &toxsave, std::unique_ptr<ToxEncrypt> passkey)
    : name{name}, passkey{std::move(passkey)}, isRemoved{false},
      encrypted{this->passkey != nullptr} {

  qDebug() << "Initialize profile for" << name;

  Settings &s = Settings::getInstance();
  // Core settings are saved per profile, need to load them before starting Core
  s.loadPersonal(name, this->passkey.get());

  // TODO(kriby): Move/refactor core initialization to remove settings
  // dependency
  //  note to self: use slots/signals for this?
  initCore(toxsave, s, isNewProfile);

  loadDatabase(password);
}

/**
 * @brief Locks and loads an existing profile and creates the associate Core*
 * instance.
 * @param name Profile name.
 * @param password Profile password.
 * @return Returns a nullptr on error. Profile pointer otherwise.
 *
 * @example If the profile is already in use return nullptr.
 */
Profile *Profile::loadProfile(QString name, const QCommandLineParser *parser,
                              const QString &password) {
  if (ProfileLocker::hasLock()) {
    qCritical() << "Tried to load profile " << name
                << ", but another profile is already locked!";
    return nullptr;
  }

  if (!ProfileLocker::lock(name)) {
    qWarning() << "Failed to lock profile " << name;
    return nullptr;
  }

  std::unique_ptr<ToxEncrypt> tmpKey = nullptr;
  QByteArray data = QByteArray();
  Profile *p = nullptr;
  qint64 fileSize = 0;

  Settings &s = Settings::getInstance();
  QString path = s.getSettingsDirPath() + name + ".tox";
  QFile saveFile(path);
  qDebug() << "Loading tox save " << path;

  if (!saveFile.exists()) {
    qWarning() << "The tox save file " << path << " was not found";
    goto fail;
  }

  if (!saveFile.open(QIODevice::ReadOnly)) {
    qCritical() << "The tox save file " << path << " couldn't' be opened";
    goto fail;
  }

  fileSize = saveFile.size();
  if (fileSize <= 0) {
    qWarning() << "The tox save file" << path << " is empty!";
    goto fail;
  }

  data = saveFile.readAll();
  //  if (ToxEncrypt::isEncrypted(data)) {
  //    if (password.isEmpty()) {
  //      qCritical()
  //          << "The tox save file is encrypted, but we don't have a
  //          password!";
  //      goto fail;
  //    }
  //
  //    tmpKey = ToxEncrypt::makeToxEncrypt(password, data);
  //    if (!tmpKey) {
  //      qCritical() << "Failed to derive key of the tox save file";
  //      goto fail;
  //    }
  //
  //    data = tmpKey->decrypt(data);
  //    if (data.isEmpty()) {
  //      qCritical() << "Failed to decrypt the tox save file";
  //      goto fail;
  //    }
  //  } else {
  //    if (!password.isEmpty()) {
  //      qWarning()
  //          << "We have a password, but the tox save file is not encrypted";
  //    }
  //  }

  saveFile.close();
  p = new Profile(name, password, false, data, std::move(tmpKey));

  s.updateProfileData(p, parser);
  return p;

// cleanup in case of error
fail:
  saveFile.close();
  ProfileLocker::unlock();
  return nullptr;
}

/**
 * @brief Creates a new profile and the associated Core* instance.
 * @param name Username.
 * @param password If password is not empty, the profile will be encrypted.
 * @return Returns a nullptr on error. Profile pointer otherwise.
 *
 * @note If the profile is already in use return nullptr.
 */
Profile *Profile::createProfile(QString name, const QCommandLineParser *parser,
                                QString password) {

  std::unique_ptr<ToxEncrypt> tmpKey;
  if (!password.isEmpty()) {
    tmpKey = ToxEncrypt::makeToxEncrypt(password);
    if (!tmpKey) {
      qCritical() << "Failed to derive key for the tox save";
      return nullptr;
    }
  }

  if (ProfileLocker::hasLock()) {
    qCritical() << "Tried to create profile " << name
                << ", but another profile is already locked!";
    return nullptr;
  }

  if (exists(name)) {
    qCritical() << "Tried to create profile " << name
                << ", but it already exists!";
    return nullptr;
  }

  if (!ProfileLocker::lock(name)) {
    qWarning() << "Failed to lock profile " << name;
    return nullptr;
  }

  Settings::getInstance().createPersonal(name);
  Profile *p =
      new Profile(name, password, true, QByteArray(), std::move(tmpKey));
  return p;
}

Profile::~Profile() {
  if (isRemoved) {
    return;
  }

  onSaveToxSave();
  Settings::getInstance().savePersonal(this);
  Settings::getInstance().sync();
  ProfileLocker::assertLock();
  assert(ProfileLocker::getCurLockName() == name);
  ProfileLocker::unlock();
}

/**
 * @brief Lists all the files in the config dir with a given extension
 * @param extension Raw extension, e.g. "jpeg" not ".jpeg".
 * @return Vector of filenames.
 */
QStringList Profile::getFilesByExt(QString extension) {
  QDir dir(Settings::getInstance().getSettingsDirPath());
  QStringList out;
  dir.setFilter(QDir::Files | QDir::NoDotAndDotDot);
  dir.setNameFilters(QStringList("*." + extension));
  QFileInfoList list = dir.entryInfoList();
  out.reserve(list.size());
  for (QFileInfo file : list) {
    out += file.completeBaseName();
  }

  return out;
}

/**
 * @brief Scan for profile, automatically importing them if needed.
 * @warning NOT thread-safe.
 */
const QStringList Profile::getAllProfileNames() {
  profiles.clear();
  QStringList toxfiles = getFilesByExt("tox"), inifiles = getFilesByExt("ini");
  for (const QString &toxfile : toxfiles) {
    if (!inifiles.contains(toxfile)) {
      Settings::getInstance().createPersonal(toxfile);
    }

    profiles.append(toxfile);
  }
  return profiles;
}

Core *Profile::getCore() {
  // TODO(sudden6): this is evil
  return core.get();
}

QString Profile::getName() const { return name; }

/**
 * @brief Starts the Core thread
 */
void Profile::startCore() {
  // kriby: code duplication belongs in initCore, but cannot yet due to
  // Core/Profile coupling

  connect(core.get(), &Core::requestSent, this, &Profile::onRequestSent);
  emit coreChanged(*core);

  core->start();

  //  const ToxId &selfId = core->getSelfId();
  //  const ToxPk &selfPk = selfId.getPublicKey();
  //  const QByteArray data = loadAvatarData(selfPk);
  //  if (data.isEmpty()) {
  //    qDebug() << "Self avatar not found, will broadcast empty avatar to
  //    friends";
  //  }
  //  // TODO(sudden6): moved here, because it crashes in the constructor
  //  // reason: Core::getInstance() returns nullptr, because it's not yet
  //  // initialized solution: kill Core::getInstance
  //  setAvatar(data);
}

/**
 * @brief Saves the profile's .tox save, encrypted if needed.
 * @warning Invalid on deleted profiles.
 */
void Profile::onSaveToxSave() {
  QByteArray data = core->getToxSaveData();
  if (!data.size()) {
    return;
  }
  // assert(data.size());
  saveToxSave(data);
}

// TODO(sudden6): handle this better maybe?
void Profile::onAvatarOfferReceived(QString friendId, QString fileId,
                                    const QByteArray &avatarHash) {
  // accept if we don't have it already
  const bool accept =
      getAvatarHash(core->getFriendPublicKey(friendId)) != avatarHash;
  core->getCoreFile()->handleAvatarOffer(friendId, fileId, accept);
}

/**
 * @brief Write the .tox save, encrypted if needed.
 * @param data Byte array of profile save.
 * @return true if successfully saved, false otherwise
 * @warning Invalid on deleted profiles.
 */
bool Profile::saveToxSave(QByteArray data) {
  assert(!isRemoved);
  ProfileLocker::assertLock();
  assert(ProfileLocker::getCurLockName() == name);

  QString path = Settings::getInstance().getSettingsDirPath() + name + ".tox";
  qDebug() << "Saving tox save to " << path;
  QSaveFile saveFile(path);
  if (!saveFile.open(QIODevice::WriteOnly)) {
    qCritical() << "Tox save file " << path << " couldn't be opened";
    return false;
  }

  if (encrypted) {
    data = passkey->encrypt(data);
    if (data.isEmpty()) {
      qCritical() << "Failed to encrypt, can't save!";
      saveFile.cancelWriting();
      return false;
    }
  }

  saveFile.write(data);

  // check if everything got written
  if (saveFile.flush()) {
    saveFile.commit();
  } else {
    saveFile.cancelWriting();
    qCritical() << "Failed to write, can't save!";
    return false;
  }
  return true;
}

/**
 * @brief Gets the path of the avatar file cached by this profile and
 * corresponding to this owner ID.
 * @param owner Path to avatar of friend with this PK will returned.
 * @param forceUnencrypted If true, return the path to the plaintext file even
 * if this is an encrypted profile.
 * @return Path to the avatar.
 */
QString Profile::avatarPath(const ToxPk &owner, bool forceUnencrypted) {
  const QString ownerStr = owner.toString();
  return Settings::getInstance().getSettingsDirPath() + "avatars/" + ownerStr + ".png";
}

/**
 * @brief Get our avatar from cache.
 * @return Avatar as QPixmap.
 */
QPixmap Profile::loadAvatar() {
  return loadAvatar(core->getSelfId().getPublicKey());
}

/**
 * @brief Get a contact's avatar from cache.
 * @param owner Friend PK to load avatar.
 * @return Avatar as QPixmap.
 */
QPixmap Profile::loadAvatar(const ToxPk &owner) {
  QPixmap pic;
  if (Settings::getInstance().getShowIdenticons()) {
    const QByteArray avatarData = loadAvatarData(owner);
    if (avatarData.isEmpty()) {
      pic = QPixmap::fromImage(Identicon(owner.getByteArray()).toImage(16));
    } else {
      pic.loadFromData(avatarData);
    }
  } else {
    pic.loadFromData(loadAvatarData(owner));
  }
  return pic;
}

/**
 * @brief Get a contact's avatar from cache.
 * @param owner Friend PK to load avatar.
 * @return Avatar as QByteArray.
 */
QByteArray Profile::loadAvatarData(const ToxPk &owner) {
  QString path = avatarPath(owner);
  bool avatarEncrypted = encrypted;
  // If the encrypted avatar isn't found, try loading the unencrypted one for
  // the same ID
  if (avatarEncrypted && !QFile::exists(path)) {
    avatarEncrypted = false;
    path = avatarPath(owner, true);
  }

  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    return {};
  }

  QByteArray pic = file.readAll();
  if (avatarEncrypted && !pic.isEmpty()) {
    pic = passkey->decrypt(pic);
    if (pic.isEmpty()) {
      qWarning() << "Failed to decrypt avatar at" << path;
    }
  }

  return pic;
}

void Profile::loadDatabase(QString password) {
  assert(core);

  if (isRemoved) {
    qDebug() << "Can't load database of removed profile";
    return;
  }
  //    auto selfId = core->getSelfId();
  //    name = selfId.toString();
  //    QByteArray salt = selfId.getPublicKey().getByteArray();
  QByteArray salt = password.toUtf8();
  //    if (salt.size() != TOX_PASS_SALT_LENGTH) {
  //      qWarning() << "Couldn't compute salt from public key" << name;
  //      GUI::showError(
  //          QObject::tr("Error"),
  //          QObject::tr(
  //              "qTox couldn't open your chat logs, they will be disabled."));
  //    }
  // At this point it's too early to load the personal settings (Nexus will do
  // it), so we always load the history, and if it fails we can't change the
  // setting now, but we keep a nullptr
  qDebug() << "Create database for" << name;
  database = std::make_shared<RawDatabase>(getDbPath(name), password, salt);
  if (database && database->isOpen()) {
    history.reset(new History(database));
  } else {
    qWarning() << "Failed to open database for profile" << name;
    GUI::showError(
        QObject::tr("Error"),
        QObject::tr(
            "qTox couldn't open your chat logs, they will be disabled."));
  }
}

/**
 * @brief Sets our own avatar
 * @param pic Picture to use as avatar, if empty an Identicon will be used
 * depending on settings
 */
void Profile::setAvatar(QByteArray pic) {
  bool loaded = false;
  QPixmap pixmap;
  QByteArray avatarData;
  const ToxPk &selfPk = core->getSelfPublicKey();
  if (!pic.isEmpty()) {
    loaded = pixmap.loadFromData(pic);
    avatarData = pic;
  } else {
    if (Settings::getInstance().getShowIdenticons()) {
      const QImage identicon = Identicon(selfPk.getByteArray()).toImage(32);
      pixmap = QPixmap::fromImage(identicon);
    } else {
      loaded = pixmap.load(":/img/contact_dark.svg");
    }
  }
  qDebug() << "image loaded=>" << loaded;
  if (!loaded) {
    return;
  }
  qDebug() << "pixmap" << pixmap.size();
  saveAvatar(selfPk, avatarData);

  emit selfAvatarChanged(pixmap);
  AvatarBroadcaster::setAvatar(avatarData);
  AvatarBroadcaster::enableAutoBroadcast();

  core->setAvatar(avatarData);
}

void Profile::setAvatarOnly(QPixmap pixmap) { emit selfAvatarChanged(pixmap); }

/**
 * @brief Sets a friends avatar
 * @param pic Picture to use as avatar, if empty an Identicon will be used
 * depending on settings
 * @param owner pk of friend
 */
void Profile::setFriendAvatar(const ToxPk owner, std::string pic) {
  qDebug() << "Profile::setFriendAvatar:" << owner.toString()
           << "picSize:" << pic.size();

  QImage pixmap;
  QByteArray avatarData;
  if (!pic.empty()) {
    auto bb = QByteArray::fromStdString(pic);

    bool loaded =
        base::Images::putToImage(bb, pixmap);
    if (!loaded) {
      return;
    }

    avatarData = bb;
    emit friendAvatarSet(owner, pic);
  } else if (Settings::getInstance().getShowIdenticons()) {
    pixmap = Identicon(owner.getByteArray()).toImage(32);
    emit friendAvatarSet(owner, pic);
  } else {
    pixmap.load(":/img/contact_dark.svg");
    emit friendAvatarRemoved(owner);
  }
  friendAvatarChanged(owner, QPixmap::fromImage(pixmap));
  saveAvatar(owner, avatarData);
}

/**
 * @brief Adds history message about friendship request attempt if history is
 * enabled
 * @param friendPk Pk of a friend which request is destined to
 * @param message Friendship request message
 */
void Profile::onRequestSent(const ToxPk &friendPk, const QString &message) {
  if (!isHistoryEnabled()) {
    return;
  }

  const QString pkStr = friendPk.toString();
  const QString inviteStr =
      Core::tr("/me offers friendship, \"%1\"").arg(message);
  const QString selfStr = core->getSelfPublicKey().toString();
  const QDateTime datetime = QDateTime::currentDateTime();
  const QString name = core->getUsername();
  history->addNewMessage(pkStr, inviteStr, selfStr, datetime, true, name);
}

/**
 * @brief Save an avatar to cache.
 * @param pic Picture to save.
 * @param owner PK of avatar owner.
 */
void Profile::saveAvatar(const ToxPk &owner, const QByteArray &pic) {
  QString path = avatarPath(owner);
  QDir(Settings::getInstance().getSettingsDirPath()).mkdir("avatars");
  if (pic.isEmpty()) {
    QFile::remove(path);
  } else {
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
      qWarning() << "Tox avatar " << path << " couldn't be saved";
      return;
    }
    file.write(pic);
    file.commit();
  }
}

/**
 * @brief Get the tox hash of a cached avatar.
 * @param owner Friend PK to get hash.
 * @return Avatar tox hash.
 */
QByteArray Profile::getAvatarHash(const ToxPk &owner) {
  //  QByteArray pic = loadAvatarData(owner);
  //  QByteArray avatarHash(TOX_HASH_LENGTH, 0);
  //  tox_hash((uint8_t *)avatarHash.data(), (uint8_t *)pic.data(), pic.size());
  //  return avatarHash;
  return QByteArray{};
}

/**
 * @brief Removes our own avatar.
 */
void Profile::removeSelfAvatar() {
  removeAvatar(core->getSelfId().getPublicKey());
}

/**
 * @brief Removes friend avatar.
 */
void Profile::removeFriendAvatar(const ToxPk &owner) { removeAvatar(owner); }

/**
 * @brief Checks that the history is enabled in the settings, and loaded
 * successfully for this profile.
 * @return True if enabled, false otherwise.
 */
bool Profile::isHistoryEnabled() {
  return Settings::getInstance().getEnableLogging() && history;
}

/**
 * @brief Get chat history.
 * @return May return a nullptr if the history failed to load.
 */
History *Profile::getHistory() { return history.get(); }

/**
 * @brief Removes a cached avatar.
 * @param owner Friend PK whose avater to delete.
 */
void Profile::removeAvatar(const ToxPk &owner) {
  QFile::remove(avatarPath(owner));
  if (owner == core->getSelfId().getPublicKey()) {
    setAvatar({});
  } else {
    setFriendAvatar(owner, {});
  }
}

bool Profile::exists(QString name) {
  QString path = Settings::getInstance().getSettingsDirPath() + name;
  return QFile::exists(path + ".tox");
}

/**
 * @brief Checks, if profile has a password.
 * @return True if we have a password set (doesn't check the actual file on
 * disk).
 */
bool Profile::isEncrypted() const { return encrypted; }

/**
 * @brief Checks if profile is encrypted.
 * @note Checks the actual file on disk.
 * @param name Profile name.
 * @return True if profile is encrypted, false otherwise.
 */
bool Profile::isEncrypted(QString name) {
  //  uint8_t data[TOX_PASS_ENCRYPTION_EXTRA_LENGTH] = {0};
  //  QString path = Settings::getInstance().getSettingsDirPath() + name +
  //  ".tox"; QFile saveFile(path); if (!saveFile.open(QIODevice::ReadOnly)) {
  //    qWarning() << "Couldn't open tox save " << path;
  //    return false;
  //  }
  //
  //  saveFile.read((char *)data, TOX_PASS_ENCRYPTION_EXTRA_LENGTH);
  //  saveFile.close();
  //
  //  return tox_is_data_encrypted(data);
  return false;
}

/**
 * @brief Removes the profile permanently.
 * Updates the profiles vector.
 * @return Vector of filenames that could not be removed.
 * @warning It is invalid to call loadToxSave or saveToxSave on a deleted
 * profile.
 */
QStringList Profile::remove() {
  if (isRemoved) {
    qWarning() << "Profile " << name << " is already removed!";
    return {};
  }
  isRemoved = true;

  qDebug() << "Removing profile" << name;
  for (int i = 0; i < profiles.size(); ++i) {
    if (profiles[i] == name) {
      profiles.removeAt(i);
      i--;
    }
  }
  QString path = Settings::getInstance().getSettingsDirPath() + name;
  ProfileLocker::unlock();

  QFile profileMain{path + ".tox"};
  QFile profileConfig{path + ".ini"};

  QStringList ret;

  if (!profileMain.remove() && profileMain.exists()) {
    ret.push_back(profileMain.fileName());
    qWarning() << "Could not remove file " << profileMain.fileName();
  }
  if (!profileConfig.remove() && profileConfig.exists()) {
    ret.push_back(profileConfig.fileName());
    qWarning() << "Could not remove file " << profileConfig.fileName();
  }

  QString dbPath = getDbPath(name);
  if (database && database->isOpen() && !database->remove() &&
      QFile::exists(dbPath)) {
    ret.push_back(dbPath);
    qWarning() << "Could not remove file " << dbPath;
  }

  history.reset();
  database.reset();

  return ret;
}

/**
 * @brief Tries to rename the profile.
 * @param newName New name for the profile.
 * @return False on error, true otherwise.
 */
bool Profile::rename(QString newName) {
  QString path = Settings::getInstance().getSettingsDirPath() + name,
          newPath = Settings::getInstance().getSettingsDirPath() + newName;

  if (!ProfileLocker::lock(newName)) {
    return false;
  }

  QFile::rename(path + ".tox", newPath + ".tox");
  QFile::rename(path + ".ini", newPath + ".ini");
  if (database) {
    database->rename(newName);
  }

  auto& qs = ok::base::OkSettings::getInstance();
  bool resetAutorun = qs.getAutorun();
  qs.setAutorun(false);
  qs.setCurrentProfile(newName);
  if (resetAutorun) {
    qs.setAutorun(true); // fixes -p flag in autostart command line
  }

  name = newName;
  return true;
}

const ToxEncrypt *Profile::getPasskey() const { return passkey.get(); }

/**
 * @brief Changes the encryption password and re-saves everything with it
 * @param newPassword Password for encryption, if empty profile will be
 * decrypted.
 * @param oldPassword Supply previous password if already encrypted or empty
 * QString if not yet encrypted.
 * @return Empty QString on success or error message on failure.
 */
QString Profile::setPassword(const QString &newPassword) {
  if (newPassword.isEmpty()) {
    return {};
  }
  //    std::unique_ptr<ToxEncrypt> newpasskey =
  //    ToxEncrypt::makeToxEncrypt(newPassword); if (!newpasskey) {
  //      qCritical() << "Failed to derive key from password, the profile won't
  //      "
  //                     "use the new password";
  //      return tr("Failed to derive key from password, the profile won't use
  //      the "
  //                "new password.");
  //    }
  //    // apply change
  //    passkey = std::move(newpasskey);
  //    encrypted = true;

  core->setPassword(newPassword);

  // apply new encryption
  onSaveToxSave();

  bool dbSuccess = false;

  // TODO: ensure the database and the tox save file use the same password
  if (database) {
    dbSuccess = database->setPassword(newPassword);
  }

  QString error{};
  if (!dbSuccess) {
    error = tr("Couldn't change password on the database, it might be "
               "corrupted or use the old "
               "password.");
  }

  QByteArray avatar = loadAvatarData(core->getSelfId().getPublicKey());
  saveAvatar(core->getSelfId().getPublicKey(), avatar);

  QVector<QString> friendList = core->getFriendList();
  QVectorIterator<QString> i(friendList);
  while (i.hasNext()) {
    const ToxPk friendPublicKey = core->getFriendPublicKey(i.next());
    saveAvatar(friendPublicKey, loadAvatarData(friendPublicKey));
  }
  return error;
}

/**
 * @brief Retrieves the path to the database file for a given profile.
 * @param profileName Profile name.
 * @return Path to database.
 */
QString Profile::getDbPath(const QString &profileName) {
  return Settings::getInstance().getSettingsDirPath() + profileName + ".db";
}
