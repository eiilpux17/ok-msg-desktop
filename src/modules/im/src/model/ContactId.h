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

#ifndef CONTACTID_H
#define CONTACTID_H

#include <QByteArray>
#include <QHash>
#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include <QString>
#include <cstdint>
#include <memory>

#include "lib/messenger/Messenger.h"
namespace module::im {

inline QRegularExpressionMatch JidMatch(const QString& strId) {
    // 正则表达式模式，这里假设username不包含@，server不包含/
    QRegularExpression re("([^@]+)@([^/]+)(/([^/]+))?");
    // 匹配输入字符串
    return re.match(strId);
}

/**
 * 联系人ID（群聊ID和个人ID的父类）
 * @see FriendId
 * @see GroupId
 */
class ContactId {
public:
    explicit ContactId();
    explicit ContactId(const ContactId& cId);
    explicit ContactId(const QString& strId, lib::messenger::ChatType type);
    explicit ContactId(const QString& username, const QString& server, lib::messenger::ChatType type);

    virtual ~ContactId() = default;

    ContactId& operator=(const ContactId& other) = default;
    ContactId& operator=(ContactId&& other) = default;
    bool operator==(const ContactId& other) const;
    bool operator!=(const ContactId& other) const;
    bool operator<(const ContactId& other) const;

    QByteArray getByteArray() const;
    virtual bool isValid() const;

    lib::messenger::ChatType getChatType() const{
        return type;
    };

    virtual QString toString() const {
        return getId();
    };

    inline QString getId() const {
        return username + "@" + server;
    }

    // inline bool isLinked() const {
    //     return linked;
    // }

    // void setLinked(bool linked_) {
    //     linked = linked_;
    // }

    // 用户名
    QString username;
    // 服务器地址
    QString server;
    // 是否建立连接（群：是否在群，个人：是否是好友）
    // bool linked = false;
    // 联系人类型（群聊、个人）
    lib::messenger::ChatType type;

    friend QDebug& operator<<(QDebug& debug, const ContactId& f);
};

inline uint qHash(const ContactId& id) {
    return qHash(id.getByteArray());
}

using ContactIdPtr = std::shared_ptr<const ContactId>;
}  // namespace module::im
#endif  // CONTACTID_H
