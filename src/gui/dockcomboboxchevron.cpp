// SPDX-FileCopyrightText: 2026 BitLoupe developers
// SPDX-License-Identifier: GPL-2.0-or-later

#include "gui/dockcomboboxchevron.h"

#include "gui/uiconfig.h"

#include <QAbstractAnimation>
#include <QAbstractItemView>
#include <QBitmap>
#include <QComboBox>
#include <QDebug>
#include <QEasingCurve>
#include <QEvent>
#include <QFrame>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>
#include <QTimer>
#include <QVariantAnimation>

namespace {

// TEMPORARY (SoC Regs crash investigation): qWarning() is stripped to a no-op
// in this app's Release build (QT_NO_WARNING_OUTPUT), so call QMessageLogger
// directly to bypass that and reach the installed file handler. Only emits
// anything when BITLOUPE_SOCREGS_DIAGNOSTICS is enabled; otherwise a real
// no-op (QMessageLogger::noDebug(), which returns QNoDebug) so it costs
// nothing in normal builds.
#ifdef BITLOUPE_SOCREGS_DIAGNOSTICS
using SocRegsLogStream = QDebug;
#else
using SocRegsLogStream = QNoDebug;
#endif
SocRegsLogStream socRegsLog()
{
#ifdef BITLOUPE_SOCREGS_DIAGNOSTICS
    return QMessageLogger(QT_MESSAGELOG_FILE, QT_MESSAGELOG_LINE, QT_MESSAGELOG_FUNC).warning();
#else
    return QMessageLogger().noDebug();
#endif
}

constexpr int kChevronAnimationMs = 150;
constexpr qreal kChevronOpacity = 0.76;

void removeFrame(QWidget* widget)
{
    QFrame* frame = qobject_cast<QFrame*>(widget);
    if (frame == nullptr)
        return;

    frame->setFrameShape(QFrame::NoFrame);
    frame->setLineWidth(0);
    frame->setMidLineWidth(0);
}

void applyRoundedMask(QWidget* widget)
{
    if (widget == nullptr || widget->size().isEmpty())
        return;

    QBitmap mask(widget->size());
    mask.fill(Qt::color0);
    QPainter painter(&mask);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(Qt::color1);
    painter.drawRoundedRect(QRectF(mask.rect()).adjusted(0, 0, -1, -1),
                            UiConfig::CompletionPopupCornerRadius,
                            UiConfig::CompletionPopupCornerRadius);
    widget->setMask(mask);
}

} // namespace

DockComboBoxChevron::DockComboBoxChevron(QComboBox* comboBox)
    : QWidget(comboBox)
    , m_comboBox(comboBox)
    , m_animation(new QVariantAnimation(this))
{
    setObjectName(QStringLiteral("bitloupeDockComboBoxChevron"));
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_NoSystemBackground);
    setAttribute(Qt::WA_TranslucentBackground);
    setAutoFillBackground(false);
    setFocusPolicy(Qt::NoFocus);

    m_animation->setDuration(kChevronAnimationMs);
    m_animation->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_animation, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
        m_rotation = value.toReal();
        update();
    });
    connect(comboBox, QOverload<int>::of(&QComboBox::activated), this, [this]() {
        setPopupOpen(false);
    });
    comboBox->installEventFilter(this);
    installPopupEventFilters();
    reposition();
    show();
}

void DockComboBoxChevron::apply(QComboBox* comboBox,
                                const QColor& textColor,
                                const QColor& outlineColor)
{
    if (comboBox == nullptr)
        return;

    DockComboBoxChevron* chevron = nullptr;
    if (QWidget* existing = comboBox->findChild<QWidget*>(
            QStringLiteral("bitloupeDockComboBoxChevron"),
            Qt::FindDirectChildrenOnly)) {
        chevron = dynamic_cast<DockComboBoxChevron*>(existing);
    }
    if (chevron == nullptr)
        chevron = new DockComboBoxChevron(comboBox);

    chevron->setColors(textColor, outlineColor);
    chevron->refresh();
}

void DockComboBoxChevron::setColors(QColor chevronColor, const QColor& outlineColor)
{
    chevronColor.setAlphaF(kChevronOpacity);
    if (m_chevronColor == chevronColor && m_outlineColor == outlineColor)
        return;

    m_chevronColor = chevronColor;
    m_outlineColor = outlineColor;
    update();
}

void DockComboBoxChevron::refresh()
{
    installPopupEventFilters();
    reposition();
    setPopupOpen(m_view != nullptr && m_view->isVisible());
    raise();
}

bool DockComboBoxChevron::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_comboBox) {
        switch (event->type()) {
        case QEvent::Move:
        case QEvent::Resize:
        case QEvent::Show:
        case QEvent::StyleChange:
            reposition();
            break;
        case QEvent::Hide:
            setPopupOpen(false);
            break;
        case QEvent::KeyPress:
        case QEvent::MouseButtonPress:
            installPopupEventFilters();
            break;
        default:
            break;
        }
    } else if (watched == m_view || watched == m_popupWindow) {
        if (event->type() == QEvent::Show) {
            socRegsLog() << "[SocRegsChevron]" << this << "popup SHOW watched=" << watched
                       << "view=" << m_view.data() << "popupWindow=" << m_popupWindow.data();
            setPopupOpen(true);
            deferStylePopupChrome();
        } else if (event->type() == QEvent::Hide) {
            socRegsLog() << "[SocRegsChevron]" << this << "popup HIDE watched=" << watched;
            setPopupOpen(false);
        } else if (event->type() == QEvent::Resize) {
            deferStylePopupChrome();
        }
    }

    return QWidget::eventFilter(watched, event);
}

void DockComboBoxChevron::paintEvent(QPaintEvent*)
{
    if (!m_chevronColor.isValid())
        return;

    const qreal dpr = devicePixelRatioF();
    const auto aligned = [dpr](qreal value) {
        return qRound(value * dpr) / dpr;
    };

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    painter.translate(QPointF(aligned(width() / 2.0), aligned(height() / 2.0)));
    painter.rotate(m_rotation);

    QPainterPath path;
    path.moveTo(QPointF(-5.0, -3.0));
    path.lineTo(QPointF(0.0, 3.0));
    path.lineTo(QPointF(5.0, -3.0));

    QPen pen(m_chevronColor, 1.65, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(path);
}

void DockComboBoxChevron::installPopupEventFilters()
{
    if (m_comboBox == nullptr)
        return;

    QAbstractItemView* view = m_comboBox->view();
    if (m_view != view) {
        socRegsLog() << "[SocRegsChevron]" << this << "view identity changed, old=" << m_view.data()
                   << "new=" << view;
        if (m_view != nullptr)
            m_view->removeEventFilter(this);
        m_view = view;
        if (m_view != nullptr)
            m_view->installEventFilter(this);
    }

    QWidget* popupWindow = m_view != nullptr ? m_view->window() : nullptr;
    if (popupWindow == m_comboBox->window())
        popupWindow = nullptr;
    if (m_popupWindow != popupWindow) {
        socRegsLog() << "[SocRegsChevron]" << this << "popupWindow identity changed, old="
                   << m_popupWindow.data() << "new=" << popupWindow;
        if (m_popupWindow != nullptr)
            m_popupWindow->removeEventFilter(this);
        m_popupWindow = popupWindow;
        if (m_popupWindow != nullptr)
            m_popupWindow->installEventFilter(this);
    }

    stylePopupChrome();
}

QWidget* DockComboBoxChevron::popupChromeWidget() const
{
    if (m_popupWindow != nullptr)
        return m_popupWindow;
    return m_view;
}

void DockComboBoxChevron::reposition()
{
    if (m_comboBox == nullptr)
        return;

    setGeometry(qMax(0, m_comboBox->width() - IndicatorWidth),
                0,
                IndicatorWidth,
                m_comboBox->height());
}

void DockComboBoxChevron::deferStylePopupChrome()
{
    // A double-click's second press can arrive while the popup Qt just opened is
    // still settling (or can toggle it closed again immediately), so a quick
    // open/close/open cycle can queue several native mask/frame updates in a row.
    // Run the actual styling on the next event-loop turn, once things have
    // settled, instead of synchronously inside the Show/Resize event; by the time
    // it runs, stylePopupChrome() re-reads the current view/window and simply
    // does nothing if the popup isn't visible anymore.
    QPointer<DockComboBoxChevron> guard(this);
    QTimer::singleShot(0, this, [guard]() {
        if (guard != nullptr)
            guard->stylePopupChrome();
    });
}

void DockComboBoxChevron::stylePopupChrome()
{
    if (m_view == nullptr || !m_view->isVisible())
        return;

    // The crash-dump call stack showed the actual fault inside Qt's OWN
    // QComboBox::showPopup() -> QWidgetPrivate::showChildren() ->
    // QObject::isWidgetType(), i.e. Qt iterating a corrupted/stale children
    // list on a LATER reopen -- not inside our own code. Re-running
    // setAutoFillBackground()/setAttribute()/setObjectName()/setStyleSheet()
    // on the SAME popup widgets on every single show is what most plausibly
    // corrupts that list (QStyleSheetStyle can create/tear down internal
    // helper children when a stylesheet is (re-)applied). Only do this
    // one-time setup once per distinct view/chrome identity, exactly like
    // installPopupEventFilters() already does for the identity itself.
    if (m_styledView != m_view) {
        socRegsLog() << "[SocRegsChevron]" << this << "styling view (first time), view=" << m_view.data();
        removeFrame(m_view);
        m_view->setAutoFillBackground(false);
        m_view->viewport()->setAutoFillBackground(false);
        m_view->viewport()->setAttribute(Qt::WA_StyledBackground, true);
        m_styledView = m_view;
    }

    QWidget* popupChrome = popupChromeWidget();
    // The container can still report itself in a not-yet-settled state right
    // after a rapid reopen (deferStylePopupChrome() runs on the next event-loop
    // turn, but the native window may not have finished showing by then) --
    // masking it in that window has correlated with the app terminating right
    // after, per crash-log evidence. Only proceed once it's actually visible.
    if (popupChrome == nullptr || !popupChrome->isVisible())
        return;

    if (m_styledChrome != popupChrome) {
        socRegsLog() << "[SocRegsChevron]" << this << "styling chrome (first time), chrome=" << popupChrome;
        removeFrame(popupChrome);
        popupChrome->setAutoFillBackground(false);
        popupChrome->setAttribute(Qt::WA_StyledBackground, true);
        if (popupChrome != m_view) {
            popupChrome->setObjectName(QStringLiteral("bitloupeDockComboBoxPopupChrome"));
            popupChrome->setStyleSheet(QStringLiteral(
                "QWidget#bitloupeDockComboBoxPopupChrome,"
                "QFrame#bitloupeDockComboBoxPopupChrome {"
                " background: transparent; border: 0;"
                "}"));
        }
        m_styledChrome = popupChrome;
    }

    // Re-masking a native top-level window on every single Show/Resize event is
    // both wasteful (the same popup got masked 6 times for one open in the
    // crash log) and risky -- skip when the widget/size haven't actually
    // changed since the last successful mask.
    if (m_maskedWidget == popupChrome && m_maskedSize == popupChrome->size())
        return;
    applyRoundedMask(popupChrome);
    m_maskedWidget = popupChrome;
    m_maskedSize = popupChrome->size();
}

void DockComboBoxChevron::setPopupOpen(bool open)
{
    if (open)
        stylePopupChrome();

    if (m_popupOpen == open && m_animation->state() != QAbstractAnimation::Running)
        return;

    m_popupOpen = open;
    m_animation->stop();
    m_animation->setStartValue(m_rotation);
    m_animation->setEndValue(open ? 180.0 : 0.0);
    m_animation->start();
}
