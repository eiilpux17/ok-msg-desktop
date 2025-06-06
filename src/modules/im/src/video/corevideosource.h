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

#ifndef COREVIDEOSOURCE_H
#define COREVIDEOSOURCE_H

#include <QMutex>
#include <atomic>
#include "lib/video/videoframe.h"
#include "lib/video/videosource.h"

namespace module::im {

class CoreVideoSource : public lib::video::VideoSource {
    Q_OBJECT
public:
    CoreVideoSource();

private:
    void pushFrame(std::unique_ptr<lib::video::vpx_image_t> frame);
    void setDeleteOnClose(bool newstate);

    void stopSource();
    void restartSource();

private:
    std::atomic_int subscribers;
    std::atomic_bool deleteOnClose;
    QMutex biglock;
    std::atomic_bool stopped;

    friend class CoreAV;
    friend class ToxFriendCall;
};
}  // namespace module::im

#endif  // COREVIDEOSOURCE_H
