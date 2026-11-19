#include "MainWindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QStyle>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    _client = new TcpClient(this);
    _fileModel = new FileListModel(
        style()->standardIcon(QStyle::SP_DirIcon),
        style()->standardIcon(QStyle::SP_FileIcon),
        this
    );

    setupUi();

    connect(_client, &TcpClient::connectionStateChanged, this, &MainWindow::updateConnectionState);
    connect(_client, &TcpClient::statusMessage, this, &MainWindow::setStatusText);

    connect(_client, &TcpClient::directoryLoaded, this, [this](QVector<RemoteFileItem> items) {
        _openBtn->setEnabled(_client->isConnected());
        _fileModel->setItems(std::move(items));
    });

    connect(_client, &TcpClient::serverError, this, [this](const QString& errText) {
        _openBtn->setEnabled(_client->isConnected());
        _fileModel->clear();
        QMessageBox::warning(this, "Ошибка сервера", errText);
    });

    connect(_client, &TcpClient::networkError, this, [this](const QString& errText) {
        QMessageBox::critical(this, "Ошибка соединения", errText);
    });

    updateConnectionState(false);
    setStatusText("Готов к подключению");
}

void MainWindow::setupUi()
{
    setWindowTitle("Удаленный проводник (TCP Клиент)");
    resize(740, 500);

    QWidget* central = new QWidget(this);
    setCentralWidget(central);

    QVBoxLayout* mainLayout = new QVBoxLayout(central);

    QHBoxLayout* connLayout = new QHBoxLayout();
    _ipEdit = new QLineEdit("127.0.0.1", this);
    _ipEdit->setPlaceholderText("IP-адрес сервера");

    _portSpin = new QSpinBox(this);
    _portSpin->setRange(1, 65535);
    _portSpin->setValue(8080);

    _connToggleBtn = new QPushButton("Подключиться", this);
    _connToggleBtn->setMinimumWidth(120);

    connLayout->addWidget(new QLabel("Адрес:", this));
    connLayout->addWidget(_ipEdit, 2);
    connLayout->addWidget(new QLabel("Порт:", this));
    connLayout->addWidget(_portSpin, 1);
    connLayout->addWidget(_connToggleBtn);

    QHBoxLayout* pathLayout = new QHBoxLayout();
    _pathEdit = new QLineEdit("C:\\Users", this);
    _pathEdit->setPlaceholderText("Введите путь на сервере");

    _openBtn = new QPushButton("Открыть", this);
    _openBtn->setMinimumWidth(120);

    pathLayout->addWidget(new QLabel("Путь:", this));
    pathLayout->addWidget(_pathEdit, 1);
    pathLayout->addWidget(_openBtn);

    _tableView = new QTableView(this);
    _tableView->setModel(_fileModel);
    _tableView->horizontalHeader()->setSectionResizeMode(FileListModel::ColName, QHeaderView::Stretch);
    _tableView->horizontalHeader()->setSectionResizeMode(FileListModel::ColType, QHeaderView::ResizeToContents);
    _tableView->horizontalHeader()->setSectionResizeMode(FileListModel::ColSize, QHeaderView::ResizeToContents);
    _tableView->verticalHeader()->setVisible(false);
    _tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    _tableView->setSelectionBehavior(QAbstractItemView::SelectRows);

    _statusLabel = new QLabel(this);

    mainLayout->addLayout(connLayout);
    mainLayout->addLayout(pathLayout);
    mainLayout->addWidget(_tableView);
    mainLayout->addWidget(_statusLabel);

    connect(_connToggleBtn, &QPushButton::clicked, this, &MainWindow::onConnectionToggleClicked);
    connect(_openBtn, &QPushButton::clicked, this, &MainWindow::onOpenClicked);
    connect(_pathEdit, &QLineEdit::returnPressed, this, &MainWindow::onOpenClicked);
}

void MainWindow::updateConnectionState(bool connected)
{
    if (!connected)
        _fileModel->clear();

    _ipEdit->setEnabled(!connected);
    _portSpin->setEnabled(!connected);

    _pathEdit->setEnabled(connected);
    _openBtn->setEnabled(connected);

    _connToggleBtn->setEnabled(true);
    _connToggleBtn->setText(connected ? "Отключиться" : "Подключиться");
}

void MainWindow::setStatusText(const QString& text)
{
    bool isConnected = (_client && _client->isConnected());
    QString dotColor = isConnected ? "#16a34a" : "#dc2626";

    _statusLabel->setText(
        QString("<span style='color: %1; font-size: 14px;'>●</span> %2").arg(dotColor, text.toHtmlEscaped())
    );
}

void MainWindow::onConnectionToggleClicked()
{
    if (_client->isConnected())
    {
        _client->disconnectFromServer();
        return;
    }

    QString ip = _ipEdit->text().trimmed();
    if (ip.isEmpty())
    {
        QMessageBox::warning(this, "Ошибка", "Укажите IP-адрес сервера.");
        return;
    }

    _ipEdit->setEnabled(false);
    _portSpin->setEnabled(false);
    _connToggleBtn->setEnabled(false);

    _client->connectToServer(ip, static_cast<quint16>(_portSpin->value()));
}

void MainWindow::onOpenClicked()
{
    if (!_client->isConnected())
        return;

    QString path = _pathEdit->text().trimmed();
    if (path.isEmpty())
    {
        QMessageBox::warning(this, "Ошибка", "Введите путь к директории.");
        return;
    }

    _openBtn->setEnabled(false);
    _client->requestDirectory(path);
}