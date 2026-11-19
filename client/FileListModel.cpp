#include "FileListModel.h"

FileListModel::FileListModel(const QIcon& folderIcon, const QIcon& fileIcon, QObject* parent)
    : QAbstractTableModel(parent)
    , _folderIcon(folderIcon)
    , _fileIcon(fileIcon)
{
}

void FileListModel::setItems(QVector<RemoteFileItem> items)
{
    beginResetModel();
    _items = std::move(items);
    endResetModel();
}

void FileListModel::clear()
{
    if (_items.isEmpty())
        return;

    beginResetModel();
    _items.clear();
    endResetModel();
}

int FileListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : _items.size();
}

int FileListModel::columnCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : ColCount;
}

QVariant FileListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= _items.size())
        return {};

    const RemoteFileItem& item = _items[index.row()];

    if (role == Qt::DisplayRole)
    {
        switch (index.column())
        {
            case ColName: return item.name;
            case ColType: return item.isDirectory ? "Папка" : "Файл";
            case ColSize: return formatSize(item.size);
        }
    }
    else if (role == Qt::DecorationRole && index.column() == ColName)
    {
        return item.isDirectory ? _folderIcon : _fileIcon;
    }
    else if (role == Qt::TextAlignmentRole && index.column() == ColSize)
    {
        return static_cast<int>(Qt::AlignRight | Qt::AlignVCenter);
    }

    return {};
}

QVariant FileListModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return {};

    switch (section)
    {
        case ColName: return "Имя";
        case ColType: return "Тип";
        case ColSize: return "Размер";
    }
    return {};
}

QString FileListModel::formatSize(uint64_t bytes)
{
    constexpr double KB = 1024.0;
    constexpr double MB = 1024.0 * 1024.0;
    constexpr double GB = 1024.0 * 1024.0 * 1024.0;

    if (bytes >= static_cast<uint64_t>(GB))
        return QString("%1 ГБ").arg(bytes / GB, 0, 'f', 2);
    if (bytes >= static_cast<uint64_t>(MB))
        return QString("%1 МБ").arg(bytes / MB, 0, 'f', 2);
    if (bytes >= static_cast<uint64_t>(KB))
        return QString("%1 КБ").arg(bytes / KB, 0, 'f', 1);
    return QString("%1 Б").arg(bytes);
}