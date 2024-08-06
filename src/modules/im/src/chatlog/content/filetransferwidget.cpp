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

#include "filetransferwidget.h"
#include "ui_filetransferwidget.h"

#include "src/core/core.h"
#include "src/core/corefile.h"
#include "src/lib/settings/style.h"
#include "src/persistence/settings.h"
#include "src/widget/gui.h"
#include "src/widget/widget.h"

#include <libexif/exif-loader.h>

#include <QBuffer>
#include <QDebug>
#include <QDesktopServices>
#include <QDesktopWidget>
#include <QFile>
#include <QFileDialog>
#include <QGraphicsSceneEvent>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QStyle>
#include <QVariantAnimation>

#include <math.h>
#include <cassert>

static QList<qreal> layoutExpandingSize(QList<qreal> baseSize, qreal total);

// The leftButton is used to accept, pause, or resume a file transfer, as well as to open a
// received file.
// The rightButton is used to cancel a file transfer, or to open the directory a file was
// downloaded to.

FileTransferWidget::FileTransferWidget(QWidget* parent, ToxFile file)
        : QWidget(parent)
        , ui(new Ui::FileTransferWidget)
        , fileInfo(file)
        , backgroundColor(Style::getColor(Style::TransferMiddle))
        , buttonColor(Style::getColor(Style::TransferWait))
        , buttonBackgroundColor(Style::getColor(Style::GroundBase))
        , active(true) {
    ui->setupUi(this);

    // hide the QWidget background (background-color: transparent doesn't seem to work)
    setAttribute(Qt::WA_TranslucentBackground, true);

    ui->previewButton->hide();
    ui->filenameLabel->setText(file.fileName);
    ui->progressBar->setValue(0);
    ui->fileSizeLabel->setText(getHumanReadableSize(file.fileSize));
    ui->etaLabel->setText("");

    backgroundColorAnimation = new QVariantAnimation(this);
    backgroundColorAnimation->setDuration(500);
    backgroundColorAnimation->setEasingCurve(QEasingCurve::OutCubic);
    connect(backgroundColorAnimation, &QVariantAnimation::valueChanged, this,
            [this](const QVariant& val) {
                backgroundColor = val.value<QColor>();
                update();
            });

    buttonColorAnimation = new QVariantAnimation(this);
    buttonColorAnimation->setDuration(500);
    buttonColorAnimation->setEasingCurve(QEasingCurve::OutCubic);
    connect(buttonColorAnimation, &QVariantAnimation::valueChanged, this,
            [this](const QVariant& val) {
                buttonColor = val.value<QColor>();
                update();
            });

    CoreFile* coreFile = CoreFile::getInstance();

    connect(ui->leftButton, &QPushButton::clicked, this, &FileTransferWidget::onLeftButtonClicked);
    connect(ui->rightButton, &QPushButton::clicked, this,
            &FileTransferWidget::onRightButtonClicked);
    connect(ui->previewButton, &QPushButton::clicked, this,
            &FileTransferWidget::onPreviewButtonClicked);

    // Set lastStatus to anything but the file's current value, this forces an update
    lastStatus =
            file.status == FileStatus::FINISHED ? FileStatus::INITIALIZING : FileStatus::FINISHED;
    updateWidget(file);

    setFixedHeight(64);

    connect(&GUI::getInstance(), &GUI::themeApplyRequest, this, [this]() {
        backgroundColorAnimation->stop();
        setBackgroundColor(lastStatus, true);
    });
}

FileTransferWidget::~FileTransferWidget() { delete ui; }

// TODO(sudden6): remove file IO from the UI
/**
 * @brief Dangerous way to find out if a path is writable.
 * @param filepath Path to file which should be deleted.
 * @return True, if file writeable, false otherwise.
 */
bool FileTransferWidget::tryRemoveFile(const QString& filepath) {
    QFile tmp(filepath);
    bool writable = tmp.open(QIODevice::WriteOnly);
    tmp.remove();
    return writable;
}

void FileTransferWidget::onFileTransferUpdate(ToxFile file) { updateWidget(file); }

bool FileTransferWidget::isActive() const { return active; }

void FileTransferWidget::acceptTransfer(const QString& filepath) {
    if (filepath.isEmpty()) {
        return;
    }

    // test if writable
    if (!tryRemoveFile(filepath)) {
        GUI::showWarning(tr("Location not writable", "Title of permissions popup"),
                         tr("You do not have permission to write that location. Choose another, or "
                            "cancel the save dialog.",
                            "text of permissions popup"));
        return;
    }

    // everything ok!
    CoreFile* coreFile = CoreFile::getInstance();
    coreFile->acceptFileRecvRequest(fileInfo.receiver, fileInfo.fileId, filepath);
}

void FileTransferWidget::setBackgroundColor(FileStatus status, bool useAnima) {
    QColor color;
    bool whiteFont;
    switch (status) {
        case FileStatus::INITIALIZING:
        case FileStatus::PAUSED:
        case FileStatus::TRANSMITTING:
            color = Style::getColor(Style::TransferMiddle);
            whiteFont = false;
            break;
        case FileStatus::BROKEN:
        case FileStatus::CANCELED:
            color = Style::getColor(Style::TransferBad);
            whiteFont = true;
            break;
        case FileStatus::FINISHED:
            color = Style::getColor(Style::TransferGood);
            whiteFont = true;
            break;
        default:
            return;
            break;
    }

    if (color != backgroundColor) {
        if (useAnima) {
            backgroundColorAnimation->setStartValue(backgroundColor);
            backgroundColorAnimation->setEndValue(color);
            backgroundColorAnimation->start();
        } else {
            backgroundColor = color;
        }
    }

    setProperty("fontColor", whiteFont ? "white" : "black");

    setStyleSheet(Style::getStylesheet("fileTransferInstance/filetransferWidget.css"));
}

void FileTransferWidget::setButtonColor(const QColor& c) {
    if (c != buttonColor) {
        buttonColorAnimation->setStartValue(buttonColor);
        buttonColorAnimation->setEndValue(c);
        buttonColorAnimation->start();
    }
}

bool FileTransferWidget::drawButtonAreaNeeded() const {
    return (ui->rightButton->isVisible() || ui->leftButton->isVisible()) &&
           !(ui->leftButton->isVisible() && ui->leftButton->objectName() == "ok");
}

void FileTransferWidget::paintEvent(QPaintEvent*) {
    // required by Hi-DPI support as border-image doesn't work.
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);

    qreal ratio = static_cast<qreal>(geometry().height()) / static_cast<qreal>(geometry().width());
    const int r = 24;
    const int buttonFieldWidth = 32;
    const int lineWidth = 1;

    // Draw the widget background:
    painter.setClipRect(QRect(0, 0, width(), height()));
    painter.setBrush(QBrush(backgroundColor));
    painter.drawRoundedRect(geometry(), r * ratio, r, Qt::RelativeSize);

    if (drawButtonAreaNeeded()) {
        // Draw the button background:
        QPainterPath buttonBackground;
        buttonBackground.addRoundedRect(width() - 2 * buttonFieldWidth - lineWidth * 2, 0,
                                        buttonFieldWidth, buttonFieldWidth + lineWidth, 50, 50,
                                        Qt::RelativeSize);
        buttonBackground.addRect(width() - 2 * buttonFieldWidth - lineWidth * 2, 0,
                                 buttonFieldWidth * 2, buttonFieldWidth / 2);
        buttonBackground.addRect(width() - 1.5 * buttonFieldWidth - lineWidth * 2, 0,
                                 buttonFieldWidth * 2, buttonFieldWidth + 1);
        buttonBackground.setFillRule(Qt::WindingFill);
        painter.setBrush(QBrush(buttonBackgroundColor));
        painter.drawPath(buttonBackground);

        // Draw the left button:
        QPainterPath leftButton;
        leftButton.addRoundedRect(QRect(width() - 2 * buttonFieldWidth - lineWidth, 0,
                                        buttonFieldWidth, buttonFieldWidth),
                                  50, 50, Qt::RelativeSize);
        leftButton.addRect(QRect(width() - 2 * buttonFieldWidth - lineWidth, 0,
                                 buttonFieldWidth / 2, buttonFieldWidth / 2));
        leftButton.addRect(QRect(width() - 1.5 * buttonFieldWidth - lineWidth, 0,
                                 buttonFieldWidth / 2, buttonFieldWidth));
        leftButton.setFillRule(Qt::WindingFill);
        painter.setBrush(QBrush(buttonColor));
        painter.drawPath(leftButton);

        // Draw the right button:
        painter.setBrush(QBrush(buttonColor));
        painter.setClipRect(
                QRect(width() - buttonFieldWidth, 0, buttonFieldWidth, buttonFieldWidth));
        painter.drawRoundedRect(geometry(), r * ratio, r, Qt::RelativeSize);
    }
}

QString FileTransferWidget::getHumanReadableSize(qint64 size) {
    static const char* suffix[] = {"B", "kiB", "MiB", "GiB", "TiB"};
    int exp = 0;

    if (size > 0) {
        exp = std::min((int)(log(size) / log(1024)), (int)(sizeof(suffix) / sizeof(suffix[0]) - 1));
    }

    return QString().setNum(size / pow(1024, exp), 'f', exp > 1 ? 2 : 0).append(suffix[exp]);
}

void FileTransferWidget::updateWidgetColor(ToxFile const& file) {
    if (lastStatus == file.status) {
        return;
    }

    switch (file.status) {
        case FileStatus::INITIALIZING:
        case FileStatus::PAUSED:
        case FileStatus::TRANSMITTING:
        case FileStatus::BROKEN:
        case FileStatus::CANCELED:
        case FileStatus::FINISHED:
            setBackgroundColor(file.status);
            break;
        default:
            qWarning() << "Invalid file status" << file.fileId;
            break;
    }
}

void FileTransferWidget::updateWidgetText(ToxFile const& file) {
    if (lastStatus == file.status && file.status != FileStatus::PAUSED) {
        return;
    }

    switch (file.status) {
        case FileStatus::INITIALIZING:
            if (file.direction == FileDirection::SENDING) {
                ui->progressLabel->setText(tr("Waiting to send...", "file transfer widget"));
            } else {
                ui->progressLabel->setText(
                        tr("Accept to receive this file", "file transfer widget"));
            }
            break;
        case FileStatus::PAUSED:
            //        ui->etaLabel->setText("");
            //        if (file.pauseStatus.localPaused()) {
            //            ui->progressLabel->setText(tr("Paused", "file transfer widget"));
            //        } else {
            //            ui->progressLabel->setText(tr("Remote Paused", "file transfer widget"));
            //        }
            break;
        case FileStatus::TRANSMITTING:
            ui->etaLabel->setText("");
            ui->progressLabel->setText(tr("Resuming...", "file transfer widget"));
            break;
        case FileStatus::BROKEN:
        case FileStatus::CANCELED:
            break;
        case FileStatus::FINISHED:
            break;
        default:
            qWarning() << "Invalid file status" << file.fileId;
            break;
    }
}

void FileTransferWidget::updatePreview(ToxFile const& file) {
    if (lastStatus == file.status) {
        return;
    }

    switch (file.status) {
        case FileStatus::INITIALIZING:
        case FileStatus::PAUSED:
        case FileStatus::TRANSMITTING:
        case FileStatus::BROKEN:
        case FileStatus::CANCELED:
            if (file.direction == FileDirection::SENDING) {
                showPreview(file.filePath);
            }
            break;
        case FileStatus::FINISHED:
            showPreview(file.filePath);
            break;
        default:
            qWarning() << "Invalid file status" << file.fileId;
            //        assert(false);
    }
}

void FileTransferWidget::updateFileProgress(ToxFile const& file) {
    switch (file.status) {
        case FileStatus::INITIALIZING:
            break;
        case FileStatus::PAUSED:
            fileProgress.resetSpeed();
            break;
        case FileStatus::TRANSMITTING: {
            if (!fileProgress.needsUpdate()) {
                break;
            }

            fileProgress.addSample(file);
            auto speed = fileProgress.getSpeed();
            auto progress = fileProgress.getProgress();
            auto remainingTime = fileProgress.getTimeLeftSeconds();

            ui->progressBar->setValue(static_cast<int>(progress * 100.0));

            // update UI
            if (speed > 0) {
                // ETA
                QTime toGo = QTime(0, 0).addSecs(remainingTime);
                QString format = toGo.hour() > 0 ? "hh:mm:ss" : "mm:ss";
                ui->etaLabel->setText(toGo.toString(format));
            } else {
                ui->etaLabel->setText("");
            }

            ui->progressLabel->setText(getHumanReadableSize(speed) + "/s");
            break;
        }
        case FileStatus::BROKEN:
        case FileStatus::CANCELED:
        case FileStatus::FINISHED: {
            ui->progressBar->hide();
            ui->progressLabel->hide();
            ui->etaLabel->hide();
            break;
        }
        default:
            qWarning() << "Invalid file status" << file.fileId;
            //        assert(false);
    }
}

void FileTransferWidget::updateSignals(ToxFile const& file) {
    if (lastStatus == file.status) {
        return;
    }

    switch (file.status) {
        case FileStatus::CANCELED:
        case FileStatus::BROKEN:
        case FileStatus::FINISHED:
            active = false;
            disconnect(CoreFile::getInstance(), nullptr, this, nullptr);
            break;
        case FileStatus::INITIALIZING:
        case FileStatus::PAUSED:
        case FileStatus::TRANSMITTING:
            break;
        default:
            qWarning() << "Invalid file status" << file.fileId;
            break;
    }
}

void FileTransferWidget::setupButtons(ToxFile const& file) {
    if (lastStatus == file.status && file.status != FileStatus::PAUSED) {
        return;
    }

    switch (file.status) {
        case FileStatus::TRANSMITTING:
            ui->leftButton->setIcon(QIcon(Style::getImagePath("fileTransferInstance/pause.svg")));
            ui->leftButton->setObjectName("pause");
            ui->leftButton->setToolTip(tr("Pause transfer"));

            ui->rightButton->setIcon(QIcon(Style::getImagePath("fileTransferInstance/no.svg")));
            ui->rightButton->setObjectName("cancel");
            ui->rightButton->setToolTip(tr("Cancel transfer"));

            setButtonColor(Style::getColor(Style::TransferGood));
            break;

        case FileStatus::PAUSED:
            //        if (file.pauseStatus.localPaused()) {
            //            ui->leftButton->setIcon(QIcon(Style::getImagePath("fileTransferInstance/arrow_white.svg")));
            //            ui->leftButton->setObjectName("resume");
            //            ui->leftButton->setToolTip(tr("Resume transfer"));
            //        } else {
            //            ui->leftButton->setIcon(QIcon(Style::getImagePath("fileTransferInstance/pause.svg")));
            //            ui->leftButton->setObjectName("pause");
            //            ui->leftButton->setToolTip(tr("Pause transfer"));
            //        }

            ui->rightButton->setIcon(QIcon(Style::getImagePath("fileTransferInstance/no.svg")));
            ui->rightButton->setObjectName("cancel");
            ui->rightButton->setToolTip(tr("Cancel transfer"));

            setButtonColor(Style::getColor(Style::TransferMiddle));
            break;

        case FileStatus::INITIALIZING:
            ui->rightButton->setIcon(QIcon(Style::getImagePath("fileTransferInstance/no.svg")));
            ui->rightButton->setObjectName("cancel");
            ui->rightButton->setToolTip(tr("Cancel transfer"));

            if (file.direction == FileDirection::SENDING) {
                ui->leftButton->setIcon(
                        QIcon(Style::getImagePath("fileTransferInstance/pause.svg")));
                ui->leftButton->setObjectName("pause");
                ui->leftButton->setToolTip(tr("Pause transfer"));
            } else {
                ui->leftButton->setIcon(QIcon(Style::getImagePath("fileTransferInstance/yes.svg")));
                ui->leftButton->setObjectName("accept");
                ui->leftButton->setToolTip(tr("Accept transfer"));
            }
            break;
        case FileStatus::CANCELED:
        case FileStatus::BROKEN:
            ui->leftButton->hide();
            ui->rightButton->hide();
            break;
        case FileStatus::FINISHED:
            ui->leftButton->setIcon(QIcon(Style::getImagePath("fileTransferInstance/yes.svg")));
            ui->leftButton->setObjectName("ok");
            ui->leftButton->setToolTip(tr("Open file"));
            ui->leftButton->show();

            ui->rightButton->setIcon(QIcon(Style::getImagePath("fileTransferInstance/dir.svg")));
            ui->rightButton->setObjectName("dir");
            ui->rightButton->setToolTip(tr("Open file directory"));
            ui->rightButton->show();

            break;
        default:
            qWarning() << "Invalid file status" << file.fileId;
            break;
    }
}

void FileTransferWidget::handleButton(QPushButton* btn) {
    qDebug() << "handle button for file:" << fileInfo.fileName;
    CoreFile* coreFile = CoreFile::getInstance();
    if (fileInfo.direction == FileDirection::SENDING) {
        if (btn->objectName() == "cancel") {
            coreFile->cancelFileSend(fileInfo.receiver, fileInfo.fileId);
        } else if (btn->objectName() == "pause") {
            coreFile->pauseResumeFile(fileInfo.receiver, fileInfo.fileId);
        } else if (btn->objectName() == "resume") {
            coreFile->pauseResumeFile(fileInfo.receiver, fileInfo.fileId);
        }
    } else  // receiving or paused
    {
        if (btn->objectName() == "cancel") {
            coreFile->cancelFileRecv(fileInfo.receiver, fileInfo.fileId);
        } else if (btn->objectName() == "pause") {
            coreFile->pauseResumeFile(fileInfo.receiver, fileInfo.fileId);
        } else if (btn->objectName() == "resume") {
            coreFile->pauseResumeFile(fileInfo.receiver, fileInfo.fileId);
        } else if (btn->objectName() == "accept") {
            QString path =
                    Settings::getInstance().getGlobalAutoAcceptDir() + "/" + fileInfo.fileName;
            if (path.isEmpty()) return;

            qDebug() << "accept file save to path:" << path;
            acceptTransfer(path);
        }
    }

    if (btn->objectName() == "ok" || btn->objectName() == "previewButton") {
        Widget::confirmExecutableOpen(QFileInfo(fileInfo.filePath));
    } else if (btn->objectName() == "dir") {
        QString dirPath = QFileInfo(fileInfo.filePath).dir().path();
        QDesktopServices::openUrl(QUrl::fromLocalFile(dirPath));
    }
}

void FileTransferWidget::showPreview(const QString& filename) {
    qDebug() << __func__ << filename;

    static const QStringList previewExtensions = {"png", "jpeg", "jpg", "gif", "svg",
                                                  "PNG", "JPEG", "JPG", "GIF", "SVG"};

    if (previewExtensions.contains(QFileInfo(filename).suffix())) {
        // Subtract to make border visible
        const int size = qMax(ui->previewButton->width(), ui->previewButton->height()) - 4;

        QFile imageFile(filename);
        if (!imageFile.open(QIODevice::ReadOnly)) {
            return;
        }

        auto imageFileData = imageFile.readAll();
        auto image = QImage::fromData(imageFileData);
        if (image.isNull()) {
            qWarning() << "Unable to read the image data!";
            return;
        }

        const int exifOrientation =
                getExifOrientation(imageFileData.constData(), imageFileData.size());
        if (exifOrientation) {
            applyTransformation(exifOrientation, image);
        }

        const QPixmap iconPixmap = scaleCropIntoSquare(QPixmap::fromImage(image), size);

        ui->previewButton->setIcon(QIcon(iconPixmap));
        ui->previewButton->setIconSize(iconPixmap.size());
        ui->previewButton->show();

        // Show mouseover preview, but make sure it's not larger than 50% of the screen
        // width/height
        const QRect desktopSize = QApplication::desktop()->geometry();
        const int maxPreviewWidth{desktopSize.width() / 2};
        const int maxPreviewHeight{desktopSize.height() / 2};
        const QImage previewImage = [&image, maxPreviewWidth, maxPreviewHeight]() {
            if (image.width() > maxPreviewWidth || image.height() > maxPreviewHeight) {
                return image.scaled(maxPreviewWidth, maxPreviewHeight, Qt::KeepAspectRatio,
                                    Qt::SmoothTransformation);
            } else {
                return image;
            }
        }();

        QByteArray imageData;
        QBuffer buffer(&imageData);
        buffer.open(QIODevice::WriteOnly);
        previewImage.save(&buffer, "PNG");
        buffer.close();
        ui->previewButton->setToolTip("<img src=data:image/png;base64," + imageData.toBase64() +
                                      "/>");
    }
}

void FileTransferWidget::onLeftButtonClicked() { handleButton(ui->leftButton); }

void FileTransferWidget::onRightButtonClicked() { handleButton(ui->rightButton); }

void FileTransferWidget::onPreviewButtonClicked() { handleButton(ui->previewButton); }

QPixmap FileTransferWidget::scaleCropIntoSquare(const QPixmap& source, const int targetSize) {
    QPixmap result;

    // Make sure smaller-than-icon images (at least one dimension is smaller) will not be
    // upscaled
    if (source.width() < targetSize || source.height() < targetSize) {
        result = source;
    } else {
        result = source.scaled(targetSize, targetSize, Qt::KeepAspectRatioByExpanding,
                               Qt::SmoothTransformation);
    }

    // Then, image has to be cropped (if needed) so it will not overflow rectangle
    // Only one dimension will be bigger after Qt::KeepAspectRatioByExpanding
    if (result.width() > targetSize) {
        return result.copy((result.width() - targetSize) / 2, 0, targetSize, targetSize);
    } else if (result.height() > targetSize) {
        return result.copy(0, (result.height() - targetSize) / 2, targetSize, targetSize);
    }

    // Picture was rectangle in the first place, no cropping
    return result;
}

int FileTransferWidget::getExifOrientation(const char* data, const int size) {
    ExifData* exifData =
            exif_data_new_from_data(reinterpret_cast<const unsigned char*>(data), size);

    if (!exifData) {
        return 0;
    }

    int orientation = 0;
    const ExifByteOrder byteOrder = exif_data_get_byte_order(exifData);
    const ExifEntry* const exifEntry = exif_data_get_entry(exifData, EXIF_TAG_ORIENTATION);
    if (exifEntry) {
        orientation = exif_get_short(exifEntry->data, byteOrder);
    }
    exif_data_free(exifData);
    return orientation;
}

void FileTransferWidget::applyTransformation(const int orientation, QImage& image) {
    QTransform exifTransform;
    switch (static_cast<ExifOrientation>(orientation)) {
        case ExifOrientation::TopLeft:
            break;
        case ExifOrientation::TopRight:
            image = image.mirrored(1, 0);
            break;
        case ExifOrientation::BottomRight:
            exifTransform.rotate(180);
            break;
        case ExifOrientation::BottomLeft:
            image = image.mirrored(0, 1);
            break;
        case ExifOrientation::LeftTop:
            exifTransform.rotate(90);
            image = image.mirrored(0, 1);
            break;
        case ExifOrientation::RightTop:
            exifTransform.rotate(-90);
            break;
        case ExifOrientation::RightBottom:
            exifTransform.rotate(-90);
            image = image.mirrored(0, 1);
            break;
        case ExifOrientation::LeftBottom:
            exifTransform.rotate(90);
            break;
        default:
            qWarning() << "Invalid exif orientation passed to applyTransformation!";
    }
    image = image.transformed(exifTransform);
}

void FileTransferWidget::updateWidget(ToxFile const& file) {
    assert(file == fileInfo);

    fileInfo = file;

    // If we repainted on every packet our gui would be *very* slow
    bool bTransmitNeedsUpdate = fileProgress.needsUpdate();

    updatePreview(file);
    updateFileProgress(file);
    updateWidgetText(file);
    updateWidgetColor(file);
    setupButtons(file);
    updateSignals(file);

    lastStatus = file.status;

    // trigger repaint
    switch (file.status) {
        case FileStatus::TRANSMITTING:
            if (!bTransmitNeedsUpdate) {
                break;
            }
        // fallthrough
        default:
            update();
    }
}

#include "src/chatlog/content/simpletext.h"

SimpleIconButtonItem::SimpleIconButtonItem(QGraphicsItem* parent) : QGraphicsItem(parent) {
    _color = Qt::black;
    _bgColor = QColor(0, 0, 0, 0);
}

void SimpleIconButtonItem::setSizeHint(QSizeF size) {
    prepareGeometryChange();
    _sh = size;
    resized = !size.isNull();
}

void SimpleIconButtonItem::setIcon(const QIcon& icon) {
    _icon = icon;
    if (!resized) prepareGeometryChange();
    update();
}

void SimpleIconButtonItem::setIconSize(const QSize& size) {
    if (_iconSize != size) {
        _iconSize = size;
        update();
    }
}

void SimpleIconButtonItem::setBackgroundColor(const QColor& color) {
    _bgColor = color;
    update();
}

void SimpleIconButtonItem::setColor(const QColor& color) {
    _color = color;
    update();
}

QSizeF SimpleIconButtonItem::sizeHint() const {
    if (_sh.isEmpty()) {
        _sh = QSizeF(_iconSize) + QSizeF(4.0, 4.0);
    }
    return _sh;
}

QRectF SimpleIconButtonItem::boundingRect() const { return QRectF(QPoint(0, 0), sizeHint()); }

void SimpleIconButtonItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
                                 QWidget* widget) {
    QRectF rect = boundingRect();
    if (_bgColor.alpha() > 0) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(_bgColor);
        painter->drawRect(rect);
    }

    if (_icon.isNull()) return;
    QPixmap pix = _icon.pixmap(widget->window()->windowHandle(), _iconSize);
    QSizeF s = QSizeF(pix.size()) / pix.devicePixelRatioF();
    QSizeF offset = (rect.size() - s) / 2;

    painter->drawPixmap(rect.left() + offset.width(), rect.top() + offset.height(), pix);
}

bool SimpleIconButtonItem::sceneEvent(QEvent* event) {
    switch (event->type()) {
        case QEvent::GraphicsSceneMousePress: {
            auto* me = static_cast<QGraphicsSceneMouseEvent*>(event);
            if (me->button() == Qt::LeftButton && me->buttons() == Qt::LeftButton) {
                mousePressed = true;
                return true;
            }
            break;
        }
        case QEvent::GraphicsSceneMouseRelease: {
            auto* me = static_cast<QGraphicsSceneMouseEvent*>(event);
            if (mousePressed && me->button() == Qt::LeftButton) {
                if (this->isUnderMouse()) {
                    emit clicked();
                }
                mousePressed = false;
                return true;
            }
            break;
        }
        default:
            break;
    }
    return QGraphicsItem::sceneEvent(event);
}

ProgressBarItem::ProgressBarItem(QGraphicsItem* parent) : QGraphicsItem(parent) {
    _size = QSize(100, 12);
}

void ProgressBarItem::setValue(int value) {
    value = qBound(minVal, value, maxVal);
    if (_value != value) {
        _value = value;
        update();
    }
}

void ProgressBarItem::setGeometry(QRectF rect) {
    prepareGeometryChange();
    setPos(rect.topLeft());
    _size = rect.size();
}

QRectF ProgressBarItem::boundingRect() const { return QRectF(QPointF(0, 0), _size); }

void ProgressBarItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
                            QWidget* widget) {
    QColor bg_color = Style::getColor(Style::StatusActive);
    QRectF br = boundingRect();
    painter->save();
    painter->setPen(Qt::NoPen);
    painter->setBrush(bg_color);
    painter->drawRect(br);

    QRectF chunc_rect = br.adjusted(2, 2, -2.5, -2.5);
    qreal progress = qreal(_value) / qreal(maxVal - minVal + 1) * chunc_rect.width();
    chunc_rect.setWidth(progress);
    if (!chunc_rect.isEmpty()) {
        QColor trunk_color = Style::getColor(Style::TransferMiddle);
        painter->setBrush(trunk_color);
        painter->drawRect(chunc_rect);
    }
    painter->restore();
}

FileTransferItem::FileTransferItem(ToxFile file)
        : fileInfo(file)
        , backgroundColor(Style::getColor(Style::TransferMiddle))
        , buttonColor(Style::getColor(Style::TransferWait))
        , buttonBackgroundColor(Style::getColor(Style::GroundBase))
        , active(true) {
    previewButton = new SimpleIconButtonItem(this);
    previewButton->setCursor(Qt::PointingHandCursor);

    leftButton = new SimpleIconButtonItem(this);
    rightButton = new SimpleIconButtonItem(this);
    leftButton->setCursor(Qt::PointingHandCursor);
    rightButton->setCursor(Qt::PointingHandCursor);
    leftButton->setBackgroundColor(buttonColor);
    rightButton->setBackgroundColor(buttonColor);

    fileNameItem = new SimpleText();
    fileNameItem->setParentItem(this);
    fileSizeItem = new SimpleText();
    fileSizeItem->setParentItem(this);
    progressItem = new SimpleText();
    progressItem->setParentItem(this);
    etaItem = new SimpleText();
    etaItem->setParentItem(this);
    fileSizeItem->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    progressItem->setAlignment(Qt::AlignCenter);
    etaItem->setAlignment(Qt::AlignVCenter | Qt::AlignRight);

    // 限制尺寸
    // leftButton->setSizeHint(QSizeF(26, 26));
    // rightButton->setSizeHint(QSizeF(26, 26));
    previewButton->setSizeHint(QSizeF(60, 60));

    progressBar = new ProgressBarItem(this);

    previewButton->hide();
    fileNameItem->setText(file.fileName);
    progressBar->setValue(0);
    fileSizeItem->setText(FileTransferWidget::getHumanReadableSize(file.fileSize));
    etaItem->setText(QString());

    connect(leftButton, &SimpleIconButtonItem::clicked, this,
            &FileTransferItem::onLeftButtonClicked);
    connect(rightButton, &SimpleIconButtonItem::clicked, this,
            &FileTransferItem::onRightButtonClicked);
    connect(previewButton, &SimpleIconButtonItem::clicked, this,
            &FileTransferItem::onPreviewButtonClicked);

    lastStatus =
            file.status == FileStatus::FINISHED ? FileStatus::INITIALIZING : FileStatus::FINISHED;
    updateWidget(file);

    connect(&GUI::getInstance(), &GUI::themeApplyRequest, this, &FileTransferItem::reloadTheme);
    reloadTheme();
}

FileTransferItem::~FileTransferItem() {}

bool FileTransferItem::tryRemoveFile(const QString& filepath) {
    QFile tmp(filepath);
    bool writable = tmp.open(QIODevice::WriteOnly);
    tmp.remove();
    return writable;
}

void FileTransferItem::onFileTransferUpdate(ToxFile file) { updateWidget(file); }

bool FileTransferItem::isActive() const { return active; }

void FileTransferItem::acceptTransfer(const QString& filepath) {
    if (filepath.isEmpty()) {
        return;
    }

    // test if writable
    if (!tryRemoveFile(filepath)) {
        GUI::showWarning(
                FileTransferWidget::tr("Location not writable", "Title of permissions popup"),
                FileTransferWidget::tr(
                        "You do not have permission to write that location. Choose another, or "
                        "cancel the save dialog.",
                        "text of permissions popup"));
        return;
    }

    // everything ok!
    CoreFile* coreFile = CoreFile::getInstance();
    coreFile->acceptFileRecvRequest(fileInfo.receiver, fileInfo.fileId, filepath);
}

void FileTransferItem::setBackgroundColor(FileStatus status) {
    QColor color;
    bool whiteLabel = false;
    switch (status) {
        case FileStatus::INITIALIZING:
        case FileStatus::PAUSED:
        case FileStatus::TRANSMITTING:
            color = Style::getColor(Style::TransferMiddle);
            whiteLabel = false;
            break;
        case FileStatus::BROKEN:
        case FileStatus::CANCELED:
            color = Style::getColor(Style::TransferBad);
            whiteLabel = true;
            break;
        case FileStatus::FINISHED:
            color = Style::getColor(Style::TransferGood);
            whiteLabel = true;
            break;
        default:
            return;
            break;
    }
    backgroundColor = color;

    QColor label_color;
    if (whiteLabel)
        label_color = Style::getExtColor("fileTransfer.label.whiteColor");
    else
        label_color = Style::getExtColor("fileTransfer.label.blackColor");
    fileNameItem->setColor(label_color);
    fileSizeItem->setColor(label_color);
    progressItem->setColor(label_color);
    update();
}

void FileTransferItem::setButtonColor(const QColor& c) {
    if (c != buttonColor) {
        buttonColor = c;
        leftButton->setBackgroundColor(c);
        rightButton->setBackgroundColor(c);
        update();
    }
}

void FileTransferItem::updateWidgetColor(ToxFile const& file) {
    if (lastStatus == file.status) {
        return;
    }

    switch (file.status) {
        case FileStatus::INITIALIZING:
        case FileStatus::PAUSED:
        case FileStatus::TRANSMITTING:
        case FileStatus::BROKEN:
        case FileStatus::CANCELED:
        case FileStatus::FINISHED:
            setBackgroundColor(file.status);
            break;
        default:
            qWarning() << "Invalid file status" << file.fileId;
            break;
    }
}

void FileTransferItem::updateWidgetText(ToxFile const& file) {
    if (lastStatus == file.status && file.status != FileStatus::PAUSED) {
        return;
    }

    switch (file.status) {
        case FileStatus::INITIALIZING:
            if (file.direction == FileDirection::SENDING) {
                progressItem->setText(
                        FileTransferWidget::tr("Waiting to send...", "file transfer widget"));
            } else {
                progressItem->setText(FileTransferWidget::tr("Accept to receive this file",
                                                             "file transfer widget"));
            }
            layoutDirty = true;
            break;
        case FileStatus::PAUSED:
            //        ui->etaLabel->setText("");
            //        if (file.pauseStatus.localPaused()) {
            //            ui->progressLabel->setText(tr("Paused", "file transfer widget"));
            //        } else {
            //            ui->progressLabel->setText(tr("Remote Paused", "file transfer widget"));
            //        }
            break;
        case FileStatus::TRANSMITTING:
            etaItem->setText("");
            etaItem->setText(FileTransferWidget::tr("Resuming...", "file transfer widget"));
            layoutDirty = true;
            break;
        case FileStatus::BROKEN:
        case FileStatus::CANCELED:
            break;
        case FileStatus::FINISHED:
            break;
        default:
            qWarning() << "Invalid file status" << file.fileId;
            break;
    }
}

void FileTransferItem::updateFileProgress(ToxFile const& file) {
    switch (file.status) {
        case FileStatus::INITIALIZING:
            break;
        case FileStatus::PAUSED:
            fileProgress.resetSpeed();
            break;
        case FileStatus::TRANSMITTING: {
            if (!fileProgress.needsUpdate()) {
                break;
            }

            fileProgress.addSample(file);
            auto speed = fileProgress.getSpeed();
            auto progress = fileProgress.getProgress();
            auto remainingTime = fileProgress.getTimeLeftSeconds();

            progressBar->setValue(static_cast<int>(progress * 100.0));

            // update UI
            if (speed > 0) {
                // ETA
                QTime toGo = QTime(0, 0).addSecs(remainingTime);
                QString format = toGo.hour() > 0 ? "hh:mm:ss" : "mm:ss";
                etaItem->setText(toGo.toString(format));
            } else {
                etaItem->setText("");
            }

            progressItem->setText(FileTransferWidget::getHumanReadableSize(speed) + "/s");
            layoutDirty = true;
            break;
        }
        case FileStatus::BROKEN:
        case FileStatus::CANCELED:
        case FileStatus::FINISHED: {
            layoutDirty =
                    progressBar->isVisible() || progressItem->isVisible() || etaItem->isVisible();
            progressBar->hide();
            progressItem->hide();
            etaItem->hide();
            layoutChildren();
            break;
        }
        default:
            qWarning() << "Invalid file status" << file.fileId;
            //        assert(false);
    }
}

void FileTransferItem::updateSignals(ToxFile const& file) {
    if (lastStatus == file.status) {
        return;
    }

    switch (file.status) {
        case FileStatus::CANCELED:
        case FileStatus::BROKEN:
        case FileStatus::FINISHED:
            active = false;
            disconnect(CoreFile::getInstance(), nullptr, this, nullptr);
            break;
        case FileStatus::INITIALIZING:
        case FileStatus::PAUSED:
        case FileStatus::TRANSMITTING:
            break;
        default:
            qWarning() << "Invalid file status" << file.fileId;
            break;
    }
}

void FileTransferItem::updatePreview(ToxFile const& file) {
    if (lastStatus == file.status) {
        return;
    }

    switch (file.status) {
        case FileStatus::INITIALIZING:
        case FileStatus::PAUSED:
        case FileStatus::TRANSMITTING:
        case FileStatus::BROKEN:
        case FileStatus::CANCELED:
            if (file.direction == FileDirection::SENDING) {
                showPreview(file.filePath);
            }
            break;
        case FileStatus::FINISHED:
            showPreview(file.filePath);
            break;
        default:
            qWarning() << "Invalid file status" << file.fileId;
    }
}

void FileTransferItem::setupButtons(ToxFile const& file) {
    if (lastStatus == file.status && file.status != FileStatus::PAUSED) {
        return;
    }

    switch (file.status) {
        case FileStatus::TRANSMITTING:
            leftButton->setIcon(QIcon(Style::getImagePath("fileTransferInstance/pause.svg")));
            leftButton->setObjectName("pause");
            leftButton->setToolTip(FileTransferWidget::tr("Pause transfer"));

            rightButton->setIcon(QIcon(Style::getImagePath("fileTransferInstance/no.svg")));
            rightButton->setObjectName("cancel");
            rightButton->setToolTip(FileTransferWidget::tr("Cancel transfer"));

            setButtonColor(Style::getColor(Style::TransferGood));
            break;

        case FileStatus::PAUSED:
            //        if (file.pauseStatus.localPaused()) {
            //            ui->leftButton->setIcon(QIcon(Style::getImagePath("fileTransferInstance/arrow_white.svg")));
            //            ui->leftButton->setObjectName("resume");
            //            ui->leftButton->setToolTip(tr("Resume transfer"));
            //        } else {
            //            ui->leftButton->setIcon(QIcon(Style::getImagePath("fileTransferInstance/pause.svg")));
            //            ui->leftButton->setObjectName("pause");
            //            ui->leftButton->setToolTip(tr("Pause transfer"));
            //        }

            rightButton->setIcon(QIcon(Style::getImagePath("fileTransferInstance/no.svg")));
            rightButton->setObjectName("cancel");
            rightButton->setToolTip(FileTransferWidget::tr("Cancel transfer"));

            setButtonColor(Style::getColor(Style::TransferMiddle));
            break;

        case FileStatus::INITIALIZING:
            rightButton->setIcon(QIcon(Style::getImagePath("fileTransferInstance/no.svg")));
            rightButton->setObjectName("cancel");
            rightButton->setToolTip(FileTransferWidget::tr("Cancel transfer"));

            if (file.direction == FileDirection::SENDING) {
                leftButton->setIcon(QIcon(Style::getImagePath("fileTransferInstance/pause.svg")));
                leftButton->setObjectName("pause");
                leftButton->setToolTip(FileTransferWidget::tr("Pause transfer"));
            } else {
                leftButton->setIcon(QIcon(Style::getImagePath("fileTransferInstance/yes.svg")));
                leftButton->setObjectName("accept");
                leftButton->setToolTip(FileTransferWidget::tr("Accept transfer"));
            }
            setButtonColor(Style::getColor(Style::TransferWait));
            break;
        case FileStatus::CANCELED:
        case FileStatus::BROKEN: {
            if (leftButton->isVisible() || rightButton->isVisible()) {
                leftButton->hide();
                rightButton->hide();
                layoutDirty = true;
            }
        } break;
        case FileStatus::FINISHED:
            layoutDirty = !leftButton->isVisible() || !rightButton->isVisible();

            leftButton->setIcon(QIcon(Style::getImagePath("fileTransferInstance/yes.svg")));
            leftButton->setObjectName("ok");
            leftButton->setToolTip(FileTransferWidget::tr("Open file"));
            leftButton->show();

            rightButton->setIcon(QIcon(Style::getImagePath("fileTransferInstance/dir.svg")));
            rightButton->setObjectName("dir");
            rightButton->setToolTip(FileTransferWidget::tr("Open file directory"));
            rightButton->show();
            setButtonColor(QColor(0, 0, 0, 0));
            break;
        default:
            qWarning() << "Invalid file status" << file.fileId;
            break;
    }
}

void FileTransferItem::handleButton(SimpleIconButtonItem* btn) {
    CoreFile* coreFile = CoreFile::getInstance();
    if (fileInfo.direction == FileDirection::SENDING) {
        if (btn->objectName() == "cancel") {
            coreFile->cancelFileSend(fileInfo.receiver, fileInfo.fileId);
        } else if (btn->objectName() == "pause") {
            coreFile->pauseResumeFile(fileInfo.receiver, fileInfo.fileId);
        } else if (btn->objectName() == "resume") {
            coreFile->pauseResumeFile(fileInfo.receiver, fileInfo.fileId);
        }
    } else  // receiving or paused
    {
        if (btn->objectName() == "cancel") {
            coreFile->cancelFileRecv(fileInfo.receiver, fileInfo.fileId);
        } else if (btn->objectName() == "pause") {
            coreFile->pauseResumeFile(fileInfo.receiver, fileInfo.fileId);
        } else if (btn->objectName() == "resume") {
            coreFile->pauseResumeFile(fileInfo.receiver, fileInfo.fileId);
        } else if (btn->objectName() == "accept") {
            QString path = QFileDialog::getSaveFileName(
                    Q_NULLPTR,
                    FileTransferWidget::tr("Save a file", "Title of the file saving dialog"),
                    Settings::getInstance().getGlobalAutoAcceptDir() + "/" + fileInfo.fileName);
            acceptTransfer(path);
        }
    }

    if (btn->objectName() == "ok" || btn->objectName() == "previewButton") {
        Widget::confirmExecutableOpen(QFileInfo(fileInfo.filePath));
    } else if (btn->objectName() == "dir") {
        QString dirPath = QFileInfo(fileInfo.filePath).dir().path();
        QDesktopServices::openUrl(QUrl::fromLocalFile(dirPath));
    }
}

void FileTransferItem::showPreview(const QString& filename) {
    qDebug() << __func__ << filename;

    static const QStringList previewExtensions = {"png", "jpeg", "jpg", "gif", "svg"};

    if (previewExtensions.contains(QFileInfo(filename).suffix().toLower())) {
        QFile imageFile(filename);
        if (!imageFile.open(QIODevice::ReadOnly)) {
            return;
        }

        auto imageFileData = imageFile.readAll();
        auto image = QImage::fromData(imageFileData);
        if (image.isNull()) {
            qWarning() << "Unable to read the image data!";
            return;
        }

        // Subtract to make border visible
        QSizeF temp = previewButton->boundingRect().size();
        const int size = qMax(temp.width(), temp.height()) - 4;
        const int exifOrientation =
                getExifOrientation(imageFileData.constData(), imageFileData.size());
        if (exifOrientation) {
            applyTransformation(exifOrientation, image);
        }

        const QPixmap iconPixmap = scaleCropIntoSquare(QPixmap::fromImage(image), size);
        previewButton->setIcon(QIcon(iconPixmap));
        previewButton->setIconSize(QSize(size, size));
        layoutDirty = !previewButton->isVisible();
        previewButton->show();

        // Show mouseover preview, but make sure it's not larger than 50% of the screen
        // width/height
        const QRect desktopSize = QApplication::desktop()->geometry();
        const int maxPreviewWidth{desktopSize.width() / 2};
        const int maxPreviewHeight{desktopSize.height() / 2};
        const QImage previewImage = [&image, maxPreviewWidth, maxPreviewHeight]() {
            if (image.width() > maxPreviewWidth || image.height() > maxPreviewHeight) {
                return image.scaled(maxPreviewWidth, maxPreviewHeight, Qt::KeepAspectRatio,
                                    Qt::SmoothTransformation);
            } else {
                return image;
            }
        }();

        QByteArray imageData;
        QBuffer buffer(&imageData);
        buffer.open(QIODevice::WriteOnly);
        previewImage.save(&buffer, "PNG");
        buffer.close();
        previewButton->setToolTip("<img src=data:image/png;base64," + imageData.toBase64() + "/>");

        layoutChildren();
    }
}

bool FileTransferItem::drawButtonAreaNeeded() const {
    return (rightButton->isVisible() || leftButton->isVisible()) &&
           !(leftButton->isVisible() && leftButton->objectName() == "ok");
}

void FileTransferItem::setWidth(qreal width) {
    // 固定大小，需要处理一次
    layoutChildren();
}

void FileTransferItem::layoutChildren() {
    if (!layoutDirty) {
        return;
    }

    layoutDirty = false;
    QRectF br = boundingRect();
    br.adjust(4, 2, -2, -2);

    // 预览按钮
    if (previewButton->isVisible()) {
        QRectF bound = previewButton->boundingRect();
        int offsety = (br.height() - bound.height()) / 2;
        previewButton->setPos(br.topLeft() + QPoint(0, offsety));
        br.setLeft(br.left() + bound.width());
    }
    br.setLeft(br.left() + 6);
    // 右按钮
    if (rightButton->isVisible()) {
        QRectF bound = rightButton->boundingRect();
        rightButton->setPos(br.topRight() - QPointF(bound.width(), 0));
        br.setRight(br.right() - bound.width() - 2);
    }
    // 左按钮
    if (leftButton->isVisible()) {
        QRectF bound = leftButton->boundingRect();
        leftButton->setPos(br.topRight() - QPointF(bound.width(), 0));
        br.setRight(br.right() - bound.width());
    }
    br.adjust(0, 2, -6, -2);
    // 中心区域
    {
        // 名称宽度
        const int spacing = 2;
        // 最下面放进度条， 名称行和状态行均分多余空间
        qreal nameHeight = fileNameItem->sizeHint().height();
        qreal statusHeight = progressItem->sizeHint().height();
        qreal progressHeight = progressBar->boundingRect().height();
        qreal space = br.height() - nameHeight - statusHeight - progressHeight - spacing - spacing;
        if (space <= 0) {
            fileNameItem->setPos(br.topLeft());
            fileNameItem->setSize(QSizeF(br.width(), -1));
            progressBar->setPos(br.left(),
                                br.top() + nameHeight + spacing + statusHeight + spacing);

            // 调整到状态区域
            br.adjust(0, nameHeight + spacing, 0, -statusHeight);
        } else {
            qreal ave = (br.height() - progressHeight) / 2;
            fileNameItem->setPos(br.left(), br.top());
            fileNameItem->setSize(QSizeF(br.width(), ave));
            progressBar->setPos(br.left(), br.bottom() - progressHeight);
            br.adjust(0, ave + spacing, 0, -progressHeight);
        }
        // 状态区域
        {
            QList<SimpleText*> lables;
            fileSizeItem->isVisible() ? lables.append(fileSizeItem) : void(0);
            progressItem->isVisible() ? lables.append(progressItem) : void(0);
            etaItem->isVisible() ? lables.append(etaItem) : void(0);
            QList<qreal> baseSize;
            for (SimpleText* label : lables) {
                baseSize << label->sizeHint().width();
            }
            // 按照1:1比例分配空间
            QList<qreal> sizes = layoutExpandingSize(baseSize, br.width());
            qreal offset = 0;
            for (int i = 0; i < lables.count(); i++) {
                qreal w = sizes.value(i);
                lables.value(i)->setSize(QSizeF(w, br.height()));
                lables.value(i)->setPos(br.topLeft() + QPointF(offset, 0));
                offset += w;
            }
        }
    }
}

QRectF FileTransferItem::boundingRect() const { return QRectF(QPointF(0, 0), QSize(300, 64)); }

void FileTransferItem::fontChanged(const QFont&) {
    const QFont f = Style::getFont(Style::Big);

    fileNameItem->setFont(f);
    fileSizeItem->setFont(f);
    progressItem->setFont(f);
    etaItem->setFont(f);

    layoutDirty = true;
    layoutChildren();
}

void FileTransferItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
                             QWidget* widget) {
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setPen(Qt::NoPen);
    painter->setBrush(backgroundColor);
    painter->drawRoundedRect(boundingRect().adjusted(0.5, 0.5, -0.5, -0.5), 4.0, 4.0);
}

void FileTransferItem::reloadTheme() {
    setBackgroundColor(lastStatus);
    fontChanged(QFont());
}

void FileTransferItem::onLeftButtonClicked() { handleButton(leftButton); }

void FileTransferItem::onRightButtonClicked() { handleButton(rightButton); }

void FileTransferItem::onPreviewButtonClicked() { handleButton(previewButton); }

QPixmap FileTransferItem::scaleCropIntoSquare(const QPixmap& source, int targetSize) {
    QPixmap result;

    // Make sure smaller-than-icon images (at least one dimension is smaller) will not be
    // upscaled
    if (source.width() < targetSize || source.height() < targetSize) {
        result = source;
    } else {
        result = source.scaled(targetSize, targetSize, Qt::KeepAspectRatioByExpanding,
                               Qt::SmoothTransformation);
    }

    // Then, image has to be cropped (if needed) so it will not overflow rectangle
    // Only one dimension will be bigger after Qt::KeepAspectRatioByExpanding
    if (result.width() > targetSize) {
        return result.copy((result.width() - targetSize) / 2, 0, targetSize, targetSize);
    } else if (result.height() > targetSize) {
        return result.copy(0, (result.height() - targetSize) / 2, targetSize, targetSize);
    }

    // Picture was rectangle in the first place, no cropping
    return result;
}

int FileTransferItem::getExifOrientation(const char* data, const int size) {
    ExifData* exifData =
            exif_data_new_from_data(reinterpret_cast<const unsigned char*>(data), size);

    if (!exifData) {
        return 0;
    }

    int orientation = 0;
    const ExifByteOrder byteOrder = exif_data_get_byte_order(exifData);
    const ExifEntry* const exifEntry = exif_data_get_entry(exifData, EXIF_TAG_ORIENTATION);
    if (exifEntry) {
        orientation = exif_get_short(exifEntry->data, byteOrder);
    }
    exif_data_free(exifData);
    return orientation;
}

void FileTransferItem::applyTransformation(const int oritentation, QImage& image) {
    QTransform exifTransform;
    switch (static_cast<ExifOrientation>(oritentation)) {
        case ExifOrientation::TopLeft:
            break;
        case ExifOrientation::TopRight:
            image = image.mirrored(1, 0);
            break;
        case ExifOrientation::BottomRight:
            exifTransform.rotate(180);
            break;
        case ExifOrientation::BottomLeft:
            image = image.mirrored(0, 1);
            break;
        case ExifOrientation::LeftTop:
            exifTransform.rotate(90);
            image = image.mirrored(0, 1);
            break;
        case ExifOrientation::RightTop:
            exifTransform.rotate(-90);
            break;
        case ExifOrientation::RightBottom:
            exifTransform.rotate(-90);
            image = image.mirrored(0, 1);
            break;
        case ExifOrientation::LeftBottom:
            exifTransform.rotate(90);
            break;
        default:
            qWarning() << "Invalid exif orientation passed to applyTransformation!";
    }
    image = image.transformed(exifTransform);
}

void FileTransferItem::updateWidget(ToxFile const& file) {
    assert(file == fileInfo);

    fileInfo = file;

    // If we repainted on every packet our gui would be *very* slow
    bool bTransmitNeedsUpdate = fileProgress.needsUpdate();

    updatePreview(file);
    updateFileProgress(file);
    updateWidgetText(file);
    updateWidgetColor(file);
    setupButtons(file);
    updateSignals(file);

    lastStatus = file.status;

    layoutChildren();

    // trigger repaint
    switch (file.status) {
        case FileStatus::TRANSMITTING:
            if (!bTransmitNeedsUpdate) {
                break;
            }
            // fallthrough
        default:
            update();
    }
}

QList<qreal> layoutExpandingSize(QList<qreal> baseSize, qreal total) {
    if (baseSize.count() == 0) return {};
    if (baseSize.count() == 1) return {total};
    if (baseSize.count() == 2) {
        qreal ext = total - baseSize.at(0) - baseSize.at(1);
        qreal diff = baseSize.at(0) - baseSize.at(1);
        if (abs(ext) < abs(diff)) {
            if ((diff >= 0) ^ (ext < 0))
                return QList<qreal>({baseSize[0] + ext, baseSize[1]});
            else
                return QList<qreal>({baseSize[0], baseSize[1] + ext});
        }
        return {total / 2, total / 2};
    }
    qreal ext = total;
    using InfoPair = QPair<int, qreal>;
    QList<InfoPair> infos;
    const int result_count = baseSize.count();
    bool enough = false;
    {
        int i = 0;
        for (qreal v : baseSize) {
            ext -= v;
            infos << InfoPair{i++, v};
        }
        if (ext > 0) {
            std::sort(infos.begin(), infos.end(),
                      [](const InfoPair& a, const InfoPair& b) { return a.second < b.second; });
            enough = infos.last().second * result_count <= total;
        }

        else {
            std::sort(infos.begin(), infos.end(),
                      [](const InfoPair& a, const InfoPair& b) { return a.second > b.second; });
            enough = infos.last().second * result_count >= total;
        }
    }
    QList<qreal> result;
    if (enough) {
        qreal ave = total / result_count;
        for (int i = 0; i < result_count; i++) {
            result.append(ave);
        }
        return result;
    }
    bool terminate = false;
    const int count = infos.count();
    for (int i = 0; i < count - 1 && !terminate; i++) {
        qreal diff = infos[i + 1].second - infos[i].second;
        qreal total_diff = diff * (i + 1);
        qreal target = 0.0;
        if (terminate = abs(total_diff) >= abs(ext); terminate)
            target = infos[i].second + ext / (i + 1);
        else
            target = infos[i + 1].second;
        for (int k = 0; k <= i; k++) {
            infos[k].second = target;
        }
        ext -= total_diff;
    }
    std::sort(infos.begin(), infos.end(),
              [](const InfoPair& a, const InfoPair& b) { return a.first < b.first; });

    std::transform(infos.begin(), infos.end(), std::back_inserter(result),
                   [](const InfoPair& a) { return a.second; });
    return result;
}
