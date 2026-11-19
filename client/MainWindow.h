#pragma once

#include <QMainWindow>
#include <QLineEdit>
#include <QSpinBox>
#include <QPushButton>
#include <QTableView>
#include <QLabel>

#include "TcpClient.h"
#include "FileListModel.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override = default;

private slots:
    void onConnectionToggleClicked();
    void onOpenClicked();
    void updateConnectionState(bool connected);
    void setStatusText(const QString& text);

private:
    void setupUi();

private:
    TcpClient*     _client    = nullptr;
    FileListModel* _fileModel = nullptr;

    QLineEdit*   _ipEdit        = nullptr;
    QSpinBox*    _portSpin      = nullptr;
    QPushButton* _connToggleBtn = nullptr;

    QLineEdit*   _pathEdit = nullptr;
    QPushButton* _openBtn  = nullptr;

    QTableView*  _tableView   = nullptr;
    QLabel*      _statusLabel = nullptr;
};