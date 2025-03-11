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
// Created by gaojie on 23-8-19.
//

#include "OkAccount.h"

namespace ok::base {

OkAccount::OkAccount(const QString& username_,const QString& password_)
        : username(username_) , password(password_){}

void OkAccount::setUsername(const QString &username_)
{
    username = username_;
}

void OkAccount::setPassword(const QString &password_)
{
    password = password_;
}


} // namespace ok::base

