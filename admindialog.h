#ifndef ADMINDIALOG_H
#define ADMINDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QRegularExpressionValidator>

class AdminDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AdminDialog(QWidget *parent = nullptr);

private slots:
    void onAddAccount();
    void onDeleteAccount();
    void onUpdateBalance();
    void onResetPin();
    void onTransfer();       
    void refreshTable();

private:
    QString hashPin(const QString &pin);

    double getBalance(const QString &card);
    bool updateBalance(const QString &card, double newBal);
    bool recordTransaction(const QString &card,
                           const QString &type,
                           double amount,
                           double balanceAfter);

private:
    QLineEdit *m_cardEdit = nullptr;
    QLineEdit *m_pinEdit = nullptr;
    QLineEdit *m_balanceEdit = nullptr;

    QLineEdit *m_fromCardEdit = nullptr;
    QLineEdit *m_toCardEdit = nullptr;
    QLineEdit *m_transferAmountEdit = nullptr;

    QPushButton *m_addButton = nullptr;
    QPushButton *m_deleteButton = nullptr;
    QPushButton *m_updateBalanceButton = nullptr;
    QPushButton *m_resetPinButton = nullptr;
    QPushButton *m_transferButton = nullptr;

    QTableWidget *m_table = nullptr;
};

#endif // ADMINDIALOG_H
