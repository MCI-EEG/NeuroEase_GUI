#ifndef DUMMYDATASOURCE_H
#define DUMMYDATASOURCE_H

#include "AbstractDataSource.h"
#include <QElapsedTimer>
#include <QRandomGenerator>
#include <QTimer>
#include <QVector>
#include <QtMath>


class DummyDataSource : public AbstractDataSource {
  Q_OBJECT
public:
  DummyDataSource(QObject *parent = nullptr);
  void start();
  void stop() override;
  double sampleRate() const override;

private slots:
  void generateData();

private:
  QTimer *timer;
  QElapsedTimer m_elapsedTimer;
  qint64 m_samplesGenerated = 0;

  double time;
  int numChannels = 8;
  QVector<double> channelPhases;
};

#endif // DUMMYDATASOURCE_H
