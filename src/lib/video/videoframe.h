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

#ifndef VIDEOFRAME_H
#define VIDEOFRAME_H

#include <QImage>
#include <QMutex>
#include <QReadWriteLock>
#include <QRect>
#include <QSize>


#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <unordered_map>

struct AVFrame;

namespace lib::video {

        // Declare type aliases
using IDType = std::uint_fast64_t;
using AtomicIDType = std::atomic_uint_fast64_t;

/**\brief Image Descriptor */
typedef struct vpx_image {
    /* Image storage dimensions */
    unsigned int w;         /**< Stored image width */
    unsigned int h;         /**< Stored image height */
    unsigned int bit_depth; /**< Stored image bit-depth */

    /* Image display dimensions */
    unsigned int d_w; /**< Displayed image width */
    unsigned int d_h; /**< Displayed image height */

    unsigned char* planes[4]; /**< pointer to the top left pixel for each plane */
    int stride[4];            /**< stride between rows for each plane */
} vpx_image_t;                /**< alias for struct vpx_image */


struct ToxYUVFrame {
public:
    bool isValid() const;
    explicit operator bool() const;

    const std::uint16_t width;
    const std::uint16_t height;

    const uint8_t* y;
    const uint8_t* u;
    const uint8_t* v;
};


struct OVideoFrame {
    IDType frameID;
    IDType sourceID;
    QImage image;

    QSize getSize() const{
        return image.size();
    }

    const QImage& getImage() const {
        return image;
    }

};


class VideoFrame {
public:


public:
    VideoFrame(IDType sourceID, AVFrame* sourceFrame, QRect dimensions, int pixFmt,
               bool freeSourceFrame = false);
    VideoFrame(IDType sourceID, AVFrame* sourceFrame, bool freeSourceFrame = false);

    ~VideoFrame();

    // Copy/Move operations are disabled for the VideoFrame, encapsulate with a std::shared_ptr to
    // manage.

    VideoFrame(const VideoFrame& other) = delete;
    VideoFrame(VideoFrame&& other) = delete;

    static void untrackFrames(const IDType& sourceID, bool releaseFrames = false);
    static std::unique_ptr<VideoFrame> convert0(IDType id, std::unique_ptr<vpx_image_t> vpxframe);
    static std::unique_ptr<OVideoFrame> convert(IDType id, std::unique_ptr<vpx_image_t> vpxframe);

    const VideoFrame& operator=(const VideoFrame& other) = delete;
    const VideoFrame& operator=(VideoFrame&& other) = delete;

    bool isValid();

    std::shared_ptr<VideoFrame> trackFrame();

    void releaseFrame();

    const AVFrame* getAVFrame(QSize frameSize, const int pixelFormat, const bool requireAligned);
    QImage toQImage(QSize frameSize = {});
    ToxYUVFrame toToxYUVFrame(QSize frameSize = {});

    IDType getFrameID() const;
    IDType getSourceID() const;
    QRect getSourceDimensions() const;
    int getSourcePixelFormat() const;

    static constexpr int dataAlignment = 32;

private:
    class FrameBufferKey {
    public:
        FrameBufferKey(const int width, const int height, const int pixFmt, const bool lineAligned);

        // Explictly state default constructor/destructor

        FrameBufferKey(const FrameBufferKey&) = default;
        FrameBufferKey(FrameBufferKey&&) = default;
        ~FrameBufferKey() = default;

        // Assignment operators are disabled for the FrameBufferKey

        const FrameBufferKey& operator=(const FrameBufferKey&) = delete;
        const FrameBufferKey& operator=(FrameBufferKey&&) = delete;

        bool operator==(const FrameBufferKey& other) const;
        bool operator!=(const FrameBufferKey& other) const;

        static size_t hash(const FrameBufferKey& key);

    public:
        const int frameWidth;
        const int frameHeight;
        const int pixelFormat;
        const bool linesizeAligned;
    };

private:
    static FrameBufferKey getFrameKey(const QSize& frameSize, const int pixFmt, const int linesize);
    static FrameBufferKey getFrameKey(const QSize& frameSize, const int pixFmt,
                                      const bool frameAligned);

    AVFrame* retrieveAVFrame(const QSize& dimensions, const int pixelFormat,
                             const bool requireAligned);
    AVFrame* generateAVFrame(const QSize& dimensions, const int pixelFormat,
                             const bool requireAligned);
    AVFrame* storeAVFrame(AVFrame* frame, const QSize& dimensions, const int pixelFormat);

    void deleteFrameBuffer();

    template <typename T>
    T toGenericObject(const QSize& dimensions, const int pixelFormat, const bool requireAligned,
                      const std::function<T(AVFrame* const)>& objectConstructor,
                      const T& nullObject);

private:
    // ID
    const IDType frameID;
    const IDType sourceID;

    // Main framebuffer store
    std::unordered_map<FrameBufferKey, AVFrame*, std::function<decltype(FrameBufferKey::hash)>>
            frameBuffer{3, FrameBufferKey::hash};

    // Source frame
    const QRect sourceDimensions;
    int sourcePixelFormat;
    const FrameBufferKey sourceFrameKey;
    const bool freeSourceFrame;

    // Reference store
    static AtomicIDType frameIDs;

    static std::unordered_map<IDType, QMutex> mutexMap;
    static std::unordered_map<IDType,
                              std::unordered_map<IDType, std::weak_ptr<VideoFrame>>>
            refsMap;

    // Concurrency
    QReadWriteLock frameLock{};
    static QReadWriteLock refsLock;
};




class FrameHandler {
public:
    virtual void onCompleted() = 0;
    virtual void onFrame(std::shared_ptr<OVideoFrame>) = 0;
};


}  // namespace lib::video
#endif  // VIDEOFRAME_H
