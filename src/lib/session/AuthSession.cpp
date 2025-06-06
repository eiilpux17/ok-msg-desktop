﻿/*
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

#include "AuthSession.h"

#include <QMutexLocker>
#include <QTimer>

#include "lib/backend/PassportService.h"
#include "profile.h"

namespace lib::session {

AuthSession::AuthSession(QObject* parent)
        : QObject(parent)
        , m_networkManager(std::make_unique<lib::network::NetworkHttp>(this))
        , _status(Status::NONE)  //
{
    qRegisterMetaType<SignInInfo>("SignInInfo");
    qRegisterMetaType<LoginResult>("LoginResult");
}

AuthSession::~AuthSession() { qDebug() << "~AuthSession"; }

Status AuthSession::status() const { return _status; }

void AuthSession::doSignIn() {
    qDebug() << __func__;

    _status = Status::CONNECTING;
    LoginResult result{_status, tr("...")};
    emit loginResult(m_signInInfo, result);

    passportService = std::make_unique<lib::backend::PassportService>(m_signInInfo.stackUrl);
    passportService->signIn(
            m_signInInfo.account, m_signInInfo.password,
            [&](lib::backend::Res<lib::backend::SysToken>& res) {
                QString& username = res.data->username;
                qDebug() << "username:" << username;
                if (res.code != 0) {
                    _status = Status::FAILURE;
                    LoginResult result{_status, res.msg, 200};
                    emit loginResult(m_signInInfo, result);
                    return;
                }

                if (!res.success()) {
                    _status = Status::FAILURE;
                    LoginResult result{_status, res.msg, 200};
                    emit loginResult(m_signInInfo, result);
                    return;
                }

                qDebug() << "Set token:" << res.data;
                setToken(*res.data);

                okAccount = std::make_unique<ok::base::OkAccount>(username);

                _status = Status::SUCCESS;
                LoginResult result{Status::SUCCESS, ""};
                emit loginResult(m_signInInfo, result);
            },
            [&](int statusCode, const QString& msg) {
                _status = Status::FAILURE;
                LoginResult result{Status::FAILURE, msg, statusCode};
                emit loginResult(m_signInInfo, result);
            });
}

void AuthSession::doLogin(const SignInInfo& signInInfo) {
    qDebug() << __func__ << signInInfo.account << "stackUrl:" << signInInfo.stackUrl;

    QMutexLocker locker(&_mutex);
    m_signInInfo = signInInfo;
    if (_status == lib::session::Status::CONNECTING) {
        qDebug(("The connection is connecting."));
        return;
    }

    if (_status == lib::session::Status::SUCCESS) {
        qDebug(("The connection is connected."));
        return;
    }

    doSignIn();
}

void AuthSession::setToken(const lib::backend::SysToken& token) {
    m_token = token;
    m_signInInfo.username = token.username.toLower();
    QTimer::singleShot((token.expiresIn - 10) * 1000, this, &AuthSession::doRefreshToken);
    emit tokenSet();
}

void AuthSession::setRefreshToken(const lib::backend::SysRefreshToken& token) {
    m_token.accessToken = token.accessToken;
    m_token.refreshToken = token.refreshToken;
    m_token.expiresIn = token.expiresIn;

    QTimer::singleShot((token.expiresIn - 10) * 1000, this, &AuthSession::doRefreshToken);
    emit refreshTokenSet(token);
}

void AuthSession::doRefreshToken() {
    qDebug() << __func__;
    passportService = std::make_unique<lib::backend::PassportService>(m_signInInfo.stackUrl);
    passportService->refresh(m_token, [&](lib::backend::Res<lib::backend::SysRefreshToken>& res) {
        if (res.code != 0) {
            qWarning() << "Refresh token error" << res.msg;
            return;
        }
        setRefreshToken(*res.data);
    });
}

}  // namespace lib::session
