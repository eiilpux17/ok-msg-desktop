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

#include "ChatLogView.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QDebug>
#include <QMouseEvent>
#include <QScrollBar>
#include <QShortcut>
#include <QTimer>
#include <algorithm>
#include <cassert>

#include "../persistence/settings.h"
#include "Bus.h"
#include "chatlinecontent.h"
#include "chatlinecontentproxy.h"
#include "chatmessage.h"
#include "content/filetransferwidget.h"
#include "src/lib/storage/settings/style.h"


#include "src/nexus.h"
#include "src/persistence/profile.h"
#include "application.h"

/**
 * @var ChatLog::repNameAfter
 * @brief repetition interval sender name (sec)
 */
namespace module::im {

template <class T> T clamp(T x, T min, T max) {
    if (x > max) return max;
    if (x < min) return min;
    return x;
}

static bool lessThanBSRectTop(const IChatItem::Ptr& lhs, const qreal& rhs) {
    return lhs->sceneBoundingRect().top() < rhs;
}

static bool lessThanBSRectBottom(const IChatItem::Ptr& lhs, const qreal& rhs) {
    return lhs->sceneBoundingRect().bottom() < rhs;
}
static bool lessThanRowIndex(const IChatItem::Ptr& lhs, const IChatItem::Ptr& rhs) {
    return lhs->getRow() < rhs->getRow();
}

ChatLogView::ChatLogView(const ContactId& cid, QWidget* parent)
        : QGraphicsView(parent), cid(cid), scrollBarValue{0} {

    // Create the scene
    busyScene = new QGraphicsScene(this);
    scene = new QGraphicsScene(this);
    scene->setItemIndexMethod(QGraphicsScene::BspTreeIndex);
    setScene(scene);
    // 连接滚动条的 valueChanged 信号到槽函数
    connect(verticalScrollBar(), &QScrollBar::valueChanged, this,
            &ChatLogView::onVScrollBarValueChanged);

    // Cfg.
    setInteractive(true);
    setAcceptDrops(false);
    setAlignment(Qt::AlignTop | Qt::AlignLeft);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setDragMode(QGraphicsView::NoDrag);
    setViewportUpdateMode(MinimalViewportUpdate);
    setContextMenuPolicy(Qt::CustomContextMenu);

    /**
     * Menu
     */
    menu = new QMenu(this);
    menu->addActions(actions());
    menu->addSeparator();


    clearAction = menu->addAction(QIcon::fromTheme("edit-clear"), QString(), this,
                                  &ChatLogView::clearChat, QKeySequence(Qt::CTRL + Qt::Key_L));
    addAction(clearAction);

    // select all action (ie. Ctrl+A)
    selectAllAction = menu->addAction(QIcon::fromTheme("edit-select-all"), QString(), this,
                                      &ChatLogView::selectAll, QKeySequence::SelectAll);

    //    selectAllAction->setIcon(QIcon::fromTheme("edit-select-all"));
    //    selectAllAction->setShortcut(QKeySequence::SelectAll);
    //    connect(selectAllAction, &QAction::triggered, this, [this]() { selectAll(); });
    addAction(selectAllAction);

    selectMultipleAction =
            menu->addAction(QIcon::fromTheme("edit-select-multiple"), QString(), this,
                            &ChatLogView::selectMultiple, QKeySequence(Qt::CTRL + Qt::Key_M));
    addAction(selectAllAction);

    //    copyLinkAction = menu.addAction(QIcon(), QString(), this, SLOT(copyLink()));
    //    menu.addSeparator();

    // copy action (ie. Ctrl+C)
    copyAction = new QAction(this);
    copyAction->setIcon(QIcon::fromTheme("edit-copy"));
    copyAction->setShortcut(QKeySequence::Copy);
    copyAction->setEnabled(false);
    connect(copyAction, &QAction::triggered, this, [this]() { copySelectedText(); });
    addAction(copyAction);

    // Ctrl+Insert shortcut
    QShortcut* copyCtrlInsShortcut = new QShortcut(QKeySequence(Qt::CTRL, Qt::Key_Insert), this);
    connect(copyCtrlInsShortcut, &QShortcut::activated, this, [this]() { copySelectedText(); });

    // This timer is used to scroll the view while the user is
    // moving the mouse past the top/bottom edge of the widget while selecting.
    selectionTimer = new QTimer(this);
    selectionTimer->setInterval(1000 / 30);
    selectionTimer->setSingleShot(false);
    selectionTimer->start();
    connect(selectionTimer, &QTimer::timeout, this, &ChatLogView::onSelectionTimerTimeout);

    // Background worker
    // Updates the layout of all chat-lines after a resize
    workerTimer = new QTimer(this);
    workerTimer->setSingleShot(false);
    workerTimer->setInterval(5);
    connect(workerTimer, &QTimer::timeout, this, &ChatLogView::onWorkerTimeout);

    // This timer is used to detect multiple clicks
    multiClickTimer = new QTimer(this);
    multiClickTimer->setSingleShot(true);
    multiClickTimer->setInterval(QApplication::doubleClickInterval());
    connect(multiClickTimer, &QTimer::timeout, this, &ChatLogView::onMultiClickTimeout);

    // selection
    connect(this, &ChatLogView::selectionChanged, this, [this]() {
        copyAction->setEnabled(hasTextToBeCopied());
        copySelectedText(true);
    });

    connect(this, &ChatLogView::customContextMenuRequested, this, &ChatLogView::onChatContextMenuRequested);

    // setBackgroundBrush(
    //         QBrush(lib::settings::Style::getInstance()->getColor(lib::settings::Style::ColorPalette::GroundBase),
    //                Qt::SolidPattern));

    retranslateUi();

    connect(ok::Application::Instance()->bus(), &ok::Bus::languageChanged,
            [&](const QString& locale0) {
                retranslateUi();
            });

    auto s = Nexus::getProfile()->getSettings();
    connect(s, &Settings::emojiFontPointSizeChanged, this, &ChatLogView::forceRelayout);
}

ChatLogView::~ChatLogView() {
    // Remove chatlines from scene
    for (const IChatItem::Ptr& l : lines) l->removeFromScene();

    if (busyNotification) busyNotification->removeFromScene();

    if (typingNotification) typingNotification->removeFromScene();
}

void ChatLogView::onChatContextMenuRequested(QPoint pos) {
    auto sender = dynamic_cast<QWidget*>(QObject::sender());

    // 获取右键时位于该坐标的item
    auto item = itemAt(pos);

    pos = sender->mapToGlobal(pos);
    if (!item) {
        menu->exec(pos);
        return;
    }
    emit itemContextMenuRequested(item, pos);

    //    // If we right-clicked on a link, give the option to copy it
    //    bool clickedOnLink = false;
    //    Text* clickedText = qobject_cast<Text*>(chatLog->getContentFromGlobalPos(pos));
    //    if (clickedText) {
    //        QPointF scenePos = chatLog->mapToScene(chatLog->mapFromGlobal(pos));
    //        QString linkTarget = clickedText->getLinkAt(scenePos);
    //        if (!linkTarget.isEmpty()) {
    //            clickedOnLink = true;
    //            copyLinkAction->setData(linkTarget);
    //        }
    //    }
    //    copyLinkAction->setVisible(clickedOnLink);
}

void ChatLogView::clearSelection() {
    if (selectionMode == SelectionMode::None) return;

    for (int i = selFirstRow; i <= selLastRow; ++i) lines[i]->selectionCleared();

    selFirstRow = -1;
    selLastRow = -1;
    selClickedCol = -1;
    selClickedRow = -1;

    selectionMode = SelectionMode::None;
    emit selectionChanged();

    updateMultiSelectionRect();
}

QRect ChatLogView::getVisibleRect() const {
    return mapToScene(viewport()->rect()).boundingRect().toRect();
}

void ChatLogView::updateSceneRect() {
    setSceneRect(calculateSceneRect());
}

void ChatLogView::layout(int start, int end, qreal width) {
    if (lines.empty()) return;

    qreal h = 0.0;

    // Line at start-1 is considered to have the correct position. All following lines are
    // positioned in respect to this line.
    if (start - 1 >= 0) h = lines[start - 1]->sceneBoundingRect().bottom() + lineSpacing;

    start = clamp<int>(start, 0, lines.size());
    end = clamp<int>(end + 1, 0, lines.size());

    for (int i = start; i < end; ++i) {
        IChatItem* l = lines[i].get();

        l->layout(width, QPointF(0.0, h));
        h += l->sceneBoundingRect().height() + lineSpacing;
    }
}

void ChatLogView::mousePressEvent(QMouseEvent* ev) {
    QGraphicsView::mousePressEvent(ev);

    if (ev->button() == Qt::LeftButton) {
        clickPos = ev->pos();
        clearSelection();
    }

    if (lastClickButton == ev->button()) {
        // Counts only single clicks and first click of doule click
        clickCount++;
    } else {
        clickCount = 1;  // restarting counter
        lastClickButton = ev->button();
    }
    lastClickPos = ev->pos();

    // Triggers on odd click counts
    handleMultiClickEvent();
}

void ChatLogView::mouseReleaseEvent(QMouseEvent* ev) {
    QGraphicsView::mouseReleaseEvent(ev);

    selectionScrollDir = AutoScrollDirection::NoDirection;

    multiClickTimer->start();
}

void ChatLogView::mouseMoveEvent(QMouseEvent* ev) {
    QGraphicsView::mouseMoveEvent(ev);

    QPointF scenePos = mapToScene(ev->pos());

    if (ev->buttons() & Qt::LeftButton) {
        // autoscroll
        if (ev->pos().y() < 0)
            selectionScrollDir = AutoScrollDirection::Up;
        else if (ev->pos().y() > height())
            selectionScrollDir = AutoScrollDirection::Down;
        else
            selectionScrollDir = AutoScrollDirection::NoDirection;

        // select
        if (selectionMode == SelectionMode::None &&
            (clickPos - ev->pos()).manhattanLength() > QApplication::startDragDistance()) {
            QPointF sceneClickPos = mapToScene(clickPos.toPoint());
            IChatItem::Ptr line = findLineByPosY(scenePos.y());
            ChatLineContent* content =
                    (line && line->selectable()) ? getContentFromPos(sceneClickPos) : nullptr;
            if (content) {
                selClickedRow = content->getRow();
                selClickedCol = content->getColumn();
                selFirstRow = content->getRow();
                selLastRow = content->getRow();

                content->selectionStarted(sceneClickPos);

                selectionMode = SelectionMode::Precise;

                // ungrab mouse grabber
                if (scene->mouseGrabberItem()) scene->mouseGrabberItem()->ungrabMouse();
            } else if (line.get()) {
                selClickedRow = line->getRow();
                selFirstRow = selClickedRow;
                selLastRow = selClickedRow;

                selectionMode = SelectionMode::Multi;
            }
        }

        if (selectionMode != SelectionMode::None) {
            ChatLineContent* content = getContentFromPos(scenePos);
            IChatItem::Ptr line = findLineByPosY(scenePos.y());

            int row;

            if (content) {
                row = content->getRow();
                int col = content->getColumn();

                if (row == selClickedRow && col == selClickedCol) {
                    // 从多行切换到精确选择，重新处理起点
                    if (selectionMode == SelectionMode::Multi)
                        content->selectionStarted(mapToScene(clickPos.toPoint()));
                    selectionMode = SelectionMode::Precise;
                    content->selectionMouseMove(scenePos);

                } else if (col != selClickedCol) {
                    selectionMode = SelectionMode::Multi;
                    // 多行选择，直接全选
                    lines[selClickedRow]->selectAll();
                }
            } else if (line.get()) {
                row = line->getRow();

                if (row != selClickedRow) {
                    selectionMode = SelectionMode::Multi;
                    lines[selClickedRow]->selectAll();
                }
            } else {
                return;
            }
            // 对变动的多行范围，重新设定选择
            auto selectionLineChange = [this](int row1, int row2, bool down) {
                int start = std::min(row1, row2) + (down ? 1 : 0);
                int end = std::max(row1, row2) + (down ? 1 : 0);
                bool isMore = ((row2 > row1) && down) || ((row2 < row1) && !down);
                for (int i = start; i < end; i++) {
                    IChatItem::Ptr line = lines.at(i);
                    if (line) {
                        if (isMore)
                            line->selectAll();
                        else
                            line->selectionCleared();
                    }
                }
            };

            if (row >= selClickedRow) {
                if (selectionMode == SelectionMode::Multi)
                    selectionLineChange(selLastRow, row, true);
                selLastRow = row;
            }

            if (row <= selClickedRow) {
                if (selectionMode == SelectionMode::Multi)
                    selectionLineChange(selFirstRow, row, false);
                selFirstRow = row;
            }

            updateMultiSelectionRect();
        }

        emit selectionChanged();
    }
}

// Much faster than QGraphicsScene::itemAt()!
ChatLineContent* ChatLogView::getContentFromPos(QPointF scenePos) const {
    if (lines.empty()) return nullptr;

    auto itr = std::lower_bound(lines.cbegin(), lines.cend(), scenePos.y(), lessThanBSRectBottom);

    // find content
    if (itr != lines.cend() && (*itr)->sceneBoundingRect().contains(scenePos))
        return (*itr)->contentAtPos(scenePos);

    return nullptr;
}

bool ChatLogView::isOverSelection(QPointF scenePos) const {
    if (selectionMode == SelectionMode::Precise) {
        ChatLineContent* content = getContentFromPos(scenePos);

        if (content) return content->isOverSelection(scenePos);
    } else if (selectionMode == SelectionMode::Multi) {
        if (selectionBox.contains(scenePos)) return true;
    }

    return false;
}

qreal ChatLogView::useableWidth() const {
    return width() - verticalScrollBar()->sizeHint().width() - margins.right() - margins.left();
}

void ChatLogView::reposition(int start, int end, qreal deltaY) {
    if (lines.isEmpty()) return;

    start = clamp<int>(start, 0, lines.size() - 1);
    end = clamp<int>(end + 1, 0, lines.size());

    for (int i = start; i < end; ++i) {
        IChatItem* l = lines[i].get();
        l->moveBy(0, deltaY);
    }
}

void ChatLogView::insertChatlineAtBottom(IChatItem::Ptr l) {
    if (!l.get()) return;

    bool stickToBtm = stickToBottom();

    // insert
    int from = lines.size();
    l->setRow(from);
    l->addToScene(scene);

    lines.append(l);

    // partial refresh
    layout(from, lines.size(), useableWidth());
    updateSceneRect();

    if (stickToBtm) scrollToBottom();

    checkVisibility();
    updateTypingNotification();
}

void ChatLogView::insertChatlineOnTop(IChatItem::Ptr l) {
    if (!l.get()) return;

    insertChatlinesOnTop(QList<IChatItem::Ptr>() << l);
}

void ChatLogView::insertChatlinesOnTop(const QList<IChatItem::Ptr>& newLines) {
    if (newLines.isEmpty()) return;

    QGraphicsScene::ItemIndexMethod oldIndexMeth = scene->itemIndexMethod();
    scene->setItemIndexMethod(QGraphicsScene::NoIndex);

    // alloc space for old and new lines
    QVector<IChatItem::Ptr> combLines;
    combLines.reserve(newLines.size() + lines.size());

    // add the new lines
    int i = 0;
    for (IChatItem::Ptr l : newLines) {
        l->addToScene(scene);
        l->visibilityChanged(false);
        l->setRow(i++);
        combLines.push_back(l);
    }

    // add the old lines
    for (IChatItem::Ptr l : lines) {
        l->setRow(i++);
        combLines.push_back(l);
    }

    lines = combLines;

    moveSelectionRectDownIfSelected(newLines.size());

    scene->setItemIndexMethod(oldIndexMeth);

    // redo layout
    startResizeWorker();
}

bool ChatLogView::stickToBottom() const {
    return verticalScrollBar()->value() == verticalScrollBar()->maximum();
}

void ChatLogView::scrollToBottom() {
    updateSceneRect();
    verticalScrollBar()->setValue(verticalScrollBar()->maximum());
}

void ChatLogView::startResizeWorker() {
    if (lines.empty()) return;

    // (re)start the worker
    if (!workerTimer->isActive()) {
        // these values must not be reevaluated while the worker is running
        workerStb = stickToBottom();

        if (!visibleLines.empty()) workerAnchorLine = visibleLines.first();
    }

    // switch to busy scene displaying the busy notification if there is a lot
    // of text to be resized
    int txt = 0;
    for (const IChatItem::Ptr& line : lines) {
        if (line->itemType() == IChatItemType::TEXT) {
            for (ChatLineContent* content : line->contents()) {
                if (content) {
                    txt += content->getContentAsText().size();
                };
            }
        }
    }

    workerLastIndex = 0;
    workerTimer->start();

    verticalScrollBar()->hide();
}

void ChatLogView::mouseDoubleClickEvent(QMouseEvent* ev) {
    QPointF scenePos = mapToScene(ev->pos());
    IChatItem::Ptr line = findLineByPosY(scenePos.y());
    if (!line) {
        return;
    }
    ChatLineContent* content = line->selectable() ? getContentFromPos(scenePos) : nullptr;

    if (content) {
        content->selectionDoubleClick(scenePos);
        selClickedCol = content->getColumn();
        selClickedRow = content->getRow();
        selFirstRow = content->getRow();
        selLastRow = content->getRow();
        selectionMode = SelectionMode::Precise;

        emit selectionChanged();
    }

    if (lastClickButton == ev->button()) {
        // Counts the second click of double click
        clickCount++;
    } else {
        clickCount = 1;  // restarting counter
        lastClickButton = ev->button();
    }
    lastClickPos = ev->pos();

    // Triggers on even click counts
    handleMultiClickEvent();
}

QString ChatLogView::getSelectedText() const {
    if (selectionMode == SelectionMode::Precise) {
        auto content = lines[selClickedRow]->centerContent();
        if (content) return content->getSelectedText();
        return QString();
    } else if (selectionMode == SelectionMode::Multi) {
        // build a nicely formatted message
        QString out;

        for (int i = selFirstRow; i <= selLastRow; ++i) {
            // if (lines[i]->content[1]->getText().isEmpty())
            //     continue;

            // QString timestamp = lines[i]->content[2]->getText().isEmpty()
            //                         ? tr("pending")
            //                         : lines[i]->content[2]->getText();
            // QString author = lines[i]->content[0]->getText();
            // QString msg = lines[i]->content[1]->getText();

            // out +=
            //     QString(out.isEmpty() ? "[%2] %1: %3" : "\n[%2] %1: %3").arg(author, timestamp,
            //     msg);
        }

        return out;
    }

    return QString();
}

bool ChatLogView::isEmpty() const {
    return lines.isEmpty();
}

bool ChatLogView::hasTextToBeCopied() const {
    return selectionMode != SelectionMode::None;
}

IChatItem::Ptr ChatLogView::getTypingNotification() const {
    return typingNotification;
}

QVector<IChatItem::Ptr> ChatLogView::getLines() {
    return lines;
}

IChatItem::Ptr ChatLogView::getLatestLine() const {
    if (!lines.empty()) {
        return lines.last();
    }
    return nullptr;
}

IChatItem::Ptr ChatLogView::getFirstLine() const {
    if (!lines.empty()) {
        return lines.first();
    }
    return nullptr;
}

/**
 * @brief Finds the chat line object at a position on screen
 * @param pos Position on screen in global coordinates
 * @sa getContentFromPos()
 */
ChatLineContent* ChatLogView::getContentFromGlobalPos(QPoint pos) const {
    return getContentFromPos(mapToScene(mapFromGlobal(pos)));
}

void ChatLogView::copySelectedText(bool toSelectionBuffer) const {
    QString text = getSelectedText();
    QClipboard* clipboard = QApplication::clipboard();

    if (clipboard && !text.isNull())
        clipboard->setText(text, toSelectionBuffer ? QClipboard::Selection : QClipboard::Clipboard);
}

void ChatLogView::setBusyNotification(IChatItem::Ptr notification) {
    if (!notification.get()) return;

    busyNotification = notification;
    busyNotification->addToScene(busyScene);
    busyNotification->visibilityChanged(true);
}

void ChatLogView::setTypingNotification(IChatItem::Ptr notification) {
    typingNotification = notification;
    typingNotification->visibilityChanged(true);
    typingNotification->setVisible(false);
    typingNotification->addToScene(scene);
    updateTypingNotification();
}

void ChatLogView::setTypingNotificationVisible(bool visible) {
    if (typingNotification.get()) {
        typingNotification->setVisible(visible);
        updateTypingNotification();
    }
}

void ChatLogView::scrollToLine(IChatItem::Ptr line) {
    if (!line.get()) return;

    updateSceneRect();
    verticalScrollBar()->setValue(line->sceneBoundingRect().top());
}

void ChatLogView::selectAll() {
    if (lines.empty()) return;

    clearSelection();

    selectionMode = SelectionMode::Multi;
    selFirstRow = 0;
    selLastRow = lines.size() - 1;

    updateMultiSelectionRect();
    emit selectionChanged();
}

void ChatLogView::selectMultiple() {
    if (lines.empty()) return;
}

void ChatLogView::fontChanged(const QFont& font) {
    for (IChatItem::Ptr l : lines) {
        l->fontChanged(font);
    }
}

void ChatLogView::reloadTheme() {
    // setBackgroundBrush(
            // QBrush(lib::settings::Style::getInstance()->getColor(lib::settings::Style::ColorPalette::GroundBase),
                   // Qt::SolidPattern));
    // selectionRectColor =
            // lib::settings::Style::getInstance()->getColor(lib::settings::Style::ColorPalette::SelectText);

    auto css = lib::settings::Style::getInstance()->getStylesheet("chatArea.css");
    setStyleSheet(css);

    for (IChatItem::Ptr l : lines) {
        l->reloadTheme();
    }
}

void ChatLogView::forceRelayout() {
    startResizeWorker();
}

void ChatLogView::checkVisibility(bool causedByScroll) {
    if (lines.empty()) return;

    // find first visible line
    auto lowerBound = std::lower_bound(lines.cbegin(), lines.cend(), getVisibleRect().top(),
                                       lessThanBSRectBottom);

    // find last visible line
    auto upperBound = std::lower_bound(lowerBound, lines.cend(), getVisibleRect().bottom(),
                                       lessThanBSRectTop);

    const IChatItem::Ptr lastLineBeforeVisible =
            lowerBound == lines.cbegin() ? IChatItem::Ptr() : *std::prev(lowerBound);

    // set visibilty
    QList<IChatItem::Ptr> newVisibleLines;
    for (auto itr = lowerBound; itr != upperBound; ++itr) {
        newVisibleLines.append(*itr);

        if (!visibleLines.contains(*itr)) (*itr)->visibilityChanged(true);

        visibleLines.removeOne(*itr);
    }

    // these lines are no longer visible
    for (IChatItem::Ptr line : visibleLines) line->visibilityChanged(false);

    visibleLines = newVisibleLines;

    // enforce order
    std::sort(visibleLines.begin(), visibleLines.end(), lessThanRowIndex);

    // if (!visibleLines.empty())
    //  qDebug() << "visible from " << visibleLines.first()->getRow() << "to " <<
    //  visibleLines.last()->getRow() << " total " << visibleLines.size();

    if (!visibleLines.isEmpty()) {
        emit firstVisibleLineChanged(lastLineBeforeVisible, visibleLines.at(0));
    }

    if (causedByScroll && lowerBound != lines.cend() && lowerBound->get()->getRow() == 0) {
        emit loadHistoryLower();
    }
}

void ChatLogView::scrollContentsBy(int dx, int dy) {
    QGraphicsView::scrollContentsBy(dx, dy);
    checkVisibility(true);
}

void ChatLogView::resizeEvent(QResizeEvent* ev) {
    bool stb = stickToBottom();

    if (ev->size().width() != ev->oldSize().width()) {
        startResizeWorker();
        stb = false;  // let the resize worker handle it
    }

    QGraphicsView::resizeEvent(ev);

    if (stb) scrollToBottom();

    updateBusyNotification();
}

void ChatLogView::updateMultiSelectionRect() {
    if (selectionMode == SelectionMode::Multi && selFirstRow >= 0 && selLastRow >= 0) {
        QRectF selBBox;
        selBBox = selBBox.united(lines[selFirstRow]->sceneBoundingRect());
        selBBox = selBBox.united(lines[selLastRow]->sceneBoundingRect());
        selectionBox = selBBox;
    } else {
        selectionBox = QRectF();
    }
}

void ChatLogView::updateTypingNotification() {
    IChatItem* notification = typingNotification.get();
    if (!notification) return;

    qreal posY = 0.0;

    if (!lines.empty()) posY = lines.last()->sceneBoundingRect().bottom() + lineSpacing;

    notification->layout(useableWidth(), QPointF(0.0, posY));
}

void ChatLogView::updateBusyNotification() {
    if (busyNotification.get()) {
        // repoisition the busy notification (centered)
        busyNotification->layout(
                useableWidth(),
                getVisibleRect().topLeft() + QPointF(0, getVisibleRect().height() / 2.0));
    }
}

IChatItem::Ptr ChatLogView::findLineByPosY(qreal yPos) const {
    auto itr = std::lower_bound(lines.cbegin(), lines.cend(), yPos, lessThanBSRectBottom);

    if (itr != lines.cend()) return *itr;

    return IChatItem::Ptr();
}

QRectF ChatLogView::calculateSceneRect() const {
    qreal bottom = (lines.empty() ? 0.0 : lines.last()->sceneBoundingRect().bottom());

    if (typingNotification.get() != nullptr)
        bottom += typingNotification->sceneBoundingRect().height() + lineSpacing;

    return QRectF(-margins.left(), -margins.top(), useableWidth(),
                  bottom + margins.bottom() + margins.top());
}

void ChatLogView::onSelectionTimerTimeout() {
    const int scrollSpeed = 10;

    switch (selectionScrollDir) {
        case AutoScrollDirection::Up:
            verticalScrollBar()->setValue(verticalScrollBar()->value() - scrollSpeed);
            break;
        case AutoScrollDirection::Down:
            verticalScrollBar()->setValue(verticalScrollBar()->value() + scrollSpeed);
            break;
        default:
            break;
    }
}

void ChatLogView::onWorkerTimeout() {
    // Fairly arbitrary but
    // large values will make the UI unresponsive
    const int stepSize = 50;

    layout(workerLastIndex, workerLastIndex + stepSize, useableWidth());
    workerLastIndex += stepSize;

    // done?
    if (workerLastIndex >= lines.size()) {
        workerTimer->stop();

        // switch back to the scene containing the chat messages
        setScene(scene);

        // make sure everything gets updated
        updateSceneRect();
        checkVisibility();
        updateTypingNotification();
        updateMultiSelectionRect();

        // scroll
        if (workerStb)
            scrollToBottom();
        else
            scrollToLine(workerAnchorLine);

        // don't keep a Ptr to the anchor line
        workerAnchorLine = IChatItem::Ptr();

        // hidden during busy screen
        verticalScrollBar()->show();

        emit workerTimeoutFinished();
    }
}

void ChatLogView::onMultiClickTimeout() {
    clickCount = 0;
}

void ChatLogView::onVScrollBarValueChanged(int value) {
    scrollBarValue = value;
    //        qDebug() <<"height"<< sceneRect().height()<<" value"<<value
    //                << verticalScrollBar()->maximum();
    if (scrollBarValue == verticalScrollBar()->maximum()) {
        // 当垂直滚动条的值改变时触发
        //           qDebug() <<"readAll";
        emit readAll();
    }
}

void ChatLogView::handleMultiClickEvent() {
    // Ignore single or double clicks
    if (clickCount < 2) return;

    switch (clickCount) {
        case 3:
            QPointF scenePos = mapToScene(lastClickPos);
            ChatLineContent* content = getContentFromPos(scenePos);

            if (content) {
                content->selectionTripleClick(scenePos);
                selClickedCol = content->getColumn();
                selClickedRow = content->getRow();
                selFirstRow = content->getRow();
                selLastRow = content->getRow();
                selectionMode = SelectionMode::Precise;

                emit selectionChanged();
            }
            break;
    }
}

void ChatLogView::showEvent(QShowEvent*) {
    if (verticalScrollBar()->maximum() == 0) {
        // 没有滚动条，发射“已读完”信号
        emit readAll();
    }

    if (verticalScrollBar()->maximum() == verticalScrollBar()->value()) {
        // 有滚动条，但是已经卷动到底部，发射“以读完”信号
        emit readAll();
    }
}

void ChatLogView::focusInEvent(QFocusEvent* ev) {
    QGraphicsView::focusInEvent(ev);

    if (selectionMode != SelectionMode::None) {
        for (int i = selFirstRow; i <= selLastRow; ++i) lines[i]->selectionFocusChanged(true);
    }
}

void ChatLogView::focusOutEvent(QFocusEvent* ev) {
    QGraphicsView::focusOutEvent(ev);

    if (selectionMode != SelectionMode::None) {
        if (selFirstRow >= 0) {
            for (int i = selFirstRow; i <= selLastRow; ++i) lines[i]->selectionFocusChanged(false);
        }
    }
}

void ChatLogView::retranslateUi() {
    clearAction->setText(tr("Clear displayed messages"));
    copyAction->setText(tr("Copy"));
    selectAllAction->setText(tr("Select all"));
    selectMultipleAction->setText(tr("Select multiple"));
}

bool ChatLogView::isActiveFileTransfer(IChatItem::Ptr l) {
    ChatLineContent* content = l->centerContent();
    ChatLineContentProxy* proxy = qobject_cast<ChatLineContentProxy*>(content);
    if (proxy) {
        QWidget* widget = proxy->getWidget();
        FileTransferWidget* transferWidget = qobject_cast<FileTransferWidget*>(widget);
        if (transferWidget && transferWidget->isActive()) return true;
    }
    return false;
}

/**
 * @brief Adjusts the selection based on chatlog changing lines
 * @param offset Amount to shift selection rect up by. Must be non-negative.
 */
void ChatLogView::moveSelectionRectUpIfSelected(int offset) {
    assert(offset >= 0);
    switch (selectionMode) {
        case SelectionMode::None:
            return;
        case SelectionMode::Precise:
            movePreciseSelectionUp(offset);
            break;
        case SelectionMode::Multi:
            moveMultiSelectionUp(offset);
            break;
    }
}

/**
 * @brief Adjusts the selections based on chatlog changing lines
 * @param offset removed from the lines indexes. Must be non-negative.
 */
void ChatLogView::moveSelectionRectDownIfSelected(int offset) {
    assert(offset >= 0);
    switch (selectionMode) {
        case SelectionMode::None:
            return;
        case SelectionMode::Precise:
            movePreciseSelectionDown(offset);
            break;
        case SelectionMode::Multi:
            moveMultiSelectionDown(offset);
            break;
    }
}

void ChatLogView::movePreciseSelectionDown(int offset) {
    assert(selFirstRow == selLastRow && selFirstRow == selClickedRow);
    const int lastLine = lines.size() - 1;
    if (selClickedRow + offset > lastLine) {
        clearSelection();
    } else {
        const int newRow = selClickedRow + offset;
        selClickedRow = newRow;
        selLastRow = newRow;
        selFirstRow = newRow;
        emit selectionChanged();
    }
}

void ChatLogView::movePreciseSelectionUp(int offset) {
    assert(selFirstRow == selLastRow && selFirstRow == selClickedRow);
    if (selClickedRow < offset) {
        clearSelection();
    } else {
        const int newRow = selClickedRow - offset;
        selClickedRow = newRow;
        selLastRow = newRow;
        selFirstRow = newRow;
        emit selectionChanged();
    }
}

void ChatLogView::moveMultiSelectionUp(int offset) {
    if (selLastRow < offset) {  // entire selection now out of bounds
        clearSelection();
    } else {
        selLastRow -= offset;
        selClickedRow = std::max(0, selClickedRow - offset);
        selFirstRow = std::max(0, selFirstRow - offset);
        updateMultiSelectionRect();
        emit selectionChanged();
    }
}

void ChatLogView::moveMultiSelectionDown(int offset) {
    const int lastLine = lines.size() - 1;
    if (selFirstRow + offset > lastLine) {  // entire selection now out of bounds
        clearSelection();
    } else {
        selFirstRow += offset;
        selClickedRow = std::min(lastLine, selClickedRow + offset);
        selLastRow = std::min(lastLine, selLastRow + offset);
        updateMultiSelectionRect();
        emit selectionChanged();
    }
}

void ChatLogView::clearChat() {
    clear();
}

void ChatLogView::clear() {
    clearSelection();

    QVector<IChatItem::Ptr> savedLines;

    for (IChatItem::Ptr l : lines) {
        if (isActiveFileTransfer(l))
            savedLines.push_back(l);
        else
            l->removeFromScene();
    }

    lines.clear();
    visibleLines.clear();
    for (IChatItem::Ptr l : savedLines) {
        insertChatlineAtBottom(l);
    }
    updateSceneRect();
}
}  // namespace module::im
