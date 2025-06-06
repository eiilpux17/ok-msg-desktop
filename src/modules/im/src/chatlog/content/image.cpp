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

#include "image.h"
#include "../pixmapcache.h"

#include <QApplication>
#include <QClipboard>
#include <QPainter>

namespace module::im {

Image::Image(QSize Size, const QString& filename)
        : ChatLineContent(ContentType::CHAT_IMAGE), size(Size) {
    pmap = PixmapCache::getInstance().get(filename, size);
}

QRectF Image::boundingRect() const {
    return QRectF(QPointF(-size.width() / 2.0, -size.height() / 2.0), size);
}

qreal Image::getAscent() const { return 0.0; }

void Image::onCopyEvent() {
    if (pmap.isNull()) {
        return;
    }

    QClipboard* clipboard = QApplication::clipboard();
    if (clipboard) clipboard->setPixmap(pmap);
}

void Image::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) {
    painter->setClipRect(boundingRect());

    painter->setRenderHint(QPainter::SmoothPixmapTransform);
    painter->translate(-size.width() / 2.0, -size.height() / 2.0);
    painter->drawPixmap(0, 0, pmap);

    Q_UNUSED(option)
    Q_UNUSED(widget)
}

void Image::setWidth(qreal width) { Q_UNUSED(width) }
const void* Image::getContent() { return &pmap; }
}  // namespace module::im