#pragma once

#include <QAbstractTableModel>
#include <QVector>
#include <QIcon>
#include <QString>
#include <cstdint>

struct RemoteFileItem
{
    bool     isDirectory = false;
    uint64_t size        = 0;
    QString  name;
};

class FileListModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    enum Column
    {
        ColName = 0,
        ColType = 1,
        ColSize = 2,
        ColCount
    };

    explicit FileListModel(const QIcon& folderIcon, const QIcon& fileIcon, QObject* parent = nullptr);

    void setItems(QVector<RemoteFileItem> items);
    void clear();

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

private:
    static QString formatSize(uint64_t bytes);

private:
    QVector<RemoteFileItem> _items;
    QIcon _folderIcon;
    QIcon _fileIcon;
};