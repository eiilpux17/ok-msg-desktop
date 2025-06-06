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

#ifndef NETCAMVIEW_H
#define NETCAMVIEW_H

#include <QVector>
#include "genericnetcamview.h"
#include "src/model/FriendId.h"

class QFrame;
class QHBoxLayout;

namespace lib::video {
class VideoSource;
struct vpx_image;
}  // namespace lib::video

namespace lib::ui {
class MovableWidget;
}

namespace module::im {

class NetCamView : public GenericNetCamView {
    Q_OBJECT

public:
    explicit NetCamView(FriendId friendPk, QWidget* parent = nullptr);
    ~NetCamView() override;

    virtual void show(lib::video::VideoSource* source, const QString& title);
    virtual void hide();

    void setSource(lib::video::VideoSource* s);
    void setTitle(const QString& title);
    void toggleVideoPreview();

protected:
    void showEvent(QShowEvent* event) final override;

private slots:
    void updateRatio();

private:
    void updateFrameSize(QSize size);

    VideoSurface* selfVideoSurface;
    lib::ui::MovableWidget* selfFrame;
    FriendId friendPk;
    bool e;
    QVector<QMetaObject::Connection> connections;
};
}  // namespace module::im
#endif  // NETCAMVIEW_H
