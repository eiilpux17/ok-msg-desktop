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

#include "DesktopInterface.h"

#include "VideoCapturerInterfaceImpl.h"
#include "VideoCapturerTrackSource.h"

#include "api/video_codecs/builtin_video_decoder_factory.h"
#include "api/video_codecs/builtin_video_encoder_factory.h"
#include "pc/video_track_source_proxy.h"

#if OK_RTC_UWP_DESKTOP
#include "modules/video_coding/codecs/h264/win/h264_mf_factory.h"
#endif

namespace lib::ortc {

std::unique_ptr<webrtc::VideoEncoderFactory> DesktopInterface::makeVideoEncoderFactory(
        bool preferHardwareEncoding, bool isScreencast) {
#if OK_RTC_UWP_DESKTOP
    return std::make_unique<webrtc::H264MFEncoderFactory>();
#else
    return webrtc::CreateBuiltinVideoEncoderFactory();
#endif
}

std::unique_ptr<webrtc::VideoDecoderFactory> DesktopInterface::makeVideoDecoderFactory() {
#if OK_RTC_UWP_DESKTOP
    return std::make_unique<webrtc::H264MFDecoderFactory>();
#else
    return webrtc::CreateBuiltinVideoDecoderFactory();
#endif
}

rtc::scoped_refptr<webrtc::VideoTrackSourceInterface> DesktopInterface::makeVideoSource(
        rtc::Thread* signalingThread, rtc::Thread* workerThread) {
    const auto videoTrackSource = rtc::scoped_refptr<VideoCapturerTrackSource>(
            new rtc::RefCountedObject<VideoCapturerTrackSource>());

    return videoTrackSource ? webrtc::VideoTrackSourceProxy::Create(
                                      signalingThread, workerThread, videoTrackSource)
                            : nullptr;
}

bool DesktopInterface::supportsEncoding(const std::string& codecName) {
    return (codecName == cricket::kH264CodecName) || (codecName == cricket::kVp8CodecName);
}

void DesktopInterface::adaptVideoSource(
        rtc::scoped_refptr<webrtc::VideoTrackSourceInterface> videoSource,
        int width,
        int height,
        int fps) {}

std::unique_ptr<VideoCapturerInterface> DesktopInterface::makeVideoCapturer(
        rtc::Thread* signalingThread, rtc::Thread* workerThread,
        rtc::scoped_refptr<webrtc::VideoTrackSourceInterface> source,
        std::string deviceId,
        std::function<void(VideoState)>
                stateUpdated,
        std::function<void(PlatformCaptureInfo)>
                captureInfoUpdated,
        std::shared_ptr<PlatformContext>
                platformContext,
        std::pair<int, int>& outResolution) {
    return std::make_unique<VideoCapturerInterfaceImpl>(signalingThread, workerThread, source, deviceId, stateUpdated, platformContext, outResolution);
}

std::unique_ptr<PlatformInterface> CreatePlatformInterface() {
    return std::make_unique<DesktopInterface>();
}

}  // namespace lib::ortc
