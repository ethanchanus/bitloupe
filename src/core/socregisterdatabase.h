// SPDX-FileCopyrightText: 2026 BitLoupe developers
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef CORE_SOCREGISTERDATABASE_H
#define CORE_SOCREGISTERDATABASE_H

#include <QList>
#include <QSqlDatabase>
#include <QString>
#include <QStringList>

class SocRegisterDatabase {
public:
    struct RegisterSummary {
        qint64 id = -1;
        QString name;
    };

    struct RegisterDetail {
        qint64 id = -1;
        QString silicon;
        QString derivative;
        QString name;
        QString referencePage;
        QString address;
        QString comment;
    };

    struct BitfieldRow {
        QString bits;
        QString bitfield;
        QString subBitfield;
        QString softwareAccess;
        QString hardwareAccess;
        QString defaultValue;
        QString description;
    };

    SocRegisterDatabase();
    ~SocRegisterDatabase();

    bool initialize(QString* errorMessage = nullptr);
    QString databasePath() const;
    QStringList siliconNames(QString* errorMessage = nullptr) const;
    QStringList derivatives(const QString& silicon, QString* errorMessage = nullptr) const;
    QList<RegisterSummary> registers(const QString& silicon,
                                     const QString& regularExpression,
                                     QString* errorMessage = nullptr) const;
    bool registerDetails(qint64 registerId,
                         RegisterDetail* detail,
                         QList<BitfieldRow>* rows,
                         QString* errorMessage = nullptr) const;

private:
    void close();

    QSqlDatabase m_database;
    QString m_databasePath;
};

#endif // CORE_SOCREGISTERDATABASE_H
