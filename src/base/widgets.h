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
#ifndef WIDGETS_H
#define WIDGETS_H

#include <QColor>
#include <QDesktopServices>
#include <QPalette>
#include <QGuiApplication>
#include <QColor>
#include <QGraphicsDropShadowEffect>
#include <QLayout>
#include <QMargins>
#include <QPalette>
#include <QTime>
#include <QWidget>
#include <QScreen>

namespace ok::base {

class Widgets {
public:
    Widgets();

    static void SetPalette(QWidget* w, QPalette::ColorRole role, QColor color) {
        if (w) {
            QPalette palette(w->palette());
            palette.setColor(role, color);
            w->setPalette(palette);
            w->setAutoFillBackground(true);
        }
    }

    static void SetPalette(QWidget* w, QPalette::ColorRole role, QBrush brush) {
        if (w) {
            w->setAutoFillBackground(true);
            QPalette palette(w->palette());
            palette.setBrush(role, brush);
            w->setPalette(palette);
        }
    }

    static void SetNoMargins(QWidget* w) {
        if (w) {
            w->setContentsMargins(QMargins(0, 0, 0, 0));
        }
    }

    static void SetNoMargins(QLayout* w) {
        if (w) {
            w->setContentsMargins(QMargins(0, 0, 0, 0));
        }
    }

    /**
     * 实例阴影shadow
     * @param w
     * @param offset
     * @param color
     * @param blurRadius
     */
    static void SetShadowEffect(QWidget* w,
                                const QPointF& offset,
                                const QColor& color,
                                qreal blurRadius) {
        auto* shadow_effect = new QGraphicsDropShadowEffect(w);
        shadow_effect->setOffset(offset);
        shadow_effect->setColor(color);
        shadow_effect->setBlurRadius(blurRadius);
        w->setGraphicsEffect(shadow_effect);
    }

    static void OpenExternUrl(const QUrl& url) { QDesktopServices::openUrl(url); }


    /**移动窗口到屏幕居中
     * @brief moveToScreenCenter
     */
    inline static void moveToScreenCenter(QWidget * w){
        if(!w) return;

        // 获取屏幕大小和窗口大小
        QRect screenGeometry = QGuiApplication::primaryScreen()->geometry();

        // 计算窗口应该放置的位置以使其居中
        auto ws = w->sizeHint();
        QPoint windowPosition = QPoint(
                (screenGeometry.width() - ws.width()) / 2,
                (screenGeometry.height() - ws.height()) / 2);

        // 移动窗口到计算出的位置
        w->move(windowPosition);
    }



};


}  // namespace ok::base

#endif  // WIDGETS_H
