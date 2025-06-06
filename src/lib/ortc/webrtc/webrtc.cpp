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
#include "webrtc.h"
#include "ok_conductor.h"

#include <memory>
#include <range/v3/range.hpp>
#include <range/v3/view.hpp>
#include <string>
#include <utility>

#include <api/audio_codecs/builtin_audio_decoder_factory.h>
#include <api/audio_codecs/builtin_audio_encoder_factory.h>
#include <api/create_peerconnection_factory.h>
#include <api/peer_connection_interface.h>
#include <api/task_queue/default_task_queue_factory.h>
#include <api/video_codecs/builtin_video_decoder_factory.h>
#include <api/video_codecs/builtin_video_encoder_factory.h>
#include <media/base/codec.h>
#include <modules/video_capture/video_capture.h>
#include <modules/video_capture/video_capture_factory.h>
#include <pc/video_track_source.h>
#include <rtc_base/helpers.h>
#include <rtc_base/logging.h>
#include <rtc_base/ssl_adapter.h>
#include <rtc_base/string_encode.h>
#include <rtc_base/thread.h>

namespace lib::ortc {

constexpr int DEVICE_NAME_MAX_LEN = 255;

void setSsrc(const SsrcGroup& ssrcGroup,
             const std::vector<Source>& sources,
             cricket::RtpMediaContentDescription* content) {
    cricket::StreamParams streamParams;
    for (auto& src : sources) {
        streamParams.ssrcs.push_back(std::stoul(src.ssrc));
        streamParams.cname = src.cname;
        streamParams.set_stream_ids({src.msid});
    };

    // ssrc-groups
    if (!ssrcGroup.ssrcs.empty()) {
        auto ssrcs = ranges::views::all(ssrcGroup.ssrcs) |
                     ranges::views::transform([](const std::string& s) {
                         return static_cast<uint32_t>(std::stoul(s));
                     }) |
                     ranges::to_vector;
        streamParams.ssrc_groups.emplace_back(cricket::SsrcGroup(ssrcGroup.semantics, ssrcs));
    }

    if (streamParams.has_ssrcs()) {
        content->AddStream(streamParams);
    }
}

void setSsrc2(const Sources& sources, const SsrcGroup& g,
              cricket::RtpMediaContentDescription* ptr) {
    if (!sources.empty()) {
        cricket::StreamParams streamParams;
        for (auto& src : sources) {
            streamParams.ssrcs.push_back(std::stoul(src.ssrc));
            //            for (auto& p : src.parameters) {
            //                if (p.name == "cname") {
            //                    streamParams.cname = p.value;
            //                } else if (p.name == "label") {
            //                    streamParams.id = p.value;
            //                } else if (p.name == "mslabel") {
            //                    streamParams.set_stream_ids({p.value});
            //                }
            //            };
            streamParams.cname = src.cname;
            streamParams.set_stream_ids({src.msid});
        }
        if (!g.ssrcs.empty()) {
            std::vector<uint32_t> ssrcs;
            std::transform(g.ssrcs.begin(), g.ssrcs.end(),  //
                           std::back_inserter(ssrcs),
                           [](auto& s) -> uint32_t { return std::stoul(s); });
            cricket::SsrcGroup ssrcGroup(g.semantics, ssrcs);
            // ssrc-groups
            streamParams.ssrc_groups.emplace_back(ssrcGroup);
        }
        // ssrc
        ptr->AddStream(streamParams);
    }
}

void setCodec(const PayloadType& pt, cricket::Codec& codec) {
    for (auto& e : pt.parameters) {
        codec.SetParam(e.name, e.value);
    }
    for (auto& e : pt.feedbacks) {
        cricket::FeedbackParam fb(e.type, e.subtype);
        codec.AddFeedbackParam(fb);
    }
}

void setRptExtensions(const HdrExts& hdrExts, cricket::RtpMediaContentDescription* ptr) {
    for (auto& hdrExt : hdrExts) {
        webrtc::RtpExtension ext(hdrExt.uri, hdrExt.id);
        ptr->AddRtpHeaderExtension(ext);
    }
}

std::unique_ptr<cricket::AudioContentDescription> addAudioSsrcBundle(
        const OMeetSSRCBundle& ssrcBundle) {
    // audio
    auto audioPtr = std::make_unique<cricket::AudioContentDescription>();
    setSsrc(ssrcBundle.audioSsrcGroups, ssrcBundle.audioSources, audioPtr.get());

    return std::move(audioPtr);
}

std::unique_ptr<cricket::VideoContentDescription> addVideoSsrcBundle(
        const OMeetSSRCBundle& ssrcBundle) {
    // video
    auto videoPtr = std::make_unique<cricket::VideoContentDescription>();
    setSsrc(ssrcBundle.videoSsrcGroups, ssrcBundle.videoSources, videoPtr.get());

    return std::move(videoPtr);
}

WebRTC::WebRTC(Mode mode, std::string res)
        : mode(mode), peer_connection_factory(nullptr), resource(std::move(res)) {
    RTC_LOG(LS_INFO) << __func__;

    _rtcConfig.sdp_semantics = webrtc::SdpSemantics::kUnifiedPlan;
    _rtcConfig.enable_implicit_rollback = false;
    _rtcConfig.enable_ice_renomination = true;

    RTC_LOG(LS_INFO) << "Creating network thread";
    network_thread = rtc::Thread::CreateWithSocketServer();
    RTC_LOG(LS_INFO) << "Network thread=>" << network_thread;
    network_thread->SetName("network_thread", this);
    RTC_LOG(LS_INFO) << "Network thread is started=>" << network_thread->Start();

    //    network_thread = std::unique_ptr<rtc::Thread>( threads->getNetworkThread());

    RTC_LOG(LS_INFO) << "Creating worker thread";
    worker_thread = rtc::Thread::Create();
    RTC_LOG(LS_INFO) << "Worker thread=>" << worker_thread;
    worker_thread->SetName("worker_thread", this);
    RTC_LOG(LS_INFO) << "Worker thread is started=>" << worker_thread->Start();

    RTC_LOG(LS_INFO) << "Creating signaling thread";
    signaling_thread = rtc::Thread::Create();
    RTC_LOG(LS_INFO) << "Signaling thread=>" << signaling_thread;

    signaling_thread->SetName("signaling_thread", this);
    RTC_LOG(LS_INFO) << "Signaling thread is started=>" << signaling_thread->Start();

    RTC_LOG(LS_INFO) << __func__ << " be created, resource is: " << resource;
}

WebRTC::~WebRTC() {
    RTC_LOG(LS_INFO) << __func__ << " destroy...";

    worker_thread->BlockingCall([this]() { destroyVideoCapture(); });

    for (const auto& it : _pcMap) {
        auto c = it.second;
        delete c;
    }

    if (isStarted()) {
        stop();
    }

    worker_thread->BlockingCall([&]() {
        adm = nullptr;
        deviceInfo.reset();
    });

    RTC_LOG(LS_INFO) << __func__ << " destroyed.";
}

bool WebRTC::start() {
    RTC_LOG(LS_INFO) << __func__ << "Starting the WebRTC...";

    // lock
    std::lock_guard<std::recursive_mutex> lock(mutex);

    //  _logSink(std::make_unique<LogSinkImpl>())
    //    rtc::LogMessage::AddLogToStream(_logSink.get(), rtc::LS_INFO);
    rtc::LogMessage::LogToDebug(rtc::LS_INFO);
    //    rtc::LogMessage::SetLogToStderr(false);

    RTC_LOG(LS_INFO) << "InitializeSSL=>" << rtc::InitializeSSL();

    auto audioEncoderFactory = webrtc::CreateBuiltinAudioEncoderFactory();
    auto audioEncoderCodecs = audioEncoderFactory->GetSupportedEncoders();
    RTC_LOG(LS_INFO) << "WebRTC BuiltIn audio supported encoders:";
    for (auto& c : audioEncoderCodecs) {
        RTC_LOG(LS_INFO) << "codec:" << c.format.name << "/" << c.format.clockrate_hz << "/"
                         << c.format.num_channels;
    }
    auto audioDecoderFactory = webrtc::CreateBuiltinAudioDecoderFactory();
    auto audioDecoderCodecs = audioDecoderFactory->GetSupportedDecoders();
    RTC_LOG(LS_INFO) << "WebRTC BuiltIn audio supported decoders:";
    for (auto& c : audioDecoderCodecs) {
        RTC_LOG(LS_INFO) << "codec:" << c.format.name << "/" << c.format.clockrate_hz << "/"
                         << c.format.num_channels;
    }

    auto videoEncoderFactory = webrtc::CreateBuiltinVideoEncoderFactory();
    RTC_LOG(LS_INFO) << "WebRTC BuiltIn video supported encoders:";
    for (auto& c : videoEncoderFactory->GetSupportedFormats()) {
        RTC_LOG(LS_INFO) << "codec:" << c.ToString();
    }

    auto videoDecoderFactory = webrtc::CreateBuiltinVideoDecoderFactory();
    RTC_LOG(LS_INFO) << "WebRTC BuiltIn video supported decoders:";
    for (auto& c : videoEncoderFactory->GetSupportedFormats()) {
        RTC_LOG(LS_INFO) << "codec:" << c.ToString();
    }

    worker_thread->BlockingCall([&]() {
        adm = webrtc::AudioDeviceModule::Create(webrtc::AudioDeviceModule::kPlatformDefaultAudio,
                                                webrtc::CreateDefaultTaskQueueFactory().get());
        RTC_LOG(LS_INFO) << __func__ << " ADM: " << adm.get();

        deviceInfo.reset(webrtc::VideoCaptureFactory::CreateDeviceInfo());
        RTC_LOG(LS_INFO) << __func__ << " DeviceInfo: " << deviceInfo.get();
    });

    peer_connection_factory =
            webrtc::CreatePeerConnectionFactory(network_thread.get(),   /* network_thread */
                                                worker_thread.get(),    /* worker_thread */
                                                signaling_thread.get(), /* signaling_thread */
                                                adm,                    /* default_adm */
                                                audioEncoderFactory,    //
                                                audioDecoderFactory,    //
                                                std::move(videoEncoderFactory),  //
                                                std::move(videoDecoderFactory),  //
                                                nullptr /* audio_mixer */,       //
                                                nullptr /* audio_processing */);

    RTC_LOG(LS_INFO) << "peer_connection_factory:" << peer_connection_factory.get();

    webrtc::PeerConnectionFactoryInterface::Options options;
    options.disable_encryption = false;
    peer_connection_factory->SetOptions(options);

    initAudioDevice();
    initVideoDevice();

    RTC_LOG(LS_INFO) << "WebRTC has be started.";
    return true;
}

bool WebRTC::stop() {
    RTC_LOG(LS_INFO) << "WebRTC will be destroy...";
    std::lock_guard<std::recursive_mutex> lock(mutex);

    // 销毁factory
    peer_connection_factory = nullptr;

    // 清除ssl
    rtc::CleanupSSL();

    RTC_LOG(LS_INFO) << "WebRTC has be destroyed.";
    return true;
}

bool WebRTC::isStarted() {
    return peer_connection_factory.get();
}

bool WebRTC::ensureStart() {
    std::lock_guard<std::recursive_mutex> lock(mutex);
    return isStarted() ? true : start();
}

void WebRTC::close() {
    for (auto pc : _pcMap) {
        pc.second->close();
    }
}

void WebRTC::addRTCHandler(OkRTCHandler* hand) {
    RTC_LOG(LS_INFO) << __func__;
    assert(hand);
    std::lock_guard<std::recursive_mutex> lock(mutex);
    _handlers.push_back(hand);
}

void WebRTC::removeRTCHandler(OkRTCHandler* hand) {
    RTC_LOG(LS_INFO) << __func__;
    assert(hand);
    std::lock_guard<std::recursive_mutex> lock(mutex);
    _handlers.erase(std::remove_if(_handlers.begin(), _handlers.end(),
                                   [&](OkRTCHandler* h) { return h == hand; }),
                    _handlers.end());
}

void WebRTC::setVideoDevice(VideoType type, const std::string &device)
{
    vDeviceType = type;
    vDeviceName = device;
}

const std::vector<OkRTCHandler*>& WebRTC::getHandlers() {
    std::lock_guard<std::recursive_mutex> lock(mutex);
    return _handlers;
}

bool WebRTC::quit(const std::string& peerId) {
    return false;
}

std::map<std::string, OIceUdp> WebRTC::getCandidates(const std::string& peerId) {
    std::map<std::string, OIceUdp> map;

    auto conductor = getConductor(peerId);
    auto sdp = conductor->getLocalDescription();

    for (const auto& item : sdp->description()->transport_infos()) {
        map.insert(std::make_pair(item.content_name, getIceFromDown(sdp, item.content_name)));
    }

    return map;
}

webrtc::SdpType WebRTC::convertSdpTypeDown(JingleSdpType type) {
    webrtc::SdpType t;
    switch (type) {
        case JingleSdpType::Answer: {
            t = webrtc::SdpType::kAnswer;
            break;
        }
        case JingleSdpType::Offer: {
            t = webrtc::SdpType::kOffer;
            break;
        }
        case JingleSdpType::Rollback: {
            t = webrtc::SdpType::kRollback;
            break;
        }
    }
    return t;
}

JingleSdpType WebRTC::convertSdpTypeUp(webrtc::SdpType type) {
    JingleSdpType jingleSdpType;
    switch (type) {
        case webrtc::SdpType::kAnswer:
            jingleSdpType = JingleSdpType::Answer;
            break;
        case webrtc::SdpType::kOffer:
            jingleSdpType = JingleSdpType::Offer;
            break;
        case webrtc::SdpType::kPrAnswer:
            jingleSdpType = JingleSdpType::Answer;
            break;
        case webrtc::SdpType::kRollback:
            jingleSdpType = JingleSdpType::Rollback;
            break;
    }
    return jingleSdpType;
}

void addSource1(const std::map<std::string, OMeetSSRCBundle>& map,
                cricket::SessionDescription* sessionDescription,
                std::vector<const webrtc::IceCandidateInterface*>& cis,
                std::vector<const webrtc::IceCandidateInterface*>& cis2,
                cricket::ContentGroup& group) {
    RTC_LOG(LS_INFO) << __func__;
    for (auto& k : map) {
        auto participant = k.first;
        auto& ssrcBundle = k.second;

        // 获取匹配content

        if (!ssrcBundle.audioSources.empty()) {
            size_t m_size = sessionDescription->contents().size();

            auto a_contents = ranges::views::all(sessionDescription->contents()) |
                              ranges::views::filter([&](const cricket::ContentInfo& c) {
                                  return c.media_description()->type() == cricket::MEDIA_TYPE_AUDIO;
                              }) |
                              ranges::to_vector;
            if (!a_contents.empty()) {
                const auto& ci = a_contents.front();
                auto jvba = ci.media_description();

                auto mid = std::to_string(m_size);
                RTC_LOG(LS_INFO) << " participant: " << participant << " audio mid is: " << mid;

                auto ti = *sessionDescription->GetTransportInfoByName(ci.name);
                ti.content_name = mid;
                sessionDescription->AddTransportInfo(ti);

                auto audioPtr = addAudioSsrcBundle(ssrcBundle);
                audioPtr->set_codecs(jvba->codecs());
                audioPtr->set_rtcp_mux(jvba->rtcp_mux());
                audioPtr->set_rtp_header_extensions(jvba->rtp_header_extensions());
                sessionDescription->AddContent(mid, cricket::MediaProtocolType::kRtp,
                                               std::move(audioPtr));

                for (auto c : cis) {
                    std::string str;
                    c->ToString(&str);

                    webrtc::SdpParseError error;
                    webrtc::IceCandidateInterface* iceCandidate =
                            webrtc::CreateIceCandidate(mid, m_size, str, &error);
                    RTC_LOG(LS_INFO) << "Add IceCandidate => {mid:" << mid
                                     << ", mline:" << m_size - 1 << "}";
                    if (!error.description.empty()) {
                        RTC_LOG(LS_WARNING) << "Unable to parse candidate: " << str;
                        continue;
                    }
                    cis2.push_back(iceCandidate);
                }

                group.AddContentName(mid);
            }
        }

        if (!ssrcBundle.videoSources.empty()) {
            auto v_contents = ranges::views::all(sessionDescription->contents()) |
                              ranges::views::filter([&](const cricket::ContentInfo& c) {
                                  return c.media_description()->type() == cricket::MEDIA_TYPE_VIDEO;
                              }) |
                              ranges::to_vector;
            if (!v_contents.empty()) {
                size_t m_size = sessionDescription->contents().size();

                const auto& ci = v_contents.front();
                auto jvbv = ci.media_description();

                auto mid = std::to_string(m_size);
                RTC_LOG(LS_INFO) << " participant: " << participant << " video mid is: " << mid;

                auto ti = *sessionDescription->GetTransportInfoByName(ci.name);
                ti.content_name = mid;
                sessionDescription->AddTransportInfo(ti);

                auto videoPtr = addVideoSsrcBundle(ssrcBundle);
                videoPtr->set_codecs(jvbv->codecs());
                videoPtr->set_rtcp_mux(jvbv->rtcp_mux());
                videoPtr->set_rtp_header_extensions(jvbv->rtp_header_extensions());

                sessionDescription->AddContent(mid, cricket::MediaProtocolType::kRtp,
                                               std::move(videoPtr));

                for (auto c : cis) {
                    std::string str;
                    c->ToString(&str);

                    webrtc::SdpParseError error;
                    webrtc::IceCandidateInterface* iceCandidate =
                            webrtc::CreateIceCandidate(mid, m_size, str, &error);
                    RTC_LOG(LS_INFO) << "Add IceCandidate => {mid:" << mid
                                     << ", mline:" << m_size - 1 << "}";
                    if (!error.description.empty()) {
                        RTC_LOG(LS_WARNING) << "Unable to parse candidate: " << str;
                        continue;
                    }
                    cis2.push_back(iceCandidate);
                }

                group.AddContentName(mid);
            }
        }
    }
}

std::unique_ptr<webrtc::SessionDescriptionInterface> WebRTC::convertSdpToDown(
        const OJingleContentMap& av) {
    auto sessionDescription = std::make_unique<cricket::SessionDescription>();
    cricket::ContentGroup group(cricket::GROUP_TYPE_BUNDLE);
    auto sdpType = convertSdpTypeDown(av.sdpType);

    std::vector<const webrtc::IceCandidateInterface*> candidates;

    int midx = 0;

    for (const auto& kv : av.getContents()) {
        auto mid = std::to_string(midx);

        auto oSdp = kv.second;
        group.AddContentName(mid);

        auto& rtp = oSdp.rtp;
        auto& iceUdp = oSdp.iceUdp;

        // iceUdp
        cricket::TransportInfo ti = convertTransportToDown(mid, iceUdp);
        sessionDescription->AddTransportInfo(ti);

        switch (rtp.media) {
            case Media::audio: {
                auto ptr = std::make_unique<cricket::AudioContentDescription>();
                setSsrc2(rtp.sources, rtp.ssrcGroup, ptr.get());
                setRptExtensions(rtp.hdrExts, ptr.get());
                ptr->set_rtcp_mux(rtp.rtcpMux);

                for (auto& pt : rtp.payloadTypes) {
                    auto codec =
                            cricket::CreateAudioCodec(pt.id, pt.name, pt.clockrate, pt.channels);
                    setCodec(pt, codec);
                    ptr->AddCodec(codec);
                }

                sessionDescription->AddContent(mid, cricket::MediaProtocolType::kRtp,
                                               std::move(ptr));

                for (const auto& item : iceUdp.candidates) {
                    auto candidate = convertCandidateToDown(item, iceUdp);
                    auto iceCandidate = webrtc::CreateIceCandidate(mid, midx, candidate);
                    candidates.push_back(iceCandidate.release());
                }

                midx++;
                break;
            }
            case Media::video: {
                auto ptr = std::make_unique<cricket::VideoContentDescription>();
                setSsrc2(rtp.sources, rtp.ssrcGroup, ptr.get());
                setRptExtensions(rtp.hdrExts, ptr.get());
                ptr->set_rtcp_mux(rtp.rtcpMux);

                for (auto& pt : rtp.payloadTypes) {
                    auto codec = cricket::CreateVideoCodec(pt.id, pt.name);
                    setCodec(pt, codec);
                    ptr->AddCodec(codec);
                }

                sessionDescription->AddContent(mid, cricket::MediaProtocolType::kRtp,
                                               std::move(ptr));
                for (const auto& item : iceUdp.candidates) {
                    auto candidate = convertCandidateToDown(item, iceUdp);
                    auto iceCandidate = webrtc::CreateIceCandidate(mid, midx, candidate);
                    candidates.push_back(iceCandidate.release());
                }
                midx++;
                break;
            }
            case Media::application: {
                auto description = createDataDescription(oSdp);
                sessionDescription->AddContent(mid, cricket::MediaProtocolType::kSctp,
                                               std::move(description));

                for (const auto& item : iceUdp.candidates) {
                    auto candidate = convertCandidateToDown(item, iceUdp);
                    auto iceCandidate = webrtc::CreateIceCandidate(mid, midx, candidate);
                    candidates.push_back(iceCandidate.release());
                }
                midx++;

                break;
            }
            default:
                break;
        }
    }

    std::vector<const webrtc::IceCandidateInterface*> cis2;

    if (!av.getSsrcBundle().empty()) {
        std::vector<const webrtc::IceCandidateInterface*> cis =
                ranges::views::all(candidates) |
                ranges::views::filter([&](auto* c) { return c->sdp_mline_index() == 0; }) |
                ranges::to_vector;
        addSource1(av.getSsrcBundle(), sessionDescription.get(), candidates, cis2, group);
    }

    sessionDescription->AddGroup(group);
    auto ptr = webrtc::CreateSessionDescription(sdpType, av.sessionId, av.sessionVersion,
                                                std::move(sessionDescription));
    for (auto c : candidates) {
        if (!ptr->AddCandidate(c)) {
            RTC_LOG(LS_WARNING) << " Can not add candidate mid: " << c->sdp_mid()
                                << " mline_index: " << c->sdp_mline_index();
        }
    }

    for (auto c : cis2) {
        if (!ptr->AddCandidate(c)) {
            RTC_LOG(LS_WARNING) << " Can not add candidate mid: " << c->sdp_mid()
                                << " mline_index: " << c->sdp_mline_index();
        }
    }

    return ptr;
}

std::unique_ptr<OJingleContentMap> WebRTC::convertSdpToUp(
        const webrtc::SessionDescriptionInterface* desc) {
    std::unique_ptr<OJingleContentMap> av = std::make_unique<OJingleContentMap>();
    av->sessionId = desc->session_id();
    av->sessionVersion = desc->session_version();

    // ContentGroup
    cricket::ContentGroup group(cricket::GROUP_TYPE_BUNDLE);

    int i = 0;
    auto sd = desc->description();
    for (auto content : sd->contents()) {
        i++;
        const std::string& mid = content.mid();

        OSdp oSdp;

        oSdp.name = mid;
        oSdp.iceUdp = getIceFromDown(desc, mid);

        auto mediaDescription = content.media_description();
        // media type
        auto mt = mediaDescription->type();

        // rtcp_mux
        oSdp.rtp.rtcpMux = mediaDescription->rtcp_mux();

        // hdrext
        auto hdrs = mediaDescription->rtp_header_extensions();

        for (auto& hdr : hdrs) {
            HdrExt hdrExt = {hdr.id, hdr.uri};
            oSdp.rtp.hdrExts.push_back(hdrExt);
        }

        // codecs
        switch (mt) {
            case cricket::MediaType::MEDIA_TYPE_AUDIO: {
                oSdp.rtp.media = Media::audio;

                convertSourceToUp(mediaDescription->streams(),
                                  oSdp.rtp.sources,
                                  oSdp.rtp.ssrcGroup,
                                  Media::audio);

                auto audio_desc = mediaDescription->as_audio();
                auto codecs = audio_desc->codecs();
                for (auto& codec : codecs) {
                    PayloadType type;
                    type.id = codec.id;
                    type.name = codec.name;
                    type.channels = codec.channels;
                    type.clockrate = codec.clockrate;
                    type.bitrate = codec.bitrate;

                    auto cps = codec.ToCodecParameters();
                    for (auto& it : cps.parameters) {
                        Parameter parameter;
                        if (parameter.name.empty()) continue;
                        parameter.name = it.first;
                        parameter.value = it.second;
                        type.parameters.emplace_back(parameter);
                    }

                    // rtcp-fb
                    for (auto& it : codec.feedback_params.params()) {
                        Feedback fb = {it.id(), it.param()};
                        type.feedbacks.push_back(fb);
                    }

                    oSdp.rtp.payloadTypes.emplace_back(type);
                }

                break;
            }
            case cricket::MediaType::MEDIA_TYPE_VIDEO: {
                oSdp.rtp.media = Media::video;

                convertSourceToUp(mediaDescription->streams(),
                                  oSdp.rtp.sources,
                                  oSdp.rtp.ssrcGroup,
                                  Media::video);

                auto video_desc = mediaDescription->as_video();
                for (auto& codec : video_desc->codecs()) {
                    // PayloadType
                    PayloadType type;
                    type.id = codec.id;
                    type.name = codec.name;
                    type.clockrate = codec.clockrate;

                    // PayloadType parameter
                    auto cps = codec.ToCodecParameters();
                    for (auto& it : cps.parameters) {
                        Parameter parameter;
                        parameter.name = it.first;
                        parameter.value = it.second;
                        type.parameters.emplace_back(parameter);
                    }

                    // rtcp-fb
                    for (auto& it : codec.feedback_params.params()) {
                        Feedback fb = {it.id(), it.param()};
                        type.feedbacks.push_back(fb);
                    }

                    oSdp.rtp.payloadTypes.emplace_back(type);
                }
                break;
            }
            case cricket::MediaType::MEDIA_TYPE_DATA: {
                oSdp.rtp.media = Media::application;
                break;
            }
            case cricket::MEDIA_TYPE_UNSUPPORTED:
                oSdp.rtp.media = Media::invalid;
                break;
        }

        av->put(mid, oSdp);
    }

    OMeetSSRCBundle ssrcBundle;
    for (auto& c : av->getContents()) {
        auto& sdp = c.second;
        if (sdp.rtp.media == Media::video) {
            ssrcBundle.videoSources = sdp.rtp.sources;
            ssrcBundle.videoSsrcGroups = sdp.rtp.ssrcGroup;
        } else if (sdp.rtp.media == Media::audio) {
            ssrcBundle.audioSources = sdp.rtp.sources;
            for (auto& item : ssrcBundle.audioSources) {
                item.name = resource + "a0";
            }
            ssrcBundle.audioSsrcGroups = sdp.rtp.ssrcGroup;
        }
    };
    av->getSsrcBundle().insert(std::make_pair(resource, ssrcBundle));
    return av;
}

std::unique_ptr<cricket::SctpDataContentDescription> createDataDescription(const OSdp& sdp) {
    auto ptr = std::make_unique<cricket::SctpDataContentDescription>();
    // rtcp-mux
    ptr->set_rtcp_mux(sdp.rtp.rtcpMux);

    if (!sdp.iceUdp.sctp.protocol.empty()) {
        ptr->set_port(sdp.iceUdp.sctp.port);
        ptr->set_protocol(cricket::kMediaProtocolDtlsSctp);
        ptr->set_use_sctpmap(true);
    }
    return std::move(ptr);
}

void WebRTC::setIceServers(const std::vector<IceServer>& ices) {
    _rtcConfig.servers.clear();
    for (const auto& ice : ices) {
        // Add the ice server.
        webrtc::PeerConnectionInterface::IceServer ss;
        ss.uri = ice.uri;
        ss.tls_cert_policy = webrtc::PeerConnectionInterface::kTlsCertPolicyInsecureNoCheck;
        ss.username = ice.username;
        ss.password = ice.password;
        _rtcConfig.servers.push_back(ss);
    }
}

Conductor* WebRTC::createConductor(const std::string& peerId, const std::string& sId, bool video) {
    auto conductor = _pcMap[peerId];
    if (conductor) {
        return conductor;
    }

    RTC_LOG(LS_INFO) << __func__ << "peer:" << peerId << " sid:" << sId << " video:" << video;

    std::lock_guard<std::recursive_mutex> lock(mutex);

    ensureStart();

    conductor = new Conductor(this, peerId, sId, this);

    if (!video) {
        linkAudioDevice(conductor);
    } else {
        // audio
        linkAudioDevice(conductor);
        linkVideoDevice(conductor);
    }

    _pcMap[peerId] = conductor;
    return conductor;
}

Conductor* WebRTC::getConductor(const std::string& peerId) {
    std::lock_guard<std::recursive_mutex> lock(mutex);
    auto c = _pcMap[peerId];
    assert(c);
    return c;
}

void WebRTC::initAudioDevice() {
    RTC_LOG(LS_INFO) << "Create audio source...";
    audioSource = peer_connection_factory->CreateAudioSource(cricket::AudioOptions());
    RTC_LOG(LS_INFO) << "Audio source is:" << audioSource.get();
}

void WebRTC::linkAudioDevice(Conductor* c) {
    RTC_LOG(LS_INFO) << __func__ << "AddTrack audio source:" << audioSource.get();

    // a=msid:<stream-id> <track-id> <mslabel>
    // f1b4629b-video-0-2 c5858a0f-fae0-4241-8eea-20eb6f91f902-2
    auto streamId = resource + "-audio-0-0";
    auto trackId = rtc::CreateRandomString(10);

    c->addLocalAudioTrack(audioSource.get(), streamId, trackId);
}

void WebRTC::initVideoDevice() {

    if(vDeviceName.empty()){
        RTC_LOG(LS_WARNING) << __func__ << "Empty video device name!";
        return;
    }

}

void WebRTC::linkVideoDevice(Conductor* c) {
    RTC_LOG(LS_INFO) << __func__;
    auto streamId = resource + "-video-0-0";
    auto trackId = rtc::CreateRandomString(10);

    auto f = getFactory();
    if (f && videoCapture) {
        videoTrack = f->CreateVideoTrack(videoCapture->source(), trackId);
        c->addLocalVideoTrack(videoTrack, streamId, trackId);
    }
}

std::string WebRTC::getVideoDeviceId(int selected) {
    auto num_devices = deviceInfo->NumberOfDevices();
    RTC_LOG(LS_INFO) << __func__ << "Get number of video devices:" << num_devices;
    if (selected >= num_devices) {
        RTC_LOG(LS_WARNING) << __func__ << "Out of selected device index: " << selected;
        return {};
    }

    char name[DEVICE_NAME_MAX_LEN] = {};
    char uid[DEVICE_NAME_MAX_LEN] = {};
    char puid[DEVICE_NAME_MAX_LEN] = {};
    deviceInfo->GetDeviceName(selected,                   //
                              name, DEVICE_NAME_MAX_LEN,  //
                              uid, DEVICE_NAME_MAX_LEN,   //
                              puid, DEVICE_NAME_MAX_LEN);

    return std::string(uid);
}

std::vector<std::string> WebRTC::getVideoDeviceList() {
    std::vector<std::string> v;
    int num_devices = deviceInfo->NumberOfDevices();
    for (int i = 0; i < num_devices; i++) {
        char name[DEVICE_NAME_MAX_LEN] = {};
        char uid[DEVICE_NAME_MAX_LEN] = {};
        char puid[DEVICE_NAME_MAX_LEN] = {};
        deviceInfo->GetDeviceName(i,                          //
                                  name, DEVICE_NAME_MAX_LEN,  //
                                  uid, DEVICE_NAME_MAX_LEN,   //
                                  puid, DEVICE_NAME_MAX_LEN);
        v.push_back(name);
    }
    return v;
}

void WebRTC::setRemoteDescription(const std::string& peerId, const OJingleContentMap& av) {
    auto conductor = getConductor(peerId);
    auto desc = convertSdpToDown(av);
    conductor->setRemoteDescription(desc.release());
}

void WebRTC::setTransportInfo(const std::string& peerId, const std::string& sId,
                              const ortc::OIceUdp& iceUdp) {
    RTC_LOG(LS_INFO) << __func__ << " peerId:" << peerId << " sId:" << sId;

    auto conductor = getConductor(peerId);

    int mline = 0;
    for (auto& _candidate : iceUdp.candidates) {
        if (_candidate.ip.empty() || _candidate.port <= 0) continue;

        std::string type;
        switch (_candidate.type) {
            case Type::Host:
                type = cricket::LOCAL_PORT_TYPE;
                break;
            case Type::PeerReflexive:
                type = cricket::PRFLX_PORT_TYPE;
                break;
            case Type::Relayed:
                type = cricket::RELAY_PORT_TYPE;
                break;
            case Type::ServerReflexive:
                type = cricket::STUN_PORT_TYPE;
                break;
        }

        cricket::Candidate candidate(_candidate.component,
                                     _candidate.protocol,
                                     ::rtc::SocketAddress(_candidate.ip, _candidate.port),
                                     _candidate.priority,
                                     iceUdp.ufrag,
                                     iceUdp.pwd,
                                     type,
                                     _candidate.generation,
                                     _candidate.foundation,
                                     _candidate.network);

        if (!_candidate.rel_addr.empty()) {
            ::rtc::SocketAddress raddr(_candidate.rel_addr, _candidate.rel_port);
            candidate.set_related_address(raddr);
        }

        auto jsep_candidate = webrtc::CreateIceCandidate(iceUdp.mid, mline, candidate);
        conductor->addCandidate(std::move(jsep_candidate));
        mline++;
    }
}

void WebRTC::setEnable(CtrlState state) {
    RTC_LOG(LS_INFO) << __func__;
    std::lock_guard<std::mutex> lock(mtx);

    if(state.enableCam){
        if(vDeviceName.empty()){
            RTC_LOG(LS_WARNING) << "Empty device name!";
            return;
        }

        //enable video
        RTC_LOG(LS_INFO) << "Creating video capture:" << vDeviceName;
        worker_thread->BlockingCall([&]() {
            videoCapture = getVideoCapture(vDeviceName, vDeviceType == VideoType::Desktop);
        });

        if (!videoSink) {
            videoSink = std::make_shared<VideoSink>(_handlers, "", "");
        }

        videoCapture->setOutput(videoSink);

    }else{
        //disbale video
        if(videoSink){
            videoSink.reset();
        }
        if(videoCapture){
            videoCapture.reset();
        }
    }

    for (auto it : _pcMap) {
        it.second->setEnable(state.enableMic, state.enableCam);
        it.second->setRemoteMute(!state.enableSpk);
    }
}

void WebRTC::setSpeakerVolume(uint32_t vol) {
    worker_thread->BlockingCall([&]() {
        uint32_t r = adm->SetSpeakerVolume(vol);
        if (r != 0) RTC_LOG(LS_WARNING) << __func__ << "set=>" << r;
    });
}

void WebRTC::addSource(const std::string& peerId,
                       const std::map<std::string, ortc::OMeetSSRCBundle>& map) {
    auto c = getConductor(peerId);

    auto desc = c->getRemoteDescription();
    if (!desc) {
        RTC_LOG(LS_WARNING) << "No remote description!";
        return;
    }

    std::string sdp;
    desc->ToString(&sdp);
    RTC_LOG(LS_INFO) << "set remote sdp:\n" << sdp;

    webrtc::SdpParseError err;
    auto x = webrtc::CreateSessionDescription(desc->type(), sdp, &err);
    if (!err.description.empty()) {
        RTC_LOG(LS_WARNING) << "CreateSessionDescription error:" << err.description;
        return;
    }

    auto& group = const_cast<cricket::ContentGroup&>(x->description()->groups().front());

    std::vector<const webrtc::IceCandidateInterface*> cis;
    for (size_t i = 0; i < x->candidates(0)->count(); i++) {
        auto ci = x->candidates(0)->at(i);
        cis.push_back(ci);
    }

    std::vector<const webrtc::IceCandidateInterface*> cis2;
    addSource1(map, x->description(), cis, cis2, group);

    for (auto* c1 : cis2) {
        if (!x->AddCandidate(c1)) {
            RTC_LOG(LS_WARNING) << " Can not add candidate mid: " << c1->sdp_mid()
                                << " mline_index: " << c1->sdp_mline_index();
        }
    }
    c->setRemoteDescription(x);
    c->CreateAnswer();
}

bool WebRTC::CreateOffer(const std::string& peerId, const std::string& sId, bool video) {
    auto conductor = createConductor(peerId, sId, video);
    conductor->CreateOffer();

    return true;
}

void WebRTC::SessionTerminate(const std::string& peerId) {
    //    quit(peerId);
}

void WebRTC::CreateAnswer(const std::string& peerId, const OJingleContentMap& av) {
    RTC_LOG(LS_INFO) << __func__ << " peerId:" << peerId;
    auto conductor = createConductor(peerId, av.sessionId, av.isVideo());
    auto sdp = convertSdpToDown(av);
    conductor->setRemoteDescription(sdp.release());
    conductor->CreateAnswer();
}

std::unique_ptr<OJingleContentMap> WebRTC::getLocalSdp(const std::string& peerId) {
    auto conductor = getConductor(peerId);
    return convertSdpToUp(conductor->getLocalDescription());
}

size_t WebRTC::getVideoSize() {
    RTC_LOG(LS_INFO) << "Create video device...";
    auto vdi = webrtc::VideoCaptureFactory::CreateDeviceInfo();
    RTC_LOG(LS_INFO) << "Video capture numbers:" << vdi->NumberOfDevices();
    return vdi->NumberOfDevices();
}

std::shared_ptr<VideoCaptureInterface> WebRTC::getVideoCapture(const std::string& deviceId,  bool isScreenCapture) {
    RTC_LOG(LS_INFO) << __func__ << " deviceId: " << deviceId;

    if (deviceId.empty()) {
        RTC_LOG(LS_WARNING) << "Empty deviceId!";
        return {};
    }

    if (auto result = videoCapture.get()) {
        RTC_LOG(LS_INFO) << " The videoCapture is existing so switch to: " << deviceId;
        result->switchToDevice(deviceId, false);
        return videoCapture;
    }

    videoCapture = VideoCaptureInterface::Create(signaling_thread.get(),  //
                                                 worker_thread.get(),     //
                                                 deviceId,
                                                 isScreenCapture);
    return videoCapture;
}

void WebRTC::destroyVideoCapture() {
    RTC_LOG(LS_INFO) << __func__;
    videoCapture.reset();
}

void WebRTC::switchVideoDevice(const std::string& deviceId) {
    if (videoCapture) {
        videoCapture->switchToDevice(deviceId, false);
    }
}

void WebRTC::switchVideoDevice(int selected) {
    auto deviceId = getVideoDeviceId(selected);
    if (!deviceId.empty() && videoCapture) {
        videoCapture->switchToDevice(deviceId, false);
    }
}

void WebRTC::onLocalDescriptionSet(const webrtc::SessionDescriptionInterface* sdp,
                                   const std::string& sId,
                                   const std::string& peerId) {
    std::string str;
    sdp->ToString(&str);
    RTC_LOG(LS_INFO) << __func__ << " sdp:\n" << str;

    auto o_sdp = convertSdpToUp(sdp);
    if (o_sdp->getContents().empty()) return;

    for (auto h : _handlers) {
        h->onLocalDescriptionSet(sId, peerId, o_sdp.get());
    }
}

void WebRTC::onDescriptionSet(const webrtc::SessionDescriptionInterface* sdp,
                              const std::string& sId, const std::string& peerId) {}

cricket::TransportInfo WebRTC::convertTransportToDown(const std::string& name,
                                                      const OIceUdp& iceUdp) {
    RTC_LOG(LS_INFO) << __func__ << " : " << name;
    cricket::TransportInfo ti;
    ti.content_name = name;
    ti.description.ice_ufrag = iceUdp.ufrag;
    ti.description.ice_pwd = iceUdp.pwd;
    ti.description.identity_fingerprint.reset(cricket::TransportDescription::CopyFingerprint(
            rtc::SSLFingerprint::CreateFromRfc4572(iceUdp.dtls.hash, iceUdp.dtls.fingerprint)));

    if (iceUdp.dtls.setup == "actpass") {
        ti.description.connection_role = cricket::CONNECTIONROLE_ACTPASS;
    } else if (iceUdp.dtls.setup == "active") {
        ti.description.connection_role = cricket::CONNECTIONROLE_ACTIVE;
    } else if (iceUdp.dtls.setup == "passive") {
        ti.description.connection_role = cricket::CONNECTIONROLE_PASSIVE;
    } else if (iceUdp.dtls.setup == "holdconn") {
        ti.description.connection_role = cricket::CONNECTIONROLE_HOLDCONN;
    } else {
        ti.description.connection_role = cricket::CONNECTIONROLE_NONE;
    }
    return ti;
}

Dtls WebRTC::getDtls(const cricket::TransportInfo& info) {
    Dtls dtls;
    if (info.description.identity_fingerprint) {
        dtls.hash = info.description.identity_fingerprint->algorithm;
        dtls.fingerprint = info.description.identity_fingerprint->GetRfc4572Fingerprint();
    }
    switch (info.description.connection_role) {
        case cricket::CONNECTIONROLE_ACTIVE:
            dtls.setup = cricket::CONNECTIONROLE_ACTIVE_STR;
            break;
        case cricket::CONNECTIONROLE_ACTPASS:
            dtls.setup = cricket::CONNECTIONROLE_ACTPASS_STR;
            break;
        case cricket::CONNECTIONROLE_HOLDCONN:
            dtls.setup = cricket::CONNECTIONROLE_HOLDCONN_STR;
            break;
        case cricket::CONNECTIONROLE_PASSIVE:
            dtls.setup = cricket::CONNECTIONROLE_PASSIVE_STR;
            break;
        case cricket::CONNECTIONROLE_NONE:
            break;
    }

    return dtls;
}

Dtls WebRTC::getDtls(const webrtc::SessionDescriptionInterface* sdp, const std::string& mid) {
    auto transportInfos = sdp->description()->transport_infos();
    auto s = ranges::views::all(transportInfos) |
             ranges::views::filter(
                     [=](cricket::TransportInfo& info) { return info.content_name == mid; }) |
             ranges::views::transform(
                     [this](cricket::TransportInfo& info) { return getDtls(info); }) |
             ranges::to_vector;
    return s.front();
}

Candidate WebRTC::convertCandidateToUp(const cricket::Candidate& cand) {
    auto c = Candidate{
            .component = cand.component(),
            .foundation = cand.foundation(),
            .generation = cand.generation(),
            .id = cand.id(),
            .ip = cand.address().ipaddr().ToString(),
            .network = cand.network_id(),
            .port = cand.address().port(),
            .priority = cand.priority(),
            .protocol = cand.protocol(),
            .tcptype = cand.tcptype(),

    };
    if (cand.type() == cricket::LOCAL_PORT_TYPE) {
        c.type = Type::Host;
    } else if (cand.type() == cricket::STUN_PORT_TYPE) {
        c.type = Type::ServerReflexive;
    } else if (cand.type() == cricket::PRFLX_PORT_TYPE) {
        c.type = Type::PeerReflexive;
    } else if (cand.type() == cricket::RELAY_PORT_TYPE) {
        c.type = Type::Relayed;
    };
    if (c.type != Type::Host && 0 < cand.related_address().port()) {
        c.rel_addr = cand.related_address().ipaddr().ToString();
        c.rel_port = cand.related_address().port();
    }
    return c;
}

cricket::Candidate WebRTC::convertCandidateToDown(const Candidate& item, const OIceUdp& iceUdp) {
    // "host" / "srflx" / "prflx" / "relay" / token @
    // http://tools.ietf.org/html/rfc5245#section-15.1
    //            if (item.type == Type::Host) {
    //                continue;
    //            }
    std::string type;
    switch (item.type) {
        case Type::Host:
            type = cricket::LOCAL_PORT_TYPE;
            break;
        case Type::PeerReflexive:
            type = cricket::PRFLX_PORT_TYPE;
            break;
        case Type::Relayed:
            type = cricket::RELAY_PORT_TYPE;
            break;
        case Type::ServerReflexive:
            type = cricket::STUN_PORT_TYPE;
            break;
    }
    assert(!type.empty());
    return cricket::Candidate(item.component,
                              item.protocol,
                              rtc::SocketAddress{item.ip, (int)item.port},
                              item.priority,
                              iceUdp.ufrag,
                              iceUdp.pwd,
                              type,
                              item.generation,
                              item.foundation,
                              item.network);
}

ortc::OIceUdp WebRTC::getIceFromDown(const webrtc::SessionDescriptionInterface* sdp,
                                     const std::string& mid) {
    auto ti = sdp->description()->GetTransportInfoByName(mid);
    auto u = OIceUdp{
            .mid = mid,
            .ufrag = ti->description.ice_ufrag,
            .pwd = ti->description.ice_pwd,
            .dtls = getDtls(sdp, mid),
    };

    for (int i = 0; i < sdp->number_of_mediasections(); i++) {
        auto col = sdp->candidates(i);
        if (col) {
            for (int j = 0; j < col->count(); ++j) {
                u.candidates.push_back(convertCandidateToUp(col->at(j)->candidate()));
            }
        }
    }
    return u;
}

void WebRTC::convertSourceToUp(const cricket::StreamParamsVec& streamParams,
                               Sources& sources,
                               SsrcGroup& ssrcGroup,
                               Media m) {
    for (auto& stream : streamParams) {
        // ssrc
        const std::string& first_stream_id = stream.first_stream_id();
        for (auto& ssrc : stream.ssrcs) {
            sources.emplace_back(Source{.ssrc = std::to_string(ssrc),
                                        .name = resource + "-" + (m == Media::audio ? "a0" : "v0"),
                                        .videoType = (m == Media::video ? "camera" : ""),
                                        .cname = stream.cname,
                                        .msid = first_stream_id + " " + stream.id});
        }

        // ssrc-group
        if (stream.has_ssrc_groups()) {
            for (auto& g : stream.ssrc_groups) {
                ssrcGroup.semantics = g.semantics;
                for (auto ssrc : g.ssrcs) {
                    ssrcGroup.ssrcs.emplace_back(std::to_string(ssrc));
                }
            }
        }
    }
}

}  // namespace lib::ortc
