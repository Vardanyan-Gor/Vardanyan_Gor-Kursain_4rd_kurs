#ifndef ACCOUNT_H
#define ACCOUNT_H

#include <QString>

class Account
{
public:
    Account(const QString &cardNumber,
            const QString &pin,
            double balance)
        : m_cardNumber(cardNumber),
        m_pin(pin),
        m_balance(balance)
    {}

    const QString& cardNumber() const { return m_cardNumber; }
    bool checkPin(const QString &pin) const { return m_pin == pin; }

    double balance() const { return m_balance; }
    void deposit(double amount) { m_balance += amount; }
    bool withdraw(double amount) {
        if (amount <= 0 || amount > m_balance) return false;
        m_balance -= amount;
        return true;
    }

private:
    QString m_cardNumber;
    QString m_pin;
    double m_balance;
};

#endif // ACCOUNT_H
