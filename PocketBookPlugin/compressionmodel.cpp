// Copyright PocketBook - Interview Task

#include <QFileInfo>
#include <QDir>

#include "compressionmodel.h"
#include "../BmpLib/bmpproxy.h"
#include "../BmpLib/bmpexceptions.h"

#include <thread>

namespace PocketBook::Ui {

CompressionModel::CompressionModel(QObject* _parent)
    : QObject(_parent)
{
}

void CompressionModel::compress(const QString& _filePath)
{
    const QString extension = ".barch";
    const auto outFilePath = changeFileExtension(_filePath, extension);
    if(!removeFileIfExists(outFilePath))
    {
        //TODO [cvs]: handle the situation if the file with the output name elready exists and can't be removed;
        return;
    }

	auto progressModel = m_progressModel;
    assert(progressModel);

    std::thread worker {[_filePath, outFilePath, progressModel, this]{
        bool compressed = false;
        QString errorMsg;

        try
        {
			progressModel->setText("Compressing");
            auto bmpImage = BmpProxy::createFromBmp(_filePath.toStdString());
            compressed = bmpImage.compress(outFilePath.toStdString(), progressModel);
        }
        catch( const FileError & _err )
        {
            errorMsg = QString(_err.what());
        }
        catch( ... )
        {
            errorMsg = QString("Unexpected Error");
        }

        if(!compressed)
        {
            emit errorOccured(errorMsg);
        }
    }};
    worker.detach();
}

void CompressionModel::decompress(const QString& _filePath)
{
    const QString extension = ".bmp";
    auto outFilePath = getUniqueFilePath(_filePath, extension);

	auto progressModel = m_progressModel;
    assert(m_progressModel);

    std::thread worker {[_filePath, outFilePath, progressModel, this] {
		bool decompressed = false;
		QString errorMsg;

		try
		{
			progressModel->setText("Decompressing");
        	auto barchImage = BmpProxy::createFromBarch(_filePath.toStdString());
			decompressed = barchImage.decompress(outFilePath.toStdString(), progressModel);
		}
		catch( const FileError & _err )
		{
			errorMsg = QString(_err.what());
		}
		catch( ... )
		{
			errorMsg = QString("Unexpected Error");
		}

		if(!decompressed)
		{
			emit errorOccured(errorMsg);
		}
    }};
    worker.detach();
}

ProgressModel* CompressionModel::getProgressModel()
{
    return m_progressModel;
}

void CompressionModel::setProgressModel(ProgressModel* _model)
{
    m_progressModel = _model;
}

QString CompressionModel::changeFileExtension(const QString& _filePath, const QString& _newExtension) const
{
    const QFileInfo fileInfo(_filePath);
    const QString baseName = fileInfo.completeBaseName();
    const QDir dir = fileInfo.dir();
    const QString newFilePath = dir.absoluteFilePath(baseName + _newExtension);

    return newFilePath;
}

bool CompressionModel::removeFileIfExists(const QString& _filePath) const
{
    const QFileInfo fileInfo(_filePath);

    if (fileInfo.exists() && fileInfo.isFile())
    {
        QFile file(_filePath);
        return file.remove();
    }

    return true;
}

QString CompressionModel::getUniqueFilePath(const QString &_filePath, const QString& _newExtension) const {
    const QFileInfo fileInfo(_filePath);
    const QString baseName = fileInfo.completeBaseName();
    const QString dir = fileInfo.absolutePath();

    const QString suffix = "_unpacked";

    auto newFilePath = QString("%1/%2%3%4").arg(dir, baseName, suffix, _newExtension);

    return newFilePath;
}

} // namespace PocketBook::Ui
