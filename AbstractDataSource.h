#ifndef ABSTRACTDATASOURCE_H
#define ABSTRACTDATASOURCE_H

#include <QObject>
#include <QVector>

class AbstractDataSource : public QObject {
  Q_OBJECT
public:
  using QObject::QObject;
  ~AbstractDataSource() override = default;

  virtual void start() = 0;
  virtual void stop() = 0;
  virtual double sampleRate() const = 0;
  virtual void setGain(int gain) { Q_UNUSED(gain); }
  virtual void setSampleRate(int sps) { Q_UNUSED(sps); }
  virtual void sendCommand(const QString &cmd) { Q_UNUSED(cmd); }
  virtual bool isConnected() const { return false; }

signals:
  void newEEGData(const QVector<double> &values);
  void statusMessage(const QString &msg);
  void impedanceReceived(const QStringList &values);
};

#endif // ABSTRACTDATASOURCE_H
