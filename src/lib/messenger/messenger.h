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

#pragma once

#include <QString>
#include <cstddef>
#include "IMFriend.h"
#include "IMGroup.h"
#include "IMMessage.h"
#include "base/jid.h"
#include "lib/ortc/ok_rtc.h"

class QDomElement;
class QDomDocument;

namespace lib::session {
class AuthSession;
}

namespace lib::messenger {
/**
 * 聊天
 */
class IM;
/**
 * 会话
 */
class IMJingle;
/**
 * 文件传输
 */
class IMFile;
/**
 * 音视频
 */
class IMCall;
/**
 *  会议
 */
class IMMeet;

class IMHandler{
public:
    virtual void onConnecting()= 0;
    virtual void onConnected()= 0;
    virtual void onDisconnected(int)= 0;
    virtual void onStarted()= 0;
    virtual void onStopped()= 0;
};

class SelfHandler {
public:
    virtual void onSelfIdChanged(QString id) = 0;
    virtual void onSelfNameChanged(QString name) = 0;
    virtual void onSelfAvatarChanged(const std::string avatar) = 0;
    virtual void onSelfStatusChanged(IMStatus status, const std::string& msg) = 0;
    virtual void onSelfVCardChanged(IMVCard& imvCard) = 0;
};

class FriendHandler {
public:
    virtual void onFriend(const IMFriend& frnd) = 0;
    virtual void onFriendRequest(QString friendId, QString msg) = 0;
    virtual void onFriendRemoved(QString friendId) = 0;
    virtual void onFriendStatus(QString friendId, IMStatus status) = 0;
    virtual void onFriendMessage(QString friendId, IMMessage message) = 0;
    virtual void onMessageSession(QString contactId, QString sid) = 0;
    virtual void onFriendNickChanged(QString friendId, QString name) = 0;
    virtual void onFriendAvatarChanged(const QString friendId, const std::string avatar) = 0;

    virtual void onFriendAliasChanged(const IMContactId& fId, const QString& alias) = 0;
    virtual void onFriendVCard(const IMContactId& fId, const IMVCard& imvCard) = 0;

    virtual void onFriendChatState(QString friendId, int state) = 0;
    virtual void onMessageReceipt(QString friendId, QString receipt) = 0;
};

class GroupHandler {
public:
    virtual void onGroup(const QString groupId, const QString name) = 0;

    virtual void onGroupInvite(const QString groupId,  //
                               const QString peerId,   //
                               const QString message) = 0;

    virtual void onGroupSubjectChanged(const QString& groupId, const QString& subject) = 0;

    virtual void onGroupMessage(const QString groupId,  //
                                const IMPeerId peerId,  //
                                const IMMessage message) = 0;

    virtual void onGroupOccupants(const QString groupId, uint size) = 0;

    virtual void onGroupInfo(QString groupId, IMGroup groupInfo) = 0;

    virtual void onGroupOccupantStatus(const QString groupId,  //
                                       IMGroupOccupant) = 0;
};

/**
 * OkIM模块对外接口
 */
class Messenger : public QObject {
    Q_OBJECT
public:
    explicit Messenger(const QString& host,
                       const QString& name,
                       const QString& password,
                       QObject* parent = nullptr);
    ~Messenger() override;

    void start();
    [[nodiscard]] bool isStarted() const;
    void stop();
    void doConnect();

    void send(const QString& xml);

    [[nodiscard]] IM* im() const {
        assert(_im);
        return _im;
    }

    IMContactId getSelfId() const;
    IMPeerId getSelfPeerId() const;
    QString getSelfUsername() const;
    QString getSelfNick() const;
    IMStatus getSelfStatus() const;

    void addIMHandler(IMHandler*);
    void addSelfHandler(SelfHandler*);
    void addGroupHandler(GroupHandler*);

    bool sendToGroup(const QString& g, const QString& msg, const QString& id);

    void receiptReceived(const QString& f, QString receipt);

    QString genUniqueId();

    /** self */
    void setSelfNickname(const QString& nickname);
    void changePassword(const QString& password);
    void setSelfAvatar(const QByteArray& avatar);

    /**
     * IMFriend (audio/video)
     */
    void addFriendHandler(FriendHandler*);

    size_t getFriendCount();

    void getFriendList(std::list<lib::messenger::IMFriend>&);

    void setFriendAlias(const QString& f, const QString& alias);

    // 添加好友
    void sendFriendRequest(const QString& username, const QString& nick, const QString& message);
    // 接受朋友邀请
    void acceptFriendRequest(const QString& f);
    // 拒绝朋友邀请
    void rejectFriendRequest(const QString& f);

    void getFriendVCard(const QString& f);

    IMStatus getFriendStatus(const QString& f);

    bool sendToFriend(const QString& f,
                      const QString& msg,
                      const QString& id,
                      bool encrypt = false);
    bool removeFriend(const QString& f);

    /**
     * Group
     */
    void loadGroupList();
    bool initRoom();

    QString createGroup(const QString& group, const QString& name);
    void joinGroup(const QString& group);
    void setRoomName(const QString& group, const QString& nick);
    void setRoomDesc(const QString& group, const QString& desc);
    void setRoomSubject(const QString& group, const QString& subject);
    void setRoomAlias(const QString& group, const QString& alias);
    bool inviteGroup(const IMContactId& group, const IMContactId& f);
    bool leaveGroup(const QString& group);
    bool destroyGroup(const QString& group);

    void sendChatState(const QString& friendId, int state);

    void requestBookmarks();

private:
    bool connectIM();

    IM* _im;
    IMJingle* jingle;



    size_t sentCount = 0;

signals:

    void incoming(const QString dom);


    void messageSent(const IMMessage& message);                  //

private slots:    
    void onEncryptedMessage(QString dom);

};

class IMCall;

enum class CallState {

    NONE = 0,

    ERROR0 = 1,

    FINISHED = 2,

    SENDING_A = 4,

    SENDING_V = 8,

    ACCEPTING_A = 16,

    ACCEPTING_V = 32,
};

class CallHandler {
public:
    virtual void onCall(const IMPeerId& peerId,  //
                        const QString& callId,   //
                        bool audio, bool video) = 0;

    virtual void onCallCreated(const IMPeerId& peerId,  //
                               const QString& callId) = 0;

    virtual void onCallRetract(const IMPeerId& peerId,  //
                               CallState state) = 0;

    virtual void onCallAcceptByOther(const IMPeerId& peerId, const QString& callId) = 0;

    virtual void onPeerConnectionChange(const IMPeerId& friendId,  //
                                        const QString& callId,     //
                                        ortc::PeerConnectionState state) = 0;

    virtual void onIceGatheringChange(const IMPeerId& friendId,  //
                                      const QString& callId,     //
                                      ortc::IceGatheringState state) = 0;

    virtual void onIceConnectionChange(const lib::messenger::IMPeerId& peerId,
                                       const QString& callId,
                                       lib::ortc::IceConnectionState state) = 0;

    virtual void receiveCallStateAccepted(const IMPeerId& friendId,  //
                                          const QString& callId,     //
                                          bool video) = 0;

    virtual void receiveCallStateRejected(const IMPeerId& friendId,  //
                                          const QString& callId,     //
                                          bool video) = 0;

    // 对方挂断
    virtual void onHangup(const IMPeerId& peerId,  //
                          CallState state) = 0;

    // 呼叫终止
    virtual void onEnd(const IMPeerId& peerId) = 0;

    virtual void onSelfVideoFrame(uint16_t w, uint16_t h,  //
                                  const uint8_t* y,        //
                                  const uint8_t* u,        //
                                  const uint8_t* v,        //
                                  int32_t ystride,         //
                                  int32_t ustride,         //
                                  int32_t vstride) = 0;

    virtual void onFriendVideoFrame(const QString& friendId,  //
                                    uint16_t w, uint16_t h,   //
                                    const uint8_t* y,         //
                                    const uint8_t* u,         //
                                    const uint8_t* v,         //
                                    int32_t ystride,          //
                                    int32_t ustride,          //
                                    int32_t vstride) = 0;
};

class MessengerCall : public QObject {
    Q_OBJECT
public:
    explicit MessengerCall(Messenger* messenger, QObject* parent = nullptr);
    ~MessengerCall() override;

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

    // 静音功能
    void setCtrlState(ortc::CtrlState state);
    void setSpeakerVolume(uint32_t vol);

private:
    IMCall* call;
};

// 不要修改顺序和值
enum class FileStatus {
    INITIALIZING = 0,
    PAUSED = 1,
    TRANSMITTING = 2,
    BROKEN = 3,
    CANCELED = 4,
    FINISHED = 5,
};

// 不要修改顺序和值
enum class FileDirection {
    SENDING = 0,
    RECEIVING = 1,
};

enum class FileControl { RESUME, PAUSE, CANCEL };

struct FileTxIBB {
    QString sid;
    int blockSize;
};

struct File {
public:
    // id = fileId & ibbId & msgId
    QString id;
    // sId: session id)
    QString sId;
    QString name;
    QString path;
    quint64 size;
    FileStatus status;
    FileDirection direction;
    FileTxIBB txIbb;
    [[__nodiscard__]] QString toString() const;
    friend QDebug& operator<<(QDebug& debug, const File& f);
};

class FileHandler {
public:
    virtual void onFileRequest(const QString& friendId, const File& file) = 0;
    virtual void onFileRecvChunk(const QString& friendId, const QString& fileId, int seq,
                                 const std::string& chunk) = 0;
    virtual void onFileRecvFinished(const QString& friendId, const QString& fileId) = 0;
    virtual void onFileSendInfo(const QString& friendId, const File& file, int m_seq,
                                int m_sentBytes, bool end) = 0;
    virtual void onFileSendAbort(const QString& friendId, const File& file, int m_sentBytes) = 0;
    virtual void onFileSendError(const QString& friendId, const File& file, int m_sentBytes) = 0;
};

/**
 * 文件传输
 */
class MessengerFile : public QObject {
    Q_OBJECT
public:
    explicit MessengerFile(Messenger* messenger, QObject* parent = nullptr);
    ~MessengerFile() override;

    void addFileHandler(FileHandler*);

    /**
     * File
     */
    void fileRejectRequest(QString friendId, const File& file);
    void fileAcceptRequest(QString friendId, const File& file);
    void fileFinishRequest(QString friendId, const QString& sId);
    void fileFinishTransfer(QString friendId, const QString& sId);
    void fileCancel(QString fileId);
    bool fileSendToFriend(const QString& f, const File& file);

private:
    IMFile* fileSender;
};

/**
 * 会议
 */

struct Meet {
    QString jid;
    QString uid;
    uint32_t startAudioMuted;
    uint32_t startVideoMuted;
    bool rtcstatsEnabled;
};

struct SourceInfo {
    // 音频禁止
    bool audioMute = false;
    // 视频禁止
    bool videoMute = false;
};

/**
 * 会议成员
 */
struct Participant {
    // 用户邮箱
    QString email;
    // 用户昵称
    QString nick;
    // 会议成员唯一标识
    QString resource;
    std::string avatarUrl;
    // IM终端标识(可定位到用户和终端)
    ok::base::Jid jid;
    QString affiliation;
    QString role;
    // 设备信息
    SourceInfo sourceInfo;
};

class MessengerMeetHandler {
public:
    virtual void onMeetCreated(const ok::base::Jid& jid,
                               bool ready,
                               const std::map<std::string, std::string>& props) = 0;

    virtual void onParticipantJoined(const ok::base::Jid& jid, const Participant& participant) = 0;

    virtual void onParticipantLeft(const ok::base::Jid& jid, const QString& participant) = 0;

    virtual void onParticipantVideoFrame(const QString& participant,
                                         const ortc::RendererImage& image) = 0;

    virtual void onParticipantMessage(const QString& participant, const QString& msg) = 0;

    virtual void onEnd() = 0;
};

class MessengerMeet : public QObject, public CallHandler {
    Q_OBJECT
public:
    explicit MessengerMeet(Messenger* messenger, QObject* parent = nullptr);
    ~MessengerMeet() override;
    /**
     * 创建会议
     * @param room
     */
    void create(const QString& room);
    /**
     * 离开会议
     */
    void leave();

    /**
     * 获取视频设备列表
     * @return
     */
    [[maybe_unused]] std::vector<std::string> getVideoDeviceList();

    /**
     * 发送消息
     * @param msg
     */
    void sendMessage(const QString& msg);

    /**
     是音频、视频、扬声器
     */
    void setCtrlState(ortc::CtrlState state);

    void addHandler(MessengerMeetHandler* hdr);
    void removeHandler(MessengerMeetHandler* hdr);

protected:
    void onCall(const IMPeerId& peerId,  //
                const QString& callId,   //
                bool audio, bool video) override;

    void onCallCreated(const IMPeerId& peerId,  //
                       const QString& callId) override;

    // 呼叫撤回：其它终端接收
    void onCallRetract(const IMPeerId& peerId,  //
                       CallState state) override;

    void onCallAcceptByOther(const IMPeerId& peerId,  //
                             const QString& callId) override;

    void onPeerConnectionChange(const IMPeerId& peerId,  //
                                const QString& callId,   //
                                ortc::PeerConnectionState state) override;

    void onIceGatheringChange(const IMPeerId& friendId,  //
                              const QString& callId,     //
                              ortc::IceGatheringState state) override;

    void onIceConnectionChange(const lib::messenger::IMPeerId& peerId,
                               const QString& callId,
                               lib::ortc::IceConnectionState state) override;

    void receiveCallStateAccepted(const IMPeerId& peerId,  //
                                  const QString& callId,   //
                                  bool video) override;

    void receiveCallStateRejected(const IMPeerId& peerId,  //
                                  const QString& callId,   //
                                  bool video) override;
    // 对方挂断
    void onHangup(const IMPeerId& peerId,  //
                  CallState state) override;

    // 结束
    void onEnd(const IMPeerId& peerId) override;

    void onSelfVideoFrame(uint16_t w, uint16_t h,  //
                          const uint8_t* y,        //
                          const uint8_t* u,        //
                          const uint8_t* v,        //
                          int32_t ystride,         //
                          int32_t ustride,         //
                          int32_t vstride) override;

    void onFriendVideoFrame(const QString& friendId,  //
                            uint16_t w, uint16_t h,   //
                            const uint8_t* y,         //
                            const uint8_t* u,         //
                            const uint8_t* v,         //
                            int32_t ystride,          //
                            int32_t ustride,          //
                            int32_t vstride) override;

private:
    IMMeet* meet;
    MessengerCall* call;
};

}  // namespace lib::messenger
