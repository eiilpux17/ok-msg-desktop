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

#include "FriendId.h"

#include <QByteArray>
#include <QDebug>
#include <QRegularExpression>
#include <QString>
#include <cassert>
#include "base/basic_types.h"

namespace module::im {

/**
 * @brief The default constructor. Creates an empty Tox key.
 */
FriendId::FriendId() : ContactId() {}

/**
 * @brief The copy constructor.
 * @param other ToxPk to copy
 */
FriendId::FriendId(const FriendId& other) : ContactId(other) {}

FriendId::FriendId(const QString& strId) : ContactId(strId, lib::messenger::ChatType::Chat) {
    // 正则表达式模式，这里假设username不包含@，server不包含/
    //      QRegularExpression re("([^@]+)@([^/]+)(/[^/]+)?");
    //      // 匹配输入字符串
    //      QRegularExpressionMatch match = re.match(strId);
    //      // 检查是否匹配成功
    //      if (!match.hasMatch()) {
    //          qWarning() << "Unable to parse contactId:"<<strId;
    //          return;
    //      }

            //      resource = match.captured(3);
            //        if(resource.startsWith("/")){
            //            resource = resource.replace("/", "");
            //        }
}

/**
 * @brief Constructs a ToxPk from bytes.
 * @param rawId The bytes to construct the ToxPk from, will read exactly
 * TOX_PUBLIC_KEY_SIZE from the specified buffer.
 */
FriendId::FriendId(const ContactId& rawId) : ContactId(rawId) {}

FriendId::FriendId(const lib::messenger::IMContactId& fId)
        : ContactId(qstring(fId.toString())
                    , lib::messenger::ChatType::Chat) {}

bool FriendId::operator==(const FriendId& other) const {
    return toString() == other.toString();
}

bool FriendId::operator<(const FriendId& other) const {
    return ContactId::operator<(other);
}

/**
 * @brief Get size of public key in bytes.
 * @return Size of public key in bytes.
 */
int FriendId::getSize() const {
    return toString().size();
}

QByteArray FriendId::getByteArray() const {
    return toString().toUtf8();
}

QString FriendId::toString() const {
    return username + "@" + server;
}

PeerId::PeerId(const lib::messenger::IMPeerId& peerId) : FriendId(peerId) {
    resource = qstring(peerId.resource);
}

PeerId::PeerId(const QString& rawId) : FriendId(rawId) {
    auto match = JidMatch(rawId);
    if (!match.hasMatch()) {
        qWarning() << "Unable to parse id:" << rawId;
        return;
    }
    resource = match.captured(4);
}

bool PeerId::isValid() const {
    return FriendId::isValid() && !resource.isEmpty();
}

QString PeerId::toString() const {
    return FriendId::toString() + "/" + resource;
}
}  // namespace module::im
