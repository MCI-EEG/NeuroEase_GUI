#ifndef DUMMYDATASOURCE_H
#define DUMMYDATASOURCE_H

#include "AbstractDataSource.h"
#include <QTimer>
#include <QVector>
#include <QtMath>
#include <QRandomGenerator>

class DummyDataSource : public AbstractDataSource
{
    Q_OBJECT
public:
    DummyDataSource(QObject *parent = nullptr);
    void start();
    void stop();
    double sampleRate() const override;

private slots:
    void generateData();

private:
    QTimer *timer;
    double time;
    int numChannels = 8;
    QVector<double> channelPhases;
};

#endif // DUMMYDATASOURCE_H

