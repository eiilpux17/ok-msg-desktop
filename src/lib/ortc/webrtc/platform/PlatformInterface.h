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

#ifndef ORTC_PLATFORM_INTERFACE_H
#define ORTC_PLATFORM_INTERFACE_H

#include <map>
#include <string>
#include "api/media_stream_interface.h"
#include "api/video_codecs/video_decoder_factory.h"
#include "api/video_codecs/video_encoder_factory.h"
#include "modules/audio_device/include/audio_device.h"
#include "rtc_base/network_monitor_factory.h"
#include "rtc_base/ref_counted_object.h"
#include "rtc_base/thread.h"

namespace lib::ortc {

enum class VideoState;

class VideoCapturerInterface;
class PlatformContext;

struct PlatformCaptureInfo {
    bool shouldBeAdaptedToReceiverAspectRate = false;
    int rotation = 0;
};

class WrappedAudioDeviceModule : public webrtc::AudioDeviceModule {
public:
    virtual void Stop() = 0;
};

class DefaultWrappedAudioDeviceModule : public WrappedAudioDeviceModule {
public:
    DefaultWrappedAudioDeviceModule(rtc::scoped_refptr<webrtc::AudioDeviceModule> impl)
            : _impl(impl) {}

    virtual ~DefaultWrappedAudioDeviceModule() {}

    virtual void Stop() override {}

    virtual int32_t ActiveAudioLayer(AudioLayer* audioLayer) const override {
        return _impl->ActiveAudioLayer(audioLayer);
    }

    virtual int32_t RegisterAudioCallback(webrtc::AudioTransport* audioCallback) override {
        return _impl->RegisterAudioCallback(audioCallback);
    }

    virtual int32_t Init() override { return _impl->Init(); }

    virtual int32_t Terminate() override { return _impl->Terminate(); }

    virtual bool Initialized() const override { return _impl->Initialized(); }

    virtual int16_t PlayoutDevices() override { return _impl->PlayoutDevices(); }

    virtual int16_t RecordingDevices() override { return _impl->RecordingDevices(); }

    virtual int32_t PlayoutDeviceName(uint16_t index, char name[webrtc::kAdmMaxDeviceNameSize],
                                      char guid[webrtc::kAdmMaxGuidSize]) override {
        return _impl->PlayoutDeviceName(index, name, guid);
    }

    virtual int32_t RecordingDeviceName(uint16_t index, char name[webrtc::kAdmMaxDeviceNameSize],
                                        char guid[webrtc::kAdmMaxGuidSize]) override {
        return _impl->RecordingDeviceName(index, name, guid);
    }

    virtual int32_t SetPlayoutDevice(uint16_t index) override {
        return _impl->SetPlayoutDevice(index);
    }

    virtual int32_t SetPlayoutDevice(WindowsDeviceType device) override {
        return _impl->SetPlayoutDevice(device);
    }

    virtual int32_t SetRecordingDevice(uint16_t index) override {
        return _impl->SetRecordingDevice(index);
    }

    virtual int32_t SetRecordingDevice(WindowsDeviceType device) override {
        return _impl->SetRecordingDevice(device);
    }

    virtual int32_t PlayoutIsAvailable(bool* available) override {
        return _impl->PlayoutIsAvailable(available);
    }

    virtual int32_t InitPlayout() override { return _impl->InitPlayout(); }

    virtual bool PlayoutIsInitialized() const override { return _impl->PlayoutIsInitialized(); }

    virtual int32_t RecordingIsAvailable(bool* available) override {
        return _impl->RecordingIsAvailable(available);
    }

    virtual int32_t InitRecording() override { return _impl->InitRecording(); }

    virtual bool RecordingIsInitialized() const override { return _impl->RecordingIsInitialized(); }

    virtual int32_t StartPlayout() override { return _impl->StartPlayout(); }

    virtual int32_t StopPlayout() override { return _impl->StopPlayout(); }

    virtual bool Playing() const override { return _impl->Playing(); }

    virtual int32_t StartRecording() override { return _impl->StartRecording(); }

    virtual int32_t StopRecording() override { return _impl->StopRecording(); }

    virtual bool Recording() const override { return _impl->Recording(); }

    virtual int32_t InitSpeaker() override { return _impl->InitSpeaker(); }

    virtual bool SpeakerIsInitialized() const override { return _impl->SpeakerIsInitialized(); }

    virtual int32_t InitMicrophone() override { return _impl->InitMicrophone(); }

    virtual bool MicrophoneIsInitialized() const override {
        return _impl->MicrophoneIsInitialized();
    }

    virtual int32_t SpeakerVolumeIsAvailable(bool* available) override {
        return _impl->SpeakerVolumeIsAvailable(available);
    }

    virtual int32_t SetSpeakerVolume(uint32_t volume) override {
        return _impl->SetSpeakerVolume(volume);
    }

    virtual int32_t SpeakerVolume(uint32_t* volume) const override {
        return _impl->SpeakerVolume(volume);
    }

    virtual int32_t MaxSpeakerVolume(uint32_t* maxVolume) const override {
        return _impl->MaxSpeakerVolume(maxVolume);
    }

    virtual int32_t MinSpeakerVolume(uint32_t* minVolume) const override {
        return _impl->MinSpeakerVolume(minVolume);
    }

    virtual int32_t MicrophoneVolumeIsAvailable(bool* available) override {
        return _impl->MicrophoneVolumeIsAvailable(available);
    }

    virtual int32_t SetMicrophoneVolume(uint32_t volume) override {
        return _impl->SetMicrophoneVolume(volume);
    }

    virtual int32_t MicrophoneVolume(uint32_t* volume) const override {
        return _impl->MicrophoneVolume(volume);
    }

    virtual int32_t MaxMicrophoneVolume(uint32_t* maxVolume) const override {
        return _impl->MaxMicrophoneVolume(maxVolume);
    }

    virtual int32_t MinMicrophoneVolume(uint32_t* minVolume) const override {
        return _impl->MinMicrophoneVolume(minVolume);
    }

    virtual int32_t SpeakerMuteIsAvailable(bool* available) override {
        return _impl->SpeakerMuteIsAvailable(available);
    }

    virtual int32_t SetSpeakerMute(bool enable) override { return _impl->SetSpeakerMute(enable); }

    virtual int32_t SpeakerMute(bool* enabled) const override {
        return _impl->SpeakerMute(enabled);
    }

    virtual int32_t MicrophoneMuteIsAvailable(bool* available) override {
        return _impl->MicrophoneMuteIsAvailable(available);
    }

    virtual int32_t SetMicrophoneMute(bool enable) override {
        return _impl->SetMicrophoneMute(enable);
    }

    virtual int32_t MicrophoneMute(bool* enabled) const override {
        return _impl->MicrophoneMute(enabled);
    }

    virtual int32_t StereoPlayoutIsAvailable(bool* available) const override {
        return _impl->StereoPlayoutIsAvailable(available);
    }

    virtual int32_t SetStereoPlayout(bool enable) override {
        return _impl->SetStereoPlayout(enable);
    }

    virtual int32_t StereoPlayout(bool* enabled) const override {
        return _impl->StereoPlayout(enabled);
    }

    virtual int32_t StereoRecordingIsAvailable(bool* available) const override {
        return _impl->StereoRecordingIsAvailable(available);
    }

    virtual int32_t SetStereoRecording(bool enable) override {
        return _impl->SetStereoRecording(enable);
    }

    virtual int32_t StereoRecording(bool* enabled) const override {
        return _impl->StereoRecording(enabled);
    }

    virtual int32_t PlayoutDelay(uint16_t* delayMS) const override {
        return _impl->PlayoutDelay(delayMS);
    }

    virtual bool BuiltInAECIsAvailable() const override { return _impl->BuiltInAECIsAvailable(); }

    virtual bool BuiltInAGCIsAvailable() const override { return _impl->BuiltInAGCIsAvailable(); }

    virtual bool BuiltInNSIsAvailable() const override { return _impl->BuiltInNSIsAvailable(); }

    virtual int32_t EnableBuiltInAEC(bool enable) override {
        return _impl->EnableBuiltInAEC(enable);
    }

    virtual int32_t EnableBuiltInAGC(bool enable) override {
        return _impl->EnableBuiltInAGC(enable);
    }

    virtual int32_t EnableBuiltInNS(bool enable) override { return _impl->EnableBuiltInNS(enable); }

    virtual int32_t GetPlayoutUnderrunCount() const override {
        return _impl->GetPlayoutUnderrunCount();
    }

#if defined(WEBRTC_IOS)
    virtual int GetPlayoutAudioParameters(webrtc::AudioParameters* params) const override {
        return _impl->GetPlayoutAudioParameters(params);
    }
    virtual int GetRecordAudioParameters(webrtc::AudioParameters* params) const override {
        return _impl->GetRecordAudioParameters(params);
    }
#endif  // WEBRTC_IOS

    rtc::scoped_refptr<webrtc::AudioDeviceModule> WrappedInstance() { return _impl; }

private:
    rtc::scoped_refptr<webrtc::AudioDeviceModule> _impl;
};

class PlatformInterface {
public:
    static PlatformInterface* SharedInstance();
    virtual ~PlatformInterface() = default;

    virtual void configurePlatformAudio() {}

    virtual std::unique_ptr<rtc::NetworkMonitorFactory> createNetworkMonitorFactory() {
        return nullptr;
    }

    virtual std::unique_ptr<webrtc::VideoEncoderFactory> makeVideoEncoderFactory(
            bool preferHardwareEncoding = false, bool isScreencast = false) = 0;
    virtual std::unique_ptr<webrtc::VideoDecoderFactory> makeVideoDecoderFactory() = 0;
    virtual bool supportsEncoding(const std::string& codecName) = 0;

    virtual rtc::scoped_refptr<webrtc::VideoTrackSourceInterface> makeVideoSource(
            rtc::Thread* signalingThread, rtc::Thread* workerThread) = 0;

    virtual void adaptVideoSource(rtc::scoped_refptr<webrtc::VideoTrackSourceInterface> videoSource,
                                  int width, int height, int fps) = 0;

    virtual std::unique_ptr<VideoCapturerInterface> makeVideoCapturer(
            rtc::Thread* signalingThread,
            rtc::Thread* workerThread,
            rtc::scoped_refptr<webrtc::VideoTrackSourceInterface>
                    source,
            std::string deviceId,
            std::function<void(VideoState)> stateUpdated,
            std::function<void(PlatformCaptureInfo)> captureInfoUpdated,
            std::shared_ptr<PlatformContext> platformContext,
            std::pair<int, int>& outResolution) = 0;

public:
    bool preferX264 = false;
};

std::unique_ptr<PlatformInterface> CreatePlatformInterface();

inline PlatformInterface* PlatformInterface::SharedInstance() {
    static const auto result = CreatePlatformInterface();
    return result.get();
}

}  // namespace lib::ortc

#endif
