#ifndef FILEDATASOURCE_H
#define FILEDATASOURCE_H

#include "AbstractDataSource.h"
#include <QElapsedTimer>
#include <QString>
#include <QTimer>
#include <QVector>

class FileDataSource : public AbstractDataSource {
  Q_OBJECT
public:
  explicit FileDataSource(const QString &filePath, QObject *parent = nullptr);

  void start() override;
  void stop() override;
  void reportInitStatus(); // New method to re-emit the load message
  double sampleRate() const override { return m_sampleRate; }

  void setSampleRate(double sr);

private slots:
  void generateFromFile();

private:
  void loadFile();

  QTimer *timer = nullptr;
  QElapsedTimer m_elapsedTimer;
  qint64 m_samplesEmitted = 0;

  QString m_filePath;
  QString m_initStatus; // Store the message here
  QVector<QVector<double>> m_samples;
  int m_index = 0;
  int m_numChannels = 8;
  double m_sampleRate = 250.0;
  bool m_isNeuroEaseFormat = false;
};

#endif // FILEDATASOURCE_H
