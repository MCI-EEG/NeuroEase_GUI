#include "DummyDataSource.h"

DummyDataSource::DummyDataSource(QObject *parent)
    : AbstractDataSource(parent), time(0)
{
    timer = new QTimer(this);
    timer->setInterval(20);
    connect(timer, &QTimer::timeout, this, &DummyDataSource::generateData);

    channelPhases.resize(numChannels);
    for (int i = 0; i < numChannels; ++i) {
        channelPhases[i] = QRandomGenerator::global()->generateDouble() * 2 * M_PI;
    }
}

void DummyDataSource::start()
{
    time = 0.0;
    timer->start();
}

void DummyDataSource::stop()
{
    timer->stop();
}

void DummyDataSource::generateData()
{
    QVector<double> values;
    double dt = 0.02; // 50 Hz

    for (int i = 0; i < numChannels; ++i)
    {
        double freq  = 4.0 + i * 1.5;
        double noise = (QRandomGenerator::global()->generateDouble() - 0.5) * 0.1;
        double value = qSin(2 * M_PI * freq * time + channelPhases[i]) + noise;
        values.append(value);
    }

    emit newEEGData(values);
    time += dt;
}

double DummyDataSource::sampleRate() const
{
    // 1 / 0.02 s
    return 50.0;
}
