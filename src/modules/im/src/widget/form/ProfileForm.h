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

#pragma once

#include <QLabel>
#include <QLineEdit>
#include <QTimer>
#include <QVBoxLayout>


namespace Ui {
class ProfileForm;
}

namespace lib::ui {
class MaskablePixmapWidget;
class CroppingLabel;
class QRWidget;
}

namespace module::im {


class IProfileInfo;


class ClickableTE : public QLabel {
    Q_OBJECT
public:
signals:
    void clicked();

protected:
    virtual void mouseReleaseEvent(QMouseEvent*) final override {
        emit clicked();
    }
};

/**
 * 个人信息表单界面
 */
class ProfileForm : public QWidget {
    Q_OBJECT
public:
    explicit ProfileForm(QWidget* parent = nullptr);
    ~ProfileForm() override;


    bool isShown() const;

public slots:
    void onSelfAvatarLoaded(const QPixmap& pic);
    void onLogoutClicked();
    void onExitClicked();
    void showToolTip(QMouseEvent *e);

protected:
    bool eventFilter(QObject* object, QEvent* event) override;
    void showEvent(QShowEvent* e) override;
    void contextMenuEvent(QContextMenuEvent* e) override;
    void focusInEvent(QFocusEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;


private slots:
    void setPasswordButtonsText();

    void copyIdClicked();
    void onNicknameEdited();
    void onExportClicked();

    void onCopyQrClicked();
    void onSaveQrClicked();
    void onDeletePassClicked();
    void onChangePassClicked();
    void onAvatarClicked();
    void showProfilePictureContextMenu(const QPoint& point);
    void showQRCode();


private:
    void retranslateUi();

    void refreshProfiles();
    static QString getSupportedImageFilter();

private:
    Ui::ProfileForm* ui;
    lib::ui::MaskablePixmapWidget* profilePicture;
    lib::ui::QRWidget* qr;
    IProfileInfo* profileInfo;
};
}  // namespace module::im
