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

#include "core.h"

#include <QCoreApplication>
#include <QString>
#include <QStringBuilder>
#include <QTimer>
#include <cassert>

#include <range/v3/all.hpp>
#include "base/compatiblerecursivemutex.h"
#include "lib/messenger/IMFriend.h"
#include "lib/messenger/Messenger.h"
#include "src/Bus.h"
#include "src/application.h"
#include "src/core/coreav.h"
#include "src/core/icoresettings.h"
#include "src/core/toxoptions.h"
#include "src/model/Friend.h"
#include "src/model/Group.h"
#include "src/model/groupinvite.h"
#include "src/model/Status.h"
#include "src/nexus.h"
#include "src/persistence/profile.h"

#include "base/jid.h"


namespace module::im {

VCard::Adr toVCardAdr(const lib::messenger::IMVCard::Adr& item) {
    return VCard::Adr{.street = item.street.c_str(),
                      .locality = item.locality.c_str(),
                      .region = item.region.c_str(),
                      .country = item.country.c_str()};
}

Friend* toFriend(const lib::messenger::IMFriend& frnd){
    return  new Friend(FriendId(frnd.id),
                      qstring(frnd.name),
                      qstring(frnd.alias),
                      frnd.isFriend(),
                      frnd.online,
                      ranges::views::all(frnd.groups) |
                              ranges::views::transform([](auto& e) { return qstring(e); }) |
                              ranges::to<QStringList>
                      );
}

VCard toVCard(const lib::messenger::IMVCard& imvCard) {
    return VCard{
                 .fullName = imvCard.fullName.c_str(),
                 .nickname = imvCard.nickname.c_str(),
                 .title = imvCard.title.c_str(),
                 .adrs = ranges::views::all(imvCard.adrs) | ranges::views::transform(toVCardAdr) |
                         ranges::to<QList>,
                 .emails = ranges::views::all(imvCard.emails) | ranges::views::transform([](auto& item) {
                               return VCard::Email{.type = item.type, .number = qstring(item.number)};
                           }) |
                           ranges::to<QList>,
                 .tels = ranges::views::all(imvCard.tels) | ranges::views::transform([](auto& item) {
                             return VCard::Tel{.type = item.type,
                                               .mobile = item.mobile,
                                               .number = qstring(item.number)};
                         }) |
                         ranges::to<QList>,
                 .photo = {.type = imvCard.photo.type.c_str(),
                           .bin = imvCard.photo.bin,
                           .url = imvCard.photo.url.c_str()}};
}

#define ASSERT_CORE_THREAD assert(QThread::currentThread() == coreThread.get())

Core::Core(QThread* coreThread)
        : messenger(nullptr), toxTimer(new QTimer(this)), coreThread(coreThread) {
    assert(coreThread);
    assert(toxTimer);

    qRegisterMetaType<PeerId>("PeerId");
    // qRegisterMetaType<Group*>("Group");
    qRegisterMetaType<GroupMessage>("GroupMessage");
    // qRegisterMetaType<Friend>("Friend");
    qRegisterMetaType<FriendId>("FriendId");
    qRegisterMetaType<FriendMessage>("FriendMessage");
    qRegisterMetaType<VCard>("VCard");

            // connect(this, &Core::avatarSet, this, [&](QByteArray buf){
            //     QPixmap pixmap;
            //     ok::base::Images::putToPixmap(buf, pixmap);
            //     ok::Application::Instance()->onAvatar(pixmap);
            // });

    toxTimer->setSingleShot(true);
    connect(toxTimer, &QTimer::timeout, this, &Core::process);
    connect(coreThread, &QThread::finished, toxTimer, &QTimer::stop);
    toxTimer->start();
}

Core::~Core() {
    // coreThread->exit(0);
    // coreThread->wait();
}

Status Core::fromToxStatus(const lib::messenger::IMStatus& status_) const {
    Status status;
    switch (status_) {
        case lib::messenger::IMStatus::Available:
        case lib::messenger::IMStatus::Chat:
            status = Status::Online;
            break;
        case lib::messenger::IMStatus::Away:
            status = Status::Away;
            break;
        case lib::messenger::IMStatus::DND:
            status = Status::Busy;
            break;
        default:
            status = Status::Offline;
            break;
    }
    return status;
}

/**
 * @brief Registers all toxcore callbacks
 * @param messenger Tox instance to register the callbacks on
 */
void Core::registerCallbacks(lib::messenger::Messenger* messenger) {
    messenger->addFriendHandler(this);
    messenger->addGroupHandler(this);
    messenger->addSelfHandler(this);
    messenger->addIMHandler(this);
}

/**
 * @brief Factory method for the Core object
 * @param savedata empty if new profile or saved data else
 * @param settings Settings specific to Core
 * @return nullptr or a Core object ready to start
 */
ToxCorePtr Core::makeToxCore(lib::messenger::Messenger* messenger,
                             const ICoreSettings* const settings,
                             ToxCoreErrors* err) {
    QThread* thread = new QThread();
    if (!thread) {
        qCritical() << "could not allocate Core thread";
        return {};
    }

    thread->setObjectName("Core");

    auto toxOptions = ToxOptions::makeToxOptions(settings);
    if (toxOptions == nullptr) {
        qCritical() << "could not allocate Tox Options data structure";
        if (err) {
            *err = ToxCoreErrors::ERROR_ALLOC;
        }
        return {};
    }

    ToxCorePtr core(new Core(thread));
    if (core == nullptr) {
        if (err) {
            *err = ToxCoreErrors::ERROR_ALLOC;
        }
        return {};
    }

    core->messenger = messenger;
    core->registerCallbacks(core->messenger);
    connect(thread, &QThread::started, core.get(), &Core::process);

    core->moveToThread(thread);
    return core;
}

void Core::onStarted() {
    qDebug() << __func__;
    //  emit avReady();
    // messenger->requestBookmarks();

    emit ok::Application::Instance() -> bus()->profileChanged(Nexus::getProfile());
    emit started();
}

void Core::onStopped() {
    qDebug() << __func__;
}

void Core::onError(const std::string& msgId, const std::string &msg)
{
    qDebug() << __func__ << msgId.c_str() << msg.c_str();
    emit errorOccurred(qstring(msgId), qstring(msg));
}

void Core::onDisconnected(int err) {
    qDebug() << __func__ <<" error:"<< err;
    mutex.unlock();
    emit disconnected(err);
}

void Core::onConnecting() {
    qDebug() << __func__;
    emit connecting();
}

void Core::onConnected() {
    qDebug() << __func__;
    mutex.unlock();
    emit connected();
    emit ok::Application::Instance() -> bus()->coreChanged(this);
}

/**
 * @brief Starts toxcore and it's event loop, can be called from any thread
 */
void Core::start() {
    qDebug() << __func__ << "...";
    QMutexLocker ml{&mutex};
    coreThread->start();
}

void Core::stop() {
    QMutexLocker ml{&mutex};
    qDebug() << __func__ << "...";
    coreThread->quit();
    messenger->stop();
}

/**
 * @brief Returns the global widget's Core instance
 */
Core* Core::getInstance() {
    return Nexus::getCore();
}

/**
 * @brief Processes toxcore events and ensure we stay connected, called by its
 * own timer
 */
void Core::process() {
    qDebug() << __func__;

    ASSERT_CORE_THREAD;
    mutex.lock();

    if (!messenger->isStarted()) {
        qDebug() << __func__ << "start...";
        messenger->start();
    } else {
        qDebug() << __func__ << "connect...";
        messenger->doConnect();
    }
    // sleep 5s to retry
    qDebug() << __func__ << "sleep 5s to retry";
    toxTimer->start(5000);
}

void Core::onFriend(const lib::messenger::IMFriend& frnd) {
    qDebug() << __func__ << frnd.id.getUsername().c_str();

            // 加入到朋友列表
    auto fi = new Friend(FriendId(frnd.id),
                         qstring(frnd.name),
                         qstring(frnd.alias),
                         frnd.isFriend(),
                         frnd.online,
                         ranges::views::all(frnd.groups) |
                                 ranges::views::transform([](auto& e) { return qstring(e); }) |
                                 ranges::to<QStringList>

                         );

    friendList.addFriend(fi);

    emit friendAdded(fi);
}

void Core::onFriendRequest(const std::string& friendId, const std::string& msg) {
    qDebug() << __func__ << friendId.c_str();
    emit friendRequestReceived(FriendId(qstring(friendId)), qstring(msg));
}

void Core::onFriendRemoved(const std::string& friendId) {
    qDebug() << __func__ << friendId.c_str() ;
    auto fid = FriendId(qstring(friendId));
    qDebug() << __func__ << fid;
    auto f = getFriend(fid);
    if(f.has_value()){
        //删除来源关系
        f.value()->removeRelation(Friend::RelationStatus::from);
    }
    emit friendRemoved(fid);
}

void Core::onFriendStatus(const std::string& friendId, lib::messenger::IMStatus status) {
    qDebug() << __func__ << friendId.c_str() << "status: " <<(int)status;
    Status status0 = fromToxStatus(status);
    auto fid = FriendId(qstring(friendId));
    auto f = getFriend(fid);
    if(f.has_value()){
        emit f.value()->statusChanged(status0);
    }
    emit friendStatusChanged(fid, status0);
}

void Core::onMessageSession(const std::string& contactId, const std::string& sid_) {
    auto cId = qstring(contactId);
    auto sid = qstring(sid_);
    qDebug() << __func__ << "contact:" << cId << "sid:" << sid;
    // emit messageSessionReceived(ContactId(cId), sid);

    auto fid = FriendId(qstring(contactId));
    auto f = getFriend(fid);

    if(f.has_value()){
        emit f.value()->messageSessionReceived(sid);
    }

    emit friendMessageSessionReceived(fid, sid);
}

void Core::onFriendMessage(const std::string& friendId_, const lib::messenger::IMMessage& message) {
    auto friendId = qstring(friendId_);
    qDebug() << __func__ << "friend:" << friendId;

    // 接收标志
    sendReceiptReceived(friendId, qstring(message.id));

    auto peerId = PeerId(qstring(message.from));

    FriendMessage msg;
    msg.isAction = false;
    msg.id = qstring(message.id);
    msg.from = peerId.getId();
    msg.from_resource = peerId.resource;
    msg.to = qstring(message.to);
    msg.content = qstring(message.body);
    msg.timestamp = QDateTime::fromSecsSinceEpoch(message.timestamp);
    msg.displayName = peerId.username;

    auto fid = FriendId(friendId);
    auto f = getFriend(fid);

    if(f.has_value()){
        emit f.value()->messageReceived(msg);
    }

    emit friendMessageReceived(fid, msg, false);
}

void Core::onFriendMessageReceipt(const std::string& friendId, const std::string& msgId) {
    auto fid = FriendId(qstring(friendId));
    auto f = getFriend(fid);

    if(f.has_value()){
        emit f.value()->messageReceipt(qstring(msgId));
    }
}

/**
 * 实现ChatState
 */
void Core::onFriendChatState(const std::string& friendId, int state) {
    qDebug() << "onFriendChatState:" << friendId.c_str() << "state:" << state;

    auto fid = FriendId(qstring(friendId));
    auto f = getFriend(fid);
    auto is = state == 2;

    if(f.has_value()){
        emit f.value()->typingChanged(is);
    }

    emit friendTypingChanged(getFriendPublicKey(friendId.c_str()), is);
}

void Core::onFriendNickChanged(const std::string& friendId, const std::string& nick) {
    qDebug() << __func__ << friendId.c_str() << "nick:" << nick.c_str();

    auto fid = FriendId(qstring(friendId));
    auto f = getFriend(fid);
    if(f.has_value()){
        f.value()->setName(qstring(nick));
    }
    emit friendNicknameChanged(getFriendPublicKey(friendId.c_str()), nick.c_str());
}

void Core::onFriendAvatarChanged(const std::string& friendId, const std::string& avatar) {
    qDebug() << __func__ << friendId.c_str() << "avatar size" << avatar.size();

    auto fid = FriendId(qstring(friendId));
    auto avt = avatar.empty() ? QByteArray() :(QByteArray::fromStdString(avatar));

    auto f = getFriend(fid);
    if(f.has_value()){
        f.value()->setAvatar(avt);
    }
    emit friendAvatarChanged(fid, avt);
}

void Core::onFriendAliasChanged(const lib::messenger::IMContactId& cId, const std::string& alias) {
    qDebug() << __func__ << alias.c_str();

    auto alias_ = qstring(alias);
    auto fid = FriendId{cId};
    auto f = getFriend(fid);
    if(f.has_value()){
        f.value()->setAlias(alias_);
    }
    // emit friendAliasChanged(fid, alias_);
}

void Core::onGroup(const std::string& groupId_, const std::string& name) {
    auto groupId = qstring(groupId_);
    qDebug() << __func__ << "groupId:" << groupId << name.c_str();
    auto* g = new Group(Nexus::getProfile(), GroupId(groupId), qstring(name));
    groupList.addGroup(g);
    emit groupAdded(g);
}

//
// void Core::onGroupInvite(Tox *tox, QString receiver, Tox_Conference_Type
// type,
//                         const uint8_t *cookie, size_t length, void *vCore) {
//  Core *core = static_cast<Core *>(vCore);
//  const QByteArray data(reinterpret_cast<const char *>(cookie), length);
//  const GroupInvite inviteInfo("", receiver, type, data);
//  switch (type) {
//  case TOX_CONFERENCE_TYPE_TEXT:
//    qDebug() << QString("Text group invite by %1").arg(receiver);
//    emit core->groupInviteReceived(inviteInfo);
//    break;
//
//  case TOX_CONFERENCE_TYPE_AV:
//    qDebug() << QString("AV group invite by %1").arg(receiver);
//    emit core->groupInviteReceived(inviteInfo);
//    break;
//
//  default:
//    qWarning() << "Group invite with unknown type " << type;
//  }
//}
//
// void Core::onGroupPeerListChange(Tox *, QString groupId, void *vCore) {
//  const auto core = static_cast<Core *>(vCore);
//  qDebug() << QString("Group %1 peerlist changed").arg(groupId);
//  // no saveRequest, this callback is called on every connection to group
//  peer,
//  // not just on brand new peers
//  emit core->groupPeerlistChanged(groupId);
//}
//
// void Core::onGroupPeerNameChange(Tox *, QString groupId, QString peerId,
//                                 const uint8_t *name, size_t length,
//                                 void *vCore) {
//  const auto newName = ToxString(name, length).getQString();
//  qDebug() << QString("Group %1, Peer %2, name changed to %3")
//                  .arg(groupId)
//                  .arg(peerId)
//                  .arg(newName);
//  auto *core = static_cast<Core *>(vCore);
//  auto peerPk = core->getGroupPeerPk(groupId, peerId);
//  emit core->groupPeerNameChanged(groupId, peerPk, newName);
//}
//
// void Core::onGroupTitleChange(Tox *, QString groupId, QString peerId,
//                              const uint8_t *cTitle, size_t length,
//                              void *vCore) {
//  Core *core = static_cast<Core *>(vCore);
//  QString author = core->getGroupPeerName(groupId, peerId);
//  emit core->saveRequest();
//  emit core->groupTitleChanged(groupId, author,
//                               ToxString(cTitle, length).getQString());
//}

void Core::onGroupInvite(const std::string& groupId_, const std::string& peerId_,
                         const std::string& message) {
    auto groupId = qstring(groupId_);
    auto peerId = qstring(peerId_);
    qDebug() << __func__ << "groupId:" << groupId << " receiver:" << peerId
             << " msg:" << message.c_str();

    GroupInvite invite(groupId, peerId, ConferenceType::TEXT, QByteArray::fromStdString(message));
    emit groupInviteReceived(invite);
}

void Core::onGroupSubjectChanged(const std::string& groupId_, const std::string& subject) {
    auto groupId = qstring(groupId_);
    qDebug() << __func__ << "groupId:" << groupId << " subject:" << subject.c_str();
    emit groupSubjectChanged(GroupId(groupId), subject.c_str());
}

void Core::onGroupMessage(const std::string& groupId_,
                          const lib::messenger::IMPeerId& peerId,
                          const lib::messenger::IMMessage& message) {
    auto groupId = qstring(groupId_);

    qDebug() << __func__ << "groupId:" << groupId << "peer:" << peerId.toString().c_str();
    qDebug() << "body:" << message.body.c_str();

    bool isAction = false;
    GroupMessage msg;
    msg.isAction = isAction;
    msg.id = message.id.c_str();
    msg.from = peerId.toFriendId().c_str();
    msg.from_resource = peerId.resource.c_str();
    msg.to = message.to.c_str();
    msg.content = message.body.c_str();
    msg.timestamp = QDateTime::fromSecsSinceEpoch(message.timestamp);
    msg.resource = peerId.resource.c_str();
    msg.nick = peerId.resource.c_str();

    emit groupMessageReceived(GroupId(groupId), msg);
}

void Core::onGroupOccupants(const std::string& groupId_, const uint size) {
    auto groupId = qstring(groupId_);
    qDebug() << __func__ << "groupId" << groupId << "size" << size;
    emit groupPeerSizeChanged(groupId, size);
}

void Core::onGroupOccupantStatus(const std::string& groupId_,  //
                                 const lib::messenger::IMGroupOccupant& occ) {
    auto groupId = qstring(groupId_);

    GroupOccupant go = {.jid = occ.jid.c_str(),
                        .nick = occ.nick.c_str(),
                        .affiliation = occ.affiliation.c_str(),
                        .role = occ.role.c_str(),
                        .status = occ.status,
                        .codes = ranges::views::all(occ.codes) | ranges::to<QList>};
    emit groupPeerStatusChanged(groupId, go);
}

void Core::onGroupInfo(const std::string& groupId_, const lib::messenger::IMGroup& groupInfo) {
    auto groupId = qstring(groupId_);
    GroupInfo info = {
            .name = groupInfo.name.c_str(),
            .description = groupInfo.description.c_str(),
            .subject = groupInfo.subject.c_str(),
            .creationdate = groupInfo.creationdate.c_str(),
            .occupants = groupInfo.occupants,
    };
    emit groupInfoReceipt(GroupId(groupId), info);
}

void Core::onMessageReceipt(const std::string& friendId, const std::string& receipt) {
    emit receiptRecieved(getFriendPublicKey(friendId.c_str()), receipt.c_str());
}

void Core::acceptFriendRequest(const FriendId& friendPk) {
    qDebug() << __func__ << friendPk.toString();

    QMutexLocker ml{&mutex};
    messenger->acceptFriendRequest(friendPk.toString().toStdString());
    emit saveRequest();
}

void Core::rejectFriendRequest(const FriendId& friendPk) {
    messenger->rejectFriendRequest(friendPk.toString().toStdString());
}


void Core::requestFriendship(const FriendId& friendId, const QString& nick,
                             const QString& message) {
    qDebug() << __func__ << friendId.toString() << nick << message;
    QMutexLocker ml{&mutex};
    messenger->sendFriendRequest(friendId.toString().toStdString(), nick.toStdString(),
                                 message.toStdString());
    emit saveRequest();
}

bool Core::sendMessageWithType(QString friendId, const QString& message, const MsgId& id,
                               bool encrypt) {
    qDebug() << __func__ << "receiver" << friendId << "message:" << message;
    if (friendId.isEmpty()) {
        qWarning() << "receiver is empty.";
        return false;
    }

    bool yes = messenger->sendToFriend(friendId.toStdString(), message.toStdString(),
                                       id.toStdString(), encrypt);
    return yes;
}

bool Core::sendMessage(QString friendId, const QString& message, const MsgId& msgId, bool encrypt) {
    QMutexLocker ml(&mutex);
    return sendMessageWithType(friendId, message, msgId, encrypt);
}

bool Core::sendAction(QString friendId, const QString& action, const MsgId& msgId, bool encrypt) {
    QMutexLocker ml(&mutex);
    return sendMessageWithType(friendId, action, msgId, encrypt);
}

void Core::sendTyping(QString friendId, bool typing) {
    QMutexLocker ml{&mutex};
    messenger->sendChatState(friendId.toStdString(), typing ? 2 : 4);
    emit failedToSetTyping(typing);
}

bool Core::sendGroupMessageWithType(QString groupId, const QString& message, const MsgId& id) {
    QMutexLocker ml{&mutex};
    return messenger->sendToGroup(groupId.toStdString(), message.toStdString(), id.toStdString());
}

bool Core::sendGroupMessage(QString groupId, const QString& message, const MsgId& id) {
    QMutexLocker ml{&mutex};
    return sendGroupMessageWithType(groupId, message, id);
}

bool Core::sendGroupAction(QString groupId, const QString& message, const MsgId& id) {
    QMutexLocker ml{&mutex};
    return sendGroupMessageWithType(groupId, message, id);
}

void Core::setGroupName(const QString& groupId, const QString& name) {
    qDebug() << __func__ << groupId << name;
    QMutexLocker ml{&mutex};
    messenger->setRoomName(groupId.toStdString(), name.toStdString());
}

void Core::setGroupSubject(const QString& groupId, const QString& subject) {
    qDebug() << __func__ << groupId << subject;
    QMutexLocker ml{&mutex};
    messenger->setRoomSubject(groupId.toStdString(), subject.toStdString());
}

void Core::setGroupDesc(const QString& groupId, const QString& desc) {
    qDebug() << __func__ << groupId << desc;
    QMutexLocker ml{&mutex};
    messenger->setRoomDesc(groupId.toStdString(), desc.toStdString());
}

void Core::setGroupAlias(const QString& groupId, const QString& alias) {
    qDebug() << __func__ << groupId << alias;
    QMutexLocker ml{&mutex};
    messenger->setRoomAlias(groupId.toStdString(), alias.toStdString());
}

std::optional<Contact *> Core::getContact(const ContactId &cid) const
{
    auto f = getFriend(cid);
    if(f.has_value()){
        return f;
    }

    return getGroup(cid);

}

bool Core::removeFriend(QString friendId) {
    qDebug() << __func__ << friendId;
    QMutexLocker ml{&mutex};
    return messenger->removeFriend(friendId.toStdString());
}

bool Core::leaveGroup(QString groupId) {
    qDebug() << __func__ << groupId;
    return messenger->leaveGroup(groupId.toStdString());
}

void Core::destroyGroup(QString groupId) {
    bool success = messenger->destroyGroup(groupId.toStdString());
    if (success) {
        emit saveRequest();
        //    av->leaveGroupCall(groupId);
    }
}

const GroupList &Core::loadGroupList()
{
    messenger->loadGroupList();
    return groupList;
}

void Core::addGroup(Group *g)
{
    groupList.addGroup(g);
}

std::optional<Group *> Core::getGroup(const ContactId &cid) const
{
    for(auto& f: groupList.getAllGroups()){
        if(f->getId().toString() == cid.toString()){
            return std::make_optional(f);
        }
    }
    return std::nullopt;
}

/**
 * @brief Returns our username, or an empty string on failure
 */
QString Core::getUsername() const {
    QMutexLocker ml{&mutex};
    return messenger->getSelfUsername().c_str();
}

void Core::setNick(const QString& nick) {
    QMutexLocker ml{&mutex};

    if (nick == getNick()) {
        return;
    }
    messenger->setSelfNickname(nick.toStdString());
    emit saveRequest();
}

QString Core::getNick() const {
    QMutexLocker ml{&mutex};
    return messenger->getSelfNick().c_str();
}

void Core::setPassword(const QString& password) {
    QMutexLocker ml{&mutex};

    if (password.isEmpty()) {
        return;
    }

    messenger->changePassword(password.toStdString());
}

/**
 * @brief Returns our Ok ID
 */
ok::base::Jid Core::getSelfPeerId() const {
    return ok::base::Jid(qstring(messenger->getSelfId().toString()));
}

/**
 * @brief Gets self public key
 * @return Self PK
 */
FriendId Core::getSelfId() const {
    return FriendId(qstring(messenger->getSelfId().toString()));
}

/**
 * @brief Returns our public and private keys
 */
QPair<QByteArray, QByteArray> Core::getKeypair() const {
    QMutexLocker ml{&mutex};

    QPair<QByteArray, QByteArray> keypair;
    //  assert(tox != nullptr);
    //
    //  QByteArray pk(TOX_PUBLIC_KEY_SIZE, 0x00);
    //  QByteArray sk(TOX_SECRET_KEY_SIZE, 0x00);
    //  tox_self_get_public_key(tox.get(), reinterpret_cast<uint8_t
    //  *>(pk.data())); tox_self_get_secret_key(tox.get(),
    //  reinterpret_cast<uint8_t *>(sk.data())); keypair.first = pk;
    //  keypair.second = sk;
    return keypair;
}

/**
 * @brief Returns our status message, or an empty string on failure
 */
QString Core::getStatusMessage() const {
    QMutexLocker ml{&mutex};
    auto status = getStatus();
    return getAssetSuffix(status);
}

/**
 * @brief Returns our user status
 */
Status Core::getStatus() const {
    assert(messenger != nullptr);
    QMutexLocker ml{&mutex};
    switch (messenger->getSelfStatus()) {
        case lib::messenger::IMStatus::Available:
        case lib::messenger::IMStatus::Chat:
            return Status::Online;
        case lib::messenger::IMStatus::Away:
        case lib::messenger::IMStatus::XA:
            return Status::Away;
        case lib::messenger::IMStatus::DND:
            return Status::Busy;
        default:
            return Status::Offline;
    }
}

void Core::setStatusMessage(const QString& message) {
    QMutexLocker ml{&mutex};

    if (message == getStatusMessage()) {
        return;
    }

            //  ToxString cMessage(message);
            //  if (!tox_self_set_status_message(tox.get(), cMessage.data(),
            //  cMessage.size(), nullptr)) {
            //    emit failedToSetStatusMessage(message);
            //    return;
            //  }

    emit saveRequest();
    emit statusMessageSet(message);
}

void Core::setStatus(Status status_) {
    QMutexLocker ml{&mutex};

    lib::messenger::IMStatus userstatus;
    switch (status_) {
        case Status::Online:
            userstatus = lib::messenger::IMStatus::Available;
            break;

        case Status::Away:
            userstatus = lib::messenger::IMStatus::Away;
            break;

        case Status::Busy:
            userstatus = lib::messenger::IMStatus::DND;
            break;
        default:
            break;
    }

    emit saveRequest();
    emit statusSet(status_);
}

void Core::setAvatar(const QByteArray& avatar) {
    // 从朋友列表寻找到自己
    auto self = getFriend(getSelfId());
    if (self.has_value()) {
        self.value()->setAvatar(avatar);
        messenger->setSelfAvatar(avatar.toStdString());
    }
}

// Declared to avoid code duplication
#define GET_FRIEND_PROPERTY(property, function, checkSize)                     \
const size_t property##Size = function##_size(tox.get(), ids[i], nullptr); \
        if ((!checkSize || property##Size) && property##Size != SIZE_MAX) {        \
            uint8_t* prop = new uint8_t[property##Size];                           \
            if (function(tox.get(), ids[i], prop, nullptr)) {                      \
                QString propStr = ToxString(prop, property##Size).getQString();    \
                emit friend##property##Changed(ids[i], propStr);                   \
    }                                                                      \
                                                                               \
            delete[] prop;                                                         \
}

void Core::loadFriends() {
    QMutexLocker ml{&mutex};
    const size_t friendCount = messenger->getFriendCount();
    qDebug() << "friendCount" << friendCount;
    if (friendCount == 0) {
        return;
    }

            //  std::list<lib::IM::IMContactId> peers = messenger->loadFriendList();
            //  for (auto itr : peers) {
            //    qDebug() << "id=" << qstring(itr.getJid())
            //             << " name=" << qstring(itr.getUsername());
            //  }

            // uint32_t *ids = new uint32_t[friendCount];
            // tox_self_get_friend_list(tox.get(), ids);
            // uint8_t friendPk[TOX_PUBLIC_KEY_SIZE] = {0x00};
            // for (size_t i = 0; i < friendCount; ++i) {
            //   if (!tox_friend_get_public_key(tox.get(), ids[i], friendPk, nullptr)) {
            //     continue;
            //   }

            //   emit friendAdded(ids[i], ToxPk(friendPk));
            //   GET_FRIEND_PROPERTY(Username, tox_friend_get_name, true);
            //   GET_FRIEND_PROPERTY(StatusMessage, tox_friend_get_status_message,
            //   false); checkLastOnline(ids[i]);
            // }
            // delete[] ids;
}

void Core::loadGroups() {
    QMutexLocker ml{&mutex};

            //  const size_t groupCount = tox_conference_get_chatlist_size(tox.get());
            //  if (groupCount == 0) {
            //    return;
            //  }
            //
            //  auto groupNumbers = new uint32_t[groupCount];
            //  tox_conference_get_chatlist(tox.get(), groupNumbers);
            //
            //  for (size_t i = 0; i < groupCount; ++i) {
            //    TOX_ERR_CONFERENCE_TITLE error;
            //    QString name;
            //    const auto groupNumber = groupNumbers[i];
            //    size_t titleSize =
            //        tox_conference_get_title_size(tox.get(), groupNumber, &error);
            //    const GroupId persistentId = getGroupPersistentId(groupNumber);
            //    const QString defaultName =
            //        tr("Groupchat %1").arg(persistentId.toString().left(8));
            //    if (LogConferenceTitleError(error)) {
            //      name = defaultName;
            //    } else {
            //      QByteArray nameByteArray =
            //          QByteArray(static_cast<int>(titleSize), Qt::Uninitialized);
            //      tox_conference_get_title(
            //          tox.get(), groupNumber,
            //          reinterpret_cast<uint8_t *>(nameByteArray.data()), &error);
            //      if (LogConferenceTitleError(error)) {
            //        name = defaultName;
            //      } else {
            //        name = ToxString(nameByteArray).getQString();
            //      }
            //    }
            //    if (getGroupAvEnabled(groupNumber)) {
            //      if (toxav_groupchat_enable_av(tox.get(), groupNumber,
            //                                    CoreAV::groupCallCallback, this)) {
            //        qCritical() << "Failed to enable audio on loaded group" <<
            //        groupNumber;
            //      }
            //    }
            //    emit emptyGroupCreated(groupNumber, persistentId, name);
            //  }

            //  delete[] groupNumbers;
}

void Core::checkLastOnline(QString friendId) {
    QMutexLocker ml{&mutex};

            //  const uint64_t lastOnline =
            //      tox_friend_get_last_online(tox.get(), receiver, nullptr);
            //  if (lastOnline != std::numeric_limits<uint64_t>::max()) {
            //    emit friendLastSeenChanged(receiver,
            //    QDateTime::fromTime_t(lastOnline));
            //  }
}

/**
 * @brief Returns the list of friendIds in our friendlist, an empty list on
 * error
 */
const FriendList& Core::loadFriendList() {
    QMutexLocker ml{&mutex};

    std::list<lib::messenger::IMFriend> fs;
    messenger->getFriendList(fs);

    auto ls = ranges::views::all(fs) |
            ranges::views::transform([](auto& e) { return toFriend(e); }) |
              ranges::to_vector;

    for (auto& item : ls) {
        friendList.addFriend(item);
    }

    return friendList;
}

std::optional<Friend*> Core::getFriend(const ContactId &cid) const
{
    for(auto& f: friendList.getAllFriends()){
        if(f->getId().toString() == cid.toString()){
            return std::make_optional(f);
        }
    }
    return std::nullopt;
}

// GroupId Core::getGroupPersistentId(QString groupId) const {
//     QMutexLocker ml{&mutex};
//     return GroupId{groupId.toUtf8()};
// }

/**
 * @brief Get number of peers in the conference.
 * @return The number of peers in the conference. UINT32_MAX on failure.
 */
uint32_t Core::getGroupNumberPeers(QString groupId) const {
    QMutexLocker ml{&mutex};
    qDebug() << "getGroupNumberPeers:" << groupId;

            //  Tox_Err_Conference_Peer_Query error;
            //  uint32_t count = tox_conference_peer_count(tox.get(), groupId, &error);
            //  if (!parsePeerQueryError(error)) {
            //    return std::numeric_limits<uint32_t>::max();
            //  }

            //  return count;
    return 0;
}

/**
 * @brief Get the name of a peer of a group
 */
QString Core::getGroupPeerName(QString groupId, QString peerId) const {
    QMutexLocker ml{&mutex};

            // from tox.h: "If peer_number == UINT32_MAX, then author is unknown (e.g.
            // initial joining the conference)."
    if (peerId == std::numeric_limits<uint32_t>::max()) {
        return {};
    }

            //  Tox_Err_Conference_Peer_Query error;
            //  size_t length =
            //      tox_conference_peer_get_name_size(tox.get(), groupId, peerId,
            //      &error);
            //  if (!parsePeerQueryError(error)) {
            //    return QString{};
            //  }
            //
            //  QByteArray name(length, Qt::Uninitialized);
            //  uint8_t *namePtr = reinterpret_cast<uint8_t *>(name.data());
            //  bool success =
            //      tox_conference_peer_get_name(tox.get(), groupId, peerId, namePtr,
            //      &error);
            //  if (!parsePeerQueryError(error)) {
            //    return QString{};
            //  }
            //  assert(success);

            //  return ToxString(name).getQString();
    return QString{};
}

/**
 * @brief Get the public key of a peer of a group
 */
PeerId Core::getGroupPeerPk(QString groupId, QString peerId) const {
    QMutexLocker ml{&mutex};

            //  uint8_t friendPk[TOX_PUBLIC_KEY_SIZE] = {0x00};
            //  Tox_Err_Conference_Peer_Query error;
            //  bool success = tox_conference_peer_get_public_key(tox.get(), groupId,
            //  peerId,
            //                                                    friendPk, &error);
            //  if (!parsePeerQueryError(error)) {
            //    return ToxPk{};
            //  }
            //  assert(success);

    auto toxPk = PeerId{peerId};
    return toxPk;
}

/**
 * @brief Get the names of the peers of a group
 */
QStringList Core::getGroupPeerNames(QString groupId) const {
    QMutexLocker ml{&mutex};

    assert(messenger != nullptr);

    uint32_t nPeers = getGroupNumberPeers(groupId);
    if (nPeers == std::numeric_limits<uint32_t>::max()) {
        qWarning() << "getGroupPeerNames: Unable to get number of peers";
        return {};
    }

    QStringList names;
    //  for (uint32_t i = 0; i < nPeers; ++i) {
    //    TOX_ERR_CONFERENCE_PEER_QUERY error;
    //    size_t length =
    //        tox_conference_peer_get_name_size(tox.get(), groupId, i, &error);
    //    if (!parsePeerQueryError(error)) {
    //      names.append(QString());
    //      continue;
    //    }
    //
    //    QByteArray name(length, Qt::Uninitialized);
    //    uint8_t *namePtr = reinterpret_cast<uint8_t *>(name.data());
    //    bool ok =
    //        tox_conference_peer_get_name(tox.get(), groupId, i, namePtr,
    //        &error);
    //    if (ok && parsePeerQueryError(error)) {
    //      names.append(ToxString(name).getQString());
    //    } else {
    //      names.append(QString());
    //    }
    //  }

            //  names.append("user1");
            //  names.append("user2");
            //  assert(names.size() == nPeers);
    return names;
}

/**
 * @brief Check, that group has audio or video stream
 * @param groupId Id of group to check
 * @return True for AV groups, false for text-only groups
 */
bool Core::getGroupAvEnabled(QString groupId) const {
    QMutexLocker ml{&mutex};
    //  TOX_ERR_CONFERENCE_GET_TYPE error;
    //  TOX_CONFERENCE_TYPE type =
    //      tox_conference_get_type(tox.get(), groupId, &error);
    //  switch (error) {
    //  case TOX_ERR_CONFERENCE_GET_TYPE_OK:
    //    break;
    //  case TOX_ERR_CONFERENCE_GET_TYPE_CONFERENCE_NOT_FOUND:
    //    qWarning() << "Conference not found";
    //    break;
    //  default:
    //    qWarning() << "Unknown error code:" << QString::number(error);
    //    break;
    //  }

            //  return type == TOX_CONFERENCE_TYPE_AV;
    return true;
}

/**
 * @brief Accept a groupchat invite.
 * @param inviteInfo Object which contains info about group invitation
 *
 * @return Conference number on success, UINT32_MAX on failure.
 */
QString Core::joinGroupchat(const GroupInvite& inviteInfo) {
    QMutexLocker ml{&mutex};
    messenger->joinGroup(inviteInfo.getGroupId().toStdString());

            //  const QString receiver = inviteInfo.getFriendId();
            //  const uint8_t confType = inviteInfo.getType();
            //  const QByteArray invite = inviteInfo.getInvite();
            //  const uint8_t *const cookie = reinterpret_cast<const uint8_t
            //  *>(invite.data()); const size_t cookieLength = invite.length(); QString
            //  groupNum{std::numeric_limits<uint32_t>::max()};

            //  switch (confType) {
            //  case TOX_CONFERENCE_TYPE_TEXT: {
            //    qDebug() << QString(
            //                    "Trying to join text groupchat invite sent by friend
            //                    %1") .arg(receiver);
            //    Tox_Err_Conference_Join error;
            //    groupNum = tox_conference_join(tox.get(), receiver, cookie,
            //    cookieLength, &error); if (!parseConferenceJoinError(error)) {
            //      groupNum = std::numeric_limits<uint32_t>::max();
            //    }
            //    break;
            //  }
            //  case TOX_CONFERENCE_TYPE_AV: {
            //    qDebug() << QString("Trying to join AV groupchat invite sent by friend
            //    %1")
            //                    .arg(receiver);
            //    groupNum = toxav_join_av_groupchat(tox.get(), receiver, cookie,
            //                                       cookieLength,
            //                                       CoreAV::groupCallCallback,
            //                                       const_cast<Core *>(this));
            //    break;
            //  }
            //  default:
            //    qWarning() << "joinGroupchat: Unknown groupchat type " << confType;
            //  }
            //  if (groupNum != std::numeric_limits<uint32_t>::max()) {
            //    emit saveRequest();
            //    emit groupJoined(groupNum, getGroupPersistentId(groupNum));
            //  }
            //  return groupNum;
    return {};
}

void Core::joinRoom(const QString& groupId) {
    messenger->joinGroup(groupId.toStdString());
}

void Core::requestRoomInfo(const QString &groupId)
{
    messenger->requestRoomInfo(groupId.toStdString());
}

void Core::inviteToGroup(const ContactId& friendId, const GroupId& groupId) {
    QMutexLocker ml{&mutex};
    messenger->inviteGroup(lib::messenger::IMContactId{groupId.toString().toStdString()},
                           lib::messenger::IMContactId{friendId.toString().toStdString()});
}

GroupId Core::createGroup(const QString& name) {
    qDebug() << __func__ << name;
    QMutexLocker ml{&mutex};
    auto id = QUuid::createUuid().toString(QUuid::StringFormat::WithoutBraces);
    const std::string& group =
            messenger->createGroup(id.split("-").at(0).toStdString(), name.toStdString());
    return GroupId(qstring(group));
}

/**
 * @brief Checks if a friend is online. Unknown friends are considered offline.
 */
bool Core::isFriendOnline(QString friendId) const {
    QMutexLocker ml{&mutex};
    //
    //  Tox_Connection connection =
    //      tox_friend_get_connection_status(tox.get(), receiver, nullptr);
    //  return connection != TOX_CONNECTION_NONE;
    return false;
}

/**
 * @brief Checks if we have a friend by public key
 */
bool Core::hasFriendWithPublicKey(const FriendId& publicKey) const {
    //  QMutexLocker ml{&coreLoopLock};
    //
    //  if (publicKey.isEmpty()) {
    //    return false;
    //  }
    //
    //  QString receiver = tox_friend_by_public_key(tox.get(),
    //  publicKey.getData(), nullptr); return receiver !=
    //  std::numeric_limits<uint32_t>::max();
    return false;
}

/**
 * @brief Get the public key part of the ToxID only
 */
inline FriendId Core::getFriendPublicKey(QString friendNumber) const {
    // qDebug() << "getFriendPublicKey" << friendNumber;
    //   QMutexLocker ml{&coreLoopLock};
    //   uint8_t rawid[TOX_PUBLIC_KEY_SIZE];
    //   if (!tox_friend_get_public_key(tox.get(), friendNumber, rawid,
    //   nullptr)) {
    //     qWarning() << "getFriendPublicKey: Getting public key failed";
    //     return ToxPk();
    //   }
    return FriendId(friendNumber.toUtf8());
}

QString Core::getFriendUsername(QString friendnumber) const {
    qWarning() << "Not implicement";
    return {};
}

void Core::setFriendAlias(const QString& friendId, const QString& alias) {
    messenger->setFriendAlias(friendId.toStdString(), alias.toStdString());
}

void Core::getFriendInfo(const QString& friendnumber) const {
    messenger->getFriendVCard(friendnumber.toStdString());
}

Status Core::getFriendStatus(const QString& friendNumber) const {
    auto status = messenger->getFriendStatus(friendNumber.toStdString());
    return status == lib::messenger::IMStatus::Available ? Status::Online : Status::Offline;
}

QStringList Core::splitMessage(const QString& message) {
    QStringList splittedMsgs;
    QByteArray ba_message{message.toUtf8()};

    /*
     * TODO: Remove this hack; the reported max message length we receive from
     * c-toxcore as of 08-02-2019 is inaccurate, causing us to generate too
     * large messages when splitting them up.
     *
     * The inconsistency lies in c-toxcore group.c:2480 using
     * MAX_GROUP_MESSAGE_DATA_LEN to verify message size is within limit, but
     * tox_max_message_length giving a different size limit to us.
     *
     * (uint32_t tox_max_message_length(void); declared in tox.h, unable to see
     * explicit definition)
     */
    const auto maxLen = 1024;

    while (ba_message.size() > maxLen) {
        int splitPos = ba_message.lastIndexOf('\n', maxLen - 1);

        if (splitPos <= 0) {
            splitPos = ba_message.lastIndexOf(' ', maxLen - 1);
        }

        if (splitPos <= 0) {
            constexpr uint8_t firstOfMultiByteMask = 0xC0;
            constexpr uint8_t multiByteMask = 0x80;
            splitPos = maxLen;
            // don't split a utf8 character
            if ((ba_message[splitPos] & multiByteMask) == multiByteMask) {
                while ((ba_message[splitPos] & firstOfMultiByteMask) != firstOfMultiByteMask) {
                    --splitPos;
                }
            }
            --splitPos;
        }
        splittedMsgs.append(QString{ba_message.left(splitPos + 1)});
        ba_message = ba_message.mid(splitPos + 1);
    }

    splittedMsgs.append(QString{ba_message});
    return splittedMsgs;
}

QString Core::getPeerName(const FriendId& id) const {
    QMutexLocker ml{&mutex};

    QString name;
    //  QString receiver = tox_friend_by_public_key(tox.get(), id.getData(),
    //  nullptr); if (receiver == std::numeric_limits<uint32_t>::max()) {
    //    qWarning() << "getPeerName: No such peer";
    //    return name;
    //  }
    //
    //  const size_t nameSize =
    //      tox_friend_get_name_size(tox.get(), receiver, nullptr);
    //  if (nameSize == SIZE_MAX) {
    //    return name;
    //  }
    //
    //  uint8_t *cname =
    //      new uint8_t[nameSize < tox_max_name_length() ? tox_max_name_length()
    //                                                   : nameSize];
    //  if (!tox_friend_get_name(tox.get(), receiver, cname, nullptr)) {
    //    qWarning() << "getPeerName: Can't get name of friend " +
    //                      QString().setNum(receiver);
    //    delete[] cname;
    //    return name;
    //  }
    //
    //  name = ToxString(cname, nameSize).getQString();
    //  delete[] cname;
    return name;
}

void Core::logout() {
    messenger->stop();
}

bool Core::isLinked(const QString &cid, lib::messenger::ChatType ct)
{
    return messenger->isLinked(cid.toStdString(), ct);
}

void Core::onSelfNameChanged(const std::string& name) {
    QMutexLocker ml{&mutex};
    emit usernameSet(name.c_str());
}

void Core::onSelfAvatarChanged(const std::string& avatar) {
    qDebug() << __func__;
    QMutexLocker ml{&mutex};
    auto a = QByteArray::fromStdString(avatar);
    emit avatarSet(a);
}

void Core::onSelfStatusChanged(lib::messenger::IMStatus userStatus, const std::string& msg) {
    QMutexLocker ml{&mutex};
    auto st = fromToxStatus(userStatus);
    emit statusSet(st);
    auto t = getTitle(st);
    if (!msg.empty()) {
        t.append(": ").append(msg.data());
    }
    emit statusMessageSet(t);
}

void Core::onSelfVCardChanged(lib::messenger::IMVCard& imvCard) {
    qDebug() << __func__ << "nick:" << imvCard.nickname.c_str();

    VCard vCard = toVCard(imvCard);

    if (!vCard.photo.bin.empty()) {
        if (vCard.photo.url.isEmpty()) {
            emit avatarSet(QByteArray::fromStdString(vCard.photo.bin));
        }
    }

    emit usernameSet(vCard.nickname);
    emit vCardSet(vCard);
}

void Core::onSelfIdChanged(const std::string& id) {
    emit idSet(ok::base::Jid(qstring(id)));
}

void Core::sendReceiptReceived(const QString& friendId, const QString& receipt) {
    qDebug() << __func__ << friendId << "receipt" << receipt;
    messenger->receiptReceived(friendId.toStdString(), receipt.toStdString());
}

void Core::requestBookmarks() {
    messenger->requestBookmarks();
}

void Core::loadGroupList() const {
    messenger->loadGroupList();
}

void Core::onFriendVCard(const lib::messenger::IMContactId& fId,
                         const lib::messenger::IMVCard& imvCard) {
    qDebug() << __func__ << fId.toString().c_str() << "nick:" << imvCard.nickname.c_str();
    VCard vCard = toVCard(imvCard);
    emit friendVCardSet(FriendId(fId), vCard);
}
}  // namespace module::im
