﻿/*
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

#include "ok_conductor.h"

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <api/audio_options.h>
#include <api/create_peerconnection_factory.h>
#include <api/data_channel_interface.h>
#include <api/jsep.h>
#include <modules/video_capture/video_capture.h>
#include <modules/video_capture/video_capture_factory.h>
#include <pc/video_track_source.h>

namespace lib::ortc {

Conductor::Conductor(WebRTC* webrtc, std::string peerId_, std::string sId_,
                     WebRTCObserver* observer)
        : peerId(std::move(peerId_))
        , sId(std::move(sId_))
        , webRtc(webrtc)
        , observer(observer)
        , _remote_audio_track(nullptr)
        , _remote_video_track(nullptr) {
    RTC_LOG(LS_INFO) << __func__ << " sId:" << sId << " peerId:" << peerId;

    assert(webRtc);
    assert(observer);
    assert(!sId.empty());
    assert(!peerId.empty());

    CreatePeerConnection();

    RTC_LOG(LS_INFO) << __func__;
}

Conductor::~Conductor() {
    RTC_LOG(LS_INFO) << __func__ << "...";
    DestroyPeerConnection();
    RTC_LOG(LS_INFO) << __func__ << "Destroyed";
}

void Conductor::CreatePeerConnection() {
    RTC_LOG(LS_INFO) << __func__ << "...";

    webrtc::PeerConnectionDependencies pc_dependencies(this);

    auto& config = webRtc->getConfig();
    for (const auto& item : config.servers) {
        RTC_LOG(LS_INFO) << " Using ice server: " << item.uri << " username:" << item.username;
    }
    auto maybe =
            webRtc->getFactory()->CreatePeerConnectionOrError(config, std::move(pc_dependencies));
    for (auto h : webRtc->getHandlers()) {
        h->onCreatePeerConnection(sId, peerId, maybe.ok());
    }

    if (!maybe.ok()) {
        return;
    }

    peer_connection_ = maybe.value();
    RTC_LOG(LS_INFO) << __func__ << " done.";
}

void Conductor::DestroyPeerConnection() {
    RTC_LOG(LS_INFO) << __func__ << "...";
    removeLocalAudioTrack();
    removeLocalVideoTrack();
    peer_connection_->Close();
    peer_connection_.release();
    RTC_LOG(LS_INFO) << __func__ << " done.";
}

size_t Conductor::getVideoCaptureSize() {
    std::unique_ptr<webrtc::VideoCaptureModule::DeviceInfo> info(
            webrtc::VideoCaptureFactory::CreateDeviceInfo());
    if (!info) {
        return 0;
    }
    int num_devices = info->NumberOfDevices();
    return num_devices;
}

void Conductor::setEnable(bool audio, bool video) {
    RTC_LOG(LS_INFO) << __func__ << " audio:" << audio << " video:" << video;
    if (_audioTrack) {
        _audioTrack->set_enabled(audio);
    }

    if (_videoTrack) {
        _videoTrack->set_enabled(video);
    }
}

void Conductor::setRemoteMute(bool mute) {
    RTC_LOG(LS_INFO) << __func__ << mute;
    if (_remote_audio_track) {
        _remote_audio_track->set_enabled(!mute);
    }
}

bool Conductor::addLocalAudioTrack(webrtc::AudioSourceInterface* _audioSource,
                                   const std::string& streamId,
                                   const std::string& trackId) {
    RTC_LOG(LS_INFO) << __func__ << " streamId: " << streamId << " trackId: " << trackId;

    _audioTrack = webRtc->getFactory()->CreateAudioTrack(trackId, _audioSource);
    RTC_LOG(LS_INFO) << "Created audio track:" << _audioTrack.get();

    auto added = peer_connection_->AddTrack(_audioTrack, {streamId});
    if (!added.ok()) {
        RTC_LOG(LS_INFO) << "Failed to add track:%1" << added.error().message();
        return false;
    }

    _audioRtpSender = added.value();
    RTC_LOG(LS_INFO) << "Added audioRtpSender ptr is:" << _audioRtpSender.get();
    return true;
}

bool Conductor::removeLocalAudioTrack() {
    RTC_LOG(LS_INFO) << __func__;
    auto result = peer_connection_->RemoveTrackOrError(_audioRtpSender);
    return result.ok();
}

bool Conductor::addLocalVideoTrack(rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> track,
                                   const std::string& streamId,
                                   const std::string& trackId) {
    RTC_LOG(LS_INFO) << __func__ << " streamId: " << streamId << " trackId: " << trackId;

    auto added = peer_connection_->AddTrack(track, {streamId});
    if (!added.ok()) {
        RTC_LOG(LS_INFO) << "Failed to add track:" << added.error().message();
        return false;
    }

    _videoRtpSender = added.value();
    RTC_LOG(LS_INFO) << "Added videoRtpSender ptr is:" << _videoRtpSender.get();
    return true;
}

bool Conductor::removeLocalVideoTrack() {
    RTC_LOG(LS_INFO) << __func__;
    auto result = peer_connection_->RemoveTrackOrError(_videoRtpSender);
    return result.ok();
}

void Conductor::OnDataChannel(rtc::scoped_refptr<webrtc::DataChannelInterface> channel) {
    RTC_LOG(LS_INFO) << __func__ << "OnDataChannel channel id:" << channel->id();
}

void Conductor::OnRenegotiationNeeded() {
    RTC_LOG(LS_INFO) << __func__;
}

/**
 * ICE 收集状态
 * @param state
 */
void Conductor::OnIceGatheringChange(webrtc::PeerConnectionInterface::IceGatheringState state) {
    RTC_LOG(LS_INFO) << __func__ << "=>"
                     << webrtc::PeerConnectionInterface::AsString(state).data();

    for (auto h : webRtc->getHandlers()) {
        h->onIceGatheringChange(sId, peerId, static_cast<ortc::IceGatheringState>(state));
    }
}

/**
 * ICE 连接状态
 * @param state
 */
void Conductor::OnIceConnectionChange(webrtc::PeerConnectionInterface::IceConnectionState state) {
    RTC_LOG(LS_INFO) << __func__ << "=>"
                     << webrtc::PeerConnectionInterface::AsString(state).data();
    for (auto h : webRtc->getHandlers()) {
        h->onIceConnectionChange(sId, peerId, static_cast<ortc::IceConnectionState>(state));
    }
}

void Conductor::OnIceCandidate(const webrtc::IceCandidateInterface* ice) {
    std::string str;
    ice->ToString(&str);
    RTC_LOG(LS_INFO) << __func__ << " : " << str;
    _candidates.push_back(str);
}

void Conductor::OnIceConnectionReceivingChange(bool receiving) {
    RTC_LOG(LS_INFO) << __func__ << receiving;
}

void Conductor::OnSignalingChange(webrtc::PeerConnectionInterface::SignalingState state) {
    RTC_LOG(LS_INFO) << __func__ << "=>"
                     << webrtc::PeerConnectionInterface::AsString(state).data();
    for (auto h : webRtc->getHandlers()) {
        h->onSignalingChange(sId, peerId, static_cast<ortc::SignalingState>(state));
    }
}

void Conductor::OnConnectionChange(webrtc::PeerConnectionInterface::PeerConnectionState state) {
    RTC_LOG(LS_INFO) << __func__ << "=>"
                     << webrtc::PeerConnectionInterface::AsString(state).data();
    for (auto h : webRtc->getHandlers()) {
        h->onPeerConnectionChange(sId, peerId, static_cast<ortc::PeerConnectionState>(state));
    }
}

void Conductor::OnTrack(rtc::scoped_refptr<webrtc::RtpTransceiverInterface> transceiver) {
    auto mid = transceiver->mid().value();
    if (mid.empty()) {
        RTC_LOG(LS_WARNING) << __func__ << " mid is empty!";
        return;
    }
    RTC_LOG(LS_INFO) << __func__ << " mid: " << mid;
    auto receiver = transceiver->receiver();

       // track
       auto track = receiver->track();
       RTC_LOG(LS_INFO) << __func__ << " kind:" << track->kind() << " trackId:" <<
       track->id();

       if (track->kind() == webrtc::MediaStreamTrackInterface::kAudioKind) {
           _remote_audio_track = static_cast<webrtc::AudioTrackInterface*>(track.get());
           RTC_LOG(LS_INFO) << __func__
                            << " Added successful remote audio track: " << _remote_audio_track;
       } else if (track->kind() == webrtc::MediaStreamTrackInterface::kVideoKind) {
           selfVideoSink = std::make_unique<VideoSink>(webRtc->getHandlers(), peerId, mid);
           RTC_LOG(LS_INFO) << __func__ << " Created video sink:" << selfVideoSink
                            << " for mid:" << mid;

           auto videoTrack = dynamic_cast<webrtc::VideoTrackInterface*>(track.get());
           videoTrack->AddOrUpdateSink(selfVideoSink.get(), rtc::VideoSinkWants());

           _videoSinks.insert(std::make_pair(mid, selfVideoSink.get()));
           RTC_LOG(LS_INFO) << __func__ << " Added successful remote video track: " <<
           videoTrack;
       }
}

void Conductor::OnAddTrack(
        rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver,
        const std::vector<rtc::scoped_refptr<webrtc::MediaStreamInterface>>& streams) {
    std::string receiverId = receiver->id();
    RTC_LOG(LS_INFO) << __func__ << " Receiver id:" << receiverId;
}

/**
 * track删除事件
 * @brief Conductor::OnRemoveTrack
 * @param receiver
 */
void Conductor::OnRemoveTrack(rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver) {
    RTC_LOG(LS_INFO) << __func__ << " receiver id: " << receiver->id();

    //    rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> track = receiver->track();
    //    if (track->kind() == webrtc::MediaStreamTrackInterface::kVideoKind) {
    //        auto remote_video_track = static_cast<webrtc::VideoTrackInterface*>(track.get());
    //    }
}

void Conductor::OnAddStream(rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) {
    RTC_LOG(LS_INFO) << __func__ << " New stream added streamId: " << stream->id();

    for (auto& track : stream->GetVideoTracks()) {
        auto trackId = track->id();
        auto _videoSink = new VideoSink(webRtc->getHandlers(), peerId, stream->id());
        RTC_LOG(LS_INFO) << __func__ << " Created video track id: " << trackId
                         << " for stream:" << stream->id();

        const std::string sinkKey = stream->id() + "_" + trackId;
        _videoSinks.insert(std::make_pair(sinkKey, _videoSink));

        RTC_LOG(LS_INFO) << __func__ << " Insert video sink key is: " << sinkKey;
        track->AddOrUpdateSink(_videoSink, rtc::VideoSinkWants());
    }

    for (const auto& track : stream->GetAudioTracks()) {
        if (track->kind() == webrtc::MediaStreamTrackInterface::kAudioKind) {
            _remote_audio_track = static_cast<webrtc::AudioTrackInterface*>(track.get());
            RTC_LOG(LS_INFO) << __func__ << " Added successful remote audio track: "
                             << _remote_audio_track->id();
        }
    }
}

void Conductor::OnRemoveStream(rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) {
    RTC_LOG(LS_INFO) << __func__ << " Stream removed: " << stream->id();
    std::vector<std::string> removed;
    for (auto& vt : stream->GetVideoTracks()) {
        removed.push_back(stream->id() + "_" + vt->id());
    }

    for (auto& k : removed) {
        auto it = _videoSinks.find(k);
        if (it != _videoSinks.end()) {
            delete it->second;
            _videoSinks.erase(it);
            RTC_LOG(LS_INFO) << __func__ << " Removed sink is: " << it->first;
        }
    }
}

/**
 * CreateOffer
 */
void Conductor::CreateOffer() {
    RTC_LOG(LS_INFO) << __func__ << "CreateOffer...";
    peer_connection_->SetLocalDescription(this);
    peer_connection_->CreateOffer(this, webrtc::PeerConnectionInterface::RTCOfferAnswerOptions());
    RTC_LOG(LS_INFO) << __func__ << "CreateOffer has done.";
}

void Conductor::close() {
    RTC_LOG(LS_INFO) << __func__;
    peer_connection_->Close();
}

/**
 * @brief CreateAnswer
 *
 * @param desc
 */
void Conductor::CreateAnswer() {
    RTC_LOG(LS_INFO) << __func__ << " ...";
    peer_connection_->CreateAnswer(this, webrtc::PeerConnectionInterface::RTCOfferAnswerOptions());
    RTC_LOG(LS_INFO) << __func__ << " done.";
}

void Conductor::setRemoteDescription(webrtc::SessionDescriptionInterface* desc) {
    RTC_LOG(LS_INFO) << __func__ << " desc type:" << desc->type();

    std::string sdp;
    desc->ToString(&sdp);
    RTC_LOG(LS_INFO) << "set remote sdp:\n" << sdp;

    webrtc::SdpParseError err;
    auto x = webrtc::CreateSessionDescription(desc->type(), sdp, &err);
    if (!err.description.empty()) {
        RTC_LOG(LS_WARNING) << " CreateSessionDescription error:" << err.description;
        return;
    }
    peer_connection_->SetRemoteDescription(this, x);
}

const webrtc::SessionDescriptionInterface* Conductor::getRemoteDescription() {
    return peer_connection_->remote_description();
}

void Conductor::setLocalDescription(webrtc::SessionDescriptionInterface* desc) {
    RTC_LOG(LS_INFO) << __func__ << " desc type:" << desc->type();

    std::string sdp;
    desc->ToString(&sdp);

    RTC_LOG(LS_INFO) << "set local sdp:\n" << sdp;
    webrtc::SdpParseError err;
    auto x = webrtc::CreateSessionDescription(desc->type(), sdp, &err);

    if (err.description.empty()) {
        peer_connection_->SetLocalDescription(this, x);
    }
}

const webrtc::SessionDescriptionInterface* Conductor::getLocalDescription() {
    return peer_connection_->local_description();
}

bool Conductor::addCandidate(std::unique_ptr<webrtc::IceCandidateInterface> candidate) {
    std::string str;
    candidate->ToString(&str);
    RTC_LOG(LS_INFO) << __func__ << " add remote candidate:"
                     << " mid:" << candidate->sdp_mid()
                     << " mline: " << candidate->sdp_mline_index() << " | " << str;

    auto added = peer_connection_->AddIceCandidate(candidate.release());
    RTC_LOG(LS_INFO) << __func__ << " => " << added;
    return added;
}

void Conductor::sessionTerminate() {
    peer_connection_->Close();
}

void Conductor::OnSessionTerminate(const std::string& sid, OkRTCHandler* handler) {}

void Conductor::OnSuccess() {
    RTC_LOG(LS_INFO) << __func__;

    //    auto* localDescription = getLocalDescription();
    //    if (localDescription) {
    //        if (observer) {
    //            observer->onLocalDescriptionSet(localDescription, sId, peerId);
    //        }
    //    }
    //
    //    auto* remoteDescription = getRemoteDescription();
    //    if (remoteDescription) {
    //        if (observer) {
    //            observer->onRemoteDescriptionSet(localDescription, sId, peerId);
    //        }
    //    }
}

void Conductor::OnSuccess(webrtc::SessionDescriptionInterface* desc) {
    std::string sdp;
    desc->ToString(&sdp);
    RTC_LOG(LS_INFO) << __func__ << " sdp:" << desc->type() << "\n" << sdp;

    if (desc->GetType() == webrtc::SdpType::kAnswer) {
        RTC_LOG(LS_INFO) << __func__ << " Set the sdp to local";
        peer_connection_->SetLocalDescription(this, desc);
    }

    if (observer) {
        observer->onDescriptionSet(desc, sId, peerId);
    }
}

void Conductor::OnFailure(webrtc::RTCError error) {
    RTC_LOG(LS_INFO) << __func__ << error.message();

    for (auto h : webRtc->getHandlers()) {
        h->onFailure(sId, peerId, error.message());
    }
}

void Conductor::OnSetRemoteDescriptionComplete(webrtc::RTCError error) {
    RTC_LOG(LS_INFO) << __func__ << " : " << error.message();
}

}  // namespace lib::ortc
