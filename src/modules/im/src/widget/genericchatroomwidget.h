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

#ifndef GENERICCHATROOMWIDGET_H
#define GENERICCHATROOMWIDGET_H

#include "genericchatitemwidget.h"
#include "src/model/Message.h"
#include "src/model/Status.h"

class QVBoxLayout;
class QHBoxLayout;

namespace lib::ui {
class CroppingLabel;
}

namespace module::im {

class ContentLayout;
class Friend;
class Group;
class Contact;

class GenericChatroomWidget : public GenericChatItemWidget {
    Q_OBJECT
public:
    explicit GenericChatroomWidget(lib::messenger::ChatType chatType, const ContactId& cid,
                                   QWidget* parent = nullptr);
    ~GenericChatroomWidget() override;
public slots:
    virtual void setAsActiveChatroom() = 0;
    virtual void setAsInactiveChatroom() = 0;

    virtual void resetEventFlags() = 0;
    virtual QString getStatusString() const = 0;
    const ContactId& getContactId() const {
        return contactId;
    };

    virtual bool eventFilter(QObject*, QEvent*) final override;

    void setName(const QString& name);
    void setStatusMsg(const QString& status);
    QString getStatusMsg() const;
    QString getTitle() const;

    void reloadTheme();

    void activate();
    void compactChange(bool compact);

signals:
    void chatroomWidgetClicked(GenericChatroomWidget* widget);
    void newWindowOpened(GenericChatroomWidget* widget);
    void middleMouseClicked();

protected:
    void mouseReleaseEvent(QMouseEvent* event) override;
    void enterEvent(QEvent* e) override;
    void leaveEvent(QEvent* e) override;

protected:
    QPoint dragStartPos;
    QColor lastColor;
    QHBoxLayout* mainLayout = nullptr;
    QVBoxLayout* textLayout = nullptr;

    ContactId contactId;
};
}  // namespace module::im
#endif  // GENERICCHATROOMWIDGET_H
