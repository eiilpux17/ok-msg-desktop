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

#include "ProfileForm.h"
#include <QApplication>
#include <QBuffer>
#include <QClipboard>
#include <QComboBox>
#include <QFileDialog>
#include <QGroupBox>
#include <QImageReader>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMap>
#include <QMenu>
#include <QMouseEvent>
#include <QWindow>

#include "lib/ui/gui.h"
#include "lib/ui/widget/QRWidget.h"
#include "lib/ui/widget/tools/CroppingLabel.h"
#include "src/core/core.h"
#include "src/lib/storage/settings/style.h"
#include "src/lib/ui/widget/tools/MaskablePixmap.h"
#include "src/nexus.h"
#include "src/persistence/settings.h"
#include "src/widget/ChatWidget.h"
#include "src/widget/contentlayout.h"
#include "src/widget/form/setpassworddialog.h"
#include "src/widget/form/settings/SettingsWidget.h"
#include "src/widget/widget.h"
#include "ui_ProfileForm.h"
#include "src/application.h"
#include "src/persistence/profile.h"
#include "src/model/profile/profileinfo.h"

namespace module::im {

static const QMap<IProfileInfo::SetAvatarResult, QString> SET_AVATAR_ERROR = {
        {IProfileInfo::SetAvatarResult::CanNotOpen, ProfileForm::tr("Unable to open this file.")},
        {IProfileInfo::SetAvatarResult::CanNotRead, ProfileForm::tr("Unable to read this image.")},
        {IProfileInfo::SetAvatarResult::TooLarge,
         ProfileForm::tr("The supplied image is too large.\nPlease use another image.")},
        {IProfileInfo::SetAvatarResult::EmptyPath, ProfileForm::tr("Empty path is unavaliable")},
};

static const QMap<IProfileInfo::RenameResult, QPair<QString, QString>> RENAME_ERROR = {
        {IProfileInfo::RenameResult::Error,
            {ProfileForm::tr("Failed to rename"), ProfileForm::tr("Couldn't rename the profile to \"%1\"")}},
        {IProfileInfo::RenameResult::ProfileAlreadyExists,
            {ProfileForm::tr("Profile already exists"), ProfileForm::tr("A profile named \"%1\" already exists.")}},
        {IProfileInfo::RenameResult::EmptyName, {ProfileForm::tr("Empty name"), ProfileForm::tr("Empty name is unavaliable")}},
};

static const QMap<IProfileInfo::SaveResult, QPair<QString, QString>> SAVE_ERROR = {
        {
                IProfileInfo::SaveResult::NoWritePermission,
                {ProfileForm::tr("Location not writable", "Title of permissions popup"),
                 ProfileForm::tr("You do not have permission to write that location. Choose "
                                 "another, or cancel the save dialog.",
                                 "text of permissions popup")},
        },
        {IProfileInfo::SaveResult::Error,
         {ProfileForm::tr("Failed to copy file"),
          ProfileForm::tr("The file you chose could not be written to.")}},
        {IProfileInfo::SaveResult::EmptyPath,
         {ProfileForm::tr("Empty path"), ProfileForm::tr("Empty path is unavaliable")}},
};

static const QPair<QString, QString> CAN_NOT_CHANGE_PASSWORD = {
        ProfileForm::tr("Couldn't change password"),
        ProfileForm::tr("Couldn't change password on the database, "
                        "it might be corrupted or use the old password.")};

ProfileForm::ProfileForm(QWidget* parent)
        : QWidget{parent}, ui{new Ui::ProfileForm}
        , qr{nullptr}
{
    // 设置窗口属性
    setWindowFlags(Qt::ToolTip | Qt::FramelessWindowHint); // 无边框工具提示窗口
    setAttribute(Qt::WA_ShowWithoutActivating); // 显示时不抢焦点
    setFocusPolicy(Qt::StrongFocus); // 确保控件可以获得焦点

    ui->setupUi(this);

    this->layout()->setContentsMargins(16, 5, 16, 11);
    ui->formLayout->setVerticalSpacing(2);

    profileInfo = new ProfileInfo(this);
    ui->nickname->setText(profileInfo->getNickname());
    ui->name_value->setText(profileInfo->getFullName());

    auto& c = profileInfo->getVCard();
    if (!c.emails.isEmpty()) {
        ui->email_value->setText(c.emails.at(c.emails.size() - 1).number);
    }

    if (!c.tels.isEmpty()) {
        for (auto& t : c.tels) {
            if (t.mobile) {
                ui->phone_value->setText(t.number);
            } else {
                ui->telephone_value->setText(t.number);
            }
        }
    }

    profileInfo->connectTo_usernameChanged(this, [this](const QString& val) {  //
        ui->name_value->setText(val);
    });
    connect(ui->nickname, &QLineEdit::editingFinished, this, &ProfileForm::onNicknameEdited);
    profileInfo->connectTo_vCardChanged(this, [this](const VCard& vCard) {
        ui->nickname->setText(vCard.nickname);
        if (!vCard.emails.isEmpty())
            ui->email_value->setText(vCard.emails.at(vCard.emails.size() - 1).number);
    });

    connect(ui->exitButton, &QPushButton::clicked, this, &ProfileForm::onExitClicked);
    connect(ui->logoutButton, &QPushButton::clicked, this, &ProfileForm::onLogoutClicked);

    for (QComboBox* cb : findChildren<QComboBox*>()) {
        cb->installEventFilter(this);
        cb->setFocusPolicy(Qt::StrongFocus);
    }

    // avatar

    QSize size(100, 100);
    profilePicture = new lib::ui::MaskablePixmapWidget(this, size, ":/img/avatar_mask.svg");
    profilePicture->setPixmap(QPixmap(":/img/contact_dark.svg"));
    profilePicture->setContextMenuPolicy(Qt::CustomContextMenu);
    profilePicture->setClickable(true);
    profilePicture->setObjectName("selfAvatar");
    profilePicture->installEventFilter(this);
    profilePicture->setAccessibleName("Profile avatar");
    profilePicture->setAccessibleDescription("Set a profile avatar shown to all contacts");
    connect(profilePicture, &lib::ui::MaskablePixmapWidget::clicked, this,
            &ProfileForm::onAvatarClicked);
    connect(profilePicture, &lib::ui::MaskablePixmapWidget::customContextMenuRequested, this,
            &ProfileForm::showProfilePictureContextMenu);

    connect(Nexus::getProfile(), &Profile::selfAvatarChanged, this,
            &ProfileForm::onSelfAvatarLoaded);

    ui->avatarBox->addWidget(profilePicture);
    ui->avatarBox->setAlignment(profilePicture, Qt::AlignCenter);
    onSelfAvatarLoaded(profileInfo->getAvatar());

    // QrCode
    ui->qrcodeButton->setIcon(QIcon(lib::settings::Style::getImagePath("window/qrcode.svg")));
    ui->qrcodeButton->setCursor(Qt::PointingHandCursor);
    // bodyUI->qrcodeButton->hide();
    qr = new lib::ui::QRWidget(size, this);
    qr->setVisible(false);
    qr->setWindowFlags(Qt::Popup);
    qr->setQRData(profileInfo->getUsername());
    // bodyUI->publicGroup->layout()->addWidget(qr);

    setStyleSheet(lib::settings::Style::getStylesheet("window/profile.css"));

    retranslateUi();
    auto a = ok::Application::Instance();
    connect(a->bus(), &ok::Bus::languageChanged,this,
            [&](const QString& locale0) {
                retranslateUi();
            });

    connect(ui->qrcodeButton, &QToolButton::clicked, this, &ProfileForm::showQRCode);

    qApp->installEventFilter(this);
}

ProfileForm::~ProfileForm() {
    delete ui;
}

bool ProfileForm::isShown() const {
    if (profilePicture->isVisible()) {
        window()->windowHandle()->alert(0);
        return true;
    }

    return false;
}

        // 可选：安装事件过滤器捕获全局鼠标事件
// bool eventFilter(QObject* obj, QEvent* event) override {
//     if (event->type() == QEvent::MouseButtonPress && obj != this) {
//         QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
//         if (!geometry().contains(mapFromGlobal(mouseEvent->globalPos()))) {
//             qDebug() << "Mouse clicked outside at global" << mouseEvent->globalPos();
//         }
//     }
//     return QWidget::eventFilter(obj, event);
// }

bool ProfileForm::eventFilter(QObject* object, QEvent* event) {
    if (event->type() == QEvent::MouseButtonPress && object != this) {
        QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
        if (!geometry().contains(mapFromGlobal(mouseEvent->globalPos()))) {
            qDebug() << "Mouse clicked outside at global" << mouseEvent->globalPos();
            close();
        }
    }
    return false;
}

void ProfileForm::showToolTip(QMouseEvent *e)
{
    // 获取绝对屏幕位置
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    QPointF globalPos = e->globalPosition(); // Qt 6: 返回QPointF
#else
    QPoint globalPos = e->globalPos();       // Qt 5: 返回QPoint
#endif
    globalPos.setX(globalPos.x()+40);
    move(globalPos); // 设置显示位置
    show();
}

void ProfileForm::showEvent(QShowEvent* e) {
    ui->username->setText(profileInfo->getUsername());

    auto& c = profileInfo->getVCard();
    if (!c.adrs.isEmpty()) {
        ui->location->setText(c.adrs.at(c.adrs.size() - 1).location());
    }

    auto& avt = profileInfo->getAvatar();
    profilePicture->setPixmap(avt);
}

void ProfileForm::contextMenuEvent(QContextMenuEvent* e) {
    QMenu menu(this);
    menu.addAction("Refresh", [this]() {
        setStyleSheet(lib::settings::Style::getStylesheet("window/profile.css"));
    });
    menu.exec(e->globalPos());
}

void ProfileForm::focusInEvent(QFocusEvent *event)
{

}

void ProfileForm::focusOutEvent(QFocusEvent *event)
{
    close();
}




void ProfileForm::showProfilePictureContextMenu(const QPoint& point) {
    const QPoint pos = profilePicture->mapToGlobal(point);

    QMenu contextMenu;
    const QIcon icon = style()->standardIcon(QStyle::SP_DialogCancelButton);
    const QAction* removeAction = contextMenu.addAction(icon, tr("Remove"));
    const QAction* selectedItem = contextMenu.exec(pos);

    if (selectedItem == removeAction) {
        profileInfo->removeAvatar();
    }
}

void ProfileForm::showQRCode() {
    QPoint pos = ui->qrcodeButton->mapToGlobal(QPoint(0, ui->qrcodeButton->height() + 2));
    qr->move(pos);
    qr->show();
}

void ProfileForm::copyIdClicked() {}

void ProfileForm::onNicknameEdited() {
    profileInfo->setNickname(ui->nickname->text());
}

void ProfileForm::onSelfAvatarLoaded(const QPixmap& pic) {
    profilePicture->setPixmap(pic);
}

QString ProfileForm::getSupportedImageFilter() {
    QString res;
    for (const auto& type : QImageReader::supportedImageFormats()) {
        res += QString("*.%1 ").arg(QString(type));
    }
    return tr("Images (%1)", "filetype filter").arg(res.left(res.size() - 1));
}

void ProfileForm::onAvatarClicked() {
    const QString filter = getSupportedImageFilter();
    const QString path = QFileDialog::getOpenFileName(Q_NULLPTR, tr("Choose a profile picture"),
                                                      QDir::homePath(), filter, nullptr);

    if (path.isEmpty()) {
        return;
    }

    const IProfileInfo::SetAvatarResult result = profileInfo->setAvatar(path);
    if (result == IProfileInfo::SetAvatarResult::OK) {
        return;
    }

    lib::ui::GUI::showError(tr("Error"), SET_AVATAR_ERROR[result]);
}

void ProfileForm::onExportClicked() {
    // save dialog title
    //    const QString path = QFileDialog::getSaveFileName(Q_NULLPTR,
    //                                                      tr("Export profile"),
    //                                                      profileInfo->getUsername() +
    //                                                      Core::TOX_EXT,
    //                                                      //: save dialog filter
    //                                                      tr("Tox save file (*.tox)"));
    //    if (path.isEmpty()) {
    //        return;
    //    }
    //
    //    const IProfileInfo::SaveResult result = profileInfo->exportProfile(path);
    //    if (result == IProfileInfo::SaveResult::OK) {
    //        return;
    //    }
    //
    //    const QPair<QString, QString> error = SAVE_ERROR[result];
    //    GUI::showWarning(error.first, error.second);
}

void ProfileForm::onLogoutClicked() {
    profileInfo->logout();
}

void ProfileForm::onExitClicked() {
    profileInfo->exit();
}

void ProfileForm::setPasswordButtonsText() {
    //    if (profileInfo->isEncrypted()) {
    //        bodyUI->changePassButton->setText(tr("Change password", "button text"));
    //        bodyUI->deletePassButton->setVisible(true);
    //    } else {
    //        bodyUI->changePassButton->setText(tr("Set profile password", "button text"));
    //        bodyUI->deletePassButton->setVisible(false);
    //    }
}

void ProfileForm::onCopyQrClicked() {
    if (!qr) return;
    profileInfo->copyQr(*qr->getImage());
}

void ProfileForm::onSaveQrClicked() {
    const QString current = profileInfo->getNickname() + ".png";

    const QString path =
            QFileDialog::getSaveFileName(Q_NULLPTR, tr("Save", "save qr image"), current,
                                         tr("Save QrCode (*.png)", "save dialog filter"));
    if (path.isEmpty()) {
        return;
    }

    const IProfileInfo::SaveResult result = profileInfo->saveQr(*qr->getImage(), path);
    if (result == IProfileInfo::SaveResult::OK) {
        return;
    }

    const QPair<QString, QString> error = SAVE_ERROR[result];
    lib::ui::GUI::showWarning(error.first, error.second);
}

void ProfileForm::onDeletePassClicked() {
    if (!profileInfo->isEncrypted()) {
        lib::ui::GUI::showInfo(tr("Nothing to remove"),
                               tr("Your profile does not have a password!"));
        return;
    }

    const QString title = tr("Really delete password?", "deletion confirmation title");
    //: deletion confirmation text
    const QString body = tr("Are you sure you want to delete your password?");
    if (!lib::ui::GUI::askQuestion(title, body)) {
        return;
    }

    if (!profileInfo->deletePassword()) {
        lib::ui::GUI::showInfo(CAN_NOT_CHANGE_PASSWORD.first, CAN_NOT_CHANGE_PASSWORD.second);
    }
}

void ProfileForm::onChangePassClicked() {
    const QString title = tr("Please enter a new password.");
    auto* dialog = new SetPasswordDialog(title, QString{}, nullptr);
    if (dialog->exec() == QDialog::Rejected) {
        return;
    }

    QString newPass = dialog->getPassword();
    if (!profileInfo->setPassword(newPass)) {
        lib::ui::GUI::showInfo(CAN_NOT_CHANGE_PASSWORD.first, CAN_NOT_CHANGE_PASSWORD.second);
    }
}

void ProfileForm::retranslateUi() {
    ui->retranslateUi(this);
    setPasswordButtonsText();
}
}  // namespace module::im
