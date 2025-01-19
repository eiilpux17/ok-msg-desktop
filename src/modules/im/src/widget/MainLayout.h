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

#ifndef MAINLAYOUT_H
#define MAINLAYOUT_H

#include <QWidget>

#include "src/widget/contentlayout.h"
namespace module::im {

/**
 * 主业务布局
 */
class MainLayout : public QWidget {
    Q_OBJECT
public:
    explicit MainLayout(QWidget* parent = nullptr);

    [[nodiscard]] virtual ContentLayout* getContentLayout() const = 0;

signals:
};
}  // namespace module::im
#endif  // MAINLAYOUT_H
