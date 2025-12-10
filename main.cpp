#include <QApplication>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>

#include "mainwindow.h"
#include "atmcontroller.h"

static bool initDatabase()
{
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName("atm.db");

    if (!db.open()) {
        qDebug() << "Не удалось открыть БД:" << db.lastError().text();
        return false;
    }

    QSqlQuery query;

    if (!query.exec(
            "CREATE TABLE IF NOT EXISTS accounts ("
            " card_number     TEXT PRIMARY KEY,"
            " pin             TEXT NOT NULL,"          
            " balance         REAL NOT NULL,"
            " failed_attempts INTEGER NOT NULL DEFAULT 0,"
            " locked_until    DATETIME NULL"
            ")"))
    {
        qDebug() << "Ошибка создания таблицы accounts:"
                 << query.lastError().text();
        return false;
    }

    if (!query.exec(
            "CREATE TABLE IF NOT EXISTS transactions ("
            " id            INTEGER PRIMARY KEY AUTOINCREMENT,"
            " card_number   TEXT NOT NULL,"
            " type          TEXT NOT NULL,"
            " amount        REAL NOT NULL,"
            " balance_after REAL NOT NULL,"
            " ts            DATETIME DEFAULT CURRENT_TIMESTAMP"
            ")"))
    {
        qDebug() << "Ошибка создания таблицы transactions:"
                 << query.lastError().text();
        return false;
    }

    if (!query.exec(
            "CREATE TABLE IF NOT EXISTS atm_state ("
            " id         INTEGER PRIMARY KEY CHECK (id = 1),"
            " cash_total REAL NOT NULL"
            ")"))
    {
        qDebug() << "Ошибка создания таблицы atm_state:"
                 << query.lastError().text();
        return false;
    }

    if (!query.exec("SELECT COUNT(*) FROM atm_state")) {
        qDebug() << "Ошибка SELECT COUNT(*) FROM atm_state:"
                 << query.lastError().text();
        return false;
    }

    int atmCount = 0;
    if (query.next()) {
        atmCount = query.value(0).toInt();
    }

    if (atmCount == 0) {
        QSqlQuery ins;
        if (!ins.exec("INSERT INTO atm_state (id, cash_total) "
                      "VALUES (1, 100000.0)"))
        {
            qDebug() << "Ошибка вставки начального состояния atm_state:"
                     << ins.lastError().text();
            return false;
        }
    }

    if (!query.exec("SELECT COUNT(*) FROM accounts")) {
        qDebug() << "Ошибка SELECT COUNT(*) FROM accounts:"
                 << query.lastError().text();
        return false;
    }

    int count = 0;
    if (query.next()) {
        count = query.value(0).toInt();
    }

    if (count == 0) {
        qDebug() << "Таблица accounts пуста, добавляю тестовые данные...";

        QString pin1 = AtmController::hashPin("1234");
        QString pin2 = AtmController::hashPin("0000");

        QSqlQuery ins;
        ins.prepare("INSERT INTO accounts (card_number, pin, balance) "
                    "VALUES (:card, :pin, :bal)");

        ins.bindValue(":card", "1111222233334444");
        ins.bindValue(":pin", pin1);
        ins.bindValue(":bal", 10000.0);
        if (!ins.exec()) {
            qDebug() << "Ошибка вставки аккаунта 1:"
                     << ins.lastError().text();
            return false;
        }

        ins.bindValue(":card", "5555666677778888");
        ins.bindValue(":pin", pin2);
        ins.bindValue(":bal", 5000.0);
        if (!ins.exec()) {
            qDebug() << "Ошибка вставки аккаунта 2:"
                     << ins.lastError().text();
            return false;
        }
    }

    return true;
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    if (!initDatabase()) {
        return -1;
    }

    MainWindow w;
    w.show();
    return a.exec();
}


