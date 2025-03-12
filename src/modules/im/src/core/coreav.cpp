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

#include "coreav.h"
#include <QCoreApplication>
#include <QDebug>
#include <QThread>
#include <QTimer>
#include <cassert>
#include "Bus.h"
#include "application.h"

#include "core.h"

#include "src/model/Group.h"
#include "src/nexus.h"

#include "lib/video/videoframe.h"
#include "src/video/corevideosource.h"
#include "src/model/Friend.h"

/**
 * 音视频
 * @brief CoreAV::CoreAV
 */
namespace module::im {

static CoreAV* instance = nullptr;

std::unique_ptr<lib::video::vpx_image> makeVpxFrame(uint16_t w, uint16_t h, const uint8_t* y,
                                                    const uint8_t* u, const uint8_t* v,
                                                    int32_t ystride, int32_t ustride,
                                                    int32_t vstride) {
    auto frame = std::make_unique<lib::video::vpx_image>();
    frame->d_h = h;
    frame->d_w = w;
    frame->planes[0] = const_cast<uint8_t*>(y);
    frame->planes[1] = const_cast<uint8_t*>(u);
    frame->planes[2] = const_cast<uint8_t*>(v);
    frame->stride[0] = ystride;
    frame->stride[1] = ustride;
    frame->stride[2] = vstride;
    return frame;
}

static ContactId makeContactId(const lib::messenger::IMPeerId& peerId){
    return ContactId(qstring(peerId.toFriendId()), lib::messenger::ChatType::Chat);
}

CoreAV::CoreAV(Core* core)
        : core(core)
        , coreavThread(new QThread{this})
        , selfVideoSource(std::make_unique<CoreVideoSource>())
        , iterateTimer(new QTimer(this)) {
    qDebug() << __func__;

    assert(coreavThread);
    assert(iterateTimer);

    qRegisterMetaType<lib::ortc::PeerConnectionState>("lib::ortc::PeerConnectionState");

    connect(this, &CoreAV::createCallToPeerId, this, &CoreAV::doCreateCallToPeerId);


    auto* s = lib::settings::OkSettings::getInstance();
    connect(s, &lib::settings::OkSettings::outVolumeChanged, this, [&](int vol) {
        if (imCall) {
            imCall->setSpeakerVolume(vol);
        }
    });

    iterateTimer->setSingleShot(true);
    connect(iterateTimer, &QTimer::timeout, this, &CoreAV::process);

    coreavThread->setObjectName("CoreAV");
    connect(coreavThread.get(), &QThread::finished, iterateTimer, &QTimer::stop);
    connect(coreavThread.get(), &QThread::started, this, &CoreAV::process);
    moveToThread(coreavThread.get());
    qDebug() << __func__ << "done.";
}

CoreAV::~CoreAV() {
    qDebug() << __func__;

    for (const auto& call : calls) {
        cancelCall(call.first);
    }
    for (const auto& call : groupCalls) {
        leaveGroupCall(call.first);
    }

    assert(calls.empty());
    assert(groupCalls.empty());

    coreavThread->quit();
    coreavThread->wait();
    coreavThread->deleteLater();
}

/**
 * @brief Factory method for CoreAV
 * @param core pointer to the Tox instance
 * @return CoreAV instance on success, {} on failure
 */
CoreAV::CoreAVPtr CoreAV::makeCoreAV(Core* core) {
    instance = new CoreAV(core);
    return CoreAVPtr{instance};
}

CoreAV* CoreAV::getInstance() {
    assert(instance);
    return instance;
}

/**
 * @brief Starts the CoreAV main loop that calls toxav's main loop
 */
void CoreAV::start() {
    qDebug() << __func__;
    coreavThread->start();
}

void CoreAV::process() {
    qDebug() << __func__;

    assert(QThread::currentThread() == coreavThread.get());

    imCall = new lib::messenger::MessengerCall(core->getMessenger());
    imCall->addCallHandler(this);

    emit ok::Application::Instance() -> bus()->coreAvChanged(this);
}

bool CoreAV::isCallStarted(const ContactId& f) const {
    QReadLocker locker{&callsLock};
    return calls.find(f) != calls.end();
}

bool CoreAV::isCallActive(const ContactId& f) const {
    QReadLocker locker{&callsLock};
    auto it = calls.find(f);
    if (it == calls.end()) {
        return false;
    }
    return isCallStarted(f) && it->second->isActive();
}

bool CoreAV::isCallVideoEnabled(const ContactId& f) const {
    QReadLocker locker{&callsLock};
    auto it = calls.find(f);
    return isCallStarted(f) && it->second->getVideoEnabled();
}

bool CoreAV::answerCall(PeerId peerId, bool video) {
    qDebug() << __func__ << "peer:" << peerId << "isVideo?" << video;
    QWriteLocker locker{&callsLock};

    auto friendId = peerId.toFriendId().toString();

    qDebug() << QString("Answering call to %1").arg(friendId);
    auto it = calls.find(peerId.toFriendId());
    assert(it != calls.end());

    QString callId = it->second->getCallId();
    qDebug() << QString("callId: %1").arg(callId);

    auto peerId2 = lib::messenger::IMPeerId(stdstring(peerId.toString()));

    auto answer = imCall->callAnswerToFriend(peerId2, callId.toStdString(), video);

    locker.unlock();

    emit avStart(peerId.toFriendId(), video);

    if (answer) {

        auto& fl = core->getFriendList();
        for(auto* f: fl.getAllFriends()){
            if(f->getIdAsString() == friendId){
                emit f->avStart(video);
                break;
            }
        }

        it->second->setActive(true);
        return true;
    } else {
        qWarning() << "Failed to answer call with error";
        calls.erase(it);
        return false;
    }

    //  TOXAV_ERR_ANSWER err;
    //
    //  const uint32_t videoBitrate = video ? VIDEO_DEFAULT_BITRATE : 0;
    //  if (toxav_answer(toxav.get(), friendNum,
    //                   Nexus::getProfile()->getSettings()->getAudioBitrate(), videoBitrate,
    //                   &err)) {
    //    it->second->setActive(true);
    //    return true;
    //  } else {
    //    qWarning() << "Failed to answer call with error" << err;
    //    toxav_call_control(toxav.get(), friendNum, TOXAV_CALL_CONTROL_CANCEL,
    //                       nullptr);
    //    calls.erase(it);
    //    return false;
    ////  }
}

/**
 * 创建一个外呼
 * @brief CoreAV::createCall
 * @param cid
 * @param video
 * @return
 */
bool CoreAV::createCall(const ContactId& cid, bool video) {
    qDebug() << __func__ << "=>" << cid.getId() << "video?" << video;

    QWriteLocker locker{&callsLock};

    auto it = calls.find(cid);
    if (it != calls.end()) {
        qWarning()
                << QString("Can't start call with %1, we're already in this call!").arg(cid.getId());
        return false;
    }

    QString sId = QString::number(QDateTime::currentDateTime().toMSecsSinceEpoch());
    if (!imCall->callToFriend(cid.getId().toStdString(), sId.toStdString(), video)) {
        qWarning() << "Failed call to friend" << cid.getId();
        return false;
    }

    // Audio backend must be set before making a call
    auto audio = Nexus::getInstance()->audio();

    ToxFriendCallPtr call = ToxFriendCallPtr(new ToxFriendCall(
            cid.getId(), lib::messenger::CallDirection::CallOut, *this, *audio, video));
    assert(call != nullptr);
    call->setCallId(sId);
    // Call object must be owned by this thread or there will be locking problems
    // with Audio
    call->moveToThread(this->thread());
    calls.emplace(cid, std::move(call));

    emit avCreate(cid, video);

    auto f = core->getFriend(cid);
    if(f.has_value()){
        emit f.value()->avCreating(video);
    }

    return true;
}

void CoreAV::rejectOrCancelCall(const PeerId& peerId) {
    qDebug() << __func__ << "peer:" << peerId;

    QWriteLocker locker{&callsLock};

    auto fId = peerId.toFriendId();
    auto it = calls.find(fId);
    if (it == calls.end()) {
        return;
    }

    auto& call = it->second;
    switch (call->getDirection()) {
        case lib::messenger::CallDirection::CallIn:
            imCall->callReject(lib::messenger::IMPeerId{stdstring(peerId.toString())},
                               stdstring(call->getCallId()));
            break;
        case lib::messenger::CallDirection::CallOut:
            auto pid = peerId.toString();
            auto cid = lib::messenger::IMContactId(stdstring(pid));
            imCall->callCancel(cid, stdstring(call->getCallId()));
            break;
    }
    calls.erase(it);

    emit avReject(fId);
}

bool CoreAV::cancelCall(const ContactId& cid) {
    QWriteLocker locker{&callsLock};

    auto friendNum = cid.getId();
    qDebug() << __func__ << friendNum;

    auto it = calls.find(cid);
    if (it == calls.end()) {
        qWarning() << QString("Can't cancel call with %1, we're already not in this call!")
                              .arg(friendNum);
        return true;
    }

    QString callId = it->second->getCallId();
    auto cId = lib::messenger::IMContactId{friendNum.toStdString()};
    imCall->callCancel(cId, callId.toStdString());

    calls.erase(cid);

    emit avCancel(cid);

    auto& fl = core->getFriendList();
    for(auto* f: fl.getAllFriends()){
        if(f->getIdAsString() == friendNum){
            emit f->avEnd();
            break;
        }
    }

    return true;
}

void CoreAV::onPeerConnectionChange(const lib::messenger::IMPeerId& peerId,
                                    const std::string& callId,
                                    lib::ortc::PeerConnectionState state) {
    qDebug() << __func__ << "peer:" << peerId.toString().c_str()
             << "state:" << static_cast<int>(state);

    auto& fl = core->getFriendList();
    for(auto* f: fl.getAllFriends()){
        if(f->getIdAsString() == peerId.toFriendId().c_str()){
            emit f->avPeerConnectionState(state);
            break;
        }
    }

}

void CoreAV::onIceGatheringChange(const lib::messenger::IMPeerId& peerId,
                                  const std::string& callId,
                                  lib::ortc::IceGatheringState state) {}

void CoreAV::onIceConnectionChange(const lib::messenger::IMPeerId& peerId,
                                   const std::string& callId,
                                   lib::ortc::IceConnectionState state) {}


void CoreAV::timeoutCall(const ContactId& cid) {
    QWriteLocker locker{&callsLock};

    if (!cancelCall(cid)) {
        qWarning() << QString("Failed to timeout call with %1").arg(cid.getId());
        return;
    }
    qDebug() << "Call with friend" << cid << "timed out";
}

bool CoreAV::sendCallAudio(QString callId, const int16_t* pcm, size_t samples, uint8_t chans,
                           uint32_t rate) const {
    // QReadLocker locker{&callsLock};

    // auto it = calls.find(callId);
    // if (it == calls.end()) {
    //     return false;
    // }

    // ToxFriendCall const& call = *it->second;

    // if (call.getMuteMic() || !call.isActive() ||
    //     !(call.getState() == lib::messenger::CallState::ACCEPTING_A)) {
    //     return true;
    // }

    return true;
}

void CoreAV::sendCallVideo(QString callId, std::shared_ptr<lib::video::VideoFrame> vframe) {
    // QWriteLocker locker{&callsLock};

    // We might be running in the FFmpeg thread and holding the CameraSource lock
    // So be careful not to deadlock with anything while toxav locks in
    // toxav_video_send_frame
    // auto it = calls.find(callId);
    // if (it == calls.end()) {
    //     return;
    // }

    // ToxFriendCall& call = *it->second;

    // if (!call.getVideoEnabled() || !call.isActive() ||
    //     !(call.getState() == lib::messenger::CallState::ACCEPTING_V)) {
    //     return;
    // }

    // if (call.getNullVideoBitrate()) {
    //     qDebug() << "Restarting video stream to friend" << callId;
    //     // QMutexLocker coreLocker{&coreLock};
    //     //    toxav_video_set_bit_rate(toxav.get(), callId, VIDEO_DEFAULT_BITRATE,
    //     //                             nullptr);
    //     call.setNullVideoBitrate(false);
    // }

    // auto frame = vframe->toToxYUVFrame();
    // if (!frame) {
    //     return;
    // }

    // TOXAV_ERR_SEND_FRAME_SYNC means toxav failed to lock, retry 5 times in this
    // case We don't want to be dropping iframes because of some lock held by
    // toxav_iterate
    //  TOXAV_ERR_SEND_FRAME err;
    //  int retries = 0;
    //  do {
    //    if (!toxav_video_send_frame(toxav.get(), callId, frame.width,
    //    frame.height,
    //                                frame.y, frame.u, frame.v, &err)) {
    //      if (err == TOXAV_ERR_SEND_FRAME_SYNC) {
    //        ++retries;
    //        QThread::usleep(500);
    //      } else {
    //        qDebug() << "toxav_video_send_frame error: " << err;
    //      }
    //    }
    //  } while (err == TOXAV_ERR_SEND_FRAME_SYNC && retries < 5);
    //  if (err == TOXAV_ERR_SEND_FRAME_SYNC) {
    //    qDebug() << "toxav_video_send_frame error: Lock busy, dropping frame";
    //  }
}

/**
 * @brief Toggles the mute state of the call's input (microphone).
 * @param f The friend assigned to the call
 */
void CoreAV::toggleMuteCallInput(const ContactId& f) {
    QWriteLocker locker{&callsLock};

    auto it = calls.find(f);
    if (it != calls.end()) {
        Call& call = *it->second;
        call.setMuteMic(!call.getMuteMic());
        imCall->setCtrlState(call.getCtrlState());
    }
}

/**
 * @brief Toggles the mute state of the call's output (speaker).
 * @param f The friend assigned to the call
 */
void CoreAV::toggleMuteCallOutput(const ContactId& f) {
    QWriteLocker locker{&callsLock};

    auto it = calls.find(f);
    if (it != calls.end()) {
        Call& call = *it->second;
        call.setMuteVol(!call.getMuteVol());
    }
    if (it != calls.end()) {
        Call& call = *it->second;
        call.setMuteMic(!call.getMuteMic());
        imCall->setCtrlState(call.getCtrlState());
    }
}

void CoreAV::groupCallCallback(void* tox, QString group, QString peer, const int16_t* data,
                               unsigned samples, uint8_t channels, uint32_t sample_rate,
                               void* core) {
    Q_UNUSED(tox);
    Core* c = static_cast<Core*>(core);

    //  CoreAV *cav = c->getAv();
    //  QReadLocker locker{&cav->callsLock};

    const FriendId peerPk = c->getGroupPeerPk(group, peer);
    emit c->groupPeerAudioPlaying(group, peerPk);

    //  auto it = cav->groupCalls.find(group);
    //  if (it == cav->groupCalls.end()) {
    //    return;
    //  }
    //
    //  ToxGroupCall &call = *it->second;
    //
    //  if (call.getMuteVol() || !call.isActive()) {
    //    return;
    //  }
    //
    //  call.playAudioBuffer(peerPk, data, samples, channels, sample_rate);
}

/**
 * @brief Called from core to make sure the source for that peer is invalidated
 * when they leave.
 * @param group Group Index
 * @param peer Peer Index
 */
void CoreAV::invalidateGroupCallPeerSource(QString group, FriendId peerPk) {
    // QWriteLocker locker{&callsLock};

    auto it = groupCalls.find(group);
    if (it == groupCalls.end()) {
        return;
    }
    it->second->removePeer(peerPk);
}

/**
 * @brief Get a call's video source.
 * @param friendNum Id of friend in call list.
 * @return Video surface to show
 */
lib::video::VideoSource* CoreAV::getVideoSourceFromCall(const ContactId& cid) const {
    QReadLocker locker{&callsLock};

    auto it = calls.find(cid);
    if (it == calls.end()) {
        return nullptr;
    }

    return it->second->getVideoSource();
}

/**
 * @brief Starts a call in an existing AV groupchat.
 * @note Call from the GUI thread.
 * @param groupId Id of group to join
 */
void CoreAV::joinGroupCall(const Group& group) {
    QWriteLocker locker{&callsLock};

    qDebug() << QString("Joining group call %1").arg(group.getIdAsString());

    // Audio backend must be set before starting a call
    auto audioCtrl = Nexus::getInstance()->audio();
    ToxGroupCallPtr groupcall = ToxGroupCallPtr(new ToxGroupCall{
            group, lib::messenger::CallDirection::CallOut, *this, *audioCtrl, false});
    // Call Objects must be owned by CoreAV or there will be locking problems with
    // Audio
    groupcall->moveToThread(this->thread());

    auto ret = groupCalls.emplace(group.getIdAsString(), std::move(groupcall));
    if (!ret.second) {
        qWarning() << "This group call already exists, not joining!";
        return;
    }
    ret.first->second->setActive(true);

    // TODO 发起群视频
    //    imCall->callToGroup(group.getId());
}

/**
 * @brief Will not leave the group, just stop the call.
 * @note Call from the GUI thread.
 * @param groupId Id of group to leave
 */
void CoreAV::leaveGroupCall(QString groupId) {
    QWriteLocker locker{&callsLock};

    qDebug() << QString("Leaving group call %1").arg(groupId);

    groupCalls.erase(groupId);
}

bool CoreAV::sendGroupCallAudio(QString groupId, const int16_t* pcm, size_t samples, uint8_t chans,
                                uint32_t rate) const {
    QReadLocker locker{&callsLock};

    std::map<QString, ToxGroupCallPtr>::const_iterator it = groupCalls.find(groupId);
    if (it == groupCalls.end()) {
        return false;
    }

    if (!it->second->isActive() || it->second->getMuteMic()) {
        return true;
    }

    //  if (toxav_group_send_audio(toxav_get_tox(toxav.get()), groupId, pcm,
    //  samples,
    //                             chans, rate) != 0)
    //    qDebug() << "toxav_group_send_audio error";
    //  return true;
    return false;
}

Call* CoreAV::getCall(const ContactId& cid) {
    auto it = calls.find(cid);
    if (it == calls.end()) {
        return nullptr;
    }
    return it->second.get();
}

void CoreAV::muteCallSpeaker(const ContactId& cid, bool mute) {
    QWriteLocker locker{&callsLock};

    auto it = calls.find(cid);
    if (it != calls.end()) {
        it->second->setMuteVol(mute);
        imCall->setCtrlState(it->second->getCtrlState());
    }
}

/**
 * @brief Mutes or unmutes the call's output (speaker).
 * @param g The group
 * @param mute True to mute, false to unmute
 */
void CoreAV::muteCallOutput(const ContactId& cid, bool mute) {
    QWriteLocker locker{&callsLock};
    auto it = calls.find(cid);
    if (it != calls.end()) {
        it->second->setMuteMic(mute);
        imCall->setCtrlState(it->second->getCtrlState());
    }
}

/**
 * @brief Returns the group calls input (microphone) state.
 * @param groupId The group id to check
 * @return true when muted, false otherwise
 */
bool CoreAV::isGroupCallInputMuted(const Group* g) const {
    QReadLocker locker{&callsLock};

    if (!g) {
        return false;
    }

    const QString groupId = g->getIdAsString();
    auto it = groupCalls.find(groupId);
    return (it != groupCalls.end()) && it->second->getMuteMic();
}

/**
 * @brief Returns the group calls output (speaker) state.
 * @param groupId The group id to check
 * @return true when muted, false otherwise
 */
bool CoreAV::isGroupCallOutputMuted(const Group* g) const {
    QReadLocker locker{&callsLock};

    if (!g) {
        return false;
    }

    const QString groupId = g->getIdAsString();
    auto it = groupCalls.find(groupId);
    return (it != groupCalls.end()) && it->second->getMuteVol();
}

/**
 * @brief Returns the calls input (microphone) mute state.
 * @param f The friend to check
 * @return true when muted, false otherwise
 */
bool CoreAV::isCallInputMuted(const ContactId& cid) const {
    QReadLocker locker{&callsLock};
    auto it = calls.find(cid);
    return (it != calls.end()) && it->second->getMuteMic();
}

/**
 * @brief Returns the calls output (speaker) mute state.
 * @param friendId The friend to check
 * @return true when muted, false otherwise
 */
bool CoreAV::isCallOutputMuted(const ContactId& cid) const {
    QReadLocker locker{&callsLock};
    auto it = calls.find(cid);
    return (it != calls.end()) && it->second->getMuteVol();
}

/**
 * @brief Signal to all peers that we're not sending video anymore.
 * @note The next frame sent cancels this.
 */
void CoreAV::sendNoVideo() {
    QWriteLocker locker{&callsLock};

    // We don't change the audio bitrate, but we signal that we're not sending
    // video anymore
    qDebug() << "CoreAV: Signaling end of video sending";
    for (auto& kv : calls) {
        ToxFriendCall& call = *kv.second;
        call.setNullVideoBitrate(true);
    }
}

void CoreAV::onCall(const lib::messenger::IMPeerId& peerId, const std::string& callId, bool audio,
                    bool video) {
    qDebug() << __func__ << "peerId:" << peerId.toString().c_str() << "callId:" << callId.c_str();

    //  CoreAV *self = static_cast<CoreAV *>(vSelf);
    QWriteLocker locker{&callsLock};

    auto peer = PeerId(peerId);
    auto cid = makeContactId(peerId);

    // Audio backend must be set before receiving a call
    auto audioCtrl = Nexus::getInstance()->audio();
    ToxFriendCallPtr call = ToxFriendCallPtr(
            new ToxFriendCall{qstring(peerId.toString()), lib::messenger::CallDirection::CallOut,
                              *this, *audioCtrl, video});
    call->setCallId(callId.c_str());
    call->moveToThread(thread());
    assert(call != nullptr);

    auto it = calls.emplace(cid, std::move(call));
    if (it.second == false) {
        qWarning() << QString("Rejecting call invite from %1, we're already in that call!")
                              .arg(peerId.toString().c_str());
        return;
    }

    emit avInvite(peer, video);

    auto f = core->getFriend(cid);
    if(f.has_value()){
        emit f.value()->avInvite(peer, video);
    }

}

void CoreAV::onCallCreating(const lib::messenger::IMPeerId& peerId,
                            const std::string& callId,
                            bool video) {
    auto friendNum = qstring(peerId.toFriendId());
    qDebug() << __func__ << friendNum << "callId:" << callId.c_str();

    auto& fl = core->getFriendList();
    for(auto* f: fl.getAllFriends()){
        if(f->getIdAsString().toStdString() == peerId.toFriendId()){
            emit f->avCreating(video);
            break;
        }
    }
}

void CoreAV::onCallCreated(const lib::messenger::IMPeerId& peerId, const std::string& callId) {

}

void CoreAV::onCallRetract(const lib::messenger::IMPeerId& peerId,
                           lib::messenger::CallState state) {
    auto cid = makeContactId(peerId);
    qDebug() << __func__ << (cid);
    auto it = calls.find(cid);
    if (it == calls.end()) {
        qWarning() << QString("Can't cancel call with %1, we're already not in this call!")
                              .arg(cid.getId());
        return;
    }

    calls.erase(cid);

    emit avRetract(PeerId(peerId));

    auto& fl = core->getFriendList();
    for(auto* f: fl.getAllFriends()){
        if(f->getIdAsString().toStdString() == peerId.toFriendId()){
            emit f->avEnd();
            break;
        }
    }
}

void CoreAV::onCallAcceptByOther(const lib::messenger::IMPeerId& peerId,
                                 const std::string& callId) {
    qDebug() << __func__ << peerId.toString().c_str() << "callId" << callId.c_str();
    auto cid = makeContactId(peerId);

    auto it = calls.find(cid);
    if (it == calls.end()) {
        qWarning() << QString("Can't cancel call with %1, we're already not in this call!")
                              .arg(cid.getId());
        return;
    }

    calls.erase(cid);

    auto& fl = core->getFriendList();
    for(auto* f: fl.getAllFriends()){
        if(f->getIdAsString().toStdString() == peerId.toFriendId()){
            emit f->avEnd();
            break;
        }
    }
}

void CoreAV::receiveCallStateAccepted(const lib::messenger::IMPeerId& peerId,
                                      const std::string& callId,
                                      bool video) {
    qDebug() << __func__ << "peerId" << peerId.toString().c_str() << "callId:" << callId.c_str();

    auto cid = makeContactId(peerId);

    stateCallback(cid,  //
                  video ? lib::messenger::CallState::ACCEPTING_V
                        : lib::messenger::CallState::ACCEPTING_A);

    if (QThread::currentThread() != coreavThread.get()) {
        emit createCallToPeerId(peerId, callId.c_str(), video);
    } else {
        imCall->callToPeerId(peerId, callId, video);
    }

    emit avAccept(cid);
    auto f = core->getFriend(cid);
    if(f.has_value()){
        emit f.value()->avAccept(video);
    }
}

void CoreAV::doCreateCallToPeerId(const lib::messenger::IMPeerId& peerId,
                                  const QString& callId,
                                  bool video) {
    imCall->callToPeerId(peerId, callId.toStdString(), video);
}

void CoreAV::receiveCallStateRejected(const lib::messenger::IMPeerId& peerId,
                                      const std::string& callId,
                                      bool video) {
    qDebug() << __func__ << "peerId:" << peerId.toString().c_str() << "callId:" << callId.c_str();
    auto cid = makeContactId(peerId);

    stateCallback(cid, lib::messenger::CallState::FINISHED);
}

void CoreAV::onHangup(const lib::messenger::IMPeerId& peerId, lib::messenger::CallState state) {
    qDebug() << __func__ << "peerId:" << peerId.toString().c_str();
    auto cid = makeContactId(peerId);
    stateCallback(cid, state);
}

void CoreAV::onEnd(const lib::messenger::IMPeerId& peerId) {
    qDebug() << __func__ << "peerId:" << peerId.toString().c_str();

    auto& fl = core->getFriendList();
    for(auto* f: fl.getAllFriends()){
        if(f->getIdAsString().toStdString() == peerId.toFriendId()){
            emit f->avEnd();
            break;
        }
    }
}

void CoreAV::onFriendVideoFrame(const std::string& friendId, uint16_t w, uint16_t h,
                                const uint8_t* y, const uint8_t* u, const uint8_t* v,
                                int32_t ystride, int32_t ustride, int32_t vstride) {
    // This callback should come from the CoreAV thread
    //     QReadLocker locker{&callsLock}; 为了提高性能暂时去掉

    auto it = calls.find(ContactId(qstring(friendId), lib::messenger::ChatType::Chat));
    if (it == calls.end()) {
        qWarning() << "Unable to find call for friend:" << friendId.c_str();
        return;
    }

    auto frame = makeVpxFrame(w, h, y, u, v, ystride, ustride, vstride);
    it->second->getVideoSource()->pushFrame(std::move(frame));
}

void CoreAV::onSelfVideoFrame(uint16_t w, uint16_t h, const uint8_t* y, const uint8_t* u,
                              const uint8_t* v, int32_t ystride, int32_t ustride, int32_t vstride) {
    auto frame = makeVpxFrame(w, h, y, u, v, ystride, ustride, vstride);
    selfVideoSource->pushFrame(std::move(frame));
}

void CoreAV::stateCallback(const ContactId& friendNum, lib::messenger::CallState state) {
    qDebug() << __func__ << "friend:" << friendNum << "state:" << (int)state;

    auto friendId = FriendId{friendNum};

    auto it = calls.find(friendNum);
    if (it == calls.end()) {
        qWarning() << QString("stateCallback called, but call %1 is already dead").arg(friendNum.getId());
        return;
    }

    // we must unlock this lock before emitting any signals
    // QWriteLocker locker{&self->callsLock};

    ToxFriendCall& call = *it->second;
    if (state == lib::messenger::CallState::ERROR0) {
        qWarning() << "Call with friend" << friendNum << "died of unnatural causes!";
        calls.erase(friendNum);

        auto& fl = core->getFriendList();
        for(auto* f: fl.getAllFriends()){
            if(f->getIdAsString() == friendNum.getId()){
                emit f->avEnd();
                break;
            }
        }
    } else if (state == lib::messenger::CallState::FINISHED) {
        qDebug() << "Call with friend" << friendNum << "finished quietly";
        calls.erase(friendNum);

        auto& fl = core->getFriendList();
        for(auto* f: fl.getAllFriends()){
            if(f->getIdAsString() == friendNum.getId()){
                emit f->avEnd();
                break;
            }
        }

    } else {
        // If our state was null, we started the call and were still ringing
        if (call.getState() != state) {
            call.setActive(true);
            bool videoEnabled = call.getVideoEnabled();
            call.setState(static_cast<lib::messenger::CallState>(state));

            auto& fl = core->getFriendList();
            for(auto* f: fl.getAllFriends()){
                if(f->getIdAsString() == friendNum.getId()){
                    emit f->avStart(videoEnabled);
                    break;
                }
            }

        } else if ((call.getState() == lib::messenger::CallState::SENDING_V) &&
                   !(state == lib::messenger::CallState::SENDING_V)) {
            qDebug() << "IMFriend" << friendNum << "stopped sending video";
            if (call.getVideoSource()) {
                call.getVideoSource()->stopSource();
            }

            call.setState(static_cast<lib::messenger::CallState>(state));
        } else if (!(call.getState() == lib::messenger::CallState::SENDING_V) &&
                   (state == lib::messenger::CallState::SENDING_V)) {
            // Workaround toxav sometimes firing callbacks for "send last frame" ->
            // "stop sending video" out of orders (even though they were sent in order
            // by the other end). We simply stop the videoSource from emitting
            // anything while the other end says it's not sending
            if (call.getVideoSource()) {
                call.getVideoSource()->restartSource();
            }

            call.setState(static_cast<lib::messenger::CallState>(state));
        }
    }

    //  locker.unlock();
}

// This is only a dummy implementation for now
void CoreAV::bitrateCallback(QString friendNum, uint32_t arate, uint32_t vrate, void* vSelf) {
    qDebug() << "Recommended bitrate with" << friendNum << " is now " << arate << "/" << vrate
             << ", ignoring it";
}

// This is only a dummy implementation for now
void CoreAV::audioBitrateCallback(QString friendNum, uint32_t rate, void* vSelf) {
    qDebug() << "Recommended audio bitrate with" << friendNum << " is now " << rate
             << ", ignoring it";
}

// This is only a dummy implementation for now
void CoreAV::videoBitrateCallback(QString friendNum, uint32_t rate, void* vSelf) {
    CoreAV* self = static_cast<CoreAV*>(vSelf);
    Q_UNUSED(self);
    qDebug() << "Recommended video bitrate with" << friendNum << " is now " << rate
             << ", ignoring it";
}

void CoreAV::audioFrameCallback(const ContactId& cid, const int16_t* pcm, size_t sampleCount,
                                uint8_t channels, uint32_t samplingRate, void* vSelf) {
    CoreAV* self = static_cast<CoreAV*>(vSelf);
    // This callback should come from the CoreAV thread
    assert(QThread::currentThread() == self->coreavThread.get());
    QReadLocker locker{&self->callsLock};

    auto it = self->calls.find(cid);
    if (it == self->calls.end()) {
        return;
    }

    ToxFriendCall& call = *it->second;

    if (call.getMuteVol()) {
        return;
    }

    call.playAudioBuffer(pcm, sampleCount, channels, samplingRate);
}

void CoreAV::videoFramePush(CoreVideoSource* videoSource,
                            std::unique_ptr<lib::video::vpx_image> frame) {
    if (!videoSource) {
        return;
    }

    videoSource->pushFrame(std::move(frame));
}
}  // namespace module::im
