// SPDX-FileCopyrightText: 2026 BitLoupe developers
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/socregisterdatabase.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QSet>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

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

struct SourceDerivative {
    QString name;
    QString registersFile;
};

struct SourceSoc {
    QString name;
    QList<SourceDerivative> derivatives;
    QString registersFile;
};

QString firstString(const QJsonObject& object, const QStringList& keys)
{
    for (const QString& key : keys) {
        const QJsonValue value = object.value(key);
        if (value.isString() && !value.toString().trimmed().isEmpty())
            return value.toString().trimmed();
    }
    return QString();
}

QJsonValue firstValue(const QJsonObject& object, const QStringList& keys)
{
    for (const QString& key : keys) {
        if (object.contains(key))
            return object.value(key);
    }
    return QJsonValue();
}

QStringList derivativeNames(const QString& value)
{
    QStringList names;
    for (const QString& part : value.split(QLatin1Char(','), Qt::SkipEmptyParts)) {
        const QString name = part.trimmed();
        if (!name.isEmpty() && !names.contains(name))
            names.append(name);
    }
    return names;
}

QString catalogPath()
{
    const QString overridePath = qEnvironmentVariable("BITLOUPE_SOC_CONFIG").trimmed();
    if (!overridePath.isEmpty())
        return QFileInfo(overridePath).absoluteFilePath();

    const QDir applicationDirectory(QCoreApplication::applicationDirPath());

    // Canonical deployed location: the conf/ folder beside the executable.
    const QString confPath = applicationDirectory.filePath(QStringLiteral("conf/socregs.conf"));
    if (QFileInfo::exists(confPath))
        return QFileInfo(confPath).absoluteFilePath();

    const QString applicationPath = applicationDirectory.filePath(QStringLiteral("socregs.conf"));
    if (QFileInfo::exists(applicationPath))
        return applicationPath;

    const QString parentPath = applicationDirectory.filePath(QStringLiteral("../socregs.conf"));
    if (QFileInfo::exists(parentPath))
        return QFileInfo(parentPath).absoluteFilePath();

    return confPath;
}

QString cachePath()
{
    const QString overridePath = qEnvironmentVariable("BITLOUPE_SOC_CACHE").trimmed();
    if (!overridePath.isEmpty())
        return QFileInfo(overridePath).absoluteFilePath();

    const QDir applicationDirectory(QCoreApplication::applicationDirPath());

    // Canonical deployed location: the data/ folder beside the executable, next to
    // the bundled register JSON files (conf/ is reserved for user-editable config).
    const QString dataDirPath = applicationDirectory.filePath(QStringLiteral("data"));
    if (QFileInfo::exists(dataDirPath))
        return QDir(dataDirPath).filePath(QStringLiteral("socregs.db"));

    const QString confConfig = applicationDirectory.filePath(QStringLiteral("conf/settings.conf"));
    if (QFileInfo::exists(confConfig))
        return QDir(QFileInfo(confConfig).absolutePath()).filePath(QStringLiteral("socregs.db"));

    const QString applicationConfig = applicationDirectory.filePath(QStringLiteral("settings.conf"));
    if (QFileInfo::exists(applicationConfig))
        return applicationDirectory.filePath(QStringLiteral("socregs.db"));

    const QFileInfo parentConfig(applicationDirectory.filePath(QStringLiteral("../settings.conf")));
    if (parentConfig.exists())
        return QDir(parentConfig.absolutePath()).filePath(QStringLiteral("socregs.db"));

    return QDir(dataDirPath).filePath(QStringLiteral("socregs.db"));
}

QString resolvedPath(const QString& path, const QString& catalogDirectory)
{
    if (path.isEmpty())
        return QString();
    return QFileInfo(QDir::isRelativePath(path)
        ? QDir(catalogDirectory).filePath(path)
        : path).absoluteFilePath();
}

bool readJsonObject(const QString& path, QJsonObject* object, QByteArray* bytes, QString* error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = QObject::tr("Could not open %1: %2")
                .arg(QDir::toNativeSeparators(path), file.errorString());
        return false;
    }

    const QByteArray content = file.readAll();
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(content, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (error)
            *error = QObject::tr("Invalid JSON file %1: %2")
                .arg(QDir::toNativeSeparators(path), parseError.errorString());
        return false;
    }

    if (object)
        *object = document.object();
    if (bytes)
        *bytes = content;
    return true;
}

bool parseCatalog(const QString& path, QList<SourceSoc>* socs, QByteArray* catalogBytes, QString* error)
{
    QJsonObject root;
    if (!readJsonObject(path, &root, catalogBytes, error))
        return false;

    QJsonArray values;
    const QJsonValue collection = firstValue(root, {
        QStringLiteral("supported_socs"), QStringLiteral("supportedSoCs"),
        QStringLiteral("silicons")
    });
    if (collection.isArray()) {
        values = collection.toArray();
    } else if (!firstString(root, {
                   QStringLiteral("silicon_name"), QStringLiteral("soc_name"),
                   QStringLiteral("name"), QStringLiteral("Support Silicon Name")
               }).isEmpty()) {
        values.append(root);
    }

    for (const QJsonValue& value : values) {
        if (!value.isObject())
            continue;
        const QJsonObject object = value.toObject();
        SourceSoc soc;
        soc.name = firstString(object, {
            QStringLiteral("silicon_name"), QStringLiteral("soc_name"),
            QStringLiteral("name"), QStringLiteral("Support Silicon Name")
        });
        soc.registersFile = firstString(object, {
            QStringLiteral("registers_file"), QStringLiteral("registers_information"),
            QStringLiteral("Registers Information")
        });

        const QJsonValue derivativesValue = firstValue(object, {
            QStringLiteral("derivatives"), QStringLiteral("supported_derivatives"),
            QStringLiteral("deravatives"), QStringLiteral("Support Derivative"),
            QStringLiteral("Support Deraviative")
        });
        const QJsonArray derivatives = derivativesValue.isArray()
            ? derivativesValue.toArray()
            : QJsonArray{derivativesValue};
        for (const QJsonValue& derivativeValue : derivatives) {
            if (derivativeValue.isString()) {
                for (const QString& name : derivativeNames(derivativeValue.toString()))
                    soc.derivatives.append({name, QString()});
            } else if (derivativeValue.isObject()) {
                const QJsonObject derivativeObject = derivativeValue.toObject();
                SourceDerivative derivative;
                derivative.name = firstString(derivativeObject, {
                    QStringLiteral("name"), QStringLiteral("derivative"),
                    QStringLiteral("deravitive")
                });
                derivative.registersFile = firstString(derivativeObject, {
                    QStringLiteral("registers_file"), QStringLiteral("registers_information"),
                    QStringLiteral("Registers Information")
                });
                if (!derivative.name.isEmpty())
                    soc.derivatives.append(derivative);
            }
        }
        if (!soc.name.isEmpty() && !soc.derivatives.isEmpty())
            socs->append(soc);
    }

    if (socs->isEmpty()) {
        if (error)
            *error = QObject::tr("No supported SoCs found in %1")
                .arg(QDir::toNativeSeparators(path));
        return false;
    }
    return true;
}

bool computeSourceHash(const QString& catalogFile,
                       const QList<SourceSoc>& socs,
                       const QByteArray& catalogBytes,
                       QByteArray* hash,
                       QString* error)
{
    QCryptographicHash md5(QCryptographicHash::Md5);
    md5.addData(QFileInfo(catalogFile).absoluteFilePath().toUtf8());
    md5.addData(catalogBytes);

    const QString catalogDirectory = QFileInfo(catalogFile).absolutePath();
    QSet<QString> paths;
    for (const SourceSoc& soc : socs) {
        for (const SourceDerivative& derivative : soc.derivatives) {
            const QString configuredPath = derivative.registersFile.isEmpty()
                ? soc.registersFile
                : derivative.registersFile;
            paths.insert(resolvedPath(configuredPath, catalogDirectory));
        }
    }

    QStringList sortedPaths = paths.values();
    sortedPaths.sort(Qt::CaseInsensitive);
    for (const QString& path : sortedPaths) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            if (error)
                *error = QObject::tr("Could not open %1: %2")
                    .arg(QDir::toNativeSeparators(path), file.errorString());
            return false;
        }
        md5.addData(path.toUtf8());
        while (!file.atEnd())
            md5.addData(file.read(1024 * 1024));
    }

    *hash = md5.result().toHex();
    return true;
}

bool execute(QSqlQuery* query, const QString& sql, QString* error)
{
    if (query->exec(sql))
        return true;
    if (error)
        *error = query->lastError().text();
    return false;
}

QString jsonScalar(const QJsonValue& value)
{
    if (value.isUndefined() || value.isNull())
        return QString();
    return value.toVariant().toString();
}

bool insertRegisterFile(QSqlDatabase& database,
                        qint64 derivativeId,
                        const QString& path,
                        QString* error)
{
    QJsonObject root;
    if (!readJsonObject(path, &root, nullptr, error))
        return false;
    const QJsonValue registersValue = root.value(QStringLiteral("registers"));
    if (!registersValue.isArray()) {
        if (error)
            *error = QObject::tr("Invalid register file %1: missing 'registers' array")
                .arg(QDir::toNativeSeparators(path));
        return false;
    }

    QSqlQuery registerQuery(database);
    registerQuery.prepare(QStringLiteral(
        "INSERT INTO registers(derivative_id, name, ref_page, address, description) "
        "VALUES(?, ?, ?, ?, ?)"));
    QSqlQuery fieldQuery(database);
    fieldQuery.prepare(QStringLiteral(
        "INSERT INTO bitfields(register_id, bits, name, sub_name, sw_type, hw_type, "
        "default_value, description, search_text) VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?)"));

    for (const QJsonValue& registerValue : registersValue.toArray()) {
        if (!registerValue.isObject())
            continue;
        const QJsonObject reg = registerValue.toObject();
        const QString registerName = reg.value(QStringLiteral("register_name")).toString();
        if (registerName.isEmpty())
            continue;
        const QString registerDescription = firstString(reg, {
            QStringLiteral("description"), QStringLiteral("comment")
        });
        registerQuery.bindValue(0, derivativeId);
        registerQuery.bindValue(1, registerName);
        registerQuery.bindValue(2, reg.value(QStringLiteral("ref")).toObject()
                           .value(QStringLiteral("page")).toVariant());
        registerQuery.bindValue(3, reg.value(QStringLiteral("address")).toString());
        registerQuery.bindValue(4, registerDescription);
        if (!registerQuery.exec()) {
            if (error)
                *error = registerQuery.lastError().text();
            return false;
        }
        const qint64 registerId = registerQuery.lastInsertId().toLongLong();

        for (const QJsonValue& bitfieldValue : reg.value(QStringLiteral("bitfields")).toArray()) {
            if (!bitfieldValue.isObject())
                continue;
            const QJsonObject bitfield = bitfieldValue.toObject();
            const QString bitfieldName = bitfield.value(QStringLiteral("name")).toString();
            const QString bitfieldDescription = bitfield.value(QStringLiteral("description")).toString();
            QJsonArray subFields = bitfield.value(QStringLiteral("sub_bitfields")).toArray();
            if (subFields.isEmpty())
                subFields = bitfield.value(QStringLiteral("enum_values")).toArray();

            const QString parentSearch = QStringList{
                registerName, registerDescription, bitfieldName, bitfieldDescription
            }.join(QLatin1Char('\n'));
            fieldQuery.bindValue(0, registerId);
            fieldQuery.bindValue(1, jsonScalar(bitfield.value(QStringLiteral("bits"))));
            fieldQuery.bindValue(2, bitfieldName);
            fieldQuery.bindValue(3, QStringLiteral("--"));
            fieldQuery.bindValue(4, bitfield.value(QStringLiteral("sw_type")).toString());
            fieldQuery.bindValue(5, bitfield.value(QStringLiteral("hw_type")).toString());
            fieldQuery.bindValue(6, jsonScalar(bitfield.value(QStringLiteral("default"))));
            fieldQuery.bindValue(7, bitfieldDescription);
            fieldQuery.bindValue(8, parentSearch);
            if (!fieldQuery.exec()) {
                if (error)
                    *error = fieldQuery.lastError().text();
                return false;
            }

            for (const QJsonValue& subValue : subFields) {
                if (!subValue.isObject())
                    continue;
                const QJsonObject sub = subValue.toObject();
                const QString subName = sub.value(QStringLiteral("name")).toString();
                const QString subDescription = sub.value(QStringLiteral("description")).toString();
                const QString description = subDescription.isEmpty()
                    ? bitfieldDescription
                    : subDescription;
                const QString search = QStringList{
                    registerName, registerDescription, bitfieldName,
                    bitfieldDescription, subName, subDescription
                }.join(QLatin1Char('\n'));

                fieldQuery.bindValue(0, registerId);
                fieldQuery.bindValue(1, QStringLiteral("--"));
                fieldQuery.bindValue(2, QStringLiteral("--"));
                fieldQuery.bindValue(3, subName.isEmpty() ? QStringLiteral("--") : subName);
                fieldQuery.bindValue(4, QStringLiteral("--"));
                fieldQuery.bindValue(5, QStringLiteral("--"));
                fieldQuery.bindValue(6, jsonScalar(sub.contains(QStringLiteral("default"))
                    ? sub.value(QStringLiteral("default"))
                    : sub.value(QStringLiteral("value"))));
                fieldQuery.bindValue(7, description);
                fieldQuery.bindValue(8, search);
                if (!fieldQuery.exec()) {
                    if (error)
                        *error = fieldQuery.lastError().text();
                    return false;
                }
            }
        }
    }
    return true;
}

bool rebuildDatabase(const QString& databasePath,
                     const QString& catalogFile,
                     const QList<SourceSoc>& socs,
                     const QByteArray& sourceHash,
                     QString* error)
{
    QDir().mkpath(QFileInfo(databasePath).absolutePath());
    QFile::remove(databasePath);

    const QString connectionName = QStringLiteral("socregs-build-%1")
        .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        database.setDatabaseName(databasePath);
        if (!database.open()) {
            if (error)
                *error = database.lastError().text();
            return false;
        }

        QSqlQuery query(database);
        const QStringList schema = {
            QStringLiteral("PRAGMA foreign_keys = ON"),
            QStringLiteral("CREATE TABLE metadata(key TEXT PRIMARY KEY, value TEXT NOT NULL)"),
            QStringLiteral("CREATE TABLE socs(id INTEGER PRIMARY KEY, name TEXT NOT NULL UNIQUE)"),
            QStringLiteral("CREATE TABLE derivatives(id INTEGER PRIMARY KEY, soc_id INTEGER NOT NULL, name TEXT NOT NULL, source_file TEXT NOT NULL, FOREIGN KEY(soc_id) REFERENCES socs(id), UNIQUE(soc_id, name))"),
            QStringLiteral("CREATE TABLE registers(id INTEGER PRIMARY KEY, derivative_id INTEGER NOT NULL, name TEXT NOT NULL, ref_page TEXT, address TEXT, description TEXT, FOREIGN KEY(derivative_id) REFERENCES derivatives(id))"),
            QStringLiteral("CREATE TABLE bitfields(id INTEGER PRIMARY KEY, register_id INTEGER NOT NULL, bits TEXT, name TEXT, sub_name TEXT, sw_type TEXT, hw_type TEXT, default_value TEXT, description TEXT, search_text TEXT NOT NULL, FOREIGN KEY(register_id) REFERENCES registers(id))"),
            QStringLiteral("CREATE INDEX idx_derivatives_soc ON derivatives(soc_id, name)"),
            QStringLiteral("CREATE INDEX idx_registers_derivative ON registers(derivative_id, name)"),
            QStringLiteral("CREATE INDEX idx_bitfields_register ON bitfields(register_id)")
        };
        for (const QString& sql : schema) {
            if (!execute(&query, sql, error)) {
                database.close();
                return false;
            }
        }
        if (!database.transaction()) {
            if (error)
                *error = database.lastError().text();
            database.close();
            return false;
        }

        query.prepare(QStringLiteral("INSERT INTO metadata(key, value) VALUES('source_md5', ?)"));
        query.addBindValue(QString::fromLatin1(sourceHash));
        if (!query.exec()) {
            if (error)
                *error = query.lastError().text();
            database.rollback();
            database.close();
            return false;
        }

        const QString catalogDirectory = QFileInfo(catalogFile).absolutePath();
        QSqlQuery socQuery(database);
        socQuery.prepare(QStringLiteral("INSERT INTO socs(name) VALUES(?)"));
        QSqlQuery derivativeQuery(database);
        derivativeQuery.prepare(QStringLiteral(
            "INSERT INTO derivatives(soc_id, name, source_file) VALUES(?, ?, ?)"));

        for (const SourceSoc& soc : socs) {
            socQuery.bindValue(0, soc.name);
            if (!socQuery.exec()) {
                if (error)
                    *error = socQuery.lastError().text();
                database.rollback();
                database.close();
                return false;
            }
            const qint64 socId = socQuery.lastInsertId().toLongLong();
            for (const SourceDerivative& derivative : soc.derivatives) {
                const QString sourceFile = resolvedPath(
                    derivative.registersFile.isEmpty() ? soc.registersFile : derivative.registersFile,
                    catalogDirectory);
                derivativeQuery.bindValue(0, socId);
                derivativeQuery.bindValue(1, derivative.name);
                derivativeQuery.bindValue(2, sourceFile);
                if (!derivativeQuery.exec()) {
                    if (error)
                        *error = derivativeQuery.lastError().text();
                    database.rollback();
                    database.close();
                    return false;
                }
                if (!insertRegisterFile(database, derivativeQuery.lastInsertId().toLongLong(),
                                        sourceFile, error)) {
                    database.rollback();
                    database.close();
                    return false;
                }
            }
        }

        if (!database.commit()) {
            if (error)
                *error = database.lastError().text();
            database.close();
            return false;
        }
        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
    return true;
}

QString databaseHash(const QString& path)
{
    if (!QFileInfo::exists(path))
        return QString();
    const QString connectionName = QStringLiteral("socregs-check-%1")
        .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    QString value;
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        database.setDatabaseName(path);
        if (database.open()) {
            QSqlQuery query(database);
            if (query.exec(QStringLiteral(
                    "SELECT value FROM metadata WHERE key='source_md5'")) && query.next()) {
                value = query.value(0).toString();
            }
            database.close();
        }
    }
    QSqlDatabase::removeDatabase(connectionName);
    return value;
}

void setQueryError(const QSqlQuery& query, QString* error)
{
    if (error)
        *error = query.lastError().text();
}

} // namespace

SocRegisterDatabase::SocRegisterDatabase() = default;

SocRegisterDatabase::~SocRegisterDatabase()
{
    close();
}

bool SocRegisterDatabase::initialize(QString* errorMessage)
{
    socRegsLog() << "[SocRegsDB]" << this << "initialize() BEGIN";
    close();
    if (!QSqlDatabase::isDriverAvailable(QStringLiteral("QSQLITE"))) {
        socRegsLog() << "[SocRegsDB]" << this << "initialize() FAILED: QSQLITE driver unavailable";
        if (errorMessage)
            *errorMessage = QObject::tr("Qt SQLite driver is not available");
        return false;
    }

    const QString catalogFile = catalogPath();
    socRegsLog() << "[SocRegsDB]" << this << "initialize() catalogFile=" << catalogFile;
    QList<SourceSoc> socs;
    QByteArray catalogBytes;
    if (!parseCatalog(catalogFile, &socs, &catalogBytes, errorMessage)) {
        socRegsLog() << "[SocRegsDB]" << this << "initialize() parseCatalog FAILED:"
                   << (errorMessage ? *errorMessage : QString());
        return false;
    }
    socRegsLog() << "[SocRegsDB]" << this << "initialize() parseCatalog OK, socs=" << socs.size();

    QByteArray sourceHash;
    if (!computeSourceHash(catalogFile, socs, catalogBytes, &sourceHash, errorMessage))
        return false;

    m_databasePath = cachePath();
    socRegsLog() << "[SocRegsDB]" << this << "initialize() databasePath=" << m_databasePath
               << "sourceHash=" << sourceHash;
    const QString existingHash = databaseHash(m_databasePath);
    socRegsLog() << "[SocRegsDB]" << this << "initialize() existingHash=" << existingHash
               << "matches=" << (existingHash == QString::fromLatin1(sourceHash));
    if (existingHash != QString::fromLatin1(sourceHash)) {
        socRegsLog() << "[SocRegsDB]" << this << "initialize() REBUILDING database (hash mismatch)";
        QElapsedTimer rebuildTimer;
        rebuildTimer.start();
        const bool rebuilt = rebuildDatabase(m_databasePath, catalogFile, socs, sourceHash, errorMessage);
        socRegsLog() << "[SocRegsDB]" << this << "initialize() rebuildDatabase" << (rebuilt ? "OK" : "FAILED")
                   << "in" << rebuildTimer.elapsed() << "ms";
        if (!rebuilt) {
            QFile::remove(m_databasePath);
            return false;
        }
    }

    const QString connectionName = QStringLiteral("socregs-runtime-%1")
        .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    socRegsLog() << "[SocRegsDB]" << this << "initialize() opening runtime connection" << connectionName;
    m_database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
    m_database.setDatabaseName(m_databasePath);
    if (!m_database.open()) {
        socRegsLog() << "[SocRegsDB]" << this << "initialize() open() FAILED:" << m_database.lastError().text();
        if (errorMessage)
            *errorMessage = m_database.lastError().text();
        close();
        return false;
    }
    socRegsLog() << "[SocRegsDB]" << this << "initialize() END OK, connectionName=" << connectionName;
    return true;
}

QString SocRegisterDatabase::databasePath() const
{
    return m_databasePath;
}

QStringList SocRegisterDatabase::siliconNames(QString* errorMessage) const
{
    socRegsLog() << "[SocRegsDB]" << this << "siliconNames() BEGIN"
               << "connectionValid=" << m_database.isValid() << "isOpen=" << m_database.isOpen();
    QStringList values;
    QSqlQuery query(m_database);
    if (!query.exec(QStringLiteral("SELECT name FROM socs ORDER BY name COLLATE NOCASE"))) {
        socRegsLog() << "[SocRegsDB]" << this << "siliconNames() exec() FAILED:" << query.lastError().text();
        setQueryError(query, errorMessage);
        return values;
    }
    while (query.next())
        values.append(query.value(0).toString());
    socRegsLog() << "[SocRegsDB]" << this << "siliconNames() END count=" << values.size() << values;
    return values;
}

QStringList SocRegisterDatabase::derivatives(const QString& silicon, QString* errorMessage) const
{
    socRegsLog() << "[SocRegsDB]" << this << "derivatives() BEGIN silicon=" << silicon
               << "connectionValid=" << m_database.isValid() << "isOpen=" << m_database.isOpen();
    QStringList values;
    QElapsedTimer timer;
    timer.start();
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
        "SELECT d.name FROM derivatives d JOIN socs s ON s.id=d.soc_id "
        "WHERE s.name=? ORDER BY d.id"));
    query.addBindValue(silicon);
    if (!query.exec()) {
        socRegsLog() << "[SocRegsDB]" << this << "derivatives() exec() FAILED:" << query.lastError().text();
        setQueryError(query, errorMessage);
        return values;
    }
    while (query.next())
        values.append(query.value(0).toString());
    socRegsLog() << "[SocRegsDB]" << this << "derivatives() END count=" << values.size()
               << "elapsedMs=" << timer.elapsed();
    return values;
}

QList<SocRegisterDatabase::RegisterSummary> SocRegisterDatabase::registers(
    const QString& silicon,
    const QString& regularExpression,
    QString* errorMessage) const
{
    socRegsLog() << "[SocRegsDB]" << this << "registers() BEGIN silicon=" << silicon
               << "regex=" << regularExpression
               << "connectionValid=" << m_database.isValid() << "isOpen=" << m_database.isOpen();
    QElapsedTimer timer;
    timer.start();
    QList<RegisterSummary> values;
    QRegularExpression expression(regularExpression, QRegularExpression::CaseInsensitiveOption);
    if (!regularExpression.isEmpty() && !expression.isValid()) {
        socRegsLog() << "[SocRegsDB]" << this << "registers() invalid regex:" << expression.errorString();
        if (errorMessage)
            *errorMessage = QObject::tr("Invalid regular expression: %1").arg(expression.errorString());
        return values;
    }

    QSqlQuery query(m_database);
    // Derivatives for a SoC commonly share the same source register file, which
    // would otherwise duplicate every register once per derivative here; group by
    // name and keep the lowest register id as the representative row for detail lookup.
    query.prepare(QStringLiteral(
        "SELECT MIN(r.id), r.name, r.description, COALESCE(group_concat(b.search_text, char(10)), '') "
        "FROM registers r "
        "JOIN derivatives d ON d.id=r.derivative_id "
        "JOIN socs s ON s.id=d.soc_id "
        "LEFT JOIN bitfields b ON b.register_id=r.id "
        "WHERE s.name=? "
        "GROUP BY r.name ORDER BY MIN(r.id)"));
    query.addBindValue(silicon);
    socRegsLog() << "[SocRegsDB]" << this << "registers() calling exec(), elapsedMsSoFar=" << timer.elapsed();
    if (!query.exec()) {
        socRegsLog() << "[SocRegsDB]" << this << "registers() exec() FAILED:" << query.lastError().text();
        setQueryError(query, errorMessage);
        return values;
    }
    socRegsLog() << "[SocRegsDB]" << this << "registers() exec() OK, elapsedMs=" << timer.elapsed()
               << "-- now iterating rows";
    while (query.next()) {
        const QString search = query.value(1).toString() + QLatin1Char('\n')
            + query.value(2).toString() + QLatin1Char('\n') + query.value(3).toString();
        if (!regularExpression.isEmpty() && !expression.match(search).hasMatch())
            continue;
        values.append({query.value(0).toLongLong(), query.value(1).toString()});
    }
    socRegsLog() << "[SocRegsDB]" << this << "registers() END matched=" << values.size()
               << "totalElapsedMs=" << timer.elapsed();
    return values;
}

bool SocRegisterDatabase::registerDetails(qint64 registerId,
                                          RegisterDetail* detail,
                                          QList<BitfieldRow>* rows,
                                          QString* errorMessage) const
{
    socRegsLog() << "[SocRegsDB]" << this << "registerDetails() BEGIN registerId=" << registerId
               << "connectionValid=" << m_database.isValid() << "isOpen=" << m_database.isOpen();
    QSqlQuery registerQuery(m_database);
    registerQuery.prepare(QStringLiteral(
        "SELECT r.id, s.name, d.name, r.name, r.ref_page, r.address, r.description "
        "FROM registers r JOIN derivatives d ON d.id=r.derivative_id "
        "JOIN socs s ON s.id=d.soc_id WHERE r.id=?"));
    registerQuery.addBindValue(registerId);
    if (!registerQuery.exec() || !registerQuery.next()) {
        socRegsLog() << "[SocRegsDB]" << this << "registerDetails() register query FAILED:"
                   << registerQuery.lastError().text();
        setQueryError(registerQuery, errorMessage);
        return false;
    }
    if (detail) {
        detail->id = registerQuery.value(0).toLongLong();
        detail->silicon = registerQuery.value(1).toString();
        detail->derivative = registerQuery.value(2).toString();
        detail->name = registerQuery.value(3).toString();
        detail->referencePage = registerQuery.value(4).toString();
        detail->address = registerQuery.value(5).toString();
        detail->comment = registerQuery.value(6).toString();
    }

    if (rows) {
        rows->clear();
        QSqlQuery fieldQuery(m_database);
        fieldQuery.prepare(QStringLiteral(
            "SELECT bits, name, sub_name, sw_type, hw_type, default_value, description "
            "FROM bitfields WHERE register_id=? ORDER BY id"));
        fieldQuery.addBindValue(registerId);
        if (!fieldQuery.exec()) {
            socRegsLog() << "[SocRegsDB]" << this << "registerDetails() bitfield query FAILED:"
                       << fieldQuery.lastError().text();
            setQueryError(fieldQuery, errorMessage);
            return false;
        }
        while (fieldQuery.next()) {
            rows->append({
                fieldQuery.value(0).toString(), fieldQuery.value(1).toString(),
                fieldQuery.value(2).toString(), fieldQuery.value(3).toString(),
                fieldQuery.value(4).toString(), fieldQuery.value(5).toString(),
                fieldQuery.value(6).toString()
            });
        }
        socRegsLog() << "[SocRegsDB]" << this << "registerDetails() bitfield rows=" << rows->size();
    }
    socRegsLog() << "[SocRegsDB]" << this << "registerDetails() END OK registerId=" << registerId;
    return true;
}

void SocRegisterDatabase::close()
{
    if (!m_database.isValid()) {
        socRegsLog() << "[SocRegsDB]" << this << "close() no-op, connection not valid";
        return;
    }
    const QString connectionName = m_database.connectionName();
    socRegsLog() << "[SocRegsDB]" << this << "close() BEGIN connectionName=" << connectionName
               << "isOpen=" << m_database.isOpen();
    m_database.close();
    m_database = QSqlDatabase();
    QSqlDatabase::removeDatabase(connectionName);
    socRegsLog() << "[SocRegsDB]" << this << "close() END, removed" << connectionName;
}
