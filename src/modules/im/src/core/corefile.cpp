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

#include "corefile.h"
#include <base/utils.h>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QMutexLocker>
#include <QRegularExpression>
#include <QThread>

#include <cassert>
#include <memory>
#include "base/compatiblerecursivemutex.h"
#include "base/uuid.h"
#include "core.h"

#include "Bus.h"
#include "application.h"
#include "src/model/status.h"
#include "src/persistence/settings.h"
#include "toxfile.h"
#include "toxstring.h"

namespace module::im {

/**
 * @class CoreFile
 * @brief Manages the file transfer service of toxcore
 */
static CoreFile* instance = nullptr;
CoreFilePtr CoreFile::makeCoreFile(Core* core) {
    assert(core != nullptr);
    CoreFilePtr result = CoreFilePtr{new CoreFile{core}};
    //  connect(core, &Core::friendStatusChanged, result.get(),
    //          &CoreFile::onConnectionStatusChanged);
    instance = result.get();
    return result;
}

CoreFile::CoreFile(Core* core)
        : core{core}, messenger{nullptr}, messengerFile{nullptr}, thread{new QThread{this}} {
    qDebug() << __func__;

    qRegisterMetaType<ToxFile>("ToxFile");
    qRegisterMetaType<ToxFile>("ToxFile&");

    thread->setObjectName("CoreAV");
    connect(thread.get(), &QThread::started, this, &CoreFile::process);
    moveToThread(thread.get());
}

CoreFile::~CoreFile() {
    qDebug() << __func__;
    thread->quit();
    thread->wait();
    thread->deleteLater();
}

CoreFile* CoreFile::getInstance() {
    assert(instance);
    return instance;
}
void CoreFile::start() {
    qDebug() << __func__;
    thread->start();
}

void CoreFile::process() {
    qDebug() << __func__;

    assert(QThread::currentThread() == thread.get());

    messenger = core->getMessenger();
    messengerFile = new lib::messenger::MessengerFile(core->getMessenger());
    messengerFile->addHandler(this);

    emit ok::Application::Instance() -> bus()->coreFileChanged(this);
}
/**
 * @brief Get corefile iteration interval.
 *
 * tox_iterate calls to get good file transfer performances
 * @return The maximum amount of time in ms that Core should wait between two
 * tox_iterate() calls.
 */
unsigned CoreFile::corefileIterationInterval() {
    /*
       Sleep at most 1000ms if we have no FT, 10 for user FTs
       There is no real difference between 10ms sleep and 50ms sleep when it
       comes to CPU usage – just keep the CPU usage low when there are no file
       transfers, and speed things up when there is an ongoing file transfer.
    */
    constexpr unsigned fileInterval = 10, idleInterval = 1000;

    for (ToxFile& file : fileMap) {
        if (file.status == FileStatus::TRANSMITTING) {
            return fileInterval;
        }
    }
    return idleInterval;
}

bool CoreFile::sendFile(QString friendId, const QFile& file_) {
    qDebug() << __func__ << friendId << file_.fileName();

    QMutexLocker{coreLoopLock};

    auto sender = qstring(messenger->getSelfId().toString());
    auto fileInfo = QFileInfo(file_);
    auto fileId = ok::base::UUID::make();
    auto sId = ok::base::UUID::make();

    auto file = ToxFile{sender,
                        friendId,
                        sId,
                        fileId,
                        fileInfo.fileName(),
                        fileInfo.filePath(),
                        (quint64)file_.size(),
                        (quint64)file_.size(),
                        FileStatus::INITIALIZING,
                        FileDirection::SENDING};

    addFile(file);
    qDebug() << "The file info is:" << file.toString();
    bool y = messengerFile->fileSendToFriend(friendId.toStdString(), file.toIMFile());
    if (y) {
        emit fileSendStarted(file);
    }
    return y;
}

void CoreFile::pauseResumeFile(QString friendId, QString fileId) {
    qDebug() << "暂不支持";
    //  QMutexLocker{coreLoopLock};
    //
    //  ToxFile *file = findFile(receiver, fileId);
    //  if (!file) {
    //    qWarning("pauseResumeFileSend: No such file in queue");
    //    return;
    //  }
    //
    //  if (file->status != FileStatus::TRANSMITTING &&
    //      file->status != FileStatus::PAUSED) {
    //    qWarning() << "pauseResumeFileSend: File is stopped";
    //    return;
    //  }
    //
    //  file->pauseStatus.localPauseToggle();
    //
    //  if (file->pauseStatus.paused()) {
    //    file->status = FileStatus::PAUSED;
    //    emit fileTransferPaused(*file);
    //  } else {
    //    file->status = FileStatus::TRANSMITTING;
    //    emit fileTransferAccepted(*file);
    //  }

    //  if (file->pauseStatus.localPaused()) {
    //    tox_file_control(tox, file->receiver, file->fileNum,
    //    TOX_FILE_CONTROL_PAUSE,
    //                     nullptr);
    //  } else {
    //    tox_file_control(tox, file->receiver, file->fileNum,
    //                     TOX_FILE_CONTROL_RESUME, nullptr);
    //  }
}

void CoreFile::cancelFileSend(QString friendId, QString fileId) {
    qDebug() << __func__ << "file" << fileId;
    QMutexLocker{coreLoopLock};

    ToxFile* file = findFile(fileId);
    if (!file) {
        qWarning("cancelFileSend: No such file in queue");
        emit fileTransferNoExisting(friendId, fileId);
        return;
    }

    file->status = FileStatus::CANCELED;
    messengerFile->fileCancel(file->fileId.toStdString());

    emit fileTransferCancelled(*file);
    removeFile(fileId);
}

void CoreFile::cancelFileRecv(QString friendId, QString fileId) {
    QMutexLocker{coreLoopLock};

    ToxFile* file = findFile(fileId);
    if (!file) {
        qWarning("cancelFileRecv: No such file in queue");
        return;
    }
    file->status = FileStatus::CANCELED;
    messengerFile->fileRejectRequest(friendId.toStdString(), file->toIMFile());
    emit fileTransferCancelled(*file);
    removeFile(fileId);
}

void CoreFile::rejectFileRecvRequest(QString friendId, QString fileId) {
    QMutexLocker{coreLoopLock};

    ToxFile* file = findFile(fileId);
    if (!file) {
        qWarning("cancelFileRecv: No such file in queue");
        return;
    }
    file->status = FileStatus::CANCELED;
    messengerFile->fileRejectRequest(friendId.toStdString(), file->toIMFile());
    emit fileTransferCancelled(*file);
    removeFile(fileId);
}

void CoreFile::acceptFileRecvRequest(QString friendId, QString fileId, QString path) {
    QMutexLocker{coreLoopLock};

    qInfo() << __func__ << "fileId:" << fileId << "friendId:" << friendId << "path:" << path;

    ToxFile* file = findFile(fileId);
    if (!file) {
        qWarning("acceptFileRecvRequest: No such file in queue");
        return;
    }
    file->setFilePath(path);
    if (!file->open(true)) {
        qWarning() << "acceptFileRecvRequest: Unable to open file";
        return;
    }
    file->status = FileStatus::TRANSMITTING;
    messengerFile->fileAcceptRequest(friendId.toStdString(), file->toIMFile());
    emit fileTransferAccepted(*file);
}

ToxFile* CoreFile::findFile(QString fileId) {
    //  qDebug() << __func__ << "fileId:" << fileId;
    QMutexLocker{coreLoopLock};

    if (fileMap.contains(fileId)) {
        return &fileMap[fileId];
    }

    qWarning() << __func__ << "fileId" << fileId << "doesn't exist";
    return nullptr;
}

const QString& CoreFile::addFile(ToxFile& file) {
    qDebug() << __func__ << "file:" << file.toString();
    assert(!file.fileId.isEmpty());

    QMutexLocker{coreLoopLock};

    auto hash = fileMap.insert(file.fileId, file);
    qDebug() << "File has been cached, fileId:" << file.fileId;
    return hash.key();
}

void CoreFile::removeFile(QString fileId) {
    qDebug() << __func__ << "fileId:" << fileId;
    QMutexLocker{coreLoopLock};
    if (!fileMap.contains(fileId)) {
        qWarning() << "removeFile: No such file in queue";
        return;
    }

    fileMap.remove(fileId);
}

QString CoreFile::getCleanFileName(QString filename) {
    QRegularExpression regex{QStringLiteral(R"([<>:"/\\|?])")};
    filename.replace(regex, "_");

    return filename;
}

void CoreFile::onFileReceiveCallback(lib::messenger::Messenger* tox, QString friendId,
                                     QString fileId, uint32_t kind, uint64_t filesize,
                                     const uint8_t* fname, size_t fnameLen, void* vCore) {
    //  Core *core = static_cast<Core *>(vCore);
    //  CoreFile *coreFile = core->getCoreFile();
    //  auto filename = ToxString(fname, fnameLen);
    //  const ToxPk friendPk = core->getFriendPublicKey(receiver);
    //
    //  if (kind == TOX_FILE_KIND_AVATAR) {
    //    if (!filesize) {
    //      qDebug() << QString("Received empty avatar request %1:%2")
    //                      .arg(receiver)
    //                      .arg(fileId);
    //      // Avatars of size 0 means explicitely no avatar
    //      tox_file_control(tox, receiver, fileId, TOX_FILE_CONTROL_CANCEL,
    //      nullptr); emit
    //      core->friendAvatarRemoved(core->getFriendPublicKey(receiver)); return;
    //    } else {
    //      if (!ToxClientStandards::IsValidAvatarSize(filesize)) {
    //        qWarning() << QString("Received avatar request from %1 with size
    //        %2.")
    //                              .arg(receiver)
    //                              .arg(filesize) +
    //                          QString(" The max size allowed for avatars is %3.
    //                          "
    //                                  "Cancelling transfer.")
    //                              .arg(ToxClientStandards::MaxAvatarSize);
    //        tox_file_control(tox, receiver, fileId, TOX_FILE_CONTROL_CANCEL,
    //                         nullptr);
    //        return;
    //      }
    //      static_assert(TOX_HASH_LENGTH <= TOX_FILE_ID_LENGTH,
    //                    "TOX_HASH_LENGTH > TOX_FILE_ID_LENGTH!");
    //      uint8_t avatarHash[TOX_FILE_ID_LENGTH];
    //      tox_file_get_file_id(tox, receiver, fileId, avatarHash, nullptr);
    //      QByteArray avatarBytes{
    //          static_cast<const char *>(static_cast<const void *>(avatarHash)),
    //          TOX_HASH_LENGTH};
    //      emit core->fileAvatarOfferReceived(receiver, fileId, avatarBytes);
    //      return;
    //    }
    //  } else {
    //    const auto cleanFileName =
    //        CoreFile::getCleanFileName(filename.getQString());
    //    if (cleanFileName != filename.getQString()) {
    //      qDebug() << QStringLiteral("Cleaned filename");
    //      filename = ToxString(cleanFileName);
    //      emit coreFile->fileNameChanged(friendPk);
    //    } else {
    //      qDebug() << QStringLiteral("filename already clean");
    //    }
    //    qDebug() << QString("Received file request %1:%2 kind %3")
    //                    .arg(receiver)
    //                    .arg(fileId)
    //                    .arg(kind);
    //  }
    //
    //  ToxFile file{fileId, receiver, filename.getBytes(), "",
    //  FileStatus::RECEIVING}; file.fileSize = filesize; file.fileKind = kind;
    //  file.resumeFileId.resize(TOX_FILE_ID_LENGTH);
    //  tox_file_get_file_id(tox, receiver, fileId,
    //                       (uint8_t *)file.resumeFileId.data(), nullptr);
    //  coreFile->addFile(receiver, fileId, file);
    //  if (kind != TOX_FILE_KIND_AVATAR) {
    //    emit coreFile->fileReceiveRequested(file);
    //  }
}

// TODO(sudden6): This whole method is a mess but needed to get stuff working
// for now
void CoreFile::handleAvatarOffer(QString friendId, QString fileId, bool accept) {
    //  if (!accept) {
    //    // If it's an avatar but we already have it cached, cancel
    //    qDebug() << QString("Received avatar request %1:%2, reject, since we
    //    have "
    //                        "it in cache.")
    //                    .arg(receiver)
    //                    .arg(fileId);
    //    tox_file_control(tox, receiver, fileId, TOX_FILE_CONTROL_CANCEL,
    //    nullptr); return;
    //  }
    //
    //  // It's an avatar and we don't have it, autoaccept the transfer
    //  qDebug()
    //      << QString(
    //             "Received avatar request %1:%2, accept, since we don't have it
    //             " "in cache.") .arg(receiver) .arg(fileId);
    //  tox_file_control(tox, receiver, fileId, TOX_FILE_CONTROL_RESUME, nullptr);
    //
    //  ToxFile file{fileId, receiver, "<avatar>", "", FileStatus::RECEIVING};
    //  file.fileSize = 0;
    //  file.fileKind = TOX_FILE_KIND_AVATAR;
    //  file.resumeFileId.resize(TOX_FILE_ID_LENGTH);
    //  tox_file_get_file_id(tox, receiver, fileId,
    //                       (uint8_t *)file.resumeFileId.data(), nullptr);
    //  addFile(receiver, fileId, file);
}

void CoreFile::onFileRequest(const std::string& sId, const std::string& from,
                             const lib::messenger::File& file) {
    qDebug() << __func__ << file.name.c_str() << "from" << from.c_str();
    auto receiver = messenger->getSelfId().toString();
    ToxFile toxFile(qstring(from), qstring(receiver), qstring(file.sId), file);
    addFile(toxFile);
    qDebug() << "file:" << toxFile.toString();
    emit fileReceiveRequested(toxFile);
}

void CoreFile::onFileControlCallback(lib::messenger::Messenger* tox,
                                     QString friendId,
                                     QString fileId,
                                     lib::messenger::FileControl control) {
    ToxFile* file = findFile(fileId);
    if (!file) {
        qWarning("onFileControlCallback: No such file in queue");
        return;
    }

    if (control == lib::messenger::FileControl::CANCEL) {
        file->status = FileStatus::CANCELED;
        emit fileTransferCancelled(*file);
        removeFile(fileId);
    } else if (control == lib::messenger::FileControl::PAUSE) {
        file->status = FileStatus::PAUSED;
        emit fileTransferRemotePausedUnpaused(*file, true);
    } else if (control == lib::messenger::FileControl::RESUME) {
        if (file->direction == FileDirection::SENDING) {
            file->status = FileStatus::TRANSMITTING;
            emit fileTransferRemotePausedUnpaused(*file, false);
        }
    } else {
        qWarning() << "Unhandled file control " << (int)control << " for file " << friendId << ':'
                   << fileId;
    }
}

void CoreFile::onFileDataCallback(lib::messenger::Messenger* tox, QString friendId, QString fileId,
                                  uint64_t pos, size_t length, void* vCore) {
    //  Core *core = static_cast<Core *>(vCore);
    //  CoreFile *coreFile = core->getCoreFile();
    //  ToxFile *file = coreFile->findFile(receiver, fileId);
    //  if (!file) {
    //    qWarning("onFileDataCallback: No such file in queue");
    //    return;
    //  }
    //
    //  // If we reached EOF, ack and cleanup the transfer
    //  if (!length) {
    //    file->status = FileStatus::FINISHED;
    //    if (file->fileKind != TOX_FILE_KIND_AVATAR) {
    //      emit coreFile->fileTransferFinished(*file);
    //      emit coreFile->fileUploadFinished(file->filePath);
    //    }
    //    coreFile->removeFile(receiver, fileId);
    //    return;
    //  }
    //
    //  std::unique_ptr<uint8_t[]> data(new uint8_t[length]);
    //  int64_t nread;
    //
    //  if (file->fileKind == TOX_FILE_KIND_AVATAR) {
    //    QByteArray chunk = file->avatarData.mid(pos, length);
    //    nread = chunk.size();
    //    memcpy(data.get(), chunk.data(), nread);
    //  } else {
    //    file->file->seek(pos);
    //    nread = file->file->read((char *)data.get(), length);
    //    if (nread <= 0) {
    //      qWarning("onFileDataCallback: Failed to read from file");
    //      file->status = FileStatus::CANCELED;
    //      emit coreFile->fileTransferCancelled(*file);
    //      tox_file_send_chunk(tox, receiver, fileId, pos, nullptr, 0, nullptr);
    //      coreFile->removeFile(receiver, fileId);
    //      return;
    //    }
    //    file->bytesSent += length;
    //    file->hashGenerator->addData((const char *)data.get(), length);
    //  }
    //
    //  if (!tox_file_send_chunk(tox, receiver, fileId, pos, data.get(), nread,
    //                           nullptr)) {
    //    qWarning("onFileDataCallback: Failed to send data chunk");
    //    return;
    //  }
    //  if (file->fileKind != TOX_FILE_KIND_AVATAR) {
    //    emit coreFile->fileTransferInfo(*file);
    //  }
}

void CoreFile::onFileSendAbort(const std::string& sId,
                               const std::string& friendId,
                               const lib::messenger::File& file_,
                               int m_sentBytes) {
    qDebug() << __func__ << file_.id.c_str();

    ToxFile* file = findFile(file_.id.c_str());
    if (!file) {
        qWarning() << __func__ << "No such file in queue";
        return;
    }
    file->bytesSent = m_sentBytes;
    file->status = FileStatus::CANCELED;
    emit fileTransferCancelled(*file);
    removeFile(file->fileId);
}

void CoreFile::onFileSendError(const std::string& sId,
                               const std::string& friendId,
                               const lib::messenger::File& file_,
                               int m_sentBytes) {
    qWarning() << __func__;
    ToxFile* file = findFile(file_.id.c_str());
    if (!file) {
        qWarning() << __func__ << "No such file in queue";
        return;
    }
    file->bytesSent = m_sentBytes;
    file->status = FileStatus::CANCELED;
    removeFile(file->fileId);
    emit fileSendFailed(friendId.c_str(), file->fileName);
}

void CoreFile::onFileStreamOpened(const std::string& sId, const std::string& friendId,
                                  const lib::messenger::File& file) {}

void CoreFile::onFileStreamClosed(const std::string& sId,
                                  const std::string& friendId,
                                  const lib::messenger::File& file_) {
    qDebug() << __func__ << friendId.c_str() << "file" << file_.id.c_str();

    ToxFile* file = findFile(file_.id.c_str());
    if (!file) {
        qWarning() << __func__ << "No such file in queue";
        return;
    }

    file->status = FileStatus::FINISHED;
    emit fileTransferFinished(*file);
    emit fileUploadFinished(file->filePath);
    removeFile(file->fileId);
}

void CoreFile::onFileStreamData(const std::string& sId, const std::string& friendId,
                                const lib::messenger::File& file_, const std::string& data,
                                int m_seq, int m_sentBytes) {
    qDebug() << __func__ << friendId.c_str() << "file" << file_.id.c_str() << "seq" << m_seq
             << "sentBytes" << m_sentBytes;

    ToxFile* file = findFile(file_.id.c_str());
    if (!file) {
        qWarning() << __func__ << "No such file in queue";
        return;
    }

    file->bytesSent += m_sentBytes;
    file->status = FileStatus::TRANSMITTING;
    emit fileTransferInfo(*file);
}

void CoreFile::onFileStreamDataAck(const std::string& sId, const std::string& friendId,
                                   const lib::messenger::File& file, uint32_t ack) {}

void CoreFile::onFileStreamError(const std::string& sId, const std::string& friendId,
                                 const lib::messenger::File& file, uint32_t m_sentBytes) {}

void CoreFile::onFileRecvChunkCallback(lib::messenger::Messenger* tox, QString friendId,
                                       QString fileId, uint64_t position, const uint8_t* data,
                                       size_t length, void* vCore) {
    //  Core *core = static_cast<Core *>(vCore);
    //  CoreFile *coreFile = core->getCoreFile();
    //  ToxFile *file = coreFile->findFile(receiver, fileId);
    //  if (!file) {
    //    qWarning("onFileRecvChunkCallback: No such file in queue");
    //    tox_file_control(tox, receiver, fileId, TOX_FILE_CONTROL_CANCEL,
    //    nullptr); return;
    //  }
    //
    //  if (file->bytesSent != position) {
    //    qWarning("onFileRecvChunkCallback: Received a chunk out-of-order,
    //    aborting "
    //             "transfer");
    //    if (file->fileKind != TOX_FILE_KIND_AVATAR) {
    //      file->status = FileStatus::CANCELED;
    //      emit coreFile->fileTransferCancelled(*file);
    //    }
    //    tox_file_control(tox, receiver, fileId, TOX_FILE_CONTROL_CANCEL,
    //    nullptr); coreFile->removeFile(receiver, fileId); return;
    //  }
    //
    //  if (!length) {
    //    file->status = FileStatus::FINISHED;
    //    if (file->fileKind == TOX_FILE_KIND_AVATAR) {
    //      QPixmap pic;
    //      pic.loadFromData(file->avatarData);
    //      if (!pic.isNull()) {
    //        qDebug() << "Got" << file->avatarData.size()
    //                 << "bytes of avatar data from" << receiver;
    //        emit core->friendAvatarChanged(core->getFriendPublicKey(receiver),
    //                                       file->avatarData);
    //      }
    //    } else {
    //      emit coreFile->fileTransferFinished(*file);
    //      emit coreFile->fileDownloadFinished(file->filePath);
    //    }
    //    coreFile->removeFile(receiver, fileId);
    //    return;
    //  }
    //
    //  if (file->fileKind == TOX_FILE_KIND_AVATAR) {
    //    file->avatarData.append((char *)data, length);
    //  } else {
    //    file->file->write((char *)data, length);
    //  }
    //  file->bytesSent += length;
    //  file->hashGenerator->addData((const char *)data, length);
    //
    //  if (file->fileKind != TOX_FILE_KIND_AVATAR) {
    //    emit coreFile->fileTransferInfo(*file);
    //  }
}

void CoreFile::onFileRecvChunk(const std::string& sId,
                               const std::string& friendId,
                               const std::string& fileId,
                               int seq,
                               const std::string& chunk) {
    ToxFile* file = findFile(qstring(fileId));
    if (!file) {
        qWarning() << ("No such file in queue");
        return;
    }

    if (file->bytesSent > file->fileSize) {
        qWarning() << ("Received a chunk out-of-order, aborting transfer");

        file->status = FileStatus::CANCELED;
        emit fileTransferCancelled(*file);

        messengerFile->fileCancel(fileId);
        removeFile(qstring(fileId));
        return;
    }

    QByteArray buf = QByteArray::fromStdString(chunk);
    file->file->write(buf);
    //    file->hashGenerator->addData(buf);
    file->bytesSent += buf.size();
    file->status = FileStatus::TRANSMITTING;
    qDebug() << "Received bytes" << buf.size() << "/" << file->fileSize;
    emit fileTransferInfo(*file);
}

void CoreFile::onFileRecvFinished(const std::string& sId,
                                  const std::string& friendId,
                                  const std::string& fileId) {
    ToxFile* file = findFile(qstring(fileId));
    if (!file) {
        qWarning() << __func__ << "No such file in queue";
        return;
    }

    file->status = FileStatus::FINISHED;

    messengerFile->fileFinishTransfer(friendId, file->sId.toStdString());

    emit fileTransferFinished(*file);
    emit fileDownloadFinished(file->filePath);

    removeFile(fileId.c_str());
}
}  // namespace module::im