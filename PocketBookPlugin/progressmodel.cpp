// Copyright PocketBook - Interview Task

#include "progressmodel.h"

namespace PocketBook::Ui {

ProgressModel::ProgressModel(QObject* _parent)
    : QObject(_parent)
{
}

void ProgressModel::init(int _min, int _max)
{
    m_minValue = _min;
    m_maxValue = _max;
}

 void ProgressModel::notifyProgress(int _current)
{
    const int percent = _current * 100 / m_maxValue;
    emit progressChanged(percent);
}

int ProgressModel::min() const
{
	return m_minValue;
}

int ProgressModel::max() const
{
	return m_maxValue;
}

const QString & ProgressModel::getText() const
{
	return m_text;
}

void ProgressModel::setText(const QString & _text)
{
	if( m_text != _text )
	{
		m_text = _text;
		emit textChanged(m_text);
	}
}

} // namespacace PocketBook::Ui

