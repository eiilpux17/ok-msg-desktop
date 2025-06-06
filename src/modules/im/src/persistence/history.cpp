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

#include <QDebug>
#include <cassert>

#include "history.h"
#include "lib/storage/db/rawdatabase.h"

namespace {

static constexpr int SCHEMA_VERSION = 4;

// 4, 增加消息会话

bool createCurrentSchema(lib::db::RawDatabase& db) {
    QVector<lib::db::RawDatabase::Query> queries;
    queries += lib::db::RawDatabase::Query(QString(
            // peers
            "CREATE TABLE peers ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT, "
            "public_key TEXT NOT NULL UNIQUE);"

            // aliases
            "CREATE TABLE aliases ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT, "
            "owner INTEGER UNIQUE, "
            "display_name BLOB NOT NULL"
            ");"

            // history
            "CREATE TABLE history (                                   "
            "   id INTEGER PRIMARY KEY AUTOINCREMENT,         "
            "   timestamp INTEGER NOT NULL,                          "
            "   receiver TEXT NOT NULL,                             "
            "   sender TEXT NOT NULL,                             "
            "   message BLOB NOT NULL,                             "
            "   data_id TEXT, "
            "   type INTEGER,"
            "   is_receipt BLOB,"
            "   sender_resource,"
            "   msg_id "
            "   );  "

            // file_transfers
            "CREATE TABLE file_transfers "
            "("
            "id INTEGER PRIMARY KEY AUTOINCREMENT, "
            "chat_id INTEGER NOT NULL, "
            "file_restart_id BLOB NOT NULL, "
            "file_name BLOB NOT NULL, "
            "file_path BLOB NOT NULL, "
            "file_hash BLOB NOT NULL, "
            "file_size INTEGER NOT NULL, "
            "direction INTEGER NOT NULL, "
            "file_state INTEGER NOT NULL);"

            // faux_offline_pending
            "CREATE TABLE faux_offline_pending (id INTEGER PRIMARY KEY);"

            // broken_messages
            "CREATE TABLE broken_messages (id INTEGER PRIMARY KEY);"

            "CREATE TABLE message_sessions"
            "("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "session_id TEXT,"
            "peer_id INTEGER"
            ");"

            ));

    queries +=
            lib::db::RawDatabase::Query(QString("PRAGMA user_version = %1;").arg(SCHEMA_VERSION));
    return db.execNow(queries);
}

bool isNewDb(std::shared_ptr<lib::db::RawDatabase>& db, bool& success) {
    bool newDb;
    if (!db->execNow(lib::db::RawDatabase::Query(
                "SELECT COUNT(*) FROM sqlite_master;",
                [&](const QVector<QVariant>& row) { newDb = row[0].toLongLong() == 0; }))) {
        db.reset();
        success = false;
        return false;
    }
    success = true;
    return newDb;
}

bool dbSchema1to2(lib::db::RawDatabase& db) {
    qDebug() << __func__;
    QVector<lib::db::RawDatabase::Query> queries;
    queries += lib::db::RawDatabase::Query(
            QStringLiteral("ALTER TABLE history ADD COLUMN sender_resource text;"));
    queries += lib::db::RawDatabase::Query(QStringLiteral("PRAGMA user_version = 2;"));
    return db.execNow(queries);
}

bool dbSchema2to3(lib::db::RawDatabase& db) {
    qDebug() << __func__;
    QVector<lib::db::RawDatabase::Query> queries;
    queries += lib::db::RawDatabase::Query(
            QStringLiteral("ALTER TABLE history ADD COLUMN msg_id text;"));
    queries += lib::db::RawDatabase::Query(QStringLiteral("PRAGMA user_version = 3;"));
    return db.execNow(queries);
}

bool dbSchema3to4(lib::db::RawDatabase& db) {
    qDebug() << __func__;
    // message session
    auto sql =
            "CREATE TABLE message_sessions"
            "("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "session_id TEXT,"
            "peer_id INTEGER"
            ");";
    QVector<lib::db::RawDatabase::Query> queries;
    queries += lib::db::RawDatabase::Query((sql));
    queries += lib::db::RawDatabase::Query(QStringLiteral("PRAGMA user_version = 4;"));
    return db.execNow(queries);
}

/**
 * @brief Upgrade the db schema
 * @return True if the schema upgrade succeded, false otherwise
 * @note On future alterations of the database all you have to do is bump the SCHEMA_VERSION
 * variable and add another case to the switch statement below. Make sure to fall through on each
 * case.
 */
bool dbSchemaUpgrade(std::shared_ptr<lib::db::RawDatabase>& db) {
    qDebug() << __func__;

    int64_t databaseSchemaVersion;
    if (!db->execNow(lib::db::RawDatabase::Query("PRAGMA user_version",
                                                 [&](const QVector<QVariant>& row) {
                                                     databaseSchemaVersion = row[0].toLongLong();
                                                 }))) {
        qCritical() << "History failed to read user_version";
        return false;
    }
    qDebug() << "Current db schema version is" << databaseSchemaVersion;

    if (databaseSchemaVersion > SCHEMA_VERSION) {
        qWarning().nospace() << "Database version (" << databaseSchemaVersion
                             << ") is newer than we currently support (" << SCHEMA_VERSION
                             << "). Please upgrade the program!";
        return false;
    }

    if (databaseSchemaVersion == SCHEMA_VERSION) {
        return true;
    }

    switch (databaseSchemaVersion) {
        case 0: {
            bool success = false;
            const bool newDb = isNewDb(db, success);
            if (!success) {
                qCritical() << "Failed to determine the db is new";
                return false;
            }
            if (newDb) {
                if (!createCurrentSchema(*db)) {
                    qCritical() << "Failed to create current db schema";
                    return false;
                }
                qDebug() << "Database created at schema version" << SCHEMA_VERSION;
                break;  // new db is the only case where we don't incrementally upgrade through each
                        // version
            }
        }
        case 1: {
            dbSchema1to2(*db.get());
            break;
        }
        case 2: {
            dbSchema2to3(*db.get());
            break;
        }
        case 3: {
            dbSchema3to4(*db.get());
            break;
        }
        // etc.
        default:
            qInfo() << "Database upgrade finished (databaseSchemaVersion" << databaseSchemaVersion
                    << "->" << SCHEMA_VERSION << ")";
    }

    return true;
}

}  // namespace

namespace module::im {

MessageState getMessageState(bool isPending, bool isBroken, bool isReceipt) {
    MessageState messageState;
    if (isPending) {
        messageState = MessageState::pending;
    } else if (isBroken) {
        messageState = MessageState::broken;
    } else if (isReceipt) {
        messageState = MessageState::receipt;
    } else {
        messageState = MessageState::complete;
    }
    return messageState;
}

/**
 * @brief Prepares the database to work with the history.
 * @param db This database will be prepared for use with the history.
 */
History::History(std::shared_ptr<lib::db::RawDatabase> db_) : db(db_) {
    if (!isValid()) {
        qWarning() << "Database not open, init failed";
        return;
    }

    const auto upgradeSucceeded = dbSchemaUpgrade(db);

    // dbSchemaUpgrade may have put us in an invalid state
    assert(upgradeSucceeded);
    //    connect(this, &History::fileInserted, this, &History::onFileInserted);

    // Cache our current peers
    // db->execLater(lib::db::RawDatabase::Query{
    //         "SELECT public_key, id FROM peers;",
    //         [this](const QVector<QVariant>& row) { peers[row[0].toString()] = row[1].toInt();
    //         }});
}

History::~History() {
    if (!isValid()) {
        return;
    }

    // We could have execLater requests pending with a lambda attached,
    // so clear the pending transactions first
    db->sync();
}

/**
 * @brief Checks if the database was opened successfully
 * @return True if database if opened, false otherwise.
 */
bool History::isValid() {
    return db && db->isOpen();
}

/**
 * @brief Checks if a friend has chat history
 * @param friendPk
 * @return True if has, false otherwise.
 */
bool History::historyExists(const FriendId& me, const FriendId& friendPk) {
    if (historyAccessBlocked()) {
        return false;
    }

    return !getMessagesForFriend(me, friendPk, 0, 1).empty();
}

/**
 * @brief Erases all the chat history from the database.
 */
void History::eraseHistory() {
    if (!isValid()) {
        return;
    }

    db->execNow(
            "DELETE FROM faux_offline_pending;"
            "DELETE FROM history;"
            "DELETE FROM aliases;"
            "DELETE FROM peers;"
            "DELETE FROM file_transfers;"
            "DELETE FROM broken_messages;"
            "VACUUM;");
}

/**
 * @brief Erases the chat history with one friend.
 * @param friendPk IMFriend public key to erase.
 */
void History::removeFriendHistory(const QString& friendPk) {
    qDebug() << __func__ << friendPk;

    if (!isValid()) {
        return;
    }

    QString sql = QString("DELETE FROM history WHERE receiver='%1' or sender='%1'; "
                          "DELETE FROM peers WHERE public_key='%1'; "
                          "DELETE FROM aliases WHERE owner='%1'; "
                          "VACUUM;")
                          .arg(friendPk);
    if (!db->execNow(sql)) {
        qWarning() << "Failed to remove friend's history";
        return;
    }
    peers.remove(friendPk);
}

uint History::addNewContact(const QString& contactId) {
    auto q = QString("INSERT OR IGNORE INTO peers (public_key) VALUES ('%1');").arg(contactId);

    db->execNow(q);

    uint id = 0;
    db->execNow({"SELECT last_insert_rowid();",
                 [&id](const QVector<QVariant>& row) { id = row[0].toUInt(); }});
    return id;
}

/**
 * @brief Generate query to insert new message in database
 * @param friendPk IMFriend publick key to save.
 * @param message Message to save.
 * @param sender Sender to save.
 * @param time Time of message sending.
 * @param isDelivered True if message was already delivered.
 * @param dispName Name, which should be displayed.
 * @param insertIdCallback Function, called after query execution.
 */
QVector<lib::db::RawDatabase::Query> History::generateNewMessageQueries(const Message& message,
                                                                        HistMessageContentType type,
                                                                        bool isDelivered,
                                                                        std::function<void(RowId)>
                                                                                insertIdCallback) {
    QVector<lib::db::RawDatabase::Query> queries;

    queries += lib::db::RawDatabase::Query(
            QString("INSERT INTO history "
                    "(timestamp, receiver, sender, message, type, data_id, "
                    "is_receipt, sender_resource, msg_id) "
                    "values (%1, '%2', '%3', '%4', %5, '%6', %7, '%8', '%9')")
                    .arg(message.timestamp.toMSecsSinceEpoch())  // 1
                    .arg(message.to)                             // 2
                    .arg(message.from)                           // 3
                    .arg(message.content)                        // 4
                    .arg((int)type)                              // 5
                    .arg(message.dataId)                         // 6
                    .arg(false)                                  // 7
                    .arg(message.from_resource)                  // 8
                    .arg(message.id),                            // 9
            insertIdCallback);

    if (!isDelivered) {
        queries += lib::db::RawDatabase::Query{
                "INSERT INTO faux_offline_pending (id) VALUES ("
                "    last_insert_rowid()"
                ");"};
    }

    return queries;
}

void History::addNewFileMessage(const ToxFile& file) {
    if (historyAccessBlocked()) {
        return;
    }

    qDebug() << "add new file:" << file.fileName;

    std::weak_ptr<History> weakThis = shared_from_this();
    FileInfo insertionData{file};

    auto insertFileTransferFn = [&](RowId rowId) {
        qDebug() << "file has be cached as rowId" << rowId.get();
    };

    Message msg = {.isAction = false,
                   .id = file.fileId,
                   .from = file.sender,
                   .to = file.receiver,
                   .content = insertionData.json(),
                   .dataId = file.fileId,
                   .timestamp = file.timestamp};

    addNewMessage(msg, HistMessageContentType::file, true, insertFileTransferFn);
}

/**
 * @brief Saves a chat message in the database.
 * @param friendPk IMFriend publick key to save.
 * @param message Message to save.
 * @param sender Sender to save.
 * @param time Time of message sending.
 * @param isDelivered True if message was already delivered.
 * @param dispName Name, which should be displayed.
 * @param insertIdCallback Function, called after query execution.
 */
void History::addNewMessage(const Message& message,
                            HistMessageContentType type,
                            bool isDelivered,
                            const std::function<void(RowId)>& insertIdCallback) {
    qDebug() << __func__ << "from:" << message.from << "message" << message.content;

    if (historyAccessBlocked()) {
        return;
    }
    auto sql = generateNewMessageQueries(message, type, isDelivered, insertIdCallback);
    db->execLater(sql);
}

void History::setFileMessage(const ToxFile& file) {
    qDebug() << __func__ << "file:" << file.fileId;

    if (historyAccessBlocked()) {
        return;
    }

    auto message = file.json();
    auto sql = QString("UPDATE history SET message = '%1' WHERE data_id = '%2'; ")
                       .arg(message)
                       .arg(file.fileId);

    db->execNow(sql);
}

QList<History::HistMessage> History::getMessageByDataId(const QString& dataId) {
    if (dataId.isEmpty()) return {};

    if (historyAccessBlocked()) {
        return {};
    }

    QString queryText = QString("SELECT "
                                "id, "               // 0
                                "timestamp, "        // 1
                                "receiver, "         // 2
                                "sender, "           // 3
                                "message, "          // 4
                                "type, "             // 5
                                "0, "                // 6
                                "0, "                // 7
                                "data_id,"           // 8
                                "is_receipt, "       // 9
                                "sender_resource, "  // 10
                                "msg_id "            // 11
                                "from history where data_id = '%1'")
                                .arg(dataId);

    // qDebug() << queryText;

    QList<HistMessage> messages;

    db->execNow(
            {queryText, [&](const QVector<QVariant>& row) { messages.append(rowToMessage(row)); }});

    return messages;
}

size_t History::getNumMessagesForFriend(const FriendId& me, const FriendId& friendPk) {
    if (historyAccessBlocked()) {
        return 0;
    }

    return getNumMessagesForFriendBeforeDate(me, friendPk, QDateTime());
}

size_t History::getNumMessagesForFriendBeforeDate(const FriendId& me, const FriendId& friendPk,
                                                  const QDateTime& date) {
    if (historyAccessBlocked()) {
        return 0;
    }

    QString link = me == friendPk ? "AND" : "OR";
    QString queryText = QString("SELECT COUNT(id) "
                                "FROM history "
                                "WHERE sender = '%1' %2 receiver = '%1'")
                                .arg(ContactId(friendPk).toString())
                                .arg(link);

    if (date.isNull()) {
        queryText += ";";
    } else {
        queryText += QString(" AND timestamp < %1;").arg(date.toMSecsSinceEpoch());
    }
    //    qDebug() << queryText;
    size_t numMessages = 0;
    auto rowCallback = [&numMessages](const QVector<QVariant>& row) {
        numMessages = row[0].toLongLong();
    };

    db->execNow({queryText, rowCallback});

    return numMessages;
}

QString History::makeSqlForFriend(const FriendId& me, const FriendId& friendPk) {
    QString link = me == friendPk ? "AND" : "OR";
    QString queryText =
            QString("SELECT history.id, "               // 0
                    "timestamp, "                       // 1
                    "receiver, "                        // 2
                    "sender, "                          // 3
                    "message, "                         // 4
                    "type, "                            // 5
                    "broken_messages.id bro_id, "       // 6
                    "faux_offline_pending.id off_id, "  // 7
                    "data_id, "                         // 8
                    "is_receipt, "                      // 9
                    "sender_resource, "                 // 10
                    "msg_id "                           // 11
                    "FROM history "
                    "LEFT JOIN faux_offline_pending ON history.id = faux_offline_pending.id "
                    "LEFT JOIN broken_messages ON history.id = broken_messages.id "
                    "WHERE (history.sender='%1' %2 history.receiver='%1') ")
                    .arg(friendPk.toString())
                    .arg(link);
    return queryText;
}

QString History::makeSqlForId(const MsgId& id) {
    QString queryText =
            QString("SELECT history.id, "               // 0
                    "timestamp, "                       // 1
                    "receiver, "                        // 2
                    "sender, "                          // 3
                    "message, "                         // 4
                    "type, "                            // 5
                    "broken_messages.id bro_id, "       // 6
                    "faux_offline_pending.id off_id, "  // 7
                    "data_id, "                         // 8
                    "is_receipt, "                      // 9
                    "sender_resource, "                 // 10
                    "msg_id "                           // 11
                    "FROM history "
                    "LEFT JOIN faux_offline_pending ON history.id = faux_offline_pending.id "
                    "LEFT JOIN broken_messages ON history.id = broken_messages.id "
                    "WHERE history.msg_id = '%1';")
                    .arg(id);
    return queryText;
}

History::HistMessage History::rowToMessage(const QVector<QVariant>& row) {
    auto id = RowId{row[0].toLongLong()};
    auto timestamp = QDateTime::fromMSecsSinceEpoch(row[1].toLongLong());
    auto receiver = row[2].toString();
    auto sender = row[3].toString();
    auto message = row[4].toString();
    auto type = row[5].toInt(0);
    auto isBroken = row[6].toInt(0);
    auto isPending = row[7].toInt(0);
    auto dataId = row[8].toString();
    auto isReceipt = row[9].toBool();
    auto sender_resource = row[10].toString();
    auto msgId = row[11].toString();
    auto ctype = static_cast<HistMessageContentType>(type);
    auto state = getMessageState(isPending, isBroken, isReceipt);
    return History::HistMessage{id,       ctype,   state,  timestamp, sender, sender_resource,
                                receiver, message, dataId, msgId};
}

QList<History::HistMessage> History::getMessagesForFriend(const FriendId& me,
                                                          const FriendId& friendPk,
                                                          size_t firstIdx,
                                                          size_t lastIdx) {
    qDebug() << __func__ << "me:" << me.toString() << " friend:" << friendPk.toString();
    if (historyAccessBlocked()) {
        return {};
    }

    auto sqlPrefix = makeSqlForFriend(me, friendPk);
    QString queryText = QString("%1 "
                                "ORDER by history.timestamp "
                                "LIMIT %2 OFFSET %3; ")
                                .arg(sqlPrefix)
                                .arg(lastIdx - firstIdx)
                                .arg(firstIdx);

    //    qDebug()<<queryText;

    QList<HistMessage> messages;
    db->execNow(
            {queryText, [&](const QVector<QVariant>& row) { messages.append(rowToMessage(row)); }});

    return messages;
}

QList<History::HistMessage> History::getMessageById(const MsgId& id) {
    QString queryText = makeSqlForId(id);
    QList<HistMessage> messages;
    db->execNow(
            {queryText, [&](const QVector<QVariant>& row) { messages.append(rowToMessage(row)); }});
    return messages;
}

QList<History::HistMessage> History::getLastMessageForFriend(const FriendId& me,
                                                             const FriendId& friendPk, uint size,
                                                             HistMessageContentType type) {
    if (historyAccessBlocked()) {
        return {};
    }

    auto sqlPrefix = makeSqlForFriend(me, friendPk);

    auto sql = QString("%1 "
                       "AND type = %3 "
                       "ORDER by history.timestamp DESC LIMIT %2;")
                       .arg(sqlPrefix)
                       .arg(size)
                       .arg((int)type);
    //        qDebug() <<"sql:"<<sql;
    QList<HistMessage> messages;
    auto rowCallback = [&](const QVector<QVariant>& row) { messages += rowToMessage(row); };
    db->execNow({sql, rowCallback});
    return messages;
}

QList<History::HistMessage> History::getUndeliveredMessagesForFriend(const FriendId& me,
                                                                     const FriendId& friendPk) {
    if (historyAccessBlocked()) {
        return {};
    }

    // auto sqlPrefix = makeSqlForFriend(me, friendPk);
    // auto queryText = QString("SELECT "
    //                          "history.id "
    //                          "FROM history "
    //                 "JOIN faux_offline_pending ON history.id = faux_offline_pending.id "
    //                 "JOIN peers chat on history.sender = chat.public_key "
    //                 "LEFT JOIN broken_messages ON history.id = broken_messages.id "
    //                 "WHERE chat.public_key='%1';")
    //                 .arg(friendPk.toString());

    // QList<qlonglong> ret;
    // auto rowCallback = [&](const QVector<QVariant>& row) { ret += row[0].toLongLong(); };
    // db->execNow({queryText, rowCallback});

    // if (ret.isEmpty()) return {};

    QList<History::HistMessage> list;
    // for (auto id : ret) {
    // list += getMessageById(id);
    // }

    return list;
}

/**
 * @brief Search phrase in chat messages
 * @param friendPk IMFriend public key
 * @param from a date message where need to start a search
 * @param phrase what need to find
 * @param parameter for search
 * @return date of the message where the phrase was found
 */
QDateTime History::getDateWhereFindPhrase(const QString& friendPk, const QDateTime& from,
                                          QString phrase, const ParameterSearch& parameter) {
    if (historyAccessBlocked()) {
        return QDateTime();
    }

    QDateTime result;
    auto rowCallback = [&result](const QVector<QVariant>& row) {
        result = QDateTime::fromMSecsSinceEpoch(row[0].toLongLong());
    };

    phrase.replace("'", "''");

    QString message;

    switch (parameter.filter) {
        case FilterSearch::Register:
            message = QStringLiteral("message LIKE '%1'").arg(phrase);
            break;
        case FilterSearch::WordsOnly:
            message = QStringLiteral("message REGEXP '%1'")
                              .arg(SearchExtraFunctions::generateFilterWordsOnly(phrase).toLower());
            break;
        case FilterSearch::RegisterAndWordsOnly:
            message = QStringLiteral("REGEXPSENSITIVE(message, '%1')")
                              .arg(SearchExtraFunctions::generateFilterWordsOnly(phrase));
            break;
        case FilterSearch::Regular:
            message = QStringLiteral("message REGEXP '%1'").arg(phrase);
            break;
        case FilterSearch::RegisterAndRegular:
            message = QStringLiteral("REGEXPSENSITIVE(message '%1')").arg(phrase);
            break;
        default:
            message = QStringLiteral("LOWER(message) LIKE '%1'").arg(phrase.toLower());
            break;
    }

    QDateTime date = from;

    if (!date.isValid()) {
        date = QDateTime::currentDateTime();
    }

    if (parameter.period == PeriodSearch::AfterDate ||
        parameter.period == PeriodSearch::BeforeDate) {
#if (QT_VERSION >= QT_VERSION_CHECK(5, 15, 0))
        date = parameter.date.startOfDay();
#else
        date = QDateTime(parameter.date);
#endif
    }

    QString period;
    switch (parameter.period) {
        case PeriodSearch::WithTheFirst:
            period = QStringLiteral("ORDER BY timestamp ASC LIMIT 1;");
            break;
        case PeriodSearch::AfterDate:
            period = QStringLiteral("AND timestamp > '%1' ORDER BY timestamp ASC LIMIT 1;")
                             .arg(date.toMSecsSinceEpoch());
            break;
        case PeriodSearch::BeforeDate:
            period = QStringLiteral("AND timestamp < '%1' ORDER BY timestamp DESC LIMIT 1;")
                             .arg(date.toMSecsSinceEpoch());
            break;
        default:
            period = QStringLiteral("AND timestamp < '%1' ORDER BY timestamp DESC LIMIT 1;")
                             .arg(date.toMSecsSinceEpoch());
            break;
    }

    QString queryText =
            QStringLiteral(
                    "SELECT timestamp "
                    "FROM history "
                    "LEFT JOIN faux_offline_pending ON history.id = faux_offline_pending.id "
                    "WHERE public_key='%1' "
                    "AND %2 "
                    "%3")
                    .arg(friendPk)
                    .arg(message)
                    .arg(period);

    db->execNow({queryText, rowCallback});

    return result;
}

/**
 * @brief Gets date boundaries in conversation with friendPk. History doesn't model conversation
 * indexes, but we can count messages between us and friendPk to effectively give us an index. This
 * function returns how many messages have happened between us <-> friendPk each time the date
 * changes
 * @param[in] friendPk ToxPk of conversation to retrieve
 * @param[in] from Start date to look from
 * @param[in] maxNum Maximum number of date boundaries to retrieve
 * @note This API may seem a little strange, why not use QDate from and QDate to? The intent is to
 * have an API that can be used to get the first item after a date (for search) and to get a list
 * of date changes (for loadHistory). We could write two separate queries but the query is fairly
 * intricate compared to our other ones so reducing duplication of it is preferable.
 */
QList<History::DateIdx> History::getNumMessagesForFriendBeforeDateBoundaries(
        const FriendId& friendPk, const QDate& from, size_t maxNum) {
    if (historyAccessBlocked()) {
        return {};
    }

    auto friendPkString = friendPk.toString();

    // No guarantee that this is the most efficient way to do this...
    // We want to count messages that happened for a friend before a
    // certain date. We do this by re-joining our table a second time
    // but this time with the only filter being that our id is less than
    // the ID of the corresponding row in the table that is grouped by day
    auto countMessagesForFriend = QString("SELECT COUNT(*) "
                                          "FROM history "
                                          "WHERE public_key = '%1' ")
                                          .arg(friendPkString);

    auto limitString = (maxNum) ? QString("LIMIT %1").arg(maxNum) : QString("");

    auto queryString = QString("SELECT (%1), (timestamp / 1000 / 60 / 60 / 24) AS day "
                               "FROM history "
                               "WHERE public_key = '%2' "
                               "AND timestamp >= %3 "
                               "GROUP by day "
                               "%4;")
                               .arg(countMessagesForFriend)
                               .arg(friendPkString)
#if (QT_VERSION >= QT_VERSION_CHECK(5, 15, 0))
                               .arg(QDateTime(from.startOfDay()).toMSecsSinceEpoch())
#else
                               .arg(QDateTime(from).toMSecsSinceEpoch())
#endif
                               .arg(limitString);

    QList<DateIdx> dateIdxs;
    auto rowCallback = [&dateIdxs](const QVector<QVariant>& row) {
        DateIdx dateIdx;
        dateIdx.numMessagesIn = row[0].toLongLong();
        dateIdx.date =
                QDateTime::fromMSecsSinceEpoch(row[1].toLongLong() * 24 * 60 * 60 * 1000).date();
        dateIdxs.append(dateIdx);
    };

    db->execNow({queryString, rowCallback});

    return dateIdxs;
}

/**
 * @brief Marks a message as delivered.
 * Removing message from the faux-offline pending messages list.
 *
 * @param id Message ID.
 */
void History::markAsDelivered(RowId messageId) {
    if (historyAccessBlocked()) {
        return;
    }

    db->execLater(QString("DELETE FROM faux_offline_pending WHERE id=%1;").arg(messageId.get()));
}

void History::markAsReceipt(RowId messageId) {
    if (historyAccessBlocked()) {
        return;
    }
    db->execLater(QString("UPDATE history SET is_receipt = 1 WHERE id=%1;").arg(messageId.get()));
}

void History::setPeerAlias(const QString& peer, const QString& alias) {
    int owner = -1;
    db->execNow({QString("SELECT id FROM peers where public_key = '%1'").arg(peer),
                 [&owner](const QVector<QVariant>& row) { owner = row[0].toLongLong(); }});
    if (owner < 0) {
        qWarning() << "Peer:" << peer << " is no existing";
        return;
    }

    auto sql = QString("INSERT OR IGNORE INTO aliases (owner, display_name) "
                       "VALUES (%1, '%2') "
                       "ON CONFLICT(owner) "
                       "DO UPDATE SET display_name = excluded.display_name;")
                       .arg(owner)
                       .arg(alias);
    db->execNow(sql);
}

QString History::getPeerAlias(const QString& friendPk) {
    QString name;
    db->execNow({QString("select display_name "
                         "from aliases a join peers p on a.owner = p.id "
                         "where p.public_key='%1'")
                         .arg(friendPk),
                 [&name](const QVector<QVariant>& row) { name = row[0].toString(); }});
    return name;
}

void History::getPeers(QList<Peer>& peers) {
    auto rowCallback = [&peers](const QVector<QVariant>& row) {
        Peer data;
        data.id = row[0].toLongLong();
        data.jid = row[1].toString();
        data.alias = row[2].toString();
        peers.append(data);
    };
    auto queryString =
            "select p.id, p.public_key, a.display_name from peers p left join aliases a on a.owner "
            "= p.id;";
    db->execNow({queryString, rowCallback});
}

Peer History::getPeer(const QString& friendPk) {
    Peer data;
    auto rowCallback = [&data](const QVector<QVariant>& row) {
        data.id = row[0].toLongLong();
        data.jid = row[1].toString();
        data.alias = row[2].toString();
    };
    auto queryString = QString("select p.id, p.public_key, a.display_name "
                               "from peers p left join aliases a on a.owner = p.id "
                               "where p.public_key = '%1';")
                               .arg(friendPk);
    db->execNow({queryString, rowCallback});
    return data;
}

MessageSession History::getMessageSession(const QString& peer) {
    MessageSession data;
    auto rowCallback = [&data](const QVector<QVariant>& row) {
        data.id = row[0].toLongLong();
        data.session_id = row[1].toString();
        data.peer_jid = row[2].toString();
    };
    auto queryString = QString("select ms.id, ms.session_id, p.public_key from message_sessions ms "
                               "left join peers p on ms.peer_id = p.id "
                               "where p.public_key = '%1';")
                               .arg(peer);
    db->execNow({queryString, rowCallback});
    return data;
}

void History::getMessageSessions(QList<MessageSession>& peers) {
    auto rowCallback = [&peers](const QVector<QVariant>& row) {
        MessageSession data;
        data.id = row[0].toLongLong();
        data.session_id = row[1].toString();
        data.peer_jid = row[2].toString();
        peers.append(data);
    };
    auto queryString =
            "select ms.id, ms.session_id, p.public_key from message_sessions ms left join peers p "
            "on ms.peer_id = p.id;";
    db->execNow({queryString, rowCallback});
}

uint History::addMessageSession(const MessageSession& ms) {
    auto peer = getPeer(ms.peer_jid);
    if (peer.jid.isEmpty()) {
        qWarning() << "Peer is no existing!";
        return 0;
    }

    if (ms.id > 0) {
        return 0;
    }

    auto old = getMessageSession(ms.peer_jid);
    if (old.session_id.isEmpty()) {
        auto q = QString("INSERT OR IGNORE INTO message_sessions (session_id, peer_id) "
                         "VALUES ('%1', %2);")
                         .arg(ms.session_id)
                         .arg(peer.id);
        db->execNow(q);

        uint id = 0;
        db->execNow({"SELECT last_insert_rowid();",
                     [&id](const QVector<QVariant>& row) { id = row[0].toUInt(); }});
        return id;
    }

    auto q = QString("UPDATE message_sessions set session_id='%1' where peer_id = %2; ")
                     .arg(ms.session_id)
                     .arg(peer.id);
    db->execNow(q);
    return old.id;
}

/**
 * @brief Determines if history access should be blocked
 * @return True if history should not be accessed
 */
bool History::historyAccessBlocked() {
    // if (!Nexus::getProfile()->getSettings()->getEnableLogging()) {
    //     qCritical() << "Blocked history access while history is disabled";
    //     return true;
    // }

    if (!isValid()) {
        return true;
    }

    return false;
}
}  // namespace module::im