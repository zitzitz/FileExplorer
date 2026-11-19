#pragma once

#pragma once

#include <QObject>
#include <QTcpSocket>
#include <QByteArray>
#include "FileListModel.h"
#include "Protocol.h"

class TcpClient : public QObject
{
    Q_OBJECT
public:
    explicit TcpClient(QObject* parent = nullptr);

    void connectToServer(const QString& host, quint16 port);
    void disconnectFromServer();
    bool isConnected() const;

    void requestDirectory(const QString& path);

signals:
    void connectionStateChanged(bool connected);
    void statusMessage(const QString& text);
    void networkError(const QString& errorText);
    void serverError(const QString& errorText);
    void directoryLoaded(QVector<RemoteFileItem> items);

private slots:
    void onSocketConnected();
    void onSocketDisconnected();
    void onSocketError(QAbstractSocket::SocketError socketError);
    void onSocketReadyRead();

private:
    void processIncomingBuffer();
    static QString statusToString(StatusCode status);

private:
    QTcpSocket* _socket = nullptr;
    QByteArray  _rxBuffer;
};