#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStackedWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QListWidget>
#include <QList>
#include <QPoint>

#include "atmcontroller.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onLoginClicked();
    void onLogoutClicked();

    void onWithdrawClicked();
    void onDepositClicked();
    void onShowHistoryClicked();
    void onChangePinClicked();
    void onTransferClicked();

    void onHistoryContextMenuRequested(const QPoint &pos);

private:
    void setupLoginPage();
    void setupMenuPage();
    void showLoginPage();
    void showMenuPage();
    void updateBalanceLabel();
    void showError(const QString &msg);
    void showInfo(const QString &msg);
    void printReceipt(const QString &operation,
                      double amount,
                      double balanceAfter,
                      const QString &extra = QString());

    QWidget *m_loginPage = nullptr;
    QWidget *m_menuPage  = nullptr;

    QStackedWidget *m_stack = nullptr;

    QLineEdit *m_cardEdit = nullptr;
    QLineEdit *m_pinEdit  = nullptr;
    QLabel    *m_loginStatusLabel = nullptr;

    QLabel      *m_balanceLabel = nullptr;
    QListWidget *m_historyList  = nullptr;

    QPushButton *m_withdrawButton   = nullptr;
    QPushButton *m_depositButton    = nullptr;
    QPushButton *m_historyButton    = nullptr;
    QPushButton *m_changePinButton  = nullptr;
    QPushButton *m_transferButton   = nullptr;
    QPushButton *m_logoutButton     = nullptr;

    AtmController m_atm;

    QList<AtmController::TransactionRecord> m_lastTransactions;
};

#endif // MAINWINDOW_H

