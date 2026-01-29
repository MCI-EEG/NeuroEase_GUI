#ifndef REALDATASOURCE_H
#define REALDATASOURCE_H

#include "AbstractDataSource.h"

#include <QTimer>
#include <QUdpSocket>


class RealDataSource : public AbstractDataSource {
  Q_OBJECT
public:
  explicit RealDataSource(QObject *parent = nullptr);
  ~RealDataSource() override;

  void start() override;
  void stop() override;
  double sampleRate() const override { return 250.0; }
  void sendCommand(const QString &cmd) override;
  bool isConnected() const override {
    return udpSocket != nullptr &&
           udpSocket->state() == QAbstractSocket::BoundState;
  }

  void setUdpPort(quint16 port);

signals:
  // Wird z.B. bei Bind-Fehler oder wenn keine Daten kommen ausgelöst
  void udpError(const QString &message);

private slots:
  void readUdp();

private:
  void initSocket();
  void closeSocket();

  QUdpSocket *udpSocket = nullptr;
  quint16 m_port = 12345;

  QTimer *watchdogTimer = nullptr;
  int watchdogMs = 2000; // 2s ohne Daten -> Fehler

  QHostAddress m_lastSenderAddress;
  quint16 m_lastSenderPort = 0;
};

#endif // REALDATASOURCE_H
