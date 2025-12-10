#include "mainwindow.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QInputDialog>
#include <QFile>
#include <QDir>
#include <QCoreApplication>
#include <QTextStream>
#include <QDateTime>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QLineEdit>
#include <QMenu>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>

#include "admindialog.h"

namespace {
const QString ADMIN_CARD = "0000000000000000";
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    m_stack = new QStackedWidget(this);
    setCentralWidget(m_stack);

    setupLoginPage();
    setupMenuPage();

    showLoginPage();
    setWindowTitle("ATM (Qt + SQLite)");
    resize(600, 450);
}

MainWindow::~MainWindow()
{
}

void MainWindow::setupLoginPage()
{
    m_loginPage = new QWidget(this);

    auto *layout = new QVBoxLayout(m_loginPage);

    auto *cardLabel = new QLabel("Номер карты:", m_loginPage);
    m_cardEdit = new QLineEdit(m_loginPage);

    m_cardEdit->setMaxLength(16);
    m_cardEdit->setValidator(new QRegularExpressionValidator(
        QRegularExpression("^[0-9]{0,16}$"), this));

    auto *pinLabel = new QLabel("PIN:", m_loginPage);
    m_pinEdit = new QLineEdit(m_loginPage);
    m_pinEdit->setEchoMode(QLineEdit::Password);

    m_pinEdit->setMaxLength(4);
    m_pinEdit->setValidator(new QRegularExpressionValidator(
        QRegularExpression("^[0-9]{0,4}$"), this));

    auto *loginButton = new QPushButton("Войти", m_loginPage);
    m_loginStatusLabel = new QLabel("", m_loginPage);

    layout->addWidget(cardLabel);
    layout->addWidget(m_cardEdit);
    layout->addWidget(pinLabel);
    layout->addWidget(m_pinEdit);
    layout->addWidget(loginButton);
    layout->addWidget(m_loginStatusLabel);
    layout->addStretch();

    connect(loginButton, &QPushButton::clicked,
            this, &MainWindow::onLoginClicked);

    m_stack->addWidget(m_loginPage);
}

void MainWindow::setupMenuPage()
{
    m_menuPage = new QWidget(this);
    auto *layout = new QVBoxLayout(m_menuPage);

    m_balanceLabel = new QLabel("Баланс: 0.00", m_menuPage);



    auto *buttonsRow = new QHBoxLayout();
    m_withdrawButton = new QPushButton("Снять", m_menuPage);
    m_depositButton  = new QPushButton("Положить", m_menuPage);
    m_transferButton = new QPushButton("Перевод", m_menuPage);

    buttonsRow->addWidget(m_withdrawButton);
    buttonsRow->addWidget(m_depositButton);
    buttonsRow->addWidget(m_transferButton);

    m_historyButton    = new QPushButton("История операций", m_menuPage);
    m_changePinButton  = new QPushButton("Сменить PIN", m_menuPage);
    m_logoutButton     = new QPushButton("Завершить сеанс", m_menuPage);

    m_historyList = new QListWidget(m_menuPage);
    m_historyList->setContextMenuPolicy(Qt::CustomContextMenu);

    layout->addWidget(m_balanceLabel);
    layout->addLayout(buttonsRow);
    layout->addWidget(m_historyButton);
    layout->addWidget(m_changePinButton);
    layout->addWidget(m_logoutButton);
    layout->addWidget(new QLabel("Последние операции:", m_menuPage));
    layout->addWidget(m_historyList);
    layout->addStretch();

    connect(m_withdrawButton, &QPushButton::clicked,
            this, &MainWindow::onWithdrawClicked);
    connect(m_depositButton, &QPushButton::clicked,
            this, &MainWindow::onDepositClicked);
    connect(m_transferButton, &QPushButton::clicked,
            this, &MainWindow::onTransferClicked);
    connect(m_historyButton, &QPushButton::clicked,
            this, &MainWindow::onShowHistoryClicked);
    connect(m_changePinButton, &QPushButton::clicked,
            this, &MainWindow::onChangePinClicked);
    connect(m_logoutButton, &QPushButton::clicked,
            this, &MainWindow::onLogoutClicked);

    connect(m_historyList, &QListWidget::customContextMenuRequested,
            this, &MainWindow::onHistoryContextMenuRequested);

    m_stack->addWidget(m_menuPage);
}

void MainWindow::showLoginPage()
{
    m_cardEdit->clear();
    m_pinEdit->clear();
    m_loginStatusLabel->clear();
    m_stack->setCurrentWidget(m_loginPage);
}

void MainWindow::showMenuPage()
{
    updateBalanceLabel();
    m_historyList->clear();
    m_stack->setCurrentWidget(m_menuPage);
}

void MainWindow::updateBalanceLabel()
{
    double balance = m_atm.currentBalance();
    m_balanceLabel->setText(QString("Баланс: %1").arg(balance, 0, 'f', 2));
}

void MainWindow::showError(const QString &msg)
{
    QMessageBox::warning(this, "Ошибка", msg);
}

void MainWindow::showInfo(const QString &msg)
{
    QMessageBox::information(this, "Информация", msg);
}

void MainWindow::printReceipt(const QString &operation,
                              double amount,
                              double balanceAfter,
                              const QString &extra)
{
    auto reply = QMessageBox::question(
        this,
        "Печать чека",
        "Печатать чек?",
        QMessageBox::Yes | QMessageBox::No
        );

    if (reply != QMessageBox::Yes)
        return;

    QString card = m_atm.currentCardNumber();
    QString maskedCard = card;
    if (maskedCard.size() > 4)
        maskedCard = QString("**** **** **** %1").arg(card.right(4));

    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
    QString fileName = QString("receipt_%1_%2.txt")
                           .arg(card.right(4))
                           .arg(timestamp);

    QDir dir(QCoreApplication::applicationDirPath() + "/receipts");
    if (!dir.exists()) dir.mkpath(".");

    QString fullPath = dir.filePath(fileName);

    QFile file(fullPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        showError("Не удалось создать файл чека.");
        return;
    }

    QTextStream out(&file);
    out << "ATM RECEIPT\n";
    out << "Date: " << QDateTime::currentDateTime()
                           .toString("yyyy-MM-dd hh:mm:ss") << "\n";
    out << "Card: " << maskedCard << "\n";
    out << "Operation: " << operation << "\n";
    out << "Amount: " << amount << "\n";
    if (!extra.isEmpty())
        out << extra << "\n";
    out << "Balance after: " << balanceAfter << "\n";

    file.close();

    showInfo("Чек сохранён в папке receipts.");
}

void MainWindow::onLoginClicked()
{
    QString card = m_cardEdit->text();
    QString pin  = m_pinEdit->text();

    if (card.length() != 16) {
        m_loginStatusLabel->setText("Номер карты должен содержать 16 цифр.");
        return;
    }

    if (pin.length() != 4) {
        m_loginStatusLabel->setText("PIN должен содержать 4 цифры.");
        return;
    }

    if (card == ADMIN_CARD && pin == "9999") {
        AdminDialog dlg(this);
        dlg.exec();
        m_pinEdit->clear();
        return;
    }

    if (m_atm.login(card, pin)) {
        showMenuPage();
    } else {
        m_loginStatusLabel->setText("Вход не выполнен. Проверьте PIN или карту.");
    }
}

void MainWindow::onLogoutClicked()
{
    m_atm.logout();
    showLoginPage();
}

void MainWindow::onWithdrawClicked()
{
    if (!m_atm.isLoggedIn()) {
        showError("Сначала войдите в систему.");
        return;
    }

    QDialog dlg(this);
    dlg.setWindowTitle("Снятие средств");

    QVBoxLayout *layout = new QVBoxLayout(&dlg);
    QLabel *label = new QLabel("Введите сумму для снятия:", &dlg);
    QLineEdit *amountEdit = new QLineEdit(&dlg);
    amountEdit->setPlaceholderText("Сумма");
    amountEdit->setValidator(new QRegularExpressionValidator(
        QRegularExpression("^[0-9]+(\\.[0-9]{1,2})?$"), amountEdit));

    layout->addWidget(label);
    layout->addWidget(amountEdit);

    QDialogButtonBox *buttons =
        new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);

    layout->addWidget(buttons);

    QObject::connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted)
        return;

    bool ok = false;
    double amount = amountEdit->text().toDouble(&ok);
    if (!ok || amount <= 0) {
        showError("Введите корректную сумму для снятия.");
        return;
    }

    if (!m_atm.withdraw(amount)) {
        showError("Невозможно снять сумму (недостаточно средств или денег в банкомате).");
        return;
    }

    updateBalanceLabel();
    double newBalance = m_atm.currentBalance();

    showInfo("Операция снятия выполнена.");
    printReceipt("Снятие", amount, newBalance);
}

void MainWindow::onDepositClicked()
{
    if (!m_atm.isLoggedIn()) {
        showError("Сначала войдите в систему.");
        return;
    }

    QDialog dlg(this);
    dlg.setWindowTitle("Пополнение счёта");

    QVBoxLayout *layout = new QVBoxLayout(&dlg);
    QLabel *label = new QLabel("Введите сумму для пополнения:", &dlg);
    QLineEdit *amountEdit = new QLineEdit(&dlg);
    amountEdit->setPlaceholderText("Сумма");
    amountEdit->setValidator(new QRegularExpressionValidator(
        QRegularExpression("^[0-9]+(\\.[0-9]{1,2})?$"), amountEdit));

    layout->addWidget(label);
    layout->addWidget(amountEdit);

    QDialogButtonBox *buttons =
        new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);

    layout->addWidget(buttons);

    QObject::connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted)
        return;

    bool ok = false;
    double amount = amountEdit->text().toDouble(&ok);
    if (!ok || amount <= 0) {
        showError("Введите корректную сумму.");
        return;
    }

    if (!m_atm.deposit(amount)) {
        showError("Ошибка при пополнении.");
        return;
    }

    updateBalanceLabel();
    double newBalance = m_atm.currentBalance();

    showInfo("Счёт пополнен.");
    printReceipt("Пополнение", amount, newBalance);
}

void MainWindow::onShowHistoryClicked()
{
    m_historyList->clear();
    m_lastTransactions = m_atm.lastTransactions(10);

    for (int i = 0; i < m_lastTransactions.size(); ++i) {
        const auto &rec = m_lastTransactions[i];

        QString typeText;
        if (rec.type == "withdraw")
            typeText = "Снятие";
        else if (rec.type == "deposit")
            typeText = "Поступление";
        else if (rec.type == "transfer_out")
            typeText = "Перевод (списание)";
        else if (rec.type == "transfer_in")
            typeText = "Перевод (зачисление)";
        else if (rec.type == "pin_change")
            typeText = "Смена PIN";
        else if (rec.type == "admin_transfer_out")
            typeText = "Админ. перевод (списание)";
        else if (rec.type == "admin_transfer_in")
            typeText = "Админ. перевод (зачисление)";
        else
            typeText = rec.type;

        QString line = QString("%1 | %2 | %3 | баланс после: %4")
                           .arg(rec.timestamp.toString("yyyy-MM-dd hh:mm:ss"))
                           .arg(typeText)
                           .arg(rec.amount, 0, 'f', 2)
                           .arg(rec.balanceAfter, 0, 'f', 2);

        auto *item = new QListWidgetItem(line);
        item->setData(Qt::UserRole, i);
        m_historyList->addItem(item);
    }

    if (m_lastTransactions.isEmpty())
        m_historyList->addItem("Операций пока нет.");
}

void MainWindow::onChangePinClicked()
{
    if (!m_atm.isLoggedIn()) {
        showError("Сначала войдите в систему.");
        return;
    }

    auto getPin = [&](const QString &title,
                      const QString &label,
                      bool &okFlag) -> QString
    {
        QInputDialog dlg(this);
        dlg.setWindowTitle(title);
        dlg.setLabelText(label);
        dlg.setTextEchoMode(QLineEdit::Password);

        QLineEdit *edit = dlg.findChild<QLineEdit *>();
        if (edit) {
            edit->setMaxLength(4);
            edit->setValidator(new QRegularExpressionValidator(
                QRegularExpression("^[0-9]{0,4}$"), edit));
        }

        if (dlg.exec() == QDialog::Accepted) {
            okFlag = true;
            QString pin = dlg.textValue().trimmed();
            return pin;
        } else {
            okFlag = false;
            return QString();
        }
    };

    bool ok1 = false, ok2 = false, ok3 = false;

    QString oldPin = getPin("Смена PIN", "Введите текущий PIN:", ok1);
    if (!ok1) return;

    QString newPin1 = getPin("Смена PIN", "Введите новый PIN:", ok2);
    if (!ok2) return;

    QString newPin2 = getPin("Смена PIN", "Повторите новый PIN:", ok3);
    if (!ok3) return;

    if (oldPin.length() != 4 || newPin1.length() != 4 || newPin2.length() != 4) {
        showError("PIN должен содержать 4 цифры.");
        return;
    }

    if (newPin1 != newPin2) {
        showError("Новый PIN и его подтверждение не совпадают.");
        return;
    }

    if (!m_atm.changePin(oldPin, newPin1)) {
        showError("Не удалось сменить PIN. Проверьте текущий PIN.");
        return;
    }

    showInfo("PIN успешно изменён.");
}

void MainWindow::onTransferClicked()
{
    if (!m_atm.isLoggedIn()) {
        showError("Сначала войдите в систему.");
        return;
    }

    QDialog dlg(this);
    dlg.setWindowTitle("Перевод средств");

    QVBoxLayout *layout = new QVBoxLayout(&dlg);

    QLabel *cardLabel = new QLabel("Номер карты получателя (16 цифр):", &dlg);
    QLineEdit *cardEdit = new QLineEdit(&dlg);
    cardEdit->setMaxLength(16);
    cardEdit->setValidator(new QRegularExpressionValidator(
        QRegularExpression("^[0-9]{0,16}$"), cardEdit));

    QLabel *amountLabel = new QLabel("Сумма перевода:", &dlg);
    QLineEdit *amountEdit = new QLineEdit(&dlg);
    amountEdit->setValidator(new QRegularExpressionValidator(
        QRegularExpression("^[0-9]+(\\.[0-9]{1,2})?$"), amountEdit));

    layout->addWidget(cardLabel);
    layout->addWidget(cardEdit);
    layout->addWidget(amountLabel);
    layout->addWidget(amountEdit);

    QDialogButtonBox *buttons =
        new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    layout->addWidget(buttons);

    QObject::connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted)
        return;

    QString targetCard = cardEdit->text().trimmed();
    if (targetCard.length() != 16) {
        showError("Номер карты получателя должен содержать 16 цифр.");
        return;
    }

    if (targetCard == ADMIN_CARD) {
        showError("Операции с админской картой запрещены.");
        return;
    }

    bool ok = false;
    double amount = amountEdit->text().toDouble(&ok);
    if (!ok || amount <= 0) {
        showError("Введите корректную сумму для перевода.");
        return;
    }

    if (!m_atm.transferTo(targetCard, amount)) {
        showError("Не удалось выполнить перевод. Проверьте сумму и номер карты.");
        return;
    }

    updateBalanceLabel();
    double newBalance = m_atm.currentBalance();

    showInfo("Перевод выполнен.");
    printReceipt("Перевод", amount, newBalance,
                 QString("Получатель: %1").arg(targetCard));
}


void MainWindow::onHistoryContextMenuRequested(const QPoint &pos)
{
    QListWidgetItem *item = m_historyList->itemAt(pos);
    if (!item)
        return;

    bool ok = false;
    int idx = item->data(Qt::UserRole).toInt(&ok);
    if (!ok || idx < 0 || idx >= m_lastTransactions.size())
        return;

    QMenu menu(this);
    QAction *printAct = menu.addAction("Печатать чек");

    QAction *chosen = menu.exec(m_historyList->viewport()->mapToGlobal(pos));
    if (chosen != printAct)
        return;

    const auto &rec = m_lastTransactions[idx];

    QString typeText;
    if (rec.type == "withdraw")
        typeText = "Снятие";
    else if (rec.type == "deposit")
        typeText = "Поступление";
    else if (rec.type == "transfer_out")
        typeText = "Перевод (списание)";
    else if (rec.type == "transfer_in")
        typeText = "Перевод (зачисление)";
    else if (rec.type == "pin_change")
        typeText = "Смена PIN";
    else if (rec.type == "admin_transfer_out")
        typeText = "Админ. перевод (списание)";
    else if (rec.type == "admin_transfer_in")
        typeText = "Админ. перевод (зачисление)";
    else
        typeText = rec.type;

    printReceipt(typeText, rec.amount, rec.balanceAfter);
}



