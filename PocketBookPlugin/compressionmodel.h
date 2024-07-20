// Copyright PocketBook - Interview Tasks

#pragma once

#include <QObject>
#include <qqmlintegration.h>

#include "progressmodel.h"

namespace PocketBook::Ui {

class CompressionModel : public QObject
{
Q_OBJECT
    Q_PROPERTY(ProgressModel* progressModel READ getProgressModel WRITE setProgressModel)
    QML_ELEMENT

public:
    explicit CompressionModel(QObject* _parent = nullptr);

    Q_INVOKABLE void compress(const QString& _filePath);
    Q_INVOKABLE void decompress(const QString& _filePath);

signals:
    void errorOccured(const QString& _text);

private:
    void setProgressModel(ProgressModel* _model);
    ProgressModel* getProgressModel();

    QString changeFileExtension(const QString& _filePath, const QString& _newExtension) const;
    bool removeFileIfExists(const QString& _filePath) const;
    QString getUniqueFilePath(const QString &_filePath, const QString& _newExtension) const;

private:
    ProgressModel* m_progressModel = nullptr;
};

} // namespace PocketBook::Ui
