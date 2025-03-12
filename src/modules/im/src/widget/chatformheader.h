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

#ifndef CHAT_FORM_HEADER
#define CHAT_FORM_HEADER

#include <QWidget>

#include "lib/ui/widget/tools/RoundedPixmapLabel.h"
#include "src/model/FriendId.h"
#include "src/model/ContactId.h"
#include "src/model/Status.h"

class QVBoxLayout;
class QHBoxLayout;
class QPushButton;
class QToolButton;
class QLabel;

namespace lib::ui {
class CroppingLabel;
class MaskablePixmapWidget;
}  // namespace lib::ui

namespace module::im {

class CallConfirmWidget;
class PeerId;
class Contact;
class Profile;

/**
 * 聊天界面头部区域
 */
class ChatFormHeader : public QWidget {
    Q_OBJECT
public:
    enum class CallButtonState {
        Disabled = 0,   // Grey
        Avaliable = 1,  // Green
        InCall = 2,     // Red
        Outgoing = 3,   // Yellow
        Incoming = 4,   // Yellow
    };
    enum class ToolButtonState {
        Disabled = 0,  // Grey
        Off = 1,       // Green
        On = 2,        // Red
    };
    enum Mode { None = 0, Audio = 1, Video = 2, AV = Audio | Video };

    ChatFormHeader(const ContactId& contactId, QWidget* parent = nullptr);
    ~ChatFormHeader();

    void setContact(const Contact* contact);
    void removeContact();
    const Contact* getContact() const;

    void setName(const QString& newName);
    void setMode(Mode mode);

    void showOutgoingCall(bool video);



    void updateMuteMicButton(bool active, bool inputMuted);
    void updateMuteVolButton(bool active, bool outputMuted);

    void updateCallButtons();
    void updateCallButtons(Status status);
    void updateCallButtons(bool online, bool audio, bool video = false);

    void updateMuteMicButton();
    void updateMuteVolButton();

    void showCallConfirm(const PeerId& peerId, bool video, const QString& displayedName);

    void setAvatar(const QPixmap& img);
    [[nodiscard]] QSize getAvatarSize() const;

    void reloadTheme();

    // TODO: Remove
    void addWidget(QWidget* widget, int stretch = 0, Qt::Alignment alignment = Qt::Alignment());
    void addLayout(QLayout* layout);
    void addStretch();

signals:
    void searchTriggered();
    void menuTriggered();
    void moreTriggered();

    void micMuteToggle();
    void volMuteToggle();

private slots:
    void retranslateUi();
    void updateButtonsView();
    void onDisplayedNameChanged(const QString& name);

private:
    void nameChanged(const QString& name);
    void updateContactStatus(Status status);

private:
    ContactId contactId;
    const Contact* contact = nullptr;

    Mode mode;
    QVBoxLayout* headTextLayout;
    // lib::ui::MaskablePixmapWidget* avatar;
    QHBoxLayout* buttonsLayout;
    lib::ui::RoundedPixmapLabel* avatar;
    lib::ui::CroppingLabel* nameLabel;
    QLabel* statusLabel = nullptr;
    QToolButton* statusIcon = nullptr;

    QPushButton* searchButton;
    QPushButton* menuButton;
    QPushButton* moreButton;

    CallButtonState callState;
    CallButtonState videoState;


    std::optional<Profile*> mProfile;
};
}  // namespace module::im
#endif  // CHAT_FORM_HEADER
