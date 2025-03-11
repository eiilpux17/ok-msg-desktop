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

#include "GroupId.h"

#include <QByteArray>
#include <QString>

#include <cassert>
namespace module::im {

/**
 * @brief The Group id
 */
GroupId::GroupId() : ContactId() {}

GroupId::GroupId(const GroupId& other) : ContactId(other.toString(), lib::messenger::ChatType::GroupChat) {}

GroupId::GroupId(const QString& rawId) : ContactId(rawId, lib::messenger::ChatType::GroupChat) {}

GroupId::GroupId(const ContactId& contactId) : ContactId(contactId) {}

/**
 * @brief Get size of public id in bytes.
 * @return Size of public id in bytes.
 */
int GroupId::getSize() const {
    return toString().size();
}

bool GroupId::operator==(const GroupId& other) const {
    return ContactId::operator==(other);
}

bool GroupId::operator<(const GroupId& other) const {
    return toString() < other.toString();
}
}  // namespace module::im
