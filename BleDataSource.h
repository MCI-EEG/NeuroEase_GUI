#ifndef BLEDATASOURCE_H
#define BLEDATASOURCE_H

#include "AbstractDataSource.h"

#include <QBluetoothDeviceDiscoveryAgent>
#include <QBluetoothLocalDevice>
#include <QLowEnergyController>
#include <QLowEnergyService>
#include <QObject>
#include <QQueue>
#include <QTimer>

class BleDataSource : public AbstractDataSource {
  Q_OBJECT
public:
  explicit BleDataSource(QObject *parent = nullptr);
  ~BleDataSource() override;

  void start() override;
  void stop() override;
  double sampleRate() const override {
    return 250.0;
  } // Assuming 250Hz like Real/Sim
  void sendCommand(const QString &cmd) override;
  bool isConnected() const { return m_isConnected; }

signals:
  void statusMessage(const QString &msg);
  void criticalError(const QString &title, const QString &msg);

private slots:
  // Discovery
  void deviceDiscovered(const QBluetoothDeviceInfo &device);
  void scanFinished();
  void deviceScanError(QBluetoothDeviceDiscoveryAgent::Error error);

  // Connection
  void deviceConnected();
  void deviceDisconnected();
  void controllerError(QLowEnergyController::Error error);
  void serviceDiscovered(const QBluetoothUuid &newService);
  void serviceScanDone();

  // Service interaction
  void serviceStateChanged(QLowEnergyService::ServiceState s);
  void serviceCharacteristicChanged(const QLowEnergyCharacteristic &c,
                                    const QByteArray &value);

private:
  void startScan();
  void connectToDevice(const QBluetoothDeviceInfo &deviceInfo);
  void parsePacket(const QByteArray &data);

  // Initial parsing buffer (TCP-style handling for BLE chunks if needed,
  // though notifications are usually packets, they might be split?
  // Usually BLE packets are small, but we use "notify_chunked" in firmware
  // which sends chunks. We need to reassemble if the firmware splits the
  // 44-byte logical packet into MTU-sized physical packets.
  QByteArray m_incomingBuffer;

  QBluetoothDeviceDiscoveryAgent *m_discoveryAgent = nullptr;
  QLowEnergyController *m_controller = nullptr;
  QLowEnergyService *m_service = nullptr;

  bool m_targetDeviceFound = false;

  // UUIDs
  const QString SERVICE_UUID = "{6E400001-B5A3-F393-E0A9-E50E24DCCA9E}";
  const QString CHAR_RX_UUID =
      "{6E400002-B5A3-F393-E0A9-E50E24DCCA9E}"; // Write (Command)
  const QString CHAR_TX_UUID =
      "{6E400003-B5A3-F393-E0A9-E50E24DCCA9E}"; // Notify (Data)

  // Expected packet size
  static const int PACKET_SIZE = 44;

  // Drop tracking
  qint64 m_lastDeviceTimestamp = 0;
  qint64 m_dropCount = 0;
  bool m_isConnected = false;
};

#endif // BLEDATASOURCE_H
