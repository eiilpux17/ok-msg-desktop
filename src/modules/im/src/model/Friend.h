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

#ifndef FRIEND_H
#define FRIEND_H

#include <QObject>
#include <QString>

#include "src/model/FriendId.h"
#include "src/model/Contact.h"
#include "src/model/Status.h"

namespace lib::messenger {
class IMFriend;
}
namespace module::im {

class Friend : public Contact , public ICallSender{
    Q_OBJECT
public:
    // 朋友关系
    enum class RelationStatus {
        none,  // 无
        to,    // 他是你的朋友
        from,  // 你是他的朋友
        both   // 互为朋友
    };

    explicit Friend(const FriendId& friendPk,
            const QString& name = {},
            const QString& alias = {},
            bool isFriend = false,
            bool is_online = false,
            const QStringList& groups = {});

    ~Friend() override;

    const FriendId& getId() const {
        return id;
    };

    QString toString() const;

    bool hasAlias() const;


    void setStatusMessage(const QString& message);
    QString getStatusMessage() const;

    void setEventFlag(bool f) override;
    bool getEventFlag() const override;

    const FriendId getPublicKey() const {
        return FriendId{Contact::getIdAsString()};
    };

    void setStatus(Status s);
    Status getStatus() const;

    void addEnd(const QString& end) {
        ends.append(end);
    }


    bool startCall(bool video) override;

signals:

    void statusChanged(Status status, bool event);
    void onlineOfflineChanged(bool isOnline);
    void statusMessageChanged(const QString& message);
    void loadChatHistory();
    void relationStatusChanged(RelationStatus rs);


    void avCreating(bool video);
    void avAccept(bool video);
    void avInvite(PeerId pid, bool video);
    void avStart(bool video);
    void avPeerConnectionState(lib::ortc::PeerConnectionState state);
    void avEnd(bool error = false);


public slots:

private:
    FriendId id;
    bool hasNewEvents{};
    QString statusMessage;
    Status friendStatus;
    QString name;
    QString nick;
    QString alias;
    bool is_friend;
    bool is_online;
    QStringList groups;

    /**
     * 朋友关系
     * @see RelationStatus
     */
    RelationStatus mRelationStatus;
    QList<QString> ends;  // 终端列表
};
}  // namespace module::im
#endif  // FRIEND_H
