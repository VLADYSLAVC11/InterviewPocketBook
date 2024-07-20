// Copyright PocketBook - Interview Task

#include "filelistmodel.h"

#include <QFileSystemWatcher>

#ifdef Q_OS_LINUX
#include <QTimer>
#endif

namespace PocketBook::Ui {

FileListModel::FileListModel(QObject *_parent)
    : QAbstractListModel(_parent)
    , m_watcher(new QFileSystemWatcher(this))
#ifdef Q_OS_LINUX
    , m_timer(new QTimer(this))
#endif
{
    connect(m_watcher, &QFileSystemWatcher::directoryChanged, this, &FileListModel::onDirectoryChanged);
#ifdef Q_OS_LINUX
    connect(m_timer, &QTimer::timeout, this, &FileListModel::onTimerTimeout);
    m_timer->start(2000);
#endif
}

int FileListModel::rowCount(const QModelIndex &_parent) const
{
    if (_parent.isValid())
        return 0;

    return static_cast<int>(m_fileInfoList.count());
}

QVariant FileListModel::data(const QModelIndex &_index, int _role) const
{
    if (!_index.isValid() || _index.row() >= m_fileInfoList.count())
        return {};

    const auto toKbts = [](const auto size) -> qint64
    {
        return qRound(static_cast<double>(size) / 1000);
    };

    const QFileInfo fileInfo = m_fileInfoList.at(_index.row());
    switch (_role)
    {
    case Qt::DisplayRole:
    case FileNameRole:
        return fileInfo.fileName();
    case FilePathRole:
        return fileInfo.filePath();
    case FileSizeRole:
        return toKbts(fileInfo.size());
    }

    return {};
}

const QString& FileListModel::getFolder() const
{
    return m_folder;
}

void FileListModel::setFolder(const QString &_folderPath)
{
    const QFile file {_folderPath};
    auto checkedFolderPath = _folderPath;

    if (!file.exists())
        checkedFolderPath = QDir::currentPath();

    const QDir dir {checkedFolderPath};

    if (!m_folder.isEmpty())
        m_watcher->removePath(m_folder);

    beginResetModel();

    m_fileInfoList.clear();
    const QStringList filters {"*.png", "*.bmp", "*.barch"};
    QFileInfoList files = dir.entryInfoList(filters);
    for (const auto &fileInfo : files) {
        m_fileInfoList.append(fileInfo);
    }

    endResetModel();

    m_folder = checkedFolderPath;
    m_watcher->addPath(m_folder);

    emit folderChanged();
}

void FileListModel::onDirectoryChanged(const QString &_path)
{
    if (_path == m_folder)
        setFolder(_path);
}

#ifdef Q_OS_LINUX
void FileListModel::onTimerTimeout()
{
    if (!m_folder.isEmpty())
        onDirectoryChanged(m_folder);
}
#endif

QHash<int, QByteArray> FileListModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[FileNameRole] = "fileName";
    roles[FilePathRole] = "filePath";
    roles[FileSizeRole] = "fileSize";
    return roles;
}

} // namespacace PocketBook::Ui
