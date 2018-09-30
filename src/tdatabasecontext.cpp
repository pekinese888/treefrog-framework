/* Copyright (c) 2015-2017, AOYAMA Kazuharu
 * All rights reserved.
 *
 * This software may be used and distributed according to the terms of
 * the New BSD License, which is incorporated herein by reference.
 */

#include <QtCore>
#include <QSqlDatabase>
#include <QThreadStorage>
#include <TWebApplication>
#include <TKvsDriver>
#include <ctime>
#include "tdatabasecontext.h"
#include "tsqldatabasepool.h"
#include "tkvsdatabasepool.h"
#include "tsystemglobal.h"

// Stores a pointer to current database context into TLS
//  - qulonglong type to prevent qThreadStorage_deleteData() function to work
static QThreadStorage<qulonglong> databaseContextPtrTls;

/*!
  \class TDatabaseContext
  \brief The TDatabaseContext class is the base class of contexts for
  database access.
*/

TDatabaseContext::TDatabaseContext() :
    sqlDatabases(),
    kvsDatabases()
{ }


TDatabaseContext::~TDatabaseContext()
{
    release();
}


QSqlDatabase &TDatabaseContext::getSqlDatabase(int id)
{
    T_TRACEFUNC("id:%d", id);

    if (!Tf::app()->isSqlDatabaseAvailable()) {
        return sqlDatabases[0].database();  // invalid database
    }

    if (id < 0 || id >= Tf::app()->sqlDatabaseSettingsCount()) {
        throw RuntimeException("error database id", __FILE__, __LINE__);
    }

    TSqlTransaction &tx = sqlDatabases[id];
    QSqlDatabase &db = tx.database();
    if (! tx.isValid()) {
        db = TSqlDatabasePool::instance()->database(id);
        beginTransaction(db, id);
    } else {
        if (! tx.isActive()) {
            tx.rebegin();
        }
    }

    idleElapsed = (uint)std::time(nullptr);
    return db;
}


void TDatabaseContext::releaseSqlDatabases()
{
    rollbackTransactions();

    for (QMap<int, TSqlTransaction>::iterator it = sqlDatabases.begin(); it != sqlDatabases.end(); ++it) {
        TSqlDatabasePool::instance()->pool(it.value().database());
    }
    sqlDatabases.clear();
}


TKvsDatabase &TDatabaseContext::getKvsDatabase(TKvsDatabase::Type type)
{
    T_TRACEFUNC("type:%d", (int)type);

    TKvsDatabase &db = kvsDatabases[(int)type];
    if (!db.isValid()) {
        db = TKvsDatabasePool::instance()->database(type);
    }

    if (db.driver()) {
        db.driver()->moveToThread(QThread::currentThread());
    }

    idleElapsed = (uint)std::time(nullptr);
    return db;
}


void TDatabaseContext::releaseKvsDatabases()
{
    for (QMap<int, TKvsDatabase>::iterator it = kvsDatabases.begin(); it != kvsDatabases.end(); ++it) {
        TKvsDatabasePool::instance()->pool(it.value());
    }
    kvsDatabases.clear();
}


void TDatabaseContext::release()
{
    // Releases all SQL database sessions
    releaseSqlDatabases();

    // Releases all KVS database sessions
    releaseKvsDatabases();

    idleElapsed = 0;
}


void TDatabaseContext::setTransactionEnabled(bool enable, int id)
{
    if (id < 0) {
        tError("Invalid database ID: %d", id);
        return;
    }
    return sqlDatabases[id].setEnabled(enable);
}

// Obsoleted function
bool TDatabaseContext::beginTransaction(QSqlDatabase &database)
{
    int id = TSqlDatabasePool::getDatabaseId(database);
    return beginTransaction(database, id);
}


bool TDatabaseContext::beginTransaction(QSqlDatabase &database, int id)
{
    return sqlDatabases[id].begin(database);
}


void TDatabaseContext::commitTransactions()
{
    for (QMap<int, TSqlTransaction>::iterator it = sqlDatabases.begin(); it != sqlDatabases.end(); ++it) {
        it.value().commit();
    }
}


bool TDatabaseContext::commitTransaction(int id)
{
    if (id < 0 || id >= sqlDatabases.count()) {
        tError("Failed to commit transaction. Invalid database ID: %d", id);
        return false;
    }
    return sqlDatabases[id].commit();
}


void TDatabaseContext::rollbackTransactions()
{
    for (QMap<int, TSqlTransaction>::iterator it = sqlDatabases.begin(); it != sqlDatabases.end(); ++it) {
        it.value().rollback();
    }
}


bool TDatabaseContext::rollbackTransaction(int id)
{
    if (id < 0 || id >= sqlDatabases.count()) {
        tError("Failed to rollback transaction. Invalid database ID: %d", id);
        return false;
    }
    return sqlDatabases[id].rollback();
}


int TDatabaseContext::idleTime() const
{
    return (idleElapsed > 0) ? (uint)std::time(nullptr) - idleElapsed : -1;
}


TDatabaseContext *TDatabaseContext::currentDatabaseContext()
{
    return reinterpret_cast<TDatabaseContext*>(databaseContextPtrTls.localData());
}


void TDatabaseContext::setCurrentDatabaseContext(TDatabaseContext *context)
{
    if (context && databaseContextPtrTls.hasLocalData()) {
        tSystemWarn("Duplicate set : setCurrentDatabaseContext()");
    }
    databaseContextPtrTls.setLocalData((qulonglong)context);
}
