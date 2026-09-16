// SPDX-FileCopyrightText: 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later

#include "gui/socregisterswidget.h"

#include "core/socregisterdatabase.h"
#include "gui/dockliststyle.h"
#include "gui/themedlineedit.h"

#include <QAbstractItemView>
#include <QBrush>
#include <QColor>
#include <QComboBox>
#include <QCoreApplication>
#include <QDebug>
#include <QElapsedTimer>
#include <QFormLayout>
#include <QFont>
#include <QFontMetrics>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QPainter>
#include <QPointer>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QShortcut>
#include <QSplitter>
#include <QTimer>
#include <QTreeWidget>
#include <QVBoxLayout>

// TEMPORARY (SoC Regs crash investigation): qWarning()/qDebug() calls are
// stripped to no-ops in this app's Release build (QT_NO_WARNING_OUTPUT /
// QT_NO_DEBUG_OUTPUT), so plain qWarning() never reaches the installed file
// handler. Call QMessageLogger directly (as versioncheck.cpp's
// versionCheckDebug() already does) to bypass that stripping. Only emits
// anything when SPEEDCRUNCH_SOCREGS_DIAGNOSTICS is enabled; otherwise a real
// no-op (QMessageLogger::noDebug(), which returns QNoDebug) so it costs
// nothing in normal builds.
#ifdef SPEEDCRUNCH_SOCREGS_DIAGNOSTICS
using SocRegsLogStream = QDebug;
#else
using SocRegsLogStream = QNoDebug;
#endif
static SocRegsLogStream socRegsLog()
{
#ifdef SPEEDCRUNCH_SOCREGS_DIAGNOSTICS
    return QMessageLogger(QT_MESSAGELOG_FILE, QT_MESSAGELOG_LINE, QT_MESSAGELOG_FUNC).warning();
#else
    return QMessageLogger().noDebug();
#endif
}

// Small rotating arc shown next to the SoC combo while a (synchronous, and
// therefore blocking) database lookup is in progress; see setDatabaseLoadingActive().
class SocBusyIndicator : public QWidget {
public:
    explicit SocBusyIndicator(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setFixedSize(16, 16);
        setVisible(false);
        m_timer.setInterval(60);
        connect(&m_timer, &QTimer::timeout, this, [this]() {
            m_angle = (m_angle + 30) % 360;
            update();
        });
    }

    void setActive(bool active)
    {
        if (active)
            m_timer.start();
        else
            m_timer.stop();
        setVisible(active);
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.translate(width() / 2.0, height() / 2.0);
        painter.rotate(m_angle);
        QPen pen(palette().color(QPalette::WindowText), 2, Qt::SolidLine, Qt::RoundCap);
        painter.setPen(pen);
        const int radius = qMin(width(), height()) / 2 - 2;
        painter.drawArc(QRect(-radius, -radius, radius * 2, radius * 2), 0, 270 * 16);
    }

private:
    QTimer m_timer;
    int m_angle = 0;
};

namespace {

QWidget* controlRow(QWidget* parent, const QString& labelText, QWidget* control,
                    QWidget* trailingWidget = nullptr)
{
    QWidget* row = new QWidget(parent);
    QHBoxLayout* layout = new QHBoxLayout(row);
    layout->setContentsMargins(8, 6, 8, 6);
    layout->addWidget(new QLabel(labelText, row));
    layout->addWidget(control, 1);
    if (trailingWidget != nullptr)
        layout->addWidget(trailingWidget);
    return row;
}

void configureTable(QTreeWidget* table)
{
    table->setRootIsDecorated(false);
    table->setEditTriggers(QTreeWidget::NoEditTriggers);
    table->setSelectionBehavior(QTreeWidget::SelectRows);
    table->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    table->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    table->setWordWrap(true);
    DockListStyle::apply(table);
}

bool parseBitRange(const QString& text, int* lowBit, int* width)
{
    const QStringList parts = text.split(QLatin1Char(':'), Qt::SkipEmptyParts);
    bool firstOk = false;
    bool secondOk = true;
    const int first = parts.value(0).trimmed().toInt(&firstOk);
    const int second = parts.size() > 1
        ? parts.value(1).trimmed().toInt(&secondOk)
        : first;
    if (!firstOk || !secondOk || first < 0 || second < 0)
        return false;
    *lowBit = qMin(first, second);
    *width = qAbs(first - second) + 1;
    return true;
}

QString actualBitfieldValue(const Quantity& value, const QString& bits)
{
    if (value.isNan() || !value.isReal() || !value.isDimensionless())
        return QStringLiteral("--");

    int lowBit = 0;
    int width = 0;
    if (!parseBitRange(bits, &lowBit, &width))
        return QStringLiteral("--");

    Quantity actual = DMath::integer(value);
    actual = actual >> Quantity(lowBit);
    actual = DMath::mask(actual, Quantity(width));
    if (actual.isNan())
        return QStringLiteral("--");
    actual.setFormat(Quantity::Format::Decimal());
    return DMath::format(actual, Quantity::Format::Decimal());
}

QString actualBitfieldDisplayValue(const QString& decimalActual)
{
    if (decimalActual == QLatin1String("--"))
        return decimalActual;
    const HNumber actualNumber(decimalActual.trimmed().toLatin1().constData());
    if (actualNumber.isNan() || !(actualNumber > HNumber(15)))
        return decimalActual;
    Quantity hexValue(actualNumber);
    hexValue.setFormat(Quantity::Format::Hexadecimal());
    // Hex form goes on its own line below the decimal value once it exceeds 15.
    return decimalActual + QStringLiteral("\n(%1)").arg(DMath::format(hexValue, Quantity::Format::Hexadecimal()));
}

QString actualDisplayValue(const Quantity& value)
{
    if (value.isNan() || !value.isReal() || !value.isDimensionless())
        return QStringLiteral("--");
    Quantity displayValue(value);
    displayValue.setFormat(Quantity::Format::Hexadecimal());
    return DMath::format(displayValue, Quantity::Format::Hexadecimal());
}

QColor actualHighlightColor()
{
    return QColor(QStringLiteral("#18864b"));
}

bool numericValuesEqual(const QString& left, const QString& right)
{
    const QByteArray leftText = left.trimmed().toLatin1();
    const QByteArray rightText = right.trimmed().toLatin1();
    const HNumber leftNumber(leftText.constData());
    const HNumber rightNumber(rightText.constData());
    return !leftNumber.isNan() && !rightNumber.isNan() && leftNumber == rightNumber;
}

// Forces the tooltip to word-wrap at a fixed width instead of Qt's default
// (which only wraps plain text once it's nearly as wide as the screen).
QString wrappedTooltip(const QString& text)
{
    return QStringLiteral("<div style='max-width:360px;'>%1</div>").arg(text.toHtmlEscaped());
}

} // namespace

SocRegistersWidget::SocRegistersWidget(QWidget* parent)
    : QWidget(parent)
    , m_database(new SocRegisterDatabase)
    , m_filterTimer(new QTimer(this))
    , m_socName(new QComboBox(this))
    , m_loadingIndicator(new SocBusyIndicator(this))
    , m_derivative(new QLabel(this))
    , m_search(new ThemedLineEdit(this))
    , m_splitter(new QSplitter(Qt::Horizontal, this))
    , m_registerList(new QTreeWidget(m_splitter))
    , m_registerListMessage(new QLabel(m_registerList))
    , m_detailPanel(new QWidget(m_splitter))
    , m_detailLocation(new QLabel(m_detailPanel))
    , m_detailSilicon(new QLabel(m_detailPanel))
    , m_detailDerivative(new QLabel(m_detailPanel))
    , m_detailRegister(new QLabel(m_detailPanel))
    , m_detailReference(new QLabel(m_detailPanel))
    , m_detailAddress(new QLabel(m_detailPanel))
    , m_detailComment(new QLabel(m_detailPanel))
    , m_detailEvaluate(new QLabel(m_detailPanel))
    , m_bitfieldTable(new QTreeWidget(m_detailPanel))
{
    socRegsLog() << "[SocRegs] SocRegistersWidget constructor ENTERED, this=" << this;
    m_socName->setObjectName(QStringLiteral("socNameCombo"));
    m_loadingIndicator->setObjectName(QStringLiteral("socNameLoadingIndicator"));
    m_derivative->setObjectName(QStringLiteral("socDerivativeDisplay"));
    m_search->setObjectName(QStringLiteral("socRegisterSearch"));
    m_registerList->setObjectName(QStringLiteral("socRegisterList"));
    m_detailPanel->setObjectName(QStringLiteral("socRegisterDetailPanel"));
    m_detailLocation->setObjectName(QStringLiteral("socDetailLocation"));
    m_detailSilicon->setObjectName(QStringLiteral("socDetailSilicon"));
    m_detailDerivative->setObjectName(QStringLiteral("socDetailDerivative"));
    m_detailRegister->setObjectName(QStringLiteral("socDetailRegister"));
    m_detailReference->setObjectName(QStringLiteral("socDetailReference"));
    m_detailAddress->setObjectName(QStringLiteral("socDetailAddress"));
    m_detailComment->setObjectName(QStringLiteral("socDetailComment"));
    m_detailEvaluate->setObjectName(QStringLiteral("socDetailEvaluate"));
    m_bitfieldTable->setObjectName(QStringLiteral("socBitfieldTable"));

    // These labels are never added to a layout -- they only hold text that
    // updateDetailHeader() reads to compose m_detailLocation. Left visible,
    // Qt renders them as unmanaged overlays at (0,0), on top of (and
    // obscuring) the real header text.
    m_detailSilicon->hide();
    m_detailDerivative->hide();
    m_detailRegister->hide();
    m_detailReference->hide();
    m_detailAddress->hide();

    m_filterTimer->setInterval(300);
    m_filterTimer->setSingleShot(true);
    m_search->setClearButtonEnabled(true);

    m_registerList->setColumnCount(1);
    m_registerList->setHeaderLabels({tr("Reg")});
    m_registerList->header()->setStretchLastSection(true);
    m_registerList->header()->setSectionResizeMode(0, QHeaderView::Interactive);
    m_registerList->headerItem()->setTextAlignment(0, Qt::AlignLeft | Qt::AlignVCenter);
    m_registerList->setTextElideMode(Qt::ElideNone);
    configureTable(m_registerList);
    m_registerListMessage->setAlignment(Qt::AlignCenter);
    m_registerListMessage->setWordWrap(true);
    m_registerListMessage->hide();

    m_detailLocation->setWordWrap(false);
    m_detailLocation->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_detailLocation->setContentsMargins(8, 8, 8, 4);

    // Unlike the Reg/Addr/Ref header (elided to one line, see
    // updateDetailHeaderElision()), the comment is shown word-wrapped in full
    // -- it can run to several sentences, and eliding it would defeat
    // "selectable and copyable".
    m_detailComment->setWordWrap(true);
    m_detailComment->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_detailComment->setContentsMargins(8, 0, 8, 4);

    QHBoxLayout* evaluateLayout = new QHBoxLayout;
    evaluateLayout->setContentsMargins(8, 0, 8, 6);
    evaluateLayout->addWidget(new QLabel(tr("Evaluate:"), m_detailPanel));
    evaluateLayout->addWidget(m_detailEvaluate, 1);
    m_detailEvaluate->setStyleSheet(QStringLiteral(
        "QLabel#socDetailEvaluate { color: #18864b; background: transparent; }"));
    QFont evaluateFont = m_detailEvaluate->font();
    evaluateFont.setBold(true);
    m_detailEvaluate->setFont(evaluateFont);

    m_bitfieldTable->setColumnCount(8);
    m_bitfieldTable->setHeaderLabels({
        tr("#"), tr("Name"), tr("Enum"), tr("SW"),
        tr("HW"), tr("Def"), tr("Eval"), tr("Desc")
    });
    configureTable(m_bitfieldTable);
    m_bitfieldTable->header()->setStretchLastSection(false);
    for (int column = 0; column < m_bitfieldTable->columnCount(); ++column)
        m_bitfieldTable->header()->setSectionResizeMode(column, QHeaderView::Interactive);
    m_bitfieldTable->headerItem()->setTextAlignment(7, Qt::AlignLeft | Qt::AlignVCenter);
    m_bitfieldTable->setTextElideMode(Qt::ElideNone);

    QWidget* masterPanel = new QWidget(m_splitter);
    masterPanel->setObjectName(QStringLiteral("socRegisterMasterPanel"));
    QVBoxLayout* masterLayout = new QVBoxLayout(masterPanel);
    masterLayout->setContentsMargins(0, 0, 0, 0);
    masterLayout->setSpacing(0);
    masterLayout->addWidget(controlRow(masterPanel, tr("SoC:"), m_socName, m_loadingIndicator));
    masterLayout->addWidget(controlRow(masterPanel, tr("Derivative:"), m_derivative));
    masterLayout->addWidget(controlRow(masterPanel, tr("Search:"), m_search));
    masterLayout->addWidget(m_registerList, 1);

    QVBoxLayout* detailLayout = new QVBoxLayout(m_detailPanel);
    detailLayout->setContentsMargins(0, 0, 0, 0);
    detailLayout->addWidget(m_detailLocation);
    detailLayout->addWidget(m_detailComment);
    detailLayout->addLayout(evaluateLayout);
    detailLayout->addWidget(m_bitfieldTable, 1);

    m_splitter->addWidget(masterPanel);
    m_splitter->addWidget(m_detailPanel);
    m_splitter->setChildrenCollapsible(false);
    m_splitter->setStretchFactor(0, 1);
    m_splitter->setStretchFactor(1, 3);
    m_detailPanel->hide();

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_splitter, 1);

    setFocusProxy(m_search);
    QWidget::setTabOrder(m_socName, m_search);
    QWidget::setTabOrder(m_search, m_registerList);
    QShortcut* escapeShortcut = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    escapeShortcut->setObjectName(QStringLiteral("socRegistersEscapeShortcut"));
    escapeShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(escapeShortcut, &QShortcut::activated,
            this, &SocRegistersWidget::focusEditorRequested);

    connect(m_filterTimer, &QTimer::timeout, this, &SocRegistersWidget::updateRegisterList);
    connect(m_search, &QLineEdit::textChanged, this, [this]() {
        updateSearchHighlight();
        m_filterTimer->start();
    });
    connect(m_socName, &QComboBox::currentIndexChanged, this, [this](int newIndex) {
        // TEMPORARY (crash investigation): trace the full selection-change
        // sequence with timing, since the reported crash happens when
        // switching silicon back and forth repeatedly.
        socRegsLog() << "[SocRegs]" << this << "currentIndexChanged BEGIN index=" << newIndex
                   << "text=" << m_socName->itemText(newIndex);
        QElapsedTimer timer;
        timer.start();
        // Selecting a SoC can trigger a first-time (slow, synchronous) cache
        // rebuild; disabling the combo for the duration rules out a rapid
        // second selection re-entering this handler while it's still running.
        setDatabaseLoadingActive(true);
        updateDerivativeDisplay();
        socRegsLog() << "[SocRegs]" << this << "after updateDerivativeDisplay, elapsedMs=" << timer.elapsed();
        updateRegisterList();
        socRegsLog() << "[SocRegs]" << this << "after updateRegisterList, elapsedMs=" << timer.elapsed();
        setDatabaseLoadingActive(false);
        socRegsLog() << "[SocRegs]" << this << "currentIndexChanged END index=" << newIndex
                   << "totalElapsedMs=" << timer.elapsed();
    });
    connect(m_registerList, &QTreeWidget::currentItemChanged,
            this, [this](QTreeWidgetItem* current) { showSelectedRegister(current); });

    socRegsLog() << "[SocRegs]" << this << "constructor: calling database->initialize()";
    if (!m_database->initialize(&m_loadError)) {
        socRegsLog() << "[SocRegs]" << this << "constructor: initialize() FAILED:" << m_loadError;
        showRegisterListMessage(m_loadError);
        return;
    }
    socRegsLog() << "[SocRegs]" << this << "constructor: initialize() OK, databasePath="
               << m_database->databasePath();
    populateSilicons();
    updateDerivativeDisplay();
    updateRegisterList();
}

SocRegistersWidget::~SocRegistersWidget()
{
    socRegsLog() << "[SocRegs]" << this << "~SocRegistersWidget BEGIN, database=" << m_database;
    delete m_database;
    socRegsLog() << "[SocRegs]" << this << "~SocRegistersWidget END";
}

QString SocRegistersWidget::selectedSocName() const
{
    return m_socName->currentText();
}

QString SocRegistersWidget::selectedDerivative() const
{
    return m_derivative->text();
}

QString SocRegistersWidget::searchText() const
{
    return m_search->text();
}

QString SocRegistersWidget::databasePath() const
{
    return m_database->databasePath();
}

QString SocRegistersWidget::selectedRegisterName() const
{
    return m_registerList->currentItem()
        ? m_registerList->currentItem()->text(0)
        : QString();
}

QString SocRegistersWidget::selectedBitfieldName() const
{
    return m_bitfieldTable->currentItem()
        ? m_bitfieldTable->currentItem()->text(1)
        : QString();
}

QString SocRegistersWidget::selectedSubBitfieldName() const
{
    return m_bitfieldTable->currentItem()
        ? m_bitfieldTable->currentItem()->text(2)
        : QString();
}

QByteArray SocRegistersWidget::splitterState() const
{
    return m_splitter->saveState();
}

void SocRegistersWidget::setActualValue(const Quantity& value)
{
    m_actualValue = value;
    m_hasActualValue = !value.isNan();
    m_detailEvaluate->setText(m_hasActualValue
        ? actualDisplayValue(m_actualValue)
        : QStringLiteral("--"));
    updateActualCells();
}

void SocRegistersWidget::clearActualValue()
{
    m_hasActualValue = false;
    m_detailEvaluate->setText(QStringLiteral("--"));
    updateActualCells();
}

void SocRegistersWidget::focusSearch()
{
    m_search->setFocus(Qt::ShortcutFocusReason);
    m_search->selectAll();
}

void SocRegistersWidget::restoreState(const QString& socName,
                                      const QString& derivative,
                                      const QString& searchText,
                                      const QString& registerName,
                                      const QString& bitfieldName,
                                      const QString& subBitfieldName,
                                      const QByteArray& splitterState)
{
    Q_UNUSED(derivative); // Derivative is now a read-only display derived from the SoC.
    {
        const QSignalBlocker blocker(m_socName);
        const int storedIndex = m_socName->findText(socName);
        m_socName->setCurrentIndex(storedIndex >= 0
            ? storedIndex
            : (m_socName->count() > 0 ? 0 : -1));
    }
    updateDerivativeDisplay();
    {
        const QSignalBlocker blocker(m_search);
        m_search->setText(searchText);
    }
    updateSearchHighlight();
    updateRegisterList();

    for (int index = 0; index < m_registerList->topLevelItemCount(); ++index) {
        QTreeWidgetItem* item = m_registerList->topLevelItem(index);
        if (item->text(0) == registerName) {
            m_registerList->setCurrentItem(item);
            break;
        }
    }
    for (int index = 0; index < m_bitfieldTable->topLevelItemCount(); ++index) {
        QTreeWidgetItem* item = m_bitfieldTable->topLevelItem(index);
        if (item->text(1) == bitfieldName && item->text(2) == subBitfieldName) {
            m_bitfieldTable->setCurrentItem(item);
            break;
        }
    }
    if (!splitterState.isEmpty())
        m_splitter->restoreState(splitterState);
}

void SocRegistersWidget::populateSilicons()
{
    const QSignalBlocker blocker(m_socName);
    m_socName->clear();
    QString error;
    m_socName->addItems(m_database->siliconNames(&error));
    if (!error.isEmpty())
        m_loadError = error;
    m_socName->setCurrentIndex(m_socName->count() > 0 ? 0 : -1);
}

void SocRegistersWidget::updateDerivativeDisplay()
{
    socRegsLog() << "[SocRegs]" << this << "updateDerivativeDisplay BEGIN soc=" << m_socName->currentText();
    QString error;
    const QStringList derivatives = m_database->derivatives(m_socName->currentText(), &error);
    socRegsLog() << "[SocRegs]" << this << "updateDerivativeDisplay derivatives() count=" << derivatives.size()
               << "error=" << error;
    if (!error.isEmpty())
        m_loadError = error;
    m_derivative->setText(derivatives.isEmpty() ? QStringLiteral("--") : derivatives.join(QStringLiteral(", ")));
    socRegsLog() << "[SocRegs]" << this << "updateDerivativeDisplay END";
}

void SocRegistersWidget::setDatabaseLoadingActive(bool active)
{
    socRegsLog() << "[SocRegs]" << this << "setDatabaseLoadingActive" << active;
    m_socName->setDisabled(active);
    m_loadingIndicator->setActive(active);
    if (active) {
        // The lookup this guards is synchronous, so without pumping paint
        // events here the disabled combo/spinner would never actually be
        // drawn before the blocking call below returns. ExcludeUserInputEvents
        // means no queued mouse/key event can be delivered while we do this,
        // so it cannot re-enter this handler.
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    }
}

void SocRegistersWidget::updateSearchHighlight()
{
    const QString term = m_search->text();
    QRegularExpression pattern(term, QRegularExpression::CaseInsensitiveOption);
    if (term.isEmpty() || !pattern.isValid())
        pattern = QRegularExpression();
    DockListStyle::setHighlightPattern(m_registerList, pattern);
    DockListStyle::setHighlightPattern(m_bitfieldTable, pattern);
}

void SocRegistersWidget::updateRegisterList()
{
    socRegsLog() << "[SocRegs]" << this << "updateRegisterList BEGIN soc=" << m_socName->currentText()
               << "search=" << m_search->text();
    m_filterTimer->stop();
    m_registerList->clear();
    clearDetails();

    if (!m_loadError.isEmpty()) {
        socRegsLog() << "[SocRegs]" << this << "updateRegisterList early-return, m_loadError=" << m_loadError;
        showRegisterListMessage(m_loadError);
        return;
    }

    QString error;
    QElapsedTimer timer;
    timer.start();
    const QList<SocRegisterDatabase::RegisterSummary> registers = m_database->registers(
        m_socName->currentText(), m_search->text(), &error);
    socRegsLog() << "[SocRegs]" << this << "updateRegisterList registers() returned" << registers.size()
               << "rows in" << timer.elapsed() << "ms, error=" << error;
    if (!error.isEmpty()) {
        showRegisterListMessage(error);
        return;
    }

    for (const SocRegisterDatabase::RegisterSummary& reg : registers) {
        QTreeWidgetItem* item = new QTreeWidgetItem(m_registerList, {reg.name});
        item->setData(0, Qt::UserRole, reg.id);
        item->setTextAlignment(0, Qt::AlignLeft | Qt::AlignVCenter);
    }

    if (m_registerList->topLevelItemCount() == 0) {
        showRegisterListMessage(tr("No match found"));
    } else {
        m_registerListMessage->hide();
    }
    socRegsLog() << "[SocRegs]" << this << "updateRegisterList END, populated"
               << m_registerList->topLevelItemCount() << "items";
}

void SocRegistersWidget::showSelectedRegister(QTreeWidgetItem* item)
{
    if (item == nullptr) {
        clearDetails();
        return;
    }

    SocRegisterDatabase::RegisterDetail detail;
    QList<SocRegisterDatabase::BitfieldRow> rows;
    QString error;
    if (!m_database->registerDetails(item->data(0, Qt::UserRole).toLongLong(),
                                     &detail, &rows, &error)) {
        clearDetails();
        showRegisterListMessage(error.isEmpty() ? tr("Could not load register details") : error);
        return;
    }

    m_detailSilicon->setText(detail.silicon);
    m_detailDerivative->setText(detail.derivative);
    m_detailRegister->setText(detail.name);
    m_detailReference->setText(detail.referencePage);
    m_detailAddress->setText(detail.address);
    m_detailComment->setText(tr("Desc: %1").arg(detail.comment));
    m_detailComment->setVisible(!detail.comment.isEmpty());
    m_detailEvaluate->setText(m_hasActualValue
        ? actualDisplayValue(m_actualValue)
        : QStringLiteral("--"));
    m_detailRows = rows;
    populateBitfieldTable();

    m_detailPanel->show();
    m_splitter->setSizes({260, 740});
    // Force an immediate layout pass so the header label has its real width
    // before we elide its text; otherwise eliding would use a stale, too-small
    // width and hide the register name that should be visible.
    if (QLayout* detailLayout = m_detailPanel->layout())
        detailLayout->activate();
    updateDetailHeader();
}

void SocRegistersWidget::populateBitfieldTable()
{
    m_bitfieldTable->clear();
    for (const SocRegisterDatabase::BitfieldRow& row : m_detailRows) {
        QTreeWidgetItem* item = new QTreeWidgetItem(m_bitfieldTable, {
            row.bits, row.bitfield, row.subBitfield, row.softwareAccess,
            row.hardwareAccess, row.defaultValue, QStringLiteral("--"), row.description
        });
        item->setToolTip(7, wrappedTooltip(row.description));
        item->setTextAlignment(7, Qt::AlignLeft | Qt::AlignVCenter);
    }
    updateActualCells();
    for (int column = 0; column < 7; ++column)
        m_bitfieldTable->resizeColumnToContents(column);
    updateBitfieldColumnWidths();
}

void SocRegistersWidget::updateActualCells()
{
    QString parentActual;
    const int rowCount = qMin(m_detailRows.size(), m_bitfieldTable->topLevelItemCount());
    for (int index = 0; index < rowCount; ++index) {
        const SocRegisterDatabase::BitfieldRow& row = m_detailRows.at(index);
        QTreeWidgetItem* item = m_bitfieldTable->topLevelItem(index);
        const bool parentBitfield = row.bits != QLatin1String("--")
            && row.bitfield != QLatin1String("--");
        const QString actual = parentBitfield && m_hasActualValue
            ? actualBitfieldValue(m_actualValue, row.bits)
            : QStringLiteral("--");
        if (parentBitfield)
            parentActual = actual;
        item->setText(6, parentBitfield ? actualBitfieldDisplayValue(actual) : actual);

        for (const int column : {2, 6, 7}) {
            item->setForeground(column, QBrush());
            QFont font = item->font(column);
            font.setBold(false);
            item->setFont(column, font);
        }
        if (parentBitfield && actual != QLatin1String("--")) {
            item->setForeground(6, QBrush(actualHighlightColor()));
            QFont actualFont = item->font(6);
            actualFont.setBold(true);
            item->setFont(6, actualFont);
        }
        const bool matchedSubfield = !parentBitfield
            && parentActual != QLatin1String("--")
            && numericValuesEqual(parentActual, row.defaultValue);
        if (matchedSubfield) {
            for (const int column : {2, 7}) {
                item->setForeground(column, QBrush(actualHighlightColor()));
                QFont matchedFont = item->font(column);
                matchedFont.setBold(true);
                item->setFont(column, matchedFont);
            }
        }
    }
    // The Actual column can grow to two lines ("<decimal>\n(<hex>)"); resync row
    // heights so a newly-added hex line is never clipped.
    updateDescriptionRowHeights();
}

int SocRegistersWidget::widestNameColumnWidth(int column) const
{
    const QFontMetrics metrics(m_bitfieldTable->font());
    int widest = metrics.horizontalAdvance(m_bitfieldTable->headerItem()->text(column));
    for (int row = 0; row < m_bitfieldTable->topLevelItemCount(); ++row) {
        const QTreeWidgetItem* item = m_bitfieldTable->topLevelItem(row);
        widest = qMax(widest, metrics.horizontalAdvance(item->text(column)));
    }
    return widest + 12; // minimal padding so the widest name isn't clipped
}

void SocRegistersWidget::updateBitfieldColumnWidths()
{
    const int availableWidth = m_bitfieldTable->viewport()->width();
    if (availableWidth <= 0)
        return;

    static const int columnPercentages[] = {7, 12, 14, 6, 6, 8, 7};
    int assignedWidth = 0;
    for (int column = 0; column < 7; ++column) {
        // Bitfield (1) and Sub-bitfield (2) are sized to exactly fit the widest displayed
        // name -- never wider than the name itself, regardless of the layout percentage.
        const int width = (column == 1 || column == 2)
            ? qMax(36, widestNameColumnWidth(column))
            : qMax(36, availableWidth * columnPercentages[column] / 100);
        m_bitfieldTable->setColumnWidth(column, width);
        assignedWidth += width;
    }
    const int descriptionWidth = qMax(
        availableWidth * 40 / 100,
        availableWidth - assignedWidth);
    m_bitfieldTable->setColumnWidth(7, descriptionWidth);
    updateDescriptionRowHeights();
}

void SocRegistersWidget::updateDetailHeader()
{
    m_detailLocationFullText = tr("Reg: %1 ; Addr: %2 ; Ref page #: %3")
        .arg(m_detailRegister->text(), m_detailAddress->text(),
             m_detailReference->text());
    m_detailLocation->setToolTip(m_detailLocationFullText);
    updateDetailHeaderElision();
    // MainWindow can restore the previously selected register (via restoreState())
    // while this widget's dock hasn't received its final layout/geometry yet (e.g.
    // QMainWindow applies saved dock sizes asynchronously). Re-check shortly after
    // control returns to the event loop, once any pending layout has settled.
    QPointer<SocRegistersWidget> guard(this);
    QTimer::singleShot(0, this, [guard]() {
        if (guard && !guard->m_detailLocationFullText.isEmpty())
            guard->updateDetailHeaderElision();
    });
}

void SocRegistersWidget::updateDetailHeaderElision()
{
    // Elide (never word-wrap) so a long register name is never broken mid-word;
    // the full text remains available via the tooltip set in updateDetailHeader().
    const QFontMetrics metrics(m_detailLocation->fontMetrics());
    const int availableWidth = qMax(0, m_detailLocation->width()
        - m_detailLocation->contentsMargins().left()
        - m_detailLocation->contentsMargins().right());
    m_detailLocation->setText(
        metrics.elidedText(m_detailLocationFullText, Qt::ElideRight, availableWidth));
}

void SocRegistersWidget::updateDescriptionRowHeights()
{
    const int descriptionWidth = qMax(1, m_bitfieldTable->columnWidth(7) - 12);
    const int actualWidth = qMax(1, m_bitfieldTable->columnWidth(6) - 12);
    const QFontMetrics metrics(m_bitfieldTable->font());
    for (int row = 0; row < m_bitfieldTable->topLevelItemCount(); ++row) {
        QTreeWidgetItem* item = m_bitfieldTable->topLevelItem(row);
        const QRect descriptionRect = metrics.boundingRect(
            QRect(0, 0, descriptionWidth, 100000),
            Qt::TextWordWrap | Qt::AlignLeft | Qt::AlignTop,
            item->text(7));
        // The Actual column can span two lines ("<decimal>\n(<hex>)"); make sure the
        // row is tall enough for that too, not just for the (possibly shorter) Desc text.
        const QRect actualRect = metrics.boundingRect(
            QRect(0, 0, actualWidth, 100000),
            Qt::TextWordWrap | Qt::AlignCenter,
            item->text(6));
        const int rowHeight = qMax(metrics.height() + 8,
            qMax(descriptionRect.height(), actualRect.height()) + 8);
        item->setSizeHint(7, QSize(descriptionWidth, rowHeight));
    }
    m_bitfieldTable->doItemsLayout();
}

void SocRegistersWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    updateBitfieldColumnWidths();
    if (!m_detailLocationFullText.isEmpty())
        updateDetailHeaderElision();
}

void SocRegistersWidget::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    // Defensive re-sync: whenever this widget becomes visible, make sure the
    // header label and bitfield columns reflect the widget's current size.
    updateBitfieldColumnWidths();
    if (!m_detailLocationFullText.isEmpty())
        updateDetailHeaderElision();
}

void SocRegistersWidget::clearDetails()
{
    m_bitfieldTable->clear();
    m_detailRows.clear();
    m_detailSilicon->clear();
    m_detailDerivative->clear();
    m_detailRegister->clear();
    m_detailReference->clear();
    m_detailAddress->clear();
    m_detailComment->clear();
    m_detailComment->setVisible(false);
    m_detailLocation->clear();
    m_detailLocation->setToolTip(QString());
    m_detailLocationFullText.clear();
    m_detailEvaluate->setText(m_hasActualValue
        ? actualDisplayValue(m_actualValue)
        : QStringLiteral("--"));
    m_detailPanel->hide();
}

void SocRegistersWidget::showRegisterListMessage(const QString& message)
{
    m_registerListMessage->setText(message);
    DockListStyle::showCenteredNoMatchLabel(m_registerList, m_registerListMessage);
}
