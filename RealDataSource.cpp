#include "RealDataSource.h"

#include <QDebug>
#include <QHostAddress>


RealDataSource::RealDataSource(QObject *parent) : AbstractDataSource(parent) {
  watchdogTimer = new QTimer(this);
  watchdogTimer->setInterval(watchdogMs);
  watchdogTimer->setSingleShot(true);

  connect(watchdogTimer, &QTimer::timeout, this, [this]() {
    if (!udpSocket)
      return;
    emit udpError(tr("No UDP data received on port %1.\nMeasurement stopped.")
                      .arg(m_port));
    stop();
  });
}

RealDataSource::~RealDataSource() { stop(); }

void RealDataSource::setUdpPort(quint16 port) { m_port = port; }

void RealDataSource::initSocket() {
  closeSocket();

  udpSocket = new QUdpSocket(this);
  bool ok =
      udpSocket->bind(QHostAddress::AnyIPv4, m_port, QUdpSocket::ShareAddress);
  if (!ok) {
    emit udpError(tr("Could not bind UDP port %1.\nPlease choose another port.")
                      .arg(m_port));
    udpSocket->deleteLater();
    udpSocket = nullptr;
    return;
  }

  connect(udpSocket, &QUdpSocket::readyRead, this, &RealDataSource::readUdp);

  // ab Start: wenn innerhalb watchdogMs nichts kommt -> Fehler
  watchdogTimer->start();
}

void RealDataSource::closeSocket() {
  if (!udpSocket)
    return;

  udpSocket->disconnect(this);
  udpSocket->close();
  udpSocket->deleteLater();
  udpSocket = nullptr;
}

void RealDataSource::start() {
  // UDP-Quelle initialisieren
  initSocket();
}

void RealDataSource::stop() {
  watchdogTimer->stop();
  closeSocket();
  m_lastSenderAddress = QHostAddress::Null;
  m_lastSenderPort = 0;
}

void RealDataSource::sendCommand(const QString &cmd) {
  if (!udpSocket || m_lastSenderAddress.isNull())
    return;

  udpSocket->writeDatagram((cmd + "\n").toUtf8(), m_lastSenderAddress,
                           m_lastSenderPort);
}

void RealDataSource::readUdp() {
  if (!udpSocket)
    return;

  // Sobald Daten kommen, Watchdog wieder neu starten
  watchdogTimer->start();

  while (udpSocket->hasPendingDatagrams()) {
    QByteArray buffer;
    buffer.resize(int(udpSocket->pendingDatagramSize()));
    QHostAddress sender;
    quint16 port;
    udpSocket->readDatagram(buffer.data(), buffer.size(), &sender, &port);

    m_lastSenderAddress = sender;
    m_lastSenderPort = port;

    // Check for text response (impedance result)
    if (buffer.startsWith("IMP:")) {
      QString text = QString::fromUtf8(buffer).trimmed();
      QString valuesStr = text.mid(4); // Remove "IMP:"
      QStringList values = valuesStr.split(',');
      emit impedanceReceived(values);
      continue;
    }

    // TODO: Hier dein echtes UDP-Protokoll parsen
    // Im Moment: Dummy mit 8 Kanälen = 0
    QVector<double> values(8, 0.0);
    emit newEEGData(values);
  }
}
