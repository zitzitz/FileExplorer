#include "TcpClient.h"
#include <QHostAddress>
#include <algorithm>
#include <cstring>

TcpClient::TcpClient(QObject* parent)
    : QObject(parent)
{
    _socket = new QTcpSocket(this);

    connect(_socket, &QTcpSocket::connected, this, &TcpClient::onSocketConnected);
    connect(_socket, &QTcpSocket::disconnected, this, &TcpClient::onSocketDisconnected);
    connect(_socket, &QTcpSocket::errorOccurred, this, &TcpClient::onSocketError);
    connect(_socket, &QTcpSocket::readyRead, this, &TcpClient::onSocketReadyRead);
}

void TcpClient::connectToServer(const QString& host, quint16 port)
{
    if (_socket->state() != QAbstractSocket::UnconnectedState)
        _socket->abort();

    _rxBuffer.clear();
    emit statusMessage("Подключение к " + host + "...");
    _socket->connectToHost(host, port);
}

void TcpClient::disconnectFromServer()
{
    if (_socket->state() == QAbstractSocket::UnconnectedState)
        return;

    _socket->disconnectFromHost();
    if (_socket->state() != QAbstractSocket::UnconnectedState)
        _socket->abort();
}

bool TcpClient::isConnected() const
{
    return _socket->state() == QAbstractSocket::ConnectedState;
}

void TcpClient::requestDirectory(const QString& path)
{
    if (!isConnected())
        return;

    QByteArray utf8Path = path.toUtf8();

    PacketHeader header;
    header.magic       = PROTOCOL_MAGIC;
    header.commandId   = CommandId::GetDirList;
    header.status      = StatusCode::Ok;
    header.payloadSize = static_cast<uint32_t>(utf8Path.size());

    _socket->write(reinterpret_cast<const char*>(&header), sizeof(header));
    _socket->write(utf8Path);
    _socket->flush();

    emit statusMessage("Запрос отправлен...");
}

void TcpClient::onSocketConnected()
{
    emit connectionStateChanged(true);
    emit statusMessage("Подключено к " + _socket->peerAddress().toString() + ":" + QString::number(_socket->peerPort()));
}

void TcpClient::onSocketDisconnected()
{
    _rxBuffer.clear();
    emit connectionStateChanged(false);
    emit statusMessage("Отключено от сервера");
}

void TcpClient::onSocketError(QAbstractSocket::SocketError)
{
    QString errMsg = _socket->errorString();

    if (_socket->state() != QAbstractSocket::UnconnectedState)
        _socket->abort();

    emit connectionStateChanged(false);
    emit statusMessage("Ошибка сети: " + errMsg);
    emit networkError(errMsg);
}

void TcpClient::onSocketReadyRead()
{
    _rxBuffer.append(_socket->readAll());
    processIncomingBuffer();
}

void TcpClient::processIncomingBuffer()
{
    while (_rxBuffer.size() >= static_cast<qsizetype>(sizeof(PacketHeader)))
    {
        PacketHeader header;
        std::memcpy(&header, _rxBuffer.constData(), sizeof(PacketHeader));

        if (header.magic != PROTOCOL_MAGIC)
        {
            emit statusMessage("Ошибка протокола: неверная сигнатура.");
            disconnectFromServer();
            return;
        }

        qsizetype totalPacketSize = static_cast<qsizetype>(sizeof(PacketHeader)) + static_cast<qsizetype>(header.payloadSize);
        if (_rxBuffer.size() < totalPacketSize)
            return;

        QByteArray payload = _rxBuffer.mid(sizeof(PacketHeader), header.payloadSize);
        _rxBuffer.remove(0, totalPacketSize);

        if (header.commandId == CommandId::GetDirList)
        {
            if (header.status != StatusCode::Ok)
            {
                QString errText = statusToString(header.status);
                emit statusMessage("Ошибка: " + errText);
                emit serverError(errText);
                continue;
            }

            if (payload.size() < static_cast<qsizetype>(sizeof(DirListResponseHeader)))
                continue;

            const char* ptr = payload.constData();
            const char* end = ptr + payload.size();

            DirListResponseHeader listHdr;
            std::memcpy(&listHdr, ptr, sizeof(DirListResponseHeader));
            ptr += sizeof(DirListResponseHeader);

            QVector<RemoteFileItem> items;
            items.reserve(static_cast<int>(listHdr.itemsCount));

            for (uint32_t i = 0; i < listHdr.itemsCount; ++i)
            {
                if (ptr + sizeof(FileItemHeader) > end)
                    break;

                FileItemHeader itemHdr;
                std::memcpy(&itemHdr, ptr, sizeof(FileItemHeader));
                ptr += sizeof(FileItemHeader);

                if (ptr + itemHdr.nameLength > end)
                    break;

                RemoteFileItem item;
                item.isDirectory = (itemHdr.isDirectory != 0);
                item.size        = itemHdr.size;
                item.name        = QString::fromUtf8(ptr, itemHdr.nameLength);
                ptr += itemHdr.nameLength;

                items.append(item);
            }

            std::sort(items.begin(), items.end(), [](const RemoteFileItem& a, const RemoteFileItem& b) {
                if (a.isDirectory != b.isDirectory)
                    return a.isDirectory > b.isDirectory;
                return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
            });

            int count = items.size();
            emit directoryLoaded(std::move(items));
            emit statusMessage(QString("Найдено элементов: %1").arg(count));
        }
    }
}

QString TcpClient::statusToString(StatusCode status)
{
    switch (status)
    {
        case StatusCode::PathNotFound:   return "Указанный путь не найден на сервере.";
        case StatusCode::AccessDenied:   return "Отказано в доступе к директории.";
        case StatusCode::InvalidPath:    return "Некорректный путь (возможно, указан файл вместо папки).";
        case StatusCode::UnknownCommand: return "Сервер не поддерживает данную команду.";
        case StatusCode::InternalError:  return "Внутренняя ошибка сервера.";
        default:                         return QString("Неизвестная ошибка (код %1).").arg(static_cast<uint16_t>(status));
    }
}