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
#ifndef LIB_UI_H
#define LIB_UI_H


#include "base/resources.h"

#include <QObject>

OK_RESOURCE_LOADER(uiRes)

namespace lib::ui{

class OkUI  : public QObject{
Q_OBJECT
public:
    void init();
private:
    OK_RESOURCE_PTR(uiRes);
};

}
#endif  // UI_H
