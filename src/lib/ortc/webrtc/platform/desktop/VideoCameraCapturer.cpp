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

#include "VideoCameraCapturer.h"

#include "api/video/i420_buffer.h"
#include "api/video/video_frame_buffer.h"
#include "api/video/video_rotation.h"
#include "modules/video_capture/video_capture_factory.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

#include <stdint.h>
#include <algorithm>
#include <iostream>
#include <memory>

namespace lib::ortc {
namespace {

constexpr auto kPreferredWidth = 1280;
constexpr auto kPreferredHeight = 720;
constexpr auto kPreferredFps = 30;

}  // namespace

VideoCameraCapturer::VideoCameraCapturer(
        rtc::Thread* signalingThread, rtc::Thread* workerThread,
        std::shared_ptr<rtc::VideoSinkInterface<webrtc::VideoFrame>> sink)
        : _sink(sink), signalingThread(signalingThread), workerThread(workerThread) {}

VideoCameraCapturer::~VideoCameraCapturer() {
    destroy();
}

void VideoCameraCapturer::create() {
    _failed = false;
    const auto info = std::unique_ptr<webrtc::VideoCaptureModule::DeviceInfo>(
            webrtc::VideoCaptureFactory::CreateDeviceInfo());
    if (!info) {
        failed();
        return;
    }
    const auto count = info->NumberOfDevices();
    if (count <= 0) {
        failed();
        return;
    }
    const auto getId = [&](int index) {
        constexpr auto kLengthLimit = 256;
        char name[kLengthLimit] = {0};
        char id[kLengthLimit] = {0};
        return (info->GetDeviceName(index, name, kLengthLimit, id, kLengthLimit) == 0)
                       ? std::string(id)
                       : std::string();
    };
    auto preferredId = std::string();
    for (auto i = 0; i != count; ++i) {
        const auto id = getId(i);
        if ((_requestedDeviceId == id) ||
            (preferredId.empty() &&
             (_requestedDeviceId.empty() || _requestedDeviceId == "default"))) {
            preferredId = id;
        }
    }
    if (create(info.get(), preferredId)) {
        return;
    }
    for (auto i = 0; i != count; ++i) {
        if (create(info.get(), getId(i))) {
            return;
        }
    }
    failed();
}

void VideoCameraCapturer::failed() {
    _failed = true;
    if (_error) {
        _error();
    }
}

bool VideoCameraCapturer::create(webrtc::VideoCaptureModule::DeviceInfo* info,
                                 const std::string& deviceId) {
    _module = webrtc::VideoCaptureFactory::Create(deviceId.c_str());
    if (!_module) {
        RTC_LOG(LS_ERROR) << "Failed to create VideoCameraCapturer '" << deviceId << "'.";
        return false;
    }
    _module->RegisterCaptureDataCallback(this);

    auto requested = webrtc::VideoCaptureCapability();
    requested.videoType = webrtc::VideoType::kI420;
    requested.width = kPreferredWidth;
    requested.height = kPreferredHeight;
    requested.maxFPS = kPreferredFps;
    info->GetBestMatchedCapability(_module->CurrentDeviceName(), requested, _capability);
    if (!_capability.width || !_capability.height || !_capability.maxFPS) {
        _capability.width = kPreferredWidth;
        _capability.height = kPreferredHeight;
        _capability.maxFPS = kPreferredFps;
    }
#ifndef WEBRTC_WIN
    _capability.videoType = webrtc::VideoType::kI420;
#endif  // WEBRTC_WIN

    if (_module->StartCapture(_capability) != 0) {
        RTC_LOG(LS_ERROR) << "Failed to start VideoCameraCapturer '" << _requestedDeviceId << "'.";
        destroy();
    }

    _dimensions = std::make_pair(_capability.width, _capability.height);
    return true;
}

void VideoCameraCapturer::setState(VideoState state) {
    if (_state == state) {
        return;
    }
    _state = state;
    if (_state == VideoState::Active) {
        create();
    } else {
        destroy();
    }
}

void VideoCameraCapturer::setDeviceId(std::string deviceId) {
    if (_requestedDeviceId == deviceId) {
        return;
    }
    destroy();
    _requestedDeviceId = deviceId;
    if (_state == VideoState::Active) {
        create();
    }
}

void VideoCameraCapturer::setPreferredCaptureAspectRatio(float aspectRatio) {
    _aspectRatio = aspectRatio;
}

void VideoCameraCapturer::setOnFatalError(std::function<void()> error) {
    _error = std::move(error);
    if (_failed && _error) {
        _error();
    }
}

std::pair<int, int> VideoCameraCapturer::resolution() const {
    return _dimensions;
}

void VideoCameraCapturer::destroy() {
    _failed = false;
    if (!_module) {
        return;
    }

    _module->StopCapture();
    _module->DeRegisterCaptureDataCallback();
    _module = nullptr;
}

void VideoCameraCapturer::OnFrame(const webrtc::VideoFrame& frame) {
    std::cout << __func__ << std::endl;

    if (_state != VideoState::Active) {
        return;
    } else if (_aspectRatio <= 0.001) {
        if (_sink) _sink->OnFrame(frame);
        return;
    }
    const auto originalWidth = frame.width();
    const auto originalHeight = frame.height();
    auto width = (originalWidth > _aspectRatio * originalHeight)
                         ? int(std::round(_aspectRatio * originalHeight))
                         : originalWidth;
    auto height = (originalWidth > _aspectRatio * originalHeight)
                          ? originalHeight
                          : int(std::round(originalHeight / _aspectRatio));
    if ((width >= originalWidth && height >= originalHeight) || !width || !height) {
        _sink->OnFrame(frame);
        return;
    }

    width &= ~int(1);
    height &= ~int(1);
    const auto left = (originalWidth - width) / 2;
    const auto top = (originalHeight - height) / 2;
    rtc::scoped_refptr<webrtc::I420Buffer> croppedBuffer =
            webrtc::I420Buffer::Create(width, height);
    croppedBuffer->CropAndScaleFrom(*frame.video_frame_buffer()->ToI420(), left, top, width,
                                    height);
    webrtc::VideoFrame::Builder croppedBuilder = webrtc::VideoFrame::Builder()
                                                         .set_video_frame_buffer(croppedBuffer)
                                                         .set_rotation(webrtc::kVideoRotation_0)
                                                         .set_timestamp_us(frame.timestamp_us())
                                                         .set_id(frame.id());
    if (frame.has_update_rect()) {
        croppedBuilder.set_update_rect(frame.update_rect().ScaleWithFrame(
                frame.width(), frame.height(), left, top, width, height, width, height));
    }
    _sink->OnFrame(croppedBuilder.build());
}

}  // namespace lib::ortc
