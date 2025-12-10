#include "admindialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QHeaderView>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QCryptographicHash>

namespace {
const QString ADMIN_CARD = "0000000000000000";
}

QString AdminDialog::hashPin(const QString &pin)
{
    QByteArray data = pin.toUtf8();
    QByteArray hash = QCryptographicHash::hash(data, QCryptographicHash::Sha256);
    return QString::fromLatin1(hash.toHex());
}

AdminDialog::AdminDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Администрирование");
    resize(750, 500);

    auto *layout = new QVBoxLayout(this);


    auto *formLayout = new QHBoxLayout();

    m_cardEdit = new QLineEdit(this);
    m_pinEdit = new QLineEdit(this);
    m_balanceEdit = new QLineEdit(this);

    m_cardEdit->setMaxLength(16);
    m_cardEdit->setValidator(new QRegularExpressionValidator(
        QRegularExpression("^[0-9]{0,16}$"), this));

    m_pinEdit->setMaxLength(4);
    m_pinEdit->setValidator(new QRegularExpressionValidator(
        QRegularExpression("^[0-9]{0,4}$"), this));

    m_balanceEdit->setValidator(new QRegularExpressionValidator(
        QRegularExpression("^[0-9]+(\\.[0-9]{1,2})?$"), this));

    formLayout->addWidget(new QLabel("Карта:"));
    formLayout->addWidget(m_cardEdit);

    formLayout->addWidget(new QLabel("PIN:"));
    formLayout->addWidget(m_pinEdit);

    formLayout->addWidget(new QLabel("Баланс:"));
    formLayout->addWidget(m_balanceEdit);

    layout->addLayout(formLayout);

    auto *btnLayout = new QHBoxLayout();

    m_addButton = new QPushButton("Добавить", this);
    m_deleteButton = new QPushButton("Удалить", this);
    m_updateBalanceButton = new QPushButton("Изменить баланс", this);
    m_resetPinButton = new QPushButton("Сброс PIN (0000)", this);

    btnLayout->addWidget(m_addButton);
    btnLayout->addWidget(m_deleteButton);
    btnLayout->addWidget(m_updateBalanceButton);
    btnLayout->addWidget(m_resetPinButton);

    layout->addLayout(btnLayout);

    auto *transferLayout = new QHBoxLayout();

    m_fromCardEdit = new QLineEdit(this);
    m_toCardEdit   = new QLineEdit(this);
    m_transferAmountEdit = new QLineEdit(this);

    m_fromCardEdit->setPlaceholderText("Карта-отправитель");
    m_toCardEdit->setPlaceholderText("Карта-получатель");
    m_transferAmountEdit->setPlaceholderText("Сумма");

    m_fromCardEdit->setMaxLength(16);
    m_fromCardEdit->setValidator(new QRegularExpressionValidator(
        QRegularExpression("^[0-9]{0,16}$"), this));

    m_toCardEdit->setMaxLength(16);
    m_toCardEdit->setValidator(new QRegularExpressionValidator(
        QRegularExpression("^[0-9]{0,16}$"), this));

    m_transferAmountEdit->setValidator(new QRegularExpressionValidator(
        QRegularExpression("^[0-9]+(\\.[0-9]{1,2})?$"), this));

    m_transferButton = new QPushButton("Перевести", this);

    transferLayout->addWidget(new QLabel("Перевод:"));
    transferLayout->addWidget(m_fromCardEdit);
    transferLayout->addWidget(m_toCardEdit);
    transferLayout->addWidget(m_transferAmountEdit);
    transferLayout->addWidget(m_transferButton);

    layout->addLayout(transferLayout);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(3);
    m_table->setHorizontalHeaderLabels({"Карта", "PIN (скрыт)", "Баланс"});
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    layout->addWidget(m_table);

    connect(m_addButton, &QPushButton::clicked, this, &AdminDialog::onAddAccount);
    connect(m_deleteButton, &QPushButton::clicked, this, &AdminDialog::onDeleteAccount);
    connect(m_updateBalanceButton, &QPushButton::clicked, this, &AdminDialog::onUpdateBalance);
    connect(m_resetPinButton, &QPushButton::clicked, this, &AdminDialog::onResetPin);
    connect(m_transferButton, &QPushButton::clicked, this, &AdminDialog::onTransfer);

    refreshTable();
}

double AdminDialog::getBalance(const QString &card)
{
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen())
        return 0.0;

    QSqlQuery q;
    q.prepare("SELECT balance FROM accounts WHERE card_number = :card");
    q.bindValue(":card", card);
    if (!q.exec())
        return 0.0;
    if (!q.next())
        return 0.0;

    return q.value(0).toDouble();
}

bool AdminDialog::updateBalance(const QString &card, double newBal)
{
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen())
        return false;

    QSqlQuery q;
    q.prepare("UPDATE accounts SET balance = :bal WHERE card_number = :card");
    q.bindValue(":bal", newBal);
    q.bindValue(":card", card);
    return q.exec();
}

bool AdminDialog::recordTransaction(const QString &card,
                                    const QString &type,
                                    double amount,
                                    double balanceAfter)
{
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen())
        return false;

    QSqlQuery q;
    q.prepare("INSERT INTO transactions "
              "(card_number, type, amount, balance_after) "
              "VALUES (:card, :type, :amount, :bal)");
    q.bindValue(":card", card);
    q.bindValue(":type", type);
    q.bindValue(":amount", amount);
    q.bindValue(":bal", balanceAfter);
    return q.exec();
}

void AdminDialog::refreshTable()
{
    m_table->setRowCount(0);

    QSqlQuery q("SELECT card_number, pin, balance FROM accounts");
    while (q.next())
    {
        int row = m_table->rowCount();
        m_table->insertRow(row);

        QString card = q.value(0).toString();
        QString bal  = q.value(2).toString();

        m_table->setItem(row, 0, new QTableWidgetItem(card));
        m_table->setItem(row, 1, new QTableWidgetItem("****"));
        m_table->setItem(row, 2, new QTableWidgetItem(bal));
    }
}

void AdminDialog::onAddAccount()
{
    QString card = m_cardEdit->text();
    QString pin  = m_pinEdit->text();
    QString bal  = m_balanceEdit->text();

    card.remove(' ');
    pin.remove(' ');

    if (card.length() != 16) {
        QMessageBox::warning(this, "Ошибка", "Карта должна содержать 16 цифр.");
        return;
    }
    if (card == ADMIN_CARD) {
        QMessageBox::warning(this, "Ошибка", "Этот номер карты зарезервирован для администратора.");
        return;
    }
    if (pin.length() != 4) {
        QMessageBox::warning(this, "Ошибка", "PIN должен содержать 4 цифры.");
        return;
    }
    if (bal.isEmpty()) {
        QMessageBox::warning(this, "Ошибка", "Введите баланс.");
        return;
    }

    QString pinHash = hashPin(pin);

    QSqlQuery q;
    q.prepare("INSERT INTO accounts (card_number, pin, balance) VALUES (?, ?, ?)");
    q.addBindValue(card);
    q.addBindValue(pinHash);
    q.addBindValue(bal.toDouble());

    if (!q.exec()) {
        QMessageBox::warning(this, "Ошибка", q.lastError().text());
        return;
    }

    refreshTable();
}

void AdminDialog::onDeleteAccount()
{
    QString card = m_cardEdit->text();
    card.remove(' ');

    if (card.length() != 16) {
        QMessageBox::warning(this, "Ошибка", "Введите корректный номер карты.");
        return;
    }
    if (card == ADMIN_CARD) {
        QMessageBox::warning(this, "Ошибка", "Нельзя удалить админскую карту.");
        return;
    }

    auto reply = QMessageBox::question(
        this,
        "Подтверждение",
        "Удалить аккаунт с картой: " + card + " ?",
        QMessageBox::Yes | QMessageBox::No
        );
    if (reply != QMessageBox::Yes)
        return;

    QSqlQuery q;
    q.prepare("DELETE FROM accounts WHERE card_number = ?");
    q.addBindValue(card);
    q.exec();

    QSqlQuery q2;
    q2.prepare("DELETE FROM transactions WHERE card_number = ?");
    q2.addBindValue(card);
    q2.exec();

    refreshTable();
}

void AdminDialog::onUpdateBalance()
{
    QString card = m_cardEdit->text();
    QString bal  = m_balanceEdit->text();

    card.remove(' ');

    if (card.length() != 16) {
        QMessageBox::warning(this, "Ошибка", "Карта должна быть 16 цифр.");
        return;
    }
    if (card == ADMIN_CARD) {
        QMessageBox::warning(this, "Ошибка", "Нельзя менять баланс админской карты.");
        return;
    }
    if (bal.isEmpty()) {
        QMessageBox::warning(this, "Ошибка", "Введите баланс.");
        return;
    }

    double newBal = bal.toDouble();
    double oldBal = getBalance(card);

    auto reply = QMessageBox::question(
        this,
        "Подтверждение",
        QString("Изменить баланс карты %1 с %2 на %3?")
            .arg(card)
            .arg(oldBal)
            .arg(newBal),
        QMessageBox::Yes | QMessageBox::No
        );
    if (reply != QMessageBox::Yes)
        return;

    if (!updateBalance(card, newBal)) {
        QMessageBox::warning(this, "Ошибка", "Не удалось изменить баланс.");
        return;
    }

    refreshTable();
}

void AdminDialog::onResetPin()
{
    QString card = m_cardEdit->text();
    card.remove(' ');

    if (card.length() != 16) {
        QMessageBox::warning(this, "Ошибка", "Карта должна содержать 16 цифр.");
        return;
    }
    if (card == ADMIN_CARD) {
        QMessageBox::warning(this, "Ошибка", "Нельзя менять PIN админской карты.");
        return;
    }

    auto reply = QMessageBox::question(
        this,
        "Подтверждение",
        "Сбросить PIN карты " + card + " на 0000?",
        QMessageBox::Yes | QMessageBox::No
        );
    if (reply != QMessageBox::Yes)
        return;

    QString newPinHash = hashPin("0000");

    QSqlQuery q;
    q.prepare("UPDATE accounts SET pin = :pin, failed_attempts = 0, locked_until = NULL "
              "WHERE card_number = :card");
    q.bindValue(":pin", newPinHash);
    q.bindValue(":card", card);
    if (!q.exec()) {
        QMessageBox::warning(this, "Ошибка", "Не удалось обновить PIN: " + q.lastError().text());
        return;
    }

    refreshTable();
}

void AdminDialog::onTransfer()
{
    QString fromCard = m_fromCardEdit->text();
    QString toCard   = m_toCardEdit->text();
    QString amountStr = m_transferAmountEdit->text();

    fromCard.remove(' ');
    toCard.remove(' ');

    if (fromCard.length() != 16 || toCard.length() != 16) {
        QMessageBox::warning(this, "Ошибка", "Обе карты должны содержать 16 цифр.");
        return;
    }
    if (fromCard == toCard) {
        QMessageBox::warning(this, "Ошибка", "Карты не должны совпадать.");
        return;
    }
    if (fromCard == ADMIN_CARD || toCard == ADMIN_CARD) {
        QMessageBox::warning(this, "Ошибка", "Операции с админской картой запрещены.");
        return;
    }

    if (amountStr.isEmpty()) {
        QMessageBox::warning(this, "Ошибка", "Введите сумму.");
        return;
    }

    double amount = amountStr.toDouble();
    if (amount <= 0) {
        QMessageBox::warning(this, "Ошибка", "Сумма должна быть положительной.");
        return;
    }

    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        QMessageBox::warning(this, "Ошибка", "База данных не открыта.");
        return;
    }

    auto reply = QMessageBox::question(
        this,
        "Подтверждение",
        QString("Перевести %1 с карты %2 на карту %3?")
            .arg(amount)
            .arg(fromCard)
            .arg(toCard),
        QMessageBox::Yes | QMessageBox::No
        );
    if (reply != QMessageBox::Yes)
        return;

    if (!db.transaction()) {
        QMessageBox::warning(this, "Ошибка", "Не удалось начать транзакцию.");
        return;
    }

    double fromBal = getBalance(fromCard);
    double toBal   = getBalance(toCard);

    if (fromBal < amount) {
        db.rollback();
        QMessageBox::warning(this, "Ошибка", "Недостаточно средств на карте отправителя.");
        return;
    }

    double newFromBal = fromBal - amount;
    double newToBal   = toBal + amount;

    if (!updateBalance(fromCard, newFromBal) ||
        !updateBalance(toCard,   newToBal)   ||
        !recordTransaction(fromCard, "admin_transfer_out", amount, newFromBal) ||
        !recordTransaction(toCard,   "admin_transfer_in",  amount, newToBal))
    {
        db.rollback();
        QMessageBox::warning(this, "Ошибка", "Не удалось выполнить перевод.");
        return;
    }

    if (!db.commit()) {
        QMessageBox::warning(this, "Ошибка", "Не удалось зафиксировать перевод.");
        return;
    }

    QMessageBox::information(this, "Готово", "Перевод выполнен.");
    refreshTable();
}

