#ifndef ATMCONTROLLER_H
#define ATMCONTROLLER_H

#include <QString>
#include <QList>
#include <QDateTime>
#include <optional>

class AtmController
{
public:
    struct TransactionRecord {
        QString type;          
        double amount;
        double balanceAfter;
        QDateTime timestamp;
    };

    AtmController();

    static QString hashPin(const QString &pin);

    bool login(const QString &cardNumber, const QString &pin);
    void logout();

    bool isLoggedIn() const { return m_currentCardNumber.has_value(); }
    QString currentCardNumber() const;

    double currentBalance() const;

    bool withdraw(double amount);
    bool deposit(double amount);
    bool transferTo(const QString &targetCardNumber, double amount);

    bool changePin(const QString &oldPin, const QString &newPin);

    QList<TransactionRecord> lastTransactions(int limit = 10) const;

private:
    std::optional<QString> m_currentCardNumber;

    double getBalanceFromDb(const QString &cardNumber) const;
    bool updateBalanceInDb(const QString &cardNumber, double newBalance);

    double getAtmCash() const;
    bool updateAtmCash(double newCash);

    bool recordTransactionFor(const QString &cardNumber,
                              const QString &type,
                              double amount,
                              double balanceAfter);
    bool recordTransaction(const QString &type,
                           double amount,
                           double balanceAfter);
};

#endif // ATMCONTROLLER_H

