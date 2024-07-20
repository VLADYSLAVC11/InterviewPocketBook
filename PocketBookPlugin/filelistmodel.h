// Copyright PocketBook - Interview Task

#pragma once

#include <QAbstractListModel>
#include <QDir>
#include <qqmlintegration.h>

class QFileSystemWatcher;
#ifdef Q_OS_LINUX
class QTimer;
#endif

namespace PocketBook::Ui {

class FileListModel
    : public QAbstractListModel
{
Q_OBJECT
    Q_PROPERTY(QString folder READ getFolder WRITE setFolder NOTIFY folderChanged)
    QML_ELEMENT

    enum FileRoles {
        FileNameRole = Qt::UserRole + 1,
        FilePathRole,
        FileSizeRole
    };

public:
    explicit FileListModel(QObject *_parent = nullptr);

    int rowCount(const QModelIndex &_parent) const override;
    QVariant data(const QModelIndex &_index, int _role) const override;

    const QString& getFolder() const;
    void setFolder(const QString &_folderPath);

    QHash<int, QByteArray> roleNames() const override;

signals:
    void folderChanged();

private slots:
    void onDirectoryChanged(const QString &_path);
#ifdef Q_OS_LINUX
    void onTimerTimeout();
#endif

private:
    QFileInfoList m_fileInfoList;
    QString m_folder;
    QFileSystemWatcher* m_watcher;
#ifdef Q_OS_LINUX
    QTimer* m_timer;
#endif

};
} // namespacace PocketBook::Ui
