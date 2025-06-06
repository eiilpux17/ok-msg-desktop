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

#ifndef CUSTOMTEXTDOCUMENT_H
#define CUSTOMTEXTDOCUMENT_H

#include <QList>
#include <QTextDocument>

#include <memory>

class QIcon;

namespace module::im {

class CustomTextDocument : public QTextDocument {
    Q_OBJECT
public:
    explicit CustomTextDocument(QObject* parent = nullptr);

protected:
    virtual QVariant loadResource(int type, const QUrl& name);

private:
    QList<std::shared_ptr<QIcon>> emoticonIcons;
};
}  // namespace module::im
#endif  // CUSTOMTEXTDOCUMENT_H
