// Copyright 2020, Verizon Media
// Licensed under the terms of MIT. See LICENSE file in project root for terms.

#ifndef DATABASECONNECTION_H
#define DATABASECONNECTION_H

#include <QSqlDatabase>
#include <QSqlDriver>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QString>
#include <QVariant>

#include "forms/evidence_filter/evidencefilter.h"
#include "models/evidence.h"

class DBQuery {
 private:
  QString _query;
  std::vector<QVariant> _values;

 public:
  DBQuery(QString query) : DBQuery(query, {}) {}
  DBQuery(QString query, std::vector<QVariant> values) {
    this->_query = query;
    this->_values = values;
  }
  inline QString query() { return _query; }
  inline std::vector<QVariant> values() { return _values; }
};

/**
 * @brief The DatabaseConnection class manages a connection to the database and provides an interface
 *        into common operations
 */
class DatabaseConnection {
 public:
  /**
   * @brief DatabaseConnection constructs a connection to a specific sqlite database.
   *
   * @see Constants::dbLocation() for the location to the database file
   * @throws DBDriverUnavailable if the required database driver does not exist
   * @throws QSqlError in other cases
   */
  DatabaseConnection();

  void connect();
  void close() noexcept;

  bool hasAppliedSystemMigration(QString systemMigrationName);
  qint64 applySystemMigration(QString systemMigrationName);

  model::Evidence getEvidenceDetails(qint64 evidenceID);
  std::vector<model::Evidence> getEvidenceWithFilters(const EvidenceFilters &filters);
  qint64 createEvidence(const QString &filepath, const QString &operationSlug,
                        const QString &serverUuid, const QString &contentType);
  void deleteEvidence(qint64 evidenceID);
  void updateEvidenceDescription(const QString &newDescription, qint64 evidenceID);
  void updateEvidenceError(const QString &errorText, qint64 evidenceID);
  void updateEvidenceSubmitted(qint64 evidenceID);

  void setEvidenceTags(const std::vector<model::Tag> &newTags, qint64 evidenceID);

 private:
  DBQuery buildGetEvidenceWithFiltersQuery(const EvidenceFilters &filters);

  /**
   * @brief getSingleField retrieves a single value/cell from the provided query. If the query
   *        returns no rows, then an invalid QVariant is returned instead.
   * @param query The query to execute
   * @param args any bindings needed to execute the statement
   * @return a QVariant holding the result, or an invalid QVariant if no rows are returned
   * @throws QSqlError when a query error occurs
   */
  QVariant getSingleField(const QString& query, const std::vector<QVariant> &args);

  /**
   * @brief migrateDB checks the migration status and then performs the full migration for any
   *        lacking update.
   * @throws exceptions/FileError if a migration file cannot be found.
   */
  void migrateDB();

  /**
   * @brief getUnappliedMigrations Retrieves a list of all of the migrations that have not been
   *        applied to the local database.
   *        Note: All sql files must end in ".sql" to be picked up
   * @return A list of all of the migrations that have not yet been applied
   * @throws BadDatabaseStateError if some migrations have been applied that are not known
   * @throws QSqlError if database queries fail
   */
  QStringList getUnappliedMigrations();

  /**
   * @brief extractMigrateUpContent Parses the given migration content and retrieves only the
   *        portion that applies to the "up" / apply logic. The "down" section is ignored. The
   *        statements are interpreted via a line with a single semicolon separating multiple
   *        statements.
   * @param allContent The (raw) contents of the sql migration script
   * @return a QStringList containing each segment for the 'up' section
   */
  static QStringList extractMigrateUpContent(const QString &allContent) noexcept;


  /**
   * @brief executeQuery Attempts to execute the given stmt with the passed args. The statement is
   *        first prepared, and arg placements can be specified with "?".
   * @param db the database connection
   * @param stmt the query to execute
   * @param args any bindings needed to execute the statement
   * @return the executed query / the effective result set
   * @throws QSqlError when a query error occurs
   */
  static QSqlQuery executeQuery(QSqlDatabase *db, const QString &stmt,
                                const std::vector<QVariant> &args = {});

  /**
   * @brief doInsert is a version of executeQuery that returns the last inserted id, rather than the
   *        underlying query/response.
   * @param db the database connection
   * @param stmt the query to execute
   * @param args any bindings needed to execute the statement
   * @return the last inserted id if successful
   * @throws QSqlError when a query error occurs
   */
  static qint64 doInsert(QSqlDatabase *db, const QString &stmt, const std::vector<QVariant> &args);

 private:
  QSqlDatabase db;

};

#endif  // DATABASECONNECTION_H
