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

#include "documentcache.h"
#include "customtextdocument.h"
namespace module::im {
DocumentCache::~DocumentCache() {
    while (!documents.isEmpty()) delete documents.pop();
}

QTextDocument* DocumentCache::pop() {
    if (documents.empty()) documents.push(new CustomTextDocument);

    return documents.pop();
}

void DocumentCache::push(QTextDocument* doc) {
    if (doc) {
        doc->clear();
        documents.push(doc);
    }
}

/**
 * @brief Returns the singleton instance.
 */
DocumentCache& DocumentCache::getInstance() {
    static DocumentCache instance;
    return instance;
}
}  // namespace module::im
