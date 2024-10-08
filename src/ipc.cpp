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

#include "ipc.h"

#include <QCoreApplication>
#include <QDebug>
#include <QThread>
#include <ctime>
#include <random>

/**
 * @var time_t IPC::lastEvent
 * @brief When last event was posted.
 *
 * @var time_t IPC::lastProcessed
 * @brief When processIpcEvents() ran last time
 */

/**
 * @class IPC
 * @brief Inter-process communication
 */
namespace ok {
#define IPC_PROTOCOL_VERSION "1"

IPC::IPC(uint32_t profileId, QObject* parent)
        : QObject(parent)
        , profileId{profileId}
        , globalMemory{APPLICATION_ID "-" IPC_PROTOCOL_VERSION} {
    qRegisterMetaType<IPCEventHandler>("IPCEventHandler");

    timer.setInterval(EVENT_TIMER_MS);
    timer.setSingleShot(true);
    connect(&timer, &QTimer::timeout, this, &IPC::processIpcEvents);
    timer.start();

    // The first started instance gets to manage the shared memory by taking ownership
    // Every time it processes events it updates the global shared timestamp "lastProcessed"
    // If the timestamp isn't updated, that's a timeout and someone else can take ownership
    // This is a safety measure, in case one of the clients crashes
    // If the owner exits normally, it can set the timestamp to 0 first to immediately give
    // ownership

    std::default_random_engine randEngine((std::random_device())());
    std::uniform_int_distribution<uint64_t> distribution;
    globalId = distribution(randEngine);

    qDebug() << "Our global IPC ID is " << globalId;
    if (globalMemory.create(sizeof(IPCMemory))) {
        if (globalMemory.lock()) {
            IPCMemory* mem = global();
            memset(mem, 0, sizeof(IPCMemory));
            mem->globalId = globalId;
            mem->lastProcessed = 0;  // time(nullptr);
            globalMemory.unlock();
        } else {
            qWarning() << "Couldn't lock to take ownership";
        }
    } else if (globalMemory.attach()) {
        qDebug() << "Attaching to the global shared memory";
    } else {
        qDebug() << "Failed to attach to the global shared memory, giving up. Error:"
                 << globalMemory.error();
        return;  // We won't be able to do any IPC without being attached, let's get outta here
    }
}

IPC::~IPC() {
    if (!globalMemory.lock()) {
        qWarning() << "Failed to lock in ~IPC";
        return;
    }

    if (isCurrentOwnerNoLock()) {
        global()->globalId = 0;
    }
    globalMemory.unlock();
}

/**
 * @brief Post IPC event.
 * @param name Name to set in IPC event.
 * @param data Data to set in IPC event (default QByteArray()).
 * @param dest Settings::getCurrentProfileId() or 0 (main instance, default).
 * @return Time the event finished or 0 on error.
 */
time_t IPC::postEvent(const QString& name, const QByteArray& data, uint32_t dest) {
    QByteArray binName = name.toUtf8();
    if (binName.length() > (int32_t)sizeof(IPCEvent::name)) {
        return 0;
    }

    if (data.length() > (int32_t)sizeof(IPCEvent::data)) {
        return 0;
    }

    if (!globalMemory.lock()) {
        qDebug() << "Failed to lock in postEvent()";
        return 0;
    }

    IPCEvent* evt = nullptr;
    IPCMemory* mem = global();
    time_t result = 0;

    for (uint32_t i = 0; !evt && i < EVENT_QUEUE_SIZE; ++i) {
        if (mem->events[i].posted == 0) {
            evt = &mem->events[i];
        }
    }

    if (evt) {
        memset(evt, 0, sizeof(IPCEvent));
        memcpy(evt->name, binName.constData(), binName.length());
        memcpy(evt->data, data.constData(), data.length());
        mem->lastEvent = evt->posted = result = qMax(mem->lastEvent + 1, time(nullptr));
        evt->dest = dest;
        //        evt->sender = getpid();
        qDebug() << "postEvent " << name << "to" << dest;
    }

    globalMemory.unlock();
    return result;
}

bool IPC::isCurrentOwner() {
    if (globalMemory.lock()) {
        const bool isOwner = isCurrentOwnerNoLock();
        globalMemory.unlock();
        return isOwner;
    } else {
        qWarning() << "isCurrentOwner failed to lock, returning false";
        return false;
    }
}

/**
 * @brief Register a handler for an IPC event
 * @param handler The handler callback. Should not block for more than a second, at worst
 */
void IPC::registerEventHandler(const QString& name, IPCEventHandler handler) {
    eventHandlers[name] = handler;
}

bool IPC::isEventAccepted(time_t time) {
    bool result = false;
    if (!globalMemory.lock()) {
        return result;
    }

    auto last = global()->lastProcessed;
    qDebug() << "last:" << last;

    if (difftime(last, time) > 0) {
        IPCMemory* mem = global();
        for (uint32_t i = 0; i < EVENT_QUEUE_SIZE; ++i) {
            if (mem->events[i].posted == time && mem->events[i].processed) {
                result = mem->events[i].accepted;
                break;
            }
        }
    }
    globalMemory.unlock();

    return result;
}

bool IPC::waitUntilAccepted(time_t postTime, int32_t timeout /*=-1*/) {
    bool result = false;
    time_t start = time(nullptr);
    forever {
        result = isEventAccepted(postTime);
        if (result || (timeout > 0 && difftime(time(nullptr), start) >= timeout)) {
            break;
        }

        qApp->processEvents();
        QThread::msleep(0);
    }
    return result;
}

bool IPC::isAttached() const { return globalMemory.isAttached(); }

void IPC::setProfileId(uint32_t profileId) { this->profileId = profileId; }

/**
 * @brief Only called when global memory IS LOCKED.
 * @return nullptr if no evnts present, IPC event otherwise
 */
IPC::IPCEvent* IPC::fetchEvent() {
    IPCMemory* mem = global();
    for (uint32_t i = 0; i < EVENT_QUEUE_SIZE; ++i) {
        IPCEvent* evt = &mem->events[i];

        // Garbage-collect events that were not processed in EVENT_GC_TIMEOUT
        // and events that were processed and EVENT_GC_TIMEOUT passed after
        // so sending instance has time to react to those events.
        if ((evt->processed && difftime(time(nullptr), evt->processed) > EVENT_GC_TIMEOUT) ||
            (!evt->processed && difftime(time(nullptr), evt->posted) > EVENT_GC_TIMEOUT)) {
            memset(evt, 0, sizeof(IPCEvent));
        }

        // #TODO need to modify this
        if (evt->posted && !evt->processed /*&& evt->sender != getpid() */ &&
            (evt->dest == profileId || (evt->dest == 0 && isCurrentOwnerNoLock()))) {
            return evt;
        }
    }

    return nullptr;
}

bool IPC::runEventHandler(IPCEventHandler handler, const QByteArray& arg) {
    bool result = false;
    if (QThread::currentThread() == qApp->thread()) {
        result = handler(arg);
    } else {
        QMetaObject::invokeMethod(this, "runEventHandler", Qt::BlockingQueuedConnection,
                                  Q_RETURN_ARG(bool, result), Q_ARG(IPCEventHandler, handler),
                                  Q_ARG(const QByteArray&, arg));
    }

    return result;
}

void IPC::processIpcEvents() {
    if (!globalMemory.lock()) {
        return;
    }
    timer.start();

    IPCMemory* mem = global();
    if (mem->globalId == globalId) {
        // We're the owner, let's process those events
        mem->lastProcessed = time(nullptr);
    } else {
        // Only the owner processes events. But if the previous owner's dead, we can take
        // ownership now
        if (difftime(time(nullptr), mem->lastProcessed) >= OWNERSHIP_TIMEOUT_S) {
            qDebug() << "Previous owner timed out, taking ownership" << mem->globalId << "->"
                     << globalId;
            // Ignore events that were not meant for this instance
            memset(mem, 0, sizeof(IPCMemory));
            mem->globalId = globalId;
            mem->lastProcessed = time(nullptr);
        }
        // Non-main instance is limited to events destined for specific profile it runs
    }

    while (IPCEvent* evt = fetchEvent()) {
        QString name = QString::fromUtf8(evt->name);
        auto it = eventHandlers.find(name);
        if (it != eventHandlers.end()) {
            evt->accepted = runEventHandler(it.value(), evt->data);
            qDebug() << "Processed event:" << name << "posted:" << evt->posted
                     << "accepted:" << evt->accepted;
            if (evt->dest == 0) {
                // Global events should be processed only by instance that accepted event.
                // Otherwise global
                // event would be consumed by very first instance that gets to check it.
                if (evt->accepted) {
                    evt->processed = time(nullptr);
                }
            } else {
                evt->processed = time(nullptr);
            }
        } else {
            qDebug() << "Received event:" << name << "without handler";
            qDebug() << "Available handlers:" << eventHandlers.keys();
        }
    }

    globalMemory.unlock();
}

/**
 * @brief Only called when global memory IS LOCKED.
 * @return true if owner, false if not owner or if error
 */
bool IPC::isCurrentOwnerNoLock() {
    const void* const data = globalMemory.data();
    if (!data) {
        qWarning() << "isCurrentOwnerNoLock failed to access the memory, returning false";
        return false;
    }
    return (*static_cast<const uint64_t*>(data) == globalId);
}

IPC::IPCMemory* IPC::global() { return static_cast<IPCMemory*>(globalMemory.data()); }

bool IPC::isAlive() {
    auto mem = global();
    if (!mem) {
        return false;
    }

    time_t last = mem->lastProcessed;
    struct tm* timeinfo = localtime(&last);
    char buffer[80] = {0};
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo);
    qDebug() << "lastProcessedTime:" << buffer;

    time_t current = time(nullptr);
    timeinfo = localtime(&current);
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo);
    qDebug() << "currentTime:" << buffer;

    int diff = difftime(current, last);
    if (diff >= OWNERSHIP_TIMEOUT_S) {
        return false;
    }

    return true;
}

}  // namespace ok
