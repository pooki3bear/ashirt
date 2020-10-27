// Copyright 2020, Verizon Media
// Licensed under the terms of MIT. See LICENSE file in project root for terms.

#include "databaseconnection.h"

#include <QDir>
#include <QVariant>
#include <iostream>
#include <vector>

#include "exceptions/databaseerr.h"
#include "exceptions/fileerror.h"
#include "helpers/constants.h"

DatabaseConnection::DatabaseConnection() {
  QString dbPath = Constants::dbLocation();
  const QString DRIVER("QSQLITE");
  if (QSqlDatabase::isDriverAvailable(DRIVER)) {
    db = QSqlDatabase::addDatabase(DRIVER);
    db.setDatabaseName(dbPath);
    auto dbFileRoot = dbPath.left(dbPath.lastIndexOf("/"));
    QDir().mkpath(dbFileRoot);
  }
  else {
    throw DBDriverUnavailableError("SQLite");
  }
}

void DatabaseConnection::connect() {
  if (!db.open()) {
    throw db.lastError();
  }
  migrateDB();
}

void DatabaseConnection::close() noexcept { db.close(); }

model::Server DatabaseConnection::getServerByUuid(QString serverUuid) {
  QString query = "SELECT"
      " uuid, server_name, access_key, secret_key, host_path, deleted_at"
      " FROM servers"
      " WHERE uuid = ?";
  auto resultSet = executeQuery(&db, query, {serverUuid});

  model::Server item;
  if (resultSet.first()) {
    item.serverUuid = resultSet.value("uuid").toString();
    item.serverName = resultSet.value("server_name").toString();
    item.accessKey = resultSet.value("access_key").toString();
    item.secretKey = resultSet.value("secret_key").toString();
    item.hostPath = resultSet.value("host_path").toString();
    auto delDate = resultSet.value("deleted_at");
    if (delDate.isNull()) {
      item.deletedAt = nullptr;
    }
    else {
      auto castedDelDate = delDate.toDateTime();
      item.deletedAt = &castedDelDate;
      item.deletedAt->setTimeSpec(Qt::UTC);
    }
  }
  return item;
}

std::vector<model::Server> DatabaseConnection::getServers(bool includeDeleted) {
  QString query = "SELECT"
      " uuid, server_name, access_key, secret_key, host_path, deleted_at"
      " FROM servers"
      " WHERE 1=1";
  if (!includeDeleted) {
    query += " AND deleted_at IS NULL";
  }
  auto resultSet = executeQuery(&db, query);

  std::vector<model::Server> rtn;

  while (resultSet.next()) {
    model::Server item;

    item.serverUuid = resultSet.value("uuid").toString();
    item.serverName = resultSet.value("server_name").toString();
    item.accessKey = resultSet.value("access_key").toString();
    item.secretKey = resultSet.value("secret_key").toString();
    item.hostPath = resultSet.value("host_path").toString();
    auto delDate = resultSet.value("deleted_at");
    if (delDate.isNull()) {
      item.deletedAt = nullptr;
    }
    else {
      auto castedDelDate = delDate.toDateTime();
      item.deletedAt = &castedDelDate;
      item.deletedAt->setTimeSpec(Qt::UTC);
    }

    rtn.push_back(item);
  }

  return rtn;
}

qint64 DatabaseConnection::createServer(model::Server newServer) {
  if (newServer.serverName.trimmed().isEmpty() || newServer.serverUuid.trimmed().isEmpty()) {
    throw BadDBData("New servers require a non-empty name and uuid");
  }
  return doInsert(&db,
                  "INSERT INTO servers"
                  " (uuid, server_name, access_key, secret_key, host_path)"
                  " VALUES"
                  " (?, ?, ?, ?, ?)",
                  {newServer.serverUuid, newServer.serverName, newServer.accessKey,
                   newServer.secretKey, newServer.hostPath});
}

void DatabaseConnection::updateServerDetails(QString newAccessKey, QString newSecretKey,
                                             QString newHostPath, QString serverUuid) {
  serverUuid = valueOrCurrentServer(serverUuid);
  QString query = "UPDATE servers SET access_key=?, secret_key=?, host_path=?"
      " WHERE uuid=?";
  executeQuery(&db, query, {newAccessKey, newSecretKey, newHostPath, serverUuid});
}

void DatabaseConnection::updateFullServerDetails(QString newName, QString newAccessKey, QString newSecretKey,
                                             QString newHostPath, QString serverUuid) {
  serverUuid = valueOrCurrentServer(serverUuid);
  QString query = "UPDATE servers SET server_name=?, access_key=?, secret_key=?, host_path=?"
      " WHERE uuid=?";
  executeQuery(&db, query, {newName, newAccessKey, newSecretKey, newHostPath, serverUuid});
}

void DatabaseConnection::deleteServer(QString serverUuid) {
  QString query = "UPDATE servers SET deleted_at = datetime('now') WHERE uuid = ?";
  executeQuery(&db, query, {serverUuid});
}

void DatabaseConnection::restoreServer(QString serverUuid) {
  QString query = "UPDATE servers SET deleted_at = NULL WHERE uuid = ?";
  executeQuery(&db, query, {serverUuid});
}

bool DatabaseConnection::hasServer(QString serverUuid) {
  return getSingleField("SELECT count(uuid) FROM servers WHERE uuid=?", {serverUuid}).toULongLong() > 0;
}

QString DatabaseConnection::accessKey(QString serverUuid) {
  serverUuid = valueOrCurrentServer(serverUuid);
  return getSingleField("SELECT access_key FROM servers WHERE uuid=?", {serverUuid}).toString();
}

QString DatabaseConnection::secretKey(QString serverUuid) {
  serverUuid = valueOrCurrentServer(serverUuid);
  return getSingleField("SELECT secret_key FROM servers WHERE uuid=?", {serverUuid}).toString();
}

QString DatabaseConnection::hostPath(QString serverUuid) {
  serverUuid = valueOrCurrentServer(serverUuid);
  return getSingleField("SELECT host_path FROM servers WHERE uuid=?", {serverUuid}).toString();
}

QString DatabaseConnection::serverName(QString serverUuid) {
  serverUuid = valueOrCurrentServer(serverUuid);
  return getSingleField("SELECT server_name FROM servers WHERE uuid=?", {serverUuid}).toString();
}

qint64 DatabaseConnection::createEvidence(const QString &filepath, const QString &operationSlug,
                                          const QString &contentType) {
  return doInsert(&db,
                  "INSERT INTO evidence"
                  " (path, operation_slug, content_type, recorded_date)"
                  " VALUES"
                  " (?, ?, ?, datetime('now'))",
                  {filepath, operationSlug, contentType});
}

model::Evidence DatabaseConnection::getEvidenceDetails(qint64 evidenceID) {
  model::Evidence rtn;
  QSqlQuery query = executeQuery(
      &db,
      "SELECT"
      " id, path, operation_slug, server_uuid, content_type, description, error, recorded_date, upload_date"
      " FROM evidence"
      " WHERE id=? LIMIT 1",
      {evidenceID});

  if (query.first()) {
    rtn.id = query.value("id").toLongLong();
    rtn.path = query.value("path").toString();
    rtn.operationSlug = query.value("operation_slug").toString();
    rtn.serverUuid = query.value("server_uuid").toString();
    rtn.contentType = query.value("content_type").toString();
    rtn.description = query.value("description").toString();
    rtn.errorText = query.value("error").toString();
    rtn.recordedDate = query.value("recorded_date").toDateTime();
    rtn.uploadDate = query.value("upload_date").toDateTime();

    rtn.recordedDate.setTimeSpec(Qt::UTC);
    rtn.uploadDate.setTimeSpec(Qt::UTC);

    auto getTagQuery = executeQuery(&db,
                                    "SELECT"
                                    " id, tag_id, name"
                                    " FROM tags"
                                    " WHERE evidence_id=?",
                                    {evidenceID});
    while (getTagQuery.next()) {
      rtn.tags.emplace_back(model::Tag(getTagQuery.value("id").toLongLong(),
                                       getTagQuery.value("tag_id").toLongLong(),
                                       getTagQuery.value("name").toString()));
    }
  }
  else {
    std::cout << "Could not find evidence with id: " << evidenceID << std::endl;
  }
  return rtn;
}

void DatabaseConnection::updateEvidenceDescription(const QString &newDescription, qint64 evidenceID) {
  executeQuery(&db, "UPDATE evidence SET description=? WHERE id=?", {newDescription, evidenceID});
}

void DatabaseConnection::deleteEvidence(qint64 evidenceID) {
  executeQuery(&db, "DELETE FROM evidence WHERE id=?", {evidenceID});
}

void DatabaseConnection::updateEvidenceError(const QString &errorText, qint64 evidenceID) {
  executeQuery(&db, "UPDATE evidence SET error=? WHERE id=?", {errorText, evidenceID});
}

void DatabaseConnection::updateEvidenceSubmitted(qint64 evidenceID) {
  executeQuery(&db, "UPDATE evidence SET upload_date=datetime('now') WHERE id=?", {evidenceID});
}

void DatabaseConnection::setEvidenceTags(const std::vector<model::Tag> &newTags,
                                         qint64 evidenceID) {
  QList<QVariant> newTagIds;
  for (const auto &tag : newTags) {
    newTagIds.push_back(tag.serverTagId);
  }
  executeQuery(&db, "DELETE FROM tags WHERE tag_id NOT IN (?) AND evidence_id = ?",
               {newTagIds, evidenceID});

  auto currentTagsResult =
      executeQuery(&db, "SELECT tag_id FROM tags WHERE evidence_id = ?", {evidenceID});
  QList<qint64> currentTags;
  while (currentTagsResult.next()) {
    currentTags.push_back(currentTagsResult.value("tag_id").toLongLong());
  }
  struct dataset {
    qint64 evidenceID = 0;
    qint64 tagID = 0;
    QString name;
  };
  std::vector<dataset> tagDataToInsert;
  QString baseQuery = "INSERT INTO tags (evidence_id, tag_id, name) VALUES ";
  for (const auto &newTag : newTags) {
    if (currentTags.count(newTag.serverTagId) == 0) {
      dataset item;
      item.evidenceID = evidenceID;
      item.tagID = newTag.serverTagId;
      item.name = newTag.tagName;
      tagDataToInsert.push_back(item);
    }
  }

  // one possible concern: we are going to be passing a lot of parameters
  // sqlite indicates it's default is 100 passed parameter, but it can "handle thousands"
  if (!tagDataToInsert.empty()) {
    std::vector<QVariant> args;
    baseQuery += "(?,?,?)";
    baseQuery += QString(", (?,?,?)").repeated(int(tagDataToInsert.size() - 1));
    for (const auto &item : tagDataToInsert) {
      args.emplace_back(item.evidenceID);
      args.emplace_back(item.tagID);
      args.emplace_back(item.name);
    }
    executeQuery(&db, baseQuery, args);
  }
}

DBQuery DatabaseConnection::buildGetEvidenceWithFiltersQuery(const EvidenceFilters &filters) {
  QString query =
      "SELECT"
      " id, path, server_uuid, operation_slug, content_type, description, error, recorded_date, upload_date"
      " FROM evidence";
  std::vector<QVariant> values;
  std::vector<QString> parts;

  if (filters.hasError != Tri::Any) {
    parts.emplace_back(" error LIKE ? ");
    // _% will ensure at least one character exists in the error column, ensuring it's populated
    values.emplace_back(filters.hasError == Tri::Yes ? "_%" : "");
  }
  if (filters.submitted != Tri::Any) {
    parts.emplace_back((filters.submitted == Tri::Yes) ? " upload_date IS NOT NULL "
                                                       : " upload_date IS NULL ");
  }
  if (!filters.operationSlug.isEmpty()) {
    parts.emplace_back(" operation_slug = ? ");
    values.emplace_back(filters.operationSlug);
  }
  if (!filters.contentType.isEmpty()) {
    parts.emplace_back(" content_type = ? ");
    values.emplace_back(filters.contentType);
  }
  if (filters.startDate.isValid()) {
    parts.emplace_back(" recorded_date >= ? ");
    values.emplace_back(filters.startDate);
  }
  if (filters.endDate.isValid()) {
    auto realEndDate = filters.endDate.addDays(1);
    parts.emplace_back(" recorded_date < ? ");
    values.emplace_back(realEndDate);
  }

  if (!parts.empty()) {
    query += " WHERE " + parts.at(0);
    for (size_t i = 1; i < parts.size(); i++) {
      query += " AND " + parts.at(i);
    }
  }
  return DBQuery(query, values);
}

std::vector<model::Evidence> DatabaseConnection::getEvidenceWithFilters(
    const EvidenceFilters &filters) {
  auto dbQuery = buildGetEvidenceWithFiltersQuery(filters);
  auto resultSet = executeQuery(&db, dbQuery.query(), dbQuery.values());

  std::vector<model::Evidence> allEvidence;
  while (resultSet.next()) {
    model::Evidence evi;
    evi.id = resultSet.value("id").toLongLong();
    evi.path = resultSet.value("path").toString();
    evi.operationSlug = resultSet.value("operation_slug").toString();
    evi.serverUuid = resultSet.value("server_uuid").toString();
    evi.contentType = resultSet.value("content_type").toString();
    evi.description = resultSet.value("description").toString();
    evi.errorText = resultSet.value("error").toString();
    evi.recordedDate = resultSet.value("recorded_date").toDateTime();
    evi.uploadDate = resultSet.value("upload_date").toDateTime();

    evi.recordedDate.setTimeSpec(Qt::UTC);
    evi.uploadDate.setTimeSpec(Qt::UTC);

    allEvidence.push_back(evi);
  }

  return allEvidence;
}

QString DatabaseConnection::currentServer() { 
  return AppSettings::getInstance().serverUuid();
}

QString DatabaseConnection::valueOrCurrentServer(QString maybeServerUuid) {
  return maybeServerUuid = (maybeServerUuid == "") ? currentServer() : maybeServerUuid;
}

QVariant DatabaseConnection::getSingleField(const QString& query, const std::vector<QVariant> &args) {
  auto resultSet = executeQuery(&db, query, args);
  if(resultSet.next()) {
    return resultSet.value(0);
  }
  return QVariant();
}

bool DatabaseConnection::hasAppliedSystemMigration(QString systemMigrationName) {
  QString query = "SELECT count(migration_name) FROM system_migrations WHERE migration_name=?";

  auto resultSet = executeQuery(&db, query, {systemMigrationName});
  if (resultSet.next()) {
    return (resultSet.value(0).toULongLong() > 0);
  }
  return false;
}

qint64 DatabaseConnection::applySystemMigration(QString systemMigrationName) {
  return doInsert(&db, "INSERT INTO system_migrations (migration_name) VALUES (?)",
                  {systemMigrationName});
}

void DatabaseConnection::migrateDB() {
  std::cout << "Checking database state" << std::endl;
  auto migrationsToApply = DatabaseConnection::getUnappliedMigrations();

  for (const QString &newMigration : migrationsToApply) {
    QFile migrationFile(":/migrations/" + newMigration);
    auto ok = migrationFile.open(QFile::ReadOnly);
    if (!ok) {
      throw FileError::mkError("Error reading migration file",
                               migrationFile.fileName().toStdString(), migrationFile.error());
    }
    auto content = QString(migrationFile.readAll());
    migrationFile.close();

    std::cout << "Applying DB Migration: " << newMigration.toStdString() << std::endl;
    auto upScript = extractMigrateUpContent(content);
    for (QString stmt : upScript) {
      if (stmt.trimmed() == "") { // skip blank statements
        continue;
      }
      executeQuery(&db, stmt);
    }
    executeQuery(&db,
                 "INSERT INTO migrations (migration_name, applied_at) VALUES (?, datetime('now'))",
                 {newMigration});
  }
  std::cout << "All DB migrations applied" << std::endl;
}

QStringList DatabaseConnection::getUnappliedMigrations() {
  QDir migrationsDir(":/migrations");

  auto allMigrations = migrationsDir.entryList(QDir::Files, QDir::Name);
  QStringList appliedMigrations;
  QStringList migrationsToApply;

  QSqlQuery dbMigrations("SELECT migration_name FROM migrations");

  if (dbMigrations.exec()) {
    while (dbMigrations.next()) {
      appliedMigrations << dbMigrations.value("migration_name").toString();
    }
  }
  // compare the two list to find gaps
  for (const QString &possibleMigration : allMigrations) {
    if (possibleMigration.right(4) != ".sql") {
      continue;  // assume non-sql files aren't actual migrations.
    }
    auto foundIndex = appliedMigrations.indexOf(possibleMigration);
    if (foundIndex == -1) {
      migrationsToApply << possibleMigration;
    }
    else {
      appliedMigrations.removeAt(foundIndex);
    }
  }
  if (!appliedMigrations.empty()) {
    throw BadDatabaseStateError();
  }
  return migrationsToApply;
}

QStringList DatabaseConnection::extractMigrateUpContent(const QString &allContent) noexcept {
  auto statementSplitter = QString("\n;");
  auto copying = false;
  QString upContent;
  for (const QString &line : allContent.split("\n")) {
    if (line.trimmed().toLower() == "-- +migrate up") {
      copying = true;
    }
    else if (line.trimmed().toLower() == "-- +migrate down") {
      if (copying) {
        break;
      }
      copying = false;
    }
    else if (copying) {
      upContent.append(line + "\n");
    }
  }
  return upContent.split(statementSplitter);
}

QSqlQuery DatabaseConnection::executeQuery(QSqlDatabase *db, const QString &stmt,
                                           const std::vector<QVariant> &args) {
  QSqlQuery query(*db);

  if (!query.prepare(stmt)) {
    throw query.lastError();
  }

  for (const auto &arg : args) {
    query.addBindValue(arg);
  }

  if (!query.exec()) {
    throw query.lastError();
  }
  return query;
}

qint64 DatabaseConnection::doInsert(QSqlDatabase *db, const QString &stmt,
                                    const std::vector<QVariant> &args) {
  auto query = executeQuery(db, stmt, args);

  return query.lastInsertId().toLongLong();
}
