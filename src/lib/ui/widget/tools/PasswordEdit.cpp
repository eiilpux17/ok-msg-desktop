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

#include "PasswordEdit.h"

#include <QCoreApplication>
namespace lib::ui {

PasswordEdit::EventHandler* PasswordEdit::eventHandler{nullptr};

PasswordEdit::PasswordEdit(QWidget* parent) : QLineEdit(parent), action(new QAction(this)) {
    setEchoMode(QLineEdit::Password);

#ifdef ENABLE_CAPSLOCK_INDICATOR
    action->setIcon(QIcon(":img/caps_lock.svg"));
    action->setToolTip(tr("CAPS-LOCK ENABLED"));
    addAction(action, QLineEdit::TrailingPosition);
#endif
}

PasswordEdit::~PasswordEdit() {
    unregisterHandler();
}

void PasswordEdit::registerHandler() {
#ifdef ENABLE_CAPSLOCK_INDICATOR
    if (!eventHandler) eventHandler = new EventHandler();
    if (!eventHandler->actions.contains(action)) eventHandler->actions.append(action);
#endif
}

void PasswordEdit::unregisterHandler() {
#ifdef ENABLE_CAPSLOCK_INDICATOR
    int idx;

    if (eventHandler && (idx = eventHandler->actions.indexOf(action)) >= 0) {
        eventHandler->actions.remove(idx);
        if (eventHandler->actions.isEmpty()) {
            delete eventHandler;
            eventHandler = nullptr;
        }
    }
#endif
}

void PasswordEdit::showEvent(QShowEvent*) {
#ifdef ENABLE_CAPSLOCK_INDICATOR
    action->setVisible(Platform::capsLockEnabled());
#endif
    registerHandler();
}

void PasswordEdit::hideEvent(QHideEvent*) {
    unregisterHandler();
}

#ifdef ENABLE_CAPSLOCK_INDICATOR
PasswordEdit::EventHandler::EventHandler() {
    QCoreApplication::instance()->installEventFilter(this);
}

PasswordEdit::EventHandler::~EventHandler() {
    QCoreApplication::instance()->removeEventFilter(this);
}

void PasswordEdit::EventHandler::updateActions() {
    bool caps = Platform::capsLockEnabled();

    for (QAction* action : actions) action->setVisible(caps);
}

bool PasswordEdit::EventHandler::eventFilter(QObject* obj, QEvent* event) {
    switch (event->type()) {
        case QEvent::WindowActivate:
        case QEvent::KeyRelease:
            updateActions();
            break;
        default:
            break;
    }

    return QObject::eventFilter(obj, event);
}
#endif  // ENABLE_CAPSLOCK_INDICATOR
}  // namespace lib::ui