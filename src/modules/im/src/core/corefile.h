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

#ifndef COREFILE_H
#define COREFILE_H

#include "src/core/core.h"
#include "src/model/FriendId.h"
#include "src/model/Status.h"
#include "File.h"

#include "base/compatiblerecursivemutex.h"

#include <QHash>
#include <QMutex>
#include <QObject>
#include <QString>

#include <cstddef>
#include <cstdint>
#include <memory>

#include "lib/messenger/Messenger.h"
namespace module::im {

class CoreFile;

using CoreFilePtr = std::unique_ptr<CoreFile>;

class CoreFile : public QObject, public lib::messenger::FileHandler {
    Q_OBJECT
public:
    ~CoreFile();

    static CoreFilePtr makeCoreFile(Core* core);
    static CoreFile* getInstance();
    void start();
    void process();

    void handleAvatarOffer(QString friendId, QString fileId, bool accept);

    bool sendFile(QString friendId, const QFile& file);

    void pauseResumeFile(QString friendId, QString fileId);
    void cancelFileSend(QString friendId, QString fileId);

    void cancelFileRecv(QString friendId, QString fileId);
    void rejectFileRecvRequest(QString friendId, QString fileId);
    void acceptFileRecvRequest(QString friendId, QString fileId, QString path);

    unsigned corefileIterationInterval();

    /**
     * File handlers
     */

    void onFileRequest(const std::string& sId, const std::string& friendId,
                       const lib::messenger::File& file) override;
    void onFileRecvChunk(const std::string& sId, const std::string& friendId,
                         const std::string& fileId, int seq, const std::string& chunk) override;
    void onFileRecvFinished(const std::string& sId,
                            const std::string& friendId,
                            const std::string& fileId) override;
    // void onFileSendInfo(const std::string& friendId, const lib::messenger::File& file, int m_seq,
    // int m_sentBytes, bool end) override;

    void onFileSendAbort(const std::string& sId, const std::string& friendId,
                         const lib::messenger::File& file, int m_sentBytes) override;
    void onFileSendError(const std::string& sId, const std::string& friendId,
                         const lib::messenger::File& file, int m_sentBytes) override;

    void onFileStreamOpened(const std::string& sId, const std::string& friendId,
                            const lib::messenger::File& file) override;

    void onFileStreamClosed(const std::string& sId, const std::string& friendId,
                            const lib::messenger::File& file) override;

    void onFileStreamData(const std::string& sId, const std::string& friendId,
                          const lib::messenger::File& file, const std::string& data, int m_seq,
                          int m_sentBytes) override;

    void onFileStreamDataAck(const std::string& sId, const std::string& friendId,
                             const lib::messenger::File& file, uint32_t ack) override;
    void onFileStreamError(const std::string& sId, const std::string& friendId,
                           const lib::messenger::File& file, uint32_t m_sentBytes) override;

signals:
    void fileSendStarted(File& file);
    void fileSendWait(File& file);
    void fileSendFailed(QString friendId, const QString& fname);

    void fileReceiveRequested(File& file);
    void fileTransferAccepted(File& file);
    void fileTransferCancelled(File& file);
    void fileTransferFinished(File& file);
    void fileTransferNoExisting(const QString& friendId, const QString& fileId);

    void fileTransferPaused(File& file);
    void fileTransferInfo(File& file);
    void fileTransferRemotePausedUnpaused(File& file, bool paused);
    void fileTransferBrokenUnbroken(File& file, bool broken);
    void fileNameChanged(const FriendId& friendPk);

    void fileUploadFinished(const QString& path);
    void fileDownloadFinished(const QString& path);

private:
    CoreFile(Core*);

    File* findFile(QString fileId);
    const QString& addFile(File& file);
    void removeFile(QString fileId);

    static QString getFriendKey(const QString& friendId, QString fileId) {
        return friendId + "-" + fileId;
    }

    lib::messenger::File buildHandlerFile(const File* toxFile);

    static void onFileReceiveCallback(lib::messenger::Messenger* tox, QString friendId,
                                      QString fileId, uint32_t kind, uint64_t filesize,
                                      const uint8_t* fname, size_t fnameLen, void* vCore);
    void onFileControlCallback(lib::messenger::Messenger* tox,
                               QString friendId,
                               QString fileId,
                               lib::messenger::FileControl control);
    static void onFileDataCallback(lib::messenger::Messenger* tox, QString friendId, QString fileId,
                                   uint64_t pos, size_t length, void* vCore);
    static void onFileRecvChunkCallback(lib::messenger::Messenger* tox, QString friendId,
                                        QString fileId, uint64_t position, const uint8_t* data,
                                        size_t length, void* vCore);

    static QString getCleanFileName(QString filename);

    Core* core;
    std::unique_ptr<QThread> thread;
    QHash<QString, File> fileMap;
    lib::messenger::Messenger* messenger;
    lib::messenger::MessengerFile* messengerFile;
    CompatibleRecursiveMutex* coreLoopLock = nullptr;
};
}  // namespace module::im
#endif  // COREFILE_H
