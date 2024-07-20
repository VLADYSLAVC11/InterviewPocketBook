// Copyright PocketBook - Interview Task

#pragma once

#include <QObject>
#include <qqmlintegration.h>

#include "../BmpLib/bmputils.h"

#include <iostream>

namespace PocketBook::Ui {

class ProgressModel
	: public QObject, public IProgressNotifier
{
    Q_OBJECT
    Q_PROPERTY(int min READ min NOTIFY minValueChanged);
    Q_PROPERTY(int max READ max NOTIFY maxValueChanged);
	Q_PROPERTY(QString text READ getText WRITE setText NOTIFY textChanged)
    QML_ELEMENT

public:
    explicit ProgressModel(QObject* _parent = nullptr);

    void init(int _min, int _max) override;
    void notifyProgress(int _current) override;

    int min() const;
    int max() const;

	const QString & getText() const;
	void setText(const QString & _text);

signals:
    void progressChanged(int);
    void minValueChanged(int);
    void maxValueChanged(int);
	void textChanged(QString const &);

private:
    // Default values equal to the percents from 0 to 100.
    int m_minValue = 0;
    int m_maxValue = 100;
	QString m_text = QString();
};

} // namespace PocketBook::Ui
