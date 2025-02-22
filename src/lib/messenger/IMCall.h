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

//
// Created by gaojie on 24-5-29.
//

#ifndef OKMSG_PROJECT_IMCALL_H
#define OKMSG_PROJECT_IMCALL_H

#include "IM.h"
#include "IMFriend.h"
#include "IMJingle.h"

namespace lib::session {
class AuthSession;
}

namespace lib::ortc {
class OkRTCManager;
}

namespace lib::messenger {

class IM;
class IMJingle;

enum class CallDirection { CallIn, CallOut };

enum CallStage {
    StageNone,
    StageMessage,  // XEP-0353: Jingle Message Initiation
    StageSession   // XEP-0166: Jingle https://xmpp.org/extensions/xep-0166.html
};

class IMCallSession : public QObject, public IMJingleSession {
    Q_OBJECT
public:
    explicit IMCallSession(const QString& sId,
                           gloox::Jingle::Session* session,
                           const IMContactId& selfId,
                           const IMPeerId& peerId,
                           lib::ortc::JingleCallType callType);

    ~IMCallSession() override;

    void start() override;
    void stop() override;

    [[nodiscard]] gloox::Jingle::Session* getSession() const;
    [[nodiscard]] inline const ortc::OJingleContent& getContext() const {
        return context;
    }

    void onAccept();
    // 被动结束
    void onTerminate();
    // 主动结束
    void doTerminate();

    void setContext(const ortc::OJingleContent&);

    const gloox::Jingle::Session::Jingle* getJingle() const;
    void setJingle(const gloox::Jingle::Session::Jingle* jingle);

    [[nodiscard]] CallDirection direction() const;

    void setCallStage(CallStage state);

    void setAccepted(bool y) {
        accepted = y;
    }

    [[nodiscard]] bool isAccepted() const {
        return accepted;
    }

    const QString& getId() const {
        return sId;
    }

    void appendIce(const ortc::OIceUdp& ice) {
        pendingIceCandidates.emplace_back(ice);
    }

    void pollIce(ok::base::Fn<void(const ortc::OIceUdp&)> fn) {
        while (!pendingIceCandidates.empty()) {
            fn(pendingIceCandidates.back());
            pendingIceCandidates.pop_back();
        }
    }

private:
    QString sId;
    gloox::Jingle::Session* session;
    IMContactId selfId;
    const gloox::Jingle::Session::Jingle* jingle;
    ortc::OJingleContent context;

    lib::ortc::JingleCallType m_callType;
    CallStage m_callStage;
    bool accepted;

    std::list<ortc::OIceUdp> pendingIceCandidates;
};

/**
 * 音视频呼叫
 */
class IMCall : public IMJingle,
               public IMSessionHandler,
               public IMHandler,
               public ortc::OkRTCHandler {
    Q_OBJECT
public:
    explicit IMCall(IM* im, QObject* parent = nullptr);
    ~IMCall() override;

    void onCreatePeerConnection(const std::string& sId, const std::string& peerId,
                                bool ok) override;

    void onFailure(const std::string& sId,
                   const std::string& peerId,
                   const std::string& error) override;

    void onIceGatheringChange(const std::string& sId,
                              const std::string& peerId,
                              ortc::IceGatheringState state) override;

    void onIceConnectionChange(const std::string& sId,
                               const std::string& peerId,
                               ortc::IceConnectionState state) override;

    void onPeerConnectionChange(const std::string& sId,
                                const std::string& peerId,
                                ortc::PeerConnectionState state) override;

    void onSignalingChange(const std::string& sId,
                           const std::string& peerId,
                           lib::ortc::SignalingState state) override;

    void onLocalDescriptionSet(const std::string& sId,       //
                               const std::string& friendId,  //
                               const ortc::OJingleContentMap* av) override;

    // onIce
    void onIce(const std::string& sId,       //
               const std::string& friendId,  //
               const lib::ortc::OIceUdp&) override;

    // Renderer
    void onRender(const lib::ortc::RendererImage& image,
                  const std::string& peerId,
                  const std::string& resource) override;

    void addCallHandler(CallHandler*);

    // 发起呼叫邀请
    bool callToFriend(const QString& f, const QString& sId, bool video);
    // 创建呼叫
    bool callToPeerId(const IMPeerId& to, const QString& sId, bool video);
    // 应答呼叫
    bool callAnswerToFriend(const IMPeerId& peer, const QString& callId, bool video);
    // 取消呼叫
    void callCancel(const IMContactId& f, const QString& sId);
    // 拒绝呼叫
    void callReject(const IMPeerId& f, const QString& sId);

    void setCtrlState(ortc::CtrlState state);
    void setSpeakerVolume(uint32_t vol);

    /**
     * jingle-message
     */
    // 处理JingleMessage消息
    void doJingleMessage(const IMPeerId& peerId, const gloox::Jingle::JingleMessage* jm);

    // 发起呼叫邀请
    void proposeJingleMessage(const QString& friendId, const QString& callId, bool video);
    // 接受
    void acceptJingleMessage(const IMPeerId& peerId, const QString& callId, bool video);
    // 拒绝
    void rejectJingleMessage(const QString& friendId, const QString& callId);
    // 撤回
    void retractJingleMessage(const QString& friendId, const QString& callId);

protected:
    void handleJingleMessage(const IMPeerId& peerId,
                             const gloox::Jingle::JingleMessage* jm) override;
    virtual bool doSessionInitiate(gloox::Jingle::Session* session,        //
                                   const gloox::Jingle::Session::Jingle*,  //
                                   const IMPeerId&) override;
    virtual bool doSessionInfo(const gloox::Jingle::Session::Jingle*, const IMPeerId&) override;
    virtual bool doSessionTerminate(gloox::Jingle::Session* session,        //
                                    const gloox::Jingle::Session::Jingle*,  //
                                    const IMPeerId&) override;

    virtual bool doSessionAccept(gloox::Jingle::Session* session,        //
                                 const gloox::Jingle::Session::Jingle*,  //
                                 const IMPeerId&) override;
    virtual bool doContentAdd(const gloox::Jingle::Session::Jingle*, const IMPeerId&) override;
    virtual bool doContentRemove(const gloox::Jingle::Session::Jingle*, const IMPeerId&) override;
    virtual bool doContentModify(const gloox::Jingle::Session::Jingle*, const IMPeerId&) override;
    virtual bool doContentAccept(const gloox::Jingle::Session::Jingle*, const IMPeerId&) override;
    virtual bool doContentReject(const gloox::Jingle::Session::Jingle*, const IMPeerId&) override;
    virtual bool doTransportInfo(const gloox::Jingle::Session::Jingle*, const IMPeerId&) override;
    virtual bool doTransportAccept(const gloox::Jingle::Session::Jingle*, const IMPeerId&) override;
    virtual bool doTransportReject(const gloox::Jingle::Session::Jingle*, const IMPeerId&) override;
    virtual bool doTransportReplace(const gloox::Jingle::Session::Jingle*,
                                    const IMPeerId&) override;
    virtual bool doSecurityInfo(const gloox::Jingle::Session::Jingle*, const IMPeerId&) override;
    virtual bool doDescriptionInfo(const gloox::Jingle::Session::Jingle*, const IMPeerId&) override;
    virtual bool doSourceAdd(const gloox::Jingle::Session::Jingle*, const IMPeerId&) override;
    virtual bool doInvalidAction(const gloox::Jingle::Session::Jingle*, const IMPeerId&) override;


    /**
     * IMHandler
     */
    void onConnecting()override;
    void onConnected()override;
    void onDisconnected(int)override;
    void onStarted()override;
    void onStopped()override;

    IMCallSession* cacheSessionInfo(const IMContactId& from,
                                    const IMPeerId& to,
                                    const QString& sId,
                                    lib::ortc::JingleCallType callType);

    void clearSessionInfo(const QString& sId) override;

    IMCallSession* createSession(const IMContactId& from,
                                 const IMPeerId& to,
                                 const QString& sId,
                                 lib::ortc::JingleCallType ct);

    IMCallSession* findSession(const QString& sId) {
        return m_sessionMap.value(sId);
    }

private:

    /**
     * 发起呼叫
     * @param friendId
     * @param video
     * @return
     */
    bool startCall(const QString& friendId, const QString& sId, bool video);

    bool sendCallToResource(const QString& friendId, const QString& sId, bool video);

    bool createCall(const IMPeerId& to, const QString& sId, bool video);

    bool answer(const IMPeerId& to, const QString& callId, bool video);

    // 取消呼叫
    void cancelCall(const IMContactId& friendId, const QString& sId);
    void rejectCall(const IMPeerId& friendId, const QString& sId);

    void join(const gloox::JID& room);

    void doForIceCompleted(const QString& sId, const QString& peerId);

    std::vector<CallHandler*> callHandlers;

    // sid -> session
    QMap<QString, IMCallSession*> m_sessionMap;

    // sid -> isVideo,在jingle-message阶段暂时保留呼叫的类型是视频（音频无需保存）。
    QMap<QString, bool> m_sidVideo;

    // 停止标志
    bool terminated = false;
    bool destroyedRtc = false;

    void destroyRtc();

public slots:

    void onImStartedCall();
};

}  // namespace lib::messenger

#endif
