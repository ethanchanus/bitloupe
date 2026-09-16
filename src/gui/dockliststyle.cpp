// SPDX-FileCopyrightText: 2026 BitLoupe developers
// SPDX-License-Identifier: GPL-2.0-or-later

#include "gui/dockliststyle.h"

#include "gui/uiconfig.h"

#include <QAbstractItemView>
#include <QAbstractItemModel>
#include <QAbstractTextDocumentLayout>
#include <QEvent>
#include <QFrame>
#include <QHeaderView>
#include <QHoverEvent>
#include <QLabel>
#include <QModelIndex>
#include <QMouseEvent>
#include <QObject>
#include <QPalette>
#include <QPainter>
#include <QRegularExpression>
#include <QStyledItemDelegate>
#include <QStyleOptionViewItem>
#include <QTextDocument>
#include <QTreeView>
#include <QVariant>
#include <QVector>
#include <QWidget>

namespace {

QColor hoverColorForView(const QAbstractItemView* view)
{
    const QColor themedHover = view->property("dockListHoverBackground").value<QColor>();
    if (themedHover.isValid())
        return themedHover;

    const QPalette palette = view->palette();
    const QColor base = palette.color(QPalette::Base);
    QColor hover = palette.color(QPalette::Highlight);
    if (hover == base)
        hover = base.lightness() < 128 ? base.lighter(135) : base.darker(108);
    return hover;
}

QColor hoverTextColorForView(const QAbstractItemView* view)
{
    const QColor themedText = view->property("dockListHoverForeground").value<QColor>();
    if (themedText.isValid())
        return themedText;

    const QPalette palette = view->palette();
    const QColor text = palette.color(QPalette::HighlightedText);
    return text.isValid() ? text : palette.color(QPalette::Text);
}

QColor selectedColorForView(const QAbstractItemView* view)
{
    const QColor themedSelection = view->property(
        view->hasFocus() ? "dockListActiveSelectionBackground"
                         : "dockListInactiveSelectionBackground").value<QColor>();
    if (themedSelection.isValid())
        return themedSelection;

    return view->palette().color(QPalette::Highlight);
}

QColor selectedTextColorForView(const QAbstractItemView* view)
{
    const QColor themedText = view->property(
        view->hasFocus() ? "dockListActiveSelectionForeground"
                         : "dockListInactiveSelectionForeground").value<QColor>();
    if (themedText.isValid())
        return themedText;

    return view->palette().color(QPalette::HighlightedText);
}

bool columnIsHidden(const QAbstractItemView* view, int column)
{
    const QTreeView* treeView = qobject_cast<const QTreeView*>(view);
    return treeView != nullptr && treeView->isColumnHidden(column);
}

QVector<int> visibleColumnsForIndex(const QAbstractItemView* view, const QModelIndex& index)
{
    QVector<int> columns;
    const QAbstractItemModel* model = view->model();
    if (model == nullptr)
        return columns;

    const int columnCount = model->columnCount(index.parent());
    const QTreeView* treeView = qobject_cast<const QTreeView*>(view);
    const QHeaderView* header = treeView != nullptr ? treeView->header() : nullptr;
    if (header != nullptr && header->count() == columnCount) {
        for (int visualIndex = 0; visualIndex < header->count(); ++visualIndex) {
            const int column = header->logicalIndex(visualIndex);
            if (!columnIsHidden(view, column))
                columns.append(column);
        }
        return columns;
    }

    for (int column = 0; column < columnCount; ++column) {
        if (!columnIsHidden(view, column))
            columns.append(column);
    }
    return columns;
}

QRect rowRectForIndex(const QAbstractItemView* view, const QModelIndex& index)
{
    QRect rowRect;
    for (const int column : visibleColumnsForIndex(view, index)) {
        const QRect cellRect = view->visualRect(index.sibling(index.row(), column));
        if (!cellRect.isEmpty())
            rowRect = rowRect.united(cellRect);
    }

    if (rowRect.isEmpty())
        rowRect = view->visualRect(index);

    return rowRect;
}

bool isFirstVisibleColumnForIndex(const QAbstractItemView* view, const QModelIndex& index)
{
    const QVector<int> columns = visibleColumnsForIndex(view, index);
    if (columns.isEmpty())
        return index.column() == 0;

    return index.column() == columns.first();
}

void fillRoundedRow(QPainter* painter, const QRect& rowRect, const QColor& color)
{
    if (rowRect.isEmpty())
        return;

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(Qt::NoPen);
    painter->setBrush(color);
    painter->drawRoundedRect(QRectF(rowRect).adjusted(1.0, 1.0, -1.0, -1.0),
                             UiConfig::DockHoveredItemCornerRadius,
                             UiConfig::DockHoveredItemCornerRadius);
    painter->restore();
}

QColor searchHighlightBackgroundColor()
{
    return QColor(QStringLiteral("#FFF59D")); // light yellow
}

QColor searchHighlightTextColor()
{
    return QColor(Qt::black); // kept dark so it stays legible on the light-yellow highlight
}

// Wraps every substring matching `pattern` in a highlighted <span>. Returns an empty
// string if there is no match, so the caller can fall back to plain-text painting.
QString highlightedHtmlForText(const QString& text, const QRegularExpression& pattern)
{
    QString html;
    int lastEnd = 0;
    bool matchedAny = false;
    QRegularExpressionMatchIterator it = pattern.globalMatch(text);
    while (it.hasNext()) {
        const QRegularExpressionMatch match = it.next();
        if (match.capturedLength() == 0)
            continue;
        matchedAny = true;
        html += text.mid(lastEnd, match.capturedStart() - lastEnd).toHtmlEscaped();
        html += QStringLiteral("<span style=\"background-color:%1; color:%2;\">%3</span>")
            .arg(searchHighlightBackgroundColor().name(),
                 searchHighlightTextColor().name(),
                 text.mid(match.capturedStart(), match.capturedLength()).toHtmlEscaped());
        lastEnd = match.capturedStart() + match.capturedLength();
    }
    if (!matchedAny)
        return QString();
    html += text.mid(lastEnd).toHtmlEscaped();
    return html;
}

// Preserves the delegate's word-wrap/alignment while rendering pre-built rich text.
void paintHighlightedHtml(QPainter* painter, const QStyleOptionViewItem& opt, const QString& html)
{
    QTextDocument document;
    document.setDefaultFont(opt.font);
    QTextOption textOption(document.defaultTextOption());
    textOption.setWrapMode(QTextOption::WordWrap);
    textOption.setAlignment(opt.displayAlignment);
    document.setDefaultTextOption(textOption);
    document.setHtml(html);
    document.setTextWidth(opt.rect.width());

    painter->save();
    painter->setClipRect(opt.rect);
    painter->translate(opt.rect.topLeft());
    const qreal verticalOffset = qMax<qreal>(0, (opt.rect.height() - document.size().height()) / 2.0);
    painter->translate(0, verticalOffset);
    QAbstractTextDocumentLayout::PaintContext context;
    context.palette = opt.palette;
    context.palette.setColor(QPalette::Text, opt.palette.color(QPalette::Text));
    document.documentLayout()->draw(painter, context);
    painter->restore();
}

class DockListItemDelegate : public QStyledItemDelegate {
public:
    explicit DockListItemDelegate(QAbstractItemView* view)
        : QStyledItemDelegate(view)
        , m_view(view)
    {
    }

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override
    {
        QStyleOptionViewItem opt(option);
        initStyleOption(&opt, index);
        // Columns center by default; a column that explicitly sets its own
        // Qt::TextAlignmentRole (e.g. a "Desc"/"Description" column, or the "Reg"
        // column in the SoC Regs register list) keeps that explicit alignment instead.
        if (!index.data(Qt::TextAlignmentRole).isValid())
            opt.displayAlignment = Qt::AlignCenter;
        const bool selected = opt.state & QStyle::State_Selected;
        const bool hovered = !selected
            && index.row() == m_view->property("dockListHoveredRow").toInt();
        if (selected) {
            painter->save();
            painter->fillRect(opt.rect, selectedColorForView(m_view));
            painter->restore();
            opt.palette.setColor(QPalette::Text, selectedTextColorForView(m_view));
            opt.palette.setColor(QPalette::WindowText, selectedTextColorForView(m_view));
            opt.palette.setColor(QPalette::HighlightedText, selectedTextColorForView(m_view));
            opt.backgroundBrush = Qt::NoBrush;
            // The selected fill is painted above; suppress style hover/focus backgrounds.
            opt.state &= ~(QStyle::State_Selected | QStyle::State_MouseOver | QStyle::State_HasFocus);
        } else if (hovered) {
            if (isFirstVisibleColumnForIndex(m_view, index))
                fillRoundedRow(painter, rowRectForIndex(m_view, index), hoverColorForView(m_view));
            opt.palette.setColor(QPalette::Text, hoverTextColorForView(m_view));
            opt.palette.setColor(QPalette::WindowText, hoverTextColorForView(m_view));
            opt.backgroundBrush = Qt::NoBrush;
            opt.state &= ~QStyle::State_MouseOver;
        }

        const QRegularExpression highlightPattern =
            m_view->property("dockListHighlightPattern").value<QRegularExpression>();
        if (highlightPattern.isValid() && !highlightPattern.pattern().isEmpty() && !opt.text.isEmpty()) {
            const QString html = highlightedHtmlForText(opt.text, highlightPattern);
            if (!html.isEmpty()) {
                // QStyledItemDelegate::paint() always re-derives opt.text from the model
                // via its own initStyleOption() call, ignoring any changes made to the
                // option we pass in -- so it cannot be used for a "background only" pass
                // here. Using it that way used to draw the original, un-highlighted text
                // underneath our rich-text overlay, causing visible ghosting/doubling.
                // Erase this cell ourselves instead and draw only the highlighted text.
                const QColor background = selected
                    ? selectedColorForView(m_view)
                    : (hovered ? hoverColorForView(m_view) : opt.palette.color(QPalette::Base));
                painter->save();
                painter->fillRect(opt.rect, background);
                painter->restore();
                paintHighlightedHtml(painter, opt, html);
                return;
            }
        }
        QStyledItemDelegate::paint(painter, opt, index);
    }

private:
    QAbstractItemView* m_view;
};

class DockListCursorFilter : public QObject {
public:
    explicit DockListCursorFilter(QAbstractItemView* view)
        : QObject(view)
        , m_view(view)
    {
    }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (watched == m_view) {
            if (event->type() == QEvent::FocusIn || event->type() == QEvent::FocusOut)
                m_view->viewport()->update();
            return QObject::eventFilter(watched, event);
        }

        if (watched != m_view->viewport())
            return QObject::eventFilter(watched, event);

        if (event->type() == QEvent::MouseMove) {
            const QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
            updateHoveredIndex(m_view->indexAt(mouseEvent->pos()));
        } else if (event->type() == QEvent::HoverMove) {
            const QHoverEvent* hoverEvent = static_cast<QHoverEvent*>(event);
            updateHoveredIndex(m_view->indexAt(hoverEvent->position().toPoint()));
        } else if (event->type() == QEvent::Leave || event->type() == QEvent::HoverLeave) {
            updateHoveredIndex(QModelIndex());
            m_view->viewport()->setCursor(Qt::ArrowCursor);
        } else if (event->type() == QEvent::Resize) {
            const QList<QLabel*> labels = m_view->viewport()->findChildren<QLabel*>(
                QString(), Qt::FindDirectChildrenOnly);
            for (QLabel* label : labels) {
                if (label->property("dockListNoMatchLabel").toBool() && label->isVisible())
                    label->setGeometry(m_view->viewport()->rect());
            }
        }

        return QObject::eventFilter(watched, event);
    }

private:
    void updateHoveredIndex(const QModelIndex& hoveredIndex)
    {
        const int previousHoveredRow = m_view->property("dockListHoveredRow").toInt();
        const int hoveredRow = hoveredIndex.isValid() ? hoveredIndex.row() : -1;
        if (hoveredRow != previousHoveredRow) {
            m_view->setProperty("dockListHoveredRow", hoveredRow);
            updateRow(previousHoveredRow);
            updateRow(hoveredRow);
        }
        m_view->viewport()->setCursor(
            hoveredIndex.isValid() ? Qt::PointingHandCursor : Qt::ArrowCursor);
    }

    void updateRow(int row)
    {
        if (row < 0 || !m_view->model())
            return;

        const QModelIndex rowIndex = m_view->model()->index(row, 0, m_view->rootIndex());
        const QRect rowRect = rowRectForIndex(m_view, rowIndex);
        if (!rowRect.isEmpty())
            m_view->viewport()->update(rowRect.adjusted(-2, -2, 2, 2));
    }

    QAbstractItemView* m_view;
};

} // namespace

namespace DockListStyle {

void apply(QAbstractItemView* view)
{
    view->setAlternatingRowColors(false);
    view->setMouseTracking(true);
    view->viewport()->setMouseTracking(true);
    view->viewport()->setAttribute(Qt::WA_Hover, true);
    view->viewport()->setCursor(Qt::ArrowCursor);
    view->setFrameShape(QFrame::NoFrame);
    view->setProperty("dockListHoveredRow", -1);
    view->setItemDelegate(new DockListItemDelegate(view));
    if (QTreeView* treeView = qobject_cast<QTreeView*>(view))
        treeView->header()->setDefaultAlignment(Qt::AlignCenter);
    DockListCursorFilter* cursorFilter = new DockListCursorFilter(view);
    view->installEventFilter(cursorFilter);
    view->viewport()->installEventFilter(cursorFilter);
}

void showCenteredNoMatchLabel(QAbstractItemView* view, QLabel* label)
{
    if (label->parentWidget() != view->viewport())
        label->setParent(view->viewport());
    const QColor foreground = view->palette().color(QPalette::Text);
    QPalette palette = label->palette();
    palette.setColor(QPalette::WindowText, foreground);
    palette.setColor(QPalette::Text, foreground);
    label->setPalette(palette);
    label->setStyleSheet(QStringLiteral("QLabel { background: transparent; color: %1; }")
                             .arg(foreground.name()));
    label->setProperty("dockListNoMatchLabel", true);
    label->setAttribute(Qt::WA_TransparentForMouseEvents);
    label->setGeometry(view->viewport()->rect());
    label->show();
    label->raise();
}

void setHighlightPattern(QAbstractItemView* view, const QRegularExpression& pattern)
{
    view->setProperty("dockListHighlightPattern", QVariant::fromValue(pattern));
    view->viewport()->update();
}

} // namespace DockListStyle
