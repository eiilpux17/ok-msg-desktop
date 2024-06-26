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

#include "ok_videosink.h"

#include <api/video/i420_buffer.h>


#include <utility>

namespace lib {
namespace ortc {

OVideoSink::OVideoSink(OkRTCRenderer *renderer, std::string peerId)
    : _renderer(renderer), _peer_id(std::move(peerId)) {
  assert(renderer);
  //  assert(!peerId.empty());
}

OVideoSink::~OVideoSink() {  }

void OVideoSink::OnFrame(const webrtc::VideoFrame &frame) {

  if (!_renderer) {
    //qDebug(("_renderer is nullptr!"));
    return;
  }

  bool conv = false;
  ortc::RendererImage _image{};
  auto vfb = frame.video_frame_buffer();
  switch (vfb->type()) {
  case webrtc::VideoFrameBuffer::Type::kI420: {
    auto buffer = vfb->GetI420();
    _image = {static_cast<size_t>(buffer->width()),
              static_cast<size_t>(buffer->height()),
              const_cast<uint8_t *>(buffer->DataY()),
              const_cast<uint8_t *>(buffer->DataU()),
              const_cast<uint8_t *>(buffer->DataV()),
              buffer->StrideY(),
              buffer->StrideU(),
              buffer->StrideV()};
    }
    conv = true;
    break;

  default:
    //qDebug(("Not supported frame type:%1")
//                  .arg(webrtc::VideoFrameBufferTypeToString(vfb->type())));
    break;
  }

  if (!conv)
    return;

  _renderer->onRender(_peer_id, _image);

  // 渲染次数计数
  _renderCount++;
}

} // namespace ortc
} // namespace lib