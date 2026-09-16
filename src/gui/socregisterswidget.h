// SPDX-FileCopyrightText: 2026 BitLoupe developers
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef GUI_SOCREGISTERSWIDGET_H
#define GUI_SOCREGISTERSWIDGET_H

#include "core/socregisterdatabase.h"
#include "math/quantity.h"

#include <QWidget>

class QLabel;
class QComboBox;
class QLineEdit;
class QResizeEvent;
class QShowEvent;
class QSplitter;
class QTimer;
class QTreeWidget;
class QTreeWidgetItem;
class SocBusyIndicator;
class SocRegistersWidget : public QWidget {
    Q_OBJECT

public:
    explicit SocRegistersWidget(QWidget* parent = nullptr);
    ~SocRegistersWidget();

    QString selectedSocName() const;
    QString selectedDerivative() const;
    QString searchText() const;
    QString databasePath() const;
    QString selectedRegisterName() const;
    QString selectedBitfieldName() const;
    QString selectedSubBitfieldName() const;
    QByteArray splitterState() const;
    void restoreState(const QString& socName,
                      const QString& derivative,
                      const QString& searchText,
                      const QString& registerName,
                      const QString& bitfieldName,
                      const QString& subBitfieldName,
                      const QByteArray& splitterState);
    void setActualValue(const Quantity& value);
    void clearActualValue();
    void focusSearch();

signals:
    void focusEditorRequested();

protected:
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;

private:
    void populateSilicons();
    void updateDerivativeDisplay();
    void updateSearchHighlight();
    void updateRegisterList();
    void showSelectedRegister(QTreeWidgetItem* item);
    void populateBitfieldTable();
    void updateActualCells();
    void updateBitfieldColumnWidths();
    int widestNameColumnWidth(int column) const;
    void updateDetailHeader();
    void updateDetailHeaderElision();
    void updateDescriptionRowHeights();
    void clearDetails();
    void showRegisterListMessage(const QString& message);
    void setDatabaseLoadingActive(bool active);

    SocRegisterDatabase* m_database;
    QTimer* m_filterTimer;
    QComboBox* m_socName;
    SocBusyIndicator* m_loadingIndicator;
    QLabel* m_derivative;
    QLineEdit* m_search;
    QSplitter* m_splitter;
    QTreeWidget* m_registerList;
    QLabel* m_registerListMessage;
    QWidget* m_detailPanel;
    QLabel* m_detailLocation;
    QLabel* m_detailSilicon;
    QLabel* m_detailDerivative;
    QLabel* m_detailRegister;
    QLabel* m_detailReference;
    QLabel* m_detailAddress;
    QLabel* m_detailComment;
    QLabel* m_detailEvaluate;
    QTreeWidget* m_bitfieldTable;
    QList<SocRegisterDatabase::BitfieldRow> m_detailRows;
    QString m_detailLocationFullText;
    Quantity m_actualValue;
    bool m_hasActualValue = false;
    QString m_loadError;
};

#endif // GUI_SOCREGISTERSWIDGET_H
