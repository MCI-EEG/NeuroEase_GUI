#include "BleDataSource.h"
#include <QDataStream>
#include <QDebug>
#include <QtEndian>

BleDataSource::BleDataSource(QObject *parent) : AbstractDataSource(parent) {
  m_discoveryAgent = new QBluetoothDeviceDiscoveryAgent(this);
  m_discoveryAgent->setLowEnergyDiscoveryTimeout(5000);

  connect(m_discoveryAgent, &QBluetoothDeviceDiscoveryAgent::deviceDiscovered,
          this, &BleDataSource::deviceDiscovered);
  connect(m_discoveryAgent, &QBluetoothDeviceDiscoveryAgent::finished, this,
          &BleDataSource::scanFinished);

  // In Qt 6, errorOccurred is used. In some Qt 5 versions, it's also available.
  // Using the modern signal name to avoid static_cast ambiguity.
  connect(m_discoveryAgent, &QBluetoothDeviceDiscoveryAgent::errorOccurred,
          this, &BleDataSource::deviceScanError);
}

BleDataSource::~BleDataSource() { stop(); }

void BleDataSource::start() {
  auto localDevices = QBluetoothLocalDevice::allDevices();
  if (localDevices.isEmpty()) {
    emit criticalError(
        tr("No Bluetooth adapter found!"),
        tr("Qt Bluetooth reports 'Dummy backend'. Please ensure Bluetooth is "
           "enabled and your hardware supports BLE."));
    return;
  }

  emit statusMessage(tr("Searching for a compatible device, please keep the "
                        "NeuroEase device switched on..."));
  m_targetDeviceFound = false;
  startScan();
}

void BleDataSource::stop() {
  if (m_discoveryAgent->isActive())
    m_discoveryAgent->stop();

  if (m_controller) {
    m_controller->disconnectFromDevice();
    delete m_controller;
    m_controller = nullptr;
  }
  m_isConnected = false;
}

void BleDataSource::startScan() {
  if (m_controller) {
    m_controller->disconnectFromDevice();
    delete m_controller;
    m_controller = nullptr;
  }

  m_discoveryAgent->stop();
  // NoMethod allows the backend to choose the best available method.
  m_discoveryAgent->start();
}

void BleDataSource::deviceDiscovered(const QBluetoothDeviceInfo &device) {
  if (m_targetDeviceFound)
    return;

  QString deviceName = device.name();
  QString deviceAddress = device.address().toString();
  QStringList uuids;
  for (const QBluetoothUuid &uuid : device.serviceUuids()) {
    uuids << uuid.toString();
  }

  qDebug() << "Found device:" << deviceName << "[" << deviceAddress << "]"
           << (uuids.isEmpty() ? "" : "UUIDs: " + uuids.join(", "));

  // Check by Service UUID or Name as fallback
  bool matchUuid = device.serviceUuids().contains(QBluetoothUuid(SERVICE_UUID));
  bool matchName = deviceName.contains("NeuroEase", Qt::CaseInsensitive) ||
                   deviceName.contains("EEG_S3_BLE", Qt::CaseInsensitive);

  if (matchUuid || matchName) {
    qDebug() << "NeuroEase/EEG_S3_BLE device identified!"
             << (matchUuid ? "(via UUID)" : "(via Name)");

    m_targetDeviceFound = true;
    m_discoveryAgent->stop();
    connectToDevice(device);
  }
}

void BleDataSource::scanFinished() {
  if (!m_targetDeviceFound) {
    emit statusMessage(tr("Scan finished. Device not found. Make sure "
                          "Bluetooth is on and the device is nearby."));
  }
}

void BleDataSource::deviceScanError(
    QBluetoothDeviceDiscoveryAgent::Error error) {
  emit statusMessage(
      QString("Scan Error: %1").arg(m_discoveryAgent->errorString()));
}

void BleDataSource::connectToDevice(const QBluetoothDeviceInfo &deviceInfo) {
  emit statusMessage(QString("Connecting to %1...").arg(deviceInfo.name()));

  m_controller = QLowEnergyController::createCentral(deviceInfo, this);
  connect(m_controller, &QLowEnergyController::connected, this,
          &BleDataSource::deviceConnected);
  connect(m_controller, &QLowEnergyController::disconnected, this,
          &BleDataSource::deviceDisconnected);
  connect(m_controller, &QLowEnergyController::errorOccurred, this,
          &BleDataSource::controllerError);
  connect(m_controller, &QLowEnergyController::serviceDiscovered, this,
          &BleDataSource::serviceDiscovered);
  connect(m_controller, &QLowEnergyController::discoveryFinished, this,
          &BleDataSource::serviceScanDone);

  m_controller->connectToDevice();
}

void BleDataSource::deviceConnected() {
  emit statusMessage("Connected. Discovering services...");
  m_controller->discoverServices();
}

void BleDataSource::deviceDisconnected() {
  emit statusMessage("Disconnected.");
  m_isConnected = false;
}

void BleDataSource::controllerError(QLowEnergyController::Error error) {
  emit statusMessage(
      QString("Controller Error: %1").arg(m_controller->errorString()));
}

void BleDataSource::serviceDiscovered(const QBluetoothUuid &newService) {
  if (newService == QBluetoothUuid(SERVICE_UUID)) {
    m_service = m_controller->createServiceObject(newService, this);
  }
}

void BleDataSource::serviceScanDone() {
  if (m_service) {
    emit statusMessage("Service found. Connecting to service...");
    connect(m_service, &QLowEnergyService::stateChanged, this,
            &BleDataSource::serviceStateChanged);
    connect(m_service, &QLowEnergyService::characteristicChanged, this,
            &BleDataSource::serviceCharacteristicChanged);
    m_service->discoverDetails();
  } else {
    emit statusMessage("Service not found on device.");
  }
}

void BleDataSource::serviceStateChanged(QLowEnergyService::ServiceState s) {
  if (s == QLowEnergyService::ServiceDiscovered) {
    QLowEnergyCharacteristic numChar =
        m_service->characteristic(QBluetoothUuid(CHAR_TX_UUID));
    if (!numChar.isValid()) {
      emit statusMessage("TX Characteristic not found.");
      return;
    }

    QLowEnergyDescriptor notification = numChar.descriptor(QBluetoothUuid(
        QBluetoothUuid::DescriptorType::ClientCharacteristicConfiguration));
    if (notification.isValid()) {
      m_service->writeDescriptor(notification,
                                 QByteArray::fromHex("0100")); // Enable Notify
      m_isConnected = true;
      emit statusMessage(tr("Successfully connected."));
    }
  }
}

void BleDataSource::serviceCharacteristicChanged(
    const QLowEnergyCharacteristic &c, const QByteArray &value) {
  if (c.uuid() == QBluetoothUuid(CHAR_TX_UUID)) {
    // Check for text response (impedance result)
    if (value.startsWith("IMP:")) {
      QString text = QString::fromUtf8(value).trimmed();
      QString valuesStr = text.mid(4); // Remove "IMP:"
      QStringList values = valuesStr.split(',');
      emit impedanceReceived(values);
      return;
    }

    m_incomingBuffer.append(value);

    // Process all complete packets in buffer
    while (m_incomingBuffer.size() >= PACKET_SIZE) {
      // Check for sync byte 0xA0 at index 0
      if (static_cast<unsigned char>(m_incomingBuffer.at(0)) == 0xA0) {
        // Determine if we have a full packet
        QByteArray packet = m_incomingBuffer.mid(0, PACKET_SIZE);
        parsePacket(packet);
        m_incomingBuffer.remove(0, PACKET_SIZE);
      } else {
        // Lost sync? Shift by 1 byte to find next 0xA0
        // This is a simple recovery mechanism.
        int nextSync = m_incomingBuffer.indexOf('\xA0', 1);
        if (nextSync != -1) {
          m_incomingBuffer.remove(0, nextSync);
        } else {
          // No sync byte found at all, clear buffer (but maybe keep last few
          // bytes if partial sync?) Safer to just clear if it's getting too
          // big, or keep scanning.
          m_incomingBuffer.clear();
        }
      }
    }
  }
}

void BleDataSource::sendCommand(const QString &cmd) {
  if (!m_service)
    return;

  QLowEnergyCharacteristic rxChar =
      m_service->characteristic(QBluetoothUuid(CHAR_RX_UUID));
  if (rxChar.isValid()) {
    m_service->writeCharacteristic(rxChar, (cmd + "\n").toUtf8());
  }
}

void BleDataSource::parsePacket(const QByteArray &data) {
  if (data.size() < PACKET_SIZE)
    return;

  // structure:
  // 0: 0xA0 (Header) -> Checked before calling parsePacket
  // 1-8: Timestamp (8 bytes, int64, microseconds)
  // 9-11: Status (Status[0], Status[1], Status[2])
  // 12-43: 8 channels * 4 bytes (int32)

  // 1. Timestamp & Drops
  QDataStream tsStream(data.mid(1, 8));
  tsStream.setByteOrder(QDataStream::LittleEndian);
  qint64 packetTs = 0;
  tsStream >> packetTs;

  if (m_lastDeviceTimestamp != 0) {
    qint64 diff = packetTs - m_lastDeviceTimestamp;
    // Expected at 250Hz: 4000 µs
    // Tolerance: 6000 µs
    const qint64 expected = 4000;
    if (diff > expected * 1.5) {
      int dropped = qRound(double(diff) / double(expected)) - 1;
      if (dropped > 0) {
        m_dropCount += dropped;
        if (m_dropCount % 50 == 0) { // Log every 50 drops
          qWarning() << "BLE Drop Detection: " << dropped
                     << "packets lost. Total:" << m_dropCount;
        }
      }
    }
  }
  m_lastDeviceTimestamp = packetTs;

  // 2. Status Byte
  // Python: if (stat[0] & 0xF0) == 0xC0
  unsigned char stat0 = static_cast<unsigned char>(data.at(9));
  if ((stat0 & 0xF0) != 0xC0) {
    // Invalid packet status (e.g. LOFF bits might be weird, or not ADS1299
    // data) We log it but maybe still process if it's just leads off? Python
    // script strictly requires it for adding to batch. Let's log warning and
    // return to be safe.
    qWarning() << "Invalid Status Byte:" << Qt::hex << stat0;
    return;
  }

  // 3. Channel Data
  QVector<double> values;
  values.reserve(8);

  QDataStream stream(data.mid(12, 32));
  stream.setByteOrder(QDataStream::LittleEndian); // ESP32 is Little Endian

  for (int i = 0; i < 8; ++i) {
    qint32 rawVal;
    stream >> rawVal;

    // ADS1299 scaling (Gain=24, Vref=4.5V, 24-bit)
    // 1 LSB = (2 * Vref / Gain) / 2^24
    // To get µV: multiply by 1,000,000
    const double vref = 4.5;
    const double gain = 24.0;
    const double lsbSize = (2.0 * vref / gain) / 16777216.0;
    const double scaleToMicrovolts = lsbSize * 1000000.0;

    values.append(static_cast<double>(rawVal) * scaleToMicrovolts);
  }

  emit newEEGData(values);
}
