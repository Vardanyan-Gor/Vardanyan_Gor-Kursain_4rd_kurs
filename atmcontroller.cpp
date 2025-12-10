#include "atmcontroller.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QDebug>
#include <QCryptographicHash>
#include <QDateTime>

namespace {
const QString ADMIN_CARD = "0000000000000000";
}

AtmController::AtmController()
{
}

QString AtmController::hashPin(const QString &pin)
{
    QByteArray data = pin.toUtf8();
    QByteArray hash = QCryptographicHash::hash(data, QCryptographicHash::Sha256);
    return QString::fromLatin1(hash.toHex());
}

QString AtmController::currentCardNumber() const
{
    if (!m_currentCardNumber.has_value())
        return QString();
    return m_currentCardNumber.value();
}

bool AtmController::login(const QString &cardNumber, const QString &pin)
{
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        qDebug() << "БД не открыта в login()";
        return false;
    }

    QSqlQuery query;
    query.prepare("SELECT pin, failed_attempts, locked_until "
                  "FROM accounts WHERE card_number = :card");
    query.bindValue(":card", cardNumber);

    if (!query.exec()) {
        qDebug() << "Ошибка login() SELECT:" << query.lastError().text();
        return false;
    }

    if (!query.next()) {
        return false;
    }

    QString storedHash = query.value(0).toString();
    int failedAttempts = query.value(1).toInt();
    QVariant lockedVar = query.value(2);

    QDateTime now = QDateTime::currentDateTime();

    if (!lockedVar.isNull()) {
        QDateTime lockedUntil = lockedVar.toDateTime();
        if (lockedUntil.isValid() && lockedUntil > now) {
            return false;
        }
    }

    QString inputHash = hashPin(pin);

    if (inputHash != storedHash) {
        failedAttempts++;

        QSqlQuery upd;
        upd.prepare("UPDATE accounts "
                    "SET failed_attempts = :fa, locked_until = :lu "
                    "WHERE card_number = :card");
        upd.bindValue(":fa", failedAttempts);

        if (failedAttempts >= 3) {
            QDateTime lockedUntil = now.addSecs(5 * 60);
            upd.bindValue(":lu", lockedUntil);
        } else {
            upd.bindValue(":lu", QVariant(QVariant::DateTime));
        }

        upd.bindValue(":card", cardNumber);

        if (!upd.exec()) {
            qDebug() << "Ошибка login() UPDATE failed_attempts:"
                     << upd.lastError().text();
        }

        return false;
    }

    QSqlQuery reset;
    reset.prepare("UPDATE accounts "
                  "SET failed_attempts = 0, locked_until = NULL "
                  "WHERE card_number = :card");
    reset.bindValue(":card", cardNumber);
    if (!reset.exec()) {
        qDebug() << "Ошибка login() RESET:" << reset.lastError().text();
    }

    m_currentCardNumber = cardNumber;
    return true;
}

void AtmController::logout()
{
    m_currentCardNumber.reset();
}

double AtmController::getBalanceFromDb(const QString &cardNumber) const
{
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        qDebug() << "БД не открыта в getBalanceFromDb()";
        return 0.0;
    }

    QSqlQuery query;
    query.prepare("SELECT balance FROM accounts WHERE card_number = :card");
    query.bindValue(":card", cardNumber);

    if (!query.exec()) {
        qDebug() << "Ошибка getBalanceFromDb():" << query.lastError().text();
        return 0.0;
    }

    if (query.next()) {
        return query.value(0).toDouble();
    }

    return 0.0;
}

bool AtmController::updateBalanceInDb(const QString &cardNumber, double newBalance)
{
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        qDebug() << "БД не открыта в updateBalanceInDb()";
        return false;
    }

    QSqlQuery query;
    query.prepare("UPDATE accounts SET balance = :bal WHERE card_number = :card");
    query.bindValue(":bal", newBalance);
    query.bindValue(":card", cardNumber);

    if (!query.exec()) {
        qDebug() << "Ошибка updateBalanceInDb():" << query.lastError().text();
        return false;
    }

    return true;
}

double AtmController::getAtmCash() const
{
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        qDebug() << "БД не открыта в getAtmCash()";
        return 0.0;
    }

    QSqlQuery query("SELECT cash_total FROM atm_state WHERE id = 1");
    if (!query.exec()) {
        qDebug() << "Ошибка getAtmCash():" << query.lastError().text();
        return 0.0;
    }

    if (query.next()) {
        return query.value(0).toDouble();
    }

    return 0.0;
}

bool AtmController::updateAtmCash(double newCash)
{
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        qDebug() << "БД не открыта в updateAtmCash()";
        return false;
    }

    QSqlQuery query;
    query.prepare("UPDATE atm_state SET cash_total = :cash WHERE id = 1");
    query.bindValue(":cash", newCash);

    if (!query.exec()) {
        qDebug() << "Ошибка updateAtmCash():" << query.lastError().text();
        return false;
    }

    return true;
}

bool AtmController::recordTransactionFor(const QString &cardNumber,
                                         const QString &type,
                                         double amount,
                                         double balanceAfter)
{
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        qDebug() << "БД не открыта в recordTransactionFor()";
        return false;
    }

    QSqlQuery query;
    query.prepare("INSERT INTO transactions "
                  "(card_number, type, amount, balance_after) "
                  "VALUES (:card, :type, :amount, :bal)");
    query.bindValue(":card", cardNumber);
    query.bindValue(":type", type);
    query.bindValue(":amount", amount);
    query.bindValue(":bal", balanceAfter);

    if (!query.exec()) {
        qDebug() << "Ошибка recordTransactionFor():" << query.lastError().text();
        return false;
    }

    return true;
}

bool AtmController::recordTransaction(const QString &type,
                                      double amount,
                                      double balanceAfter)
{
    if (!m_currentCardNumber.has_value())
        return false;
    return recordTransactionFor(m_currentCardNumber.value(),
                                type, amount, balanceAfter);
}

double AtmController::currentBalance() const
{
    if (!m_currentCardNumber.has_value())
        return 0.0;
    return getBalanceFromDb(m_currentCardNumber.value());
}

bool AtmController::withdraw(double amount)
{
    if (!m_currentCardNumber.has_value())
        return false;
    if (amount <= 0)
        return false;

    QString card = m_currentCardNumber.value();
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        qDebug() << "БД не открыта в withdraw()";
        return false;
    }

    double balance = getBalanceFromDb(card);
    double atmCash = getAtmCash();

    if (amount > balance || amount > atmCash) {
        return false;
    }

    double newBalance = balance - amount;
    double newAtmCash = atmCash - amount;

    if (!db.transaction()) {
        qDebug() << "Не удалось начать транзакцию в withdraw()";
        return false;
    }

    if (!updateBalanceInDb(card, newBalance)) {
        db.rollback();
        return false;
    }

    if (!updateAtmCash(newAtmCash)) {
        db.rollback();
        return false;
    }

    if (!recordTransaction("withdraw", amount, newBalance)) {
        db.rollback();
        return false;
    }

    if (!db.commit()) {
        qDebug() << "Не удалось зафиксировать транзакцию в withdraw()";
        return false;
    }

    return true;
}

bool AtmController::deposit(double amount)
{
    if (!m_currentCardNumber.has_value())
        return false;
    if (amount <= 0)
        return false;

    QString card = m_currentCardNumber.value();
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        qDebug() << "БД не открыта в deposit()";
        return false;
    }

    double balance = getBalanceFromDb(card);
    double newBalance = balance + amount;

    if (!db.transaction()) {
        qDebug() << "Не удалось начать транзакцию в deposit()";
        return false;
    }

    if (!updateBalanceInDb(card, newBalance)) {
        db.rollback();
        return false;
    }

    if (!recordTransaction("deposit", amount, newBalance)) {
        db.rollback();
        return false;
    }

    if (!db.commit()) {
        qDebug() << "Не удалось зафиксировать транзакцию в deposit()";
        return false;
    }

    return true;
}

bool AtmController::transferTo(const QString &targetCardNumber, double amount)
{
    if (!m_currentCardNumber.has_value())
        return false;
    if (amount <= 0)
        return false;

    QString sourceCard = m_currentCardNumber.value();
    QString targetCard = targetCardNumber.trimmed();

    if (targetCard.isEmpty() || targetCard == sourceCard)
        return false;

    if (targetCard == ADMIN_CARD)
        return false;

    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        qDebug() << "БД не открыта в transferTo()";
        return false;
    }

    if (!db.transaction()) {
        qDebug() << "Не удалось начать транзакцию в transferTo()";
        return false;
    }

    double sourceBalance = getBalanceFromDb(sourceCard);
    if (amount > sourceBalance) {
        db.rollback();
        return false;
    }

    QSqlQuery query;
    query.prepare("SELECT balance FROM accounts WHERE card_number = :card");
    query.bindValue(":card", targetCard);

    if (!query.exec()) {
        qDebug() << "Ошибка transferTo() SELECT target:"
                 << query.lastError().text();
        db.rollback();
        return false;
    }

    if (!query.next()) {
        db.rollback();
        return false;
    }

    double targetBalance = query.value(0).toDouble();

    double newSourceBalance = sourceBalance - amount;
    double newTargetBalance = targetBalance + amount;

    if (!updateBalanceInDb(sourceCard, newSourceBalance)) {
        db.rollback();
        return false;
    }

    if (!updateBalanceInDb(targetCard, newTargetBalance)) {
        db.rollback();
        return false;
    }

    if (!recordTransactionFor(sourceCard, "transfer_out",
                              amount, newSourceBalance)) {
        db.rollback();
        return false;
    }

    if (!recordTransactionFor(targetCard, "transfer_in",
                              amount, newTargetBalance)) {
        db.rollback();
        return false;
    }

    if (!db.commit()) {
        qDebug() << "Не удалось зафиксировать транзакцию в transferTo()";
        return false;
    }

    return true;
}

bool AtmController::changePin(const QString &oldPin, const QString &newPin)
{
    if (!m_currentCardNumber.has_value())
        return false;

    if (newPin.isEmpty())
        return false;

    QString card = m_currentCardNumber.value();
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        qDebug() << "БД не открыта в changePin()";
        return false;
    }

    QSqlQuery check;
    check.prepare("SELECT pin FROM accounts WHERE card_number = :card");
    check.bindValue(":card", card);

    if (!check.exec()) {
        qDebug() << "Ошибка changePin() SELECT:" << check.lastError().text();
        return false;
    }

    if (!check.next())
        return false;

    QString storedHash = check.value(0).toString();
    QString oldHash = hashPin(oldPin);

    if (storedHash != oldHash)
        return false;

    QString newHash = hashPin(newPin);

    QSqlQuery upd;
    upd.prepare("UPDATE accounts "
                "SET pin = :pin, failed_attempts = 0, locked_until = NULL "
                "WHERE card_number = :card");
    upd.bindValue(":pin", newHash);
    upd.bindValue(":card", card);

    if (!upd.exec()) {
        qDebug() << "Ошибка changePin() UPDATE:" << upd.lastError().text();
        return false;
    }

    double balance = getBalanceFromDb(card);
    recordTransaction("pin_change", 0.0, balance);

    return true;
}

QList<AtmController::TransactionRecord>
AtmController::lastTransactions(int limit) const
{
    QList<TransactionRecord> list;

    if (!m_currentCardNumber.has_value())
        return list;

    QString card = m_currentCardNumber.value();

    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        qDebug() << "БД не открыта в lastTransactions()";
        return list;
    }

    QString sql =
        "SELECT type, amount, balance_after, ts "
        "FROM transactions "
        "WHERE card_number = :card "
        "ORDER BY ts DESC "
        "LIMIT " + QString::number(limit);

    QSqlQuery query;
    query.prepare(sql);
    query.bindValue(":card", card);

    if (!query.exec()) {
        qDebug() << "Ошибка lastTransactions():" << query.lastError().text();
        return list;
    }

    while (query.next()) {
        TransactionRecord rec;
        rec.type = query.value(0).toString();
        rec.amount = query.value(1).toDouble();
        rec.balanceAfter = query.value(2).toDouble();
        rec.timestamp = query.value(3).toDateTime();
        list.append(rec);
    }

    return list;
}
