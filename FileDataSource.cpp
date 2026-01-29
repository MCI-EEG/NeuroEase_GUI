#include "FileDataSource.h"

#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTextStream>

FileDataSource::FileDataSource(const QString &filePath, QObject *parent)
    : AbstractDataSource(parent), m_filePath(filePath) {
  timer = new QTimer(this);
  connect(timer, &QTimer::timeout, this, &FileDataSource::generateFromFile);

  loadFile();
}

void FileDataSource::loadFile() {
  m_samples.clear();
  m_index = 0;
  m_isNeuroEaseFormat = false; // Reset format flag

  QFile file(m_filePath);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    qWarning() << "Could not open EEG sample file:" << m_filePath;
    return;
  }

  emit statusMessage("Opening file: " + m_filePath);

  QTextStream in(&file);

  // Header einlesen (Zeilen beginnen mit '%')
  while (!in.atEnd()) {
    qint64 pos = file.pos();
    QString line = in.readLine().trimmed();
    if (!line.startsWith('%')) {
      file.seek(pos);
      in.seek(pos);
      break;
    }

    // Check for Custom Format
    if (line.contains("Format = NeuroEaseCSV", Qt::CaseInsensitive)) {
      m_isNeuroEaseFormat = true;
      qInfo() << "Detected NeuroEase CSV Format (Values in uV)";
      emit statusMessage("Detected NeuroEase CSV Format (Values in uV)");
    }

    // Qt6-kompatible Regular Expression:
    QRegularExpression rx("Sample Rate\\s*=\\s*([0-9\\.]+)");
    auto match = rx.match(line);
    if (match.hasMatch()) {
      double sr = match.captured(1).toDouble();
      if (sr > 0) {
        m_sampleRate = sr;
        qInfo() << "Detected sample rate from file:" << m_sampleRate;
        emit statusMessage(
            QString("Detected sample rate from file: %1 Hz").arg(m_sampleRate));
      }
    }
  }

  setSampleRate(m_sampleRate);

  // Daten lesen
  while (!in.atEnd()) {
    QString line = in.readLine().trimmed();
    if (line.isEmpty())
      continue;
    if (line.startsWith('%'))
      continue;

    QStringList parts = line.split(',');
    if (parts.size() < 1 + m_numChannels)
      continue;

    QVector<double> values;
    values.reserve(m_numChannels);

    for (int ch = 0; ch < m_numChannels; ++ch) {
      bool ok = false;
      double raw = parts[1 + ch].trimmed().toDouble(&ok);
      if (!ok)
        raw = 0.0;

      double value = raw;
      if (!m_isNeuroEaseFormat) {
        value = raw / 50000.0; // OpenBCI legacy scaling
      }

      values.append(value);
    }

    m_samples.append(values);
  }

  qInfo() << "Loaded" << m_samples.size() << "EEG samples from" << m_filePath
          << "with SR =" << m_sampleRate << "Hz";
  m_initStatus = QString("Loaded %1 EEG samples from %2 with SR = %3 Hz")
                     .arg(m_samples.size())
                     .arg(QFileInfo(m_filePath).fileName())
                     .arg(m_sampleRate);

  qInfo() << m_initStatus;
  emit statusMessage(m_initStatus);
}

void FileDataSource::reportInitStatus() {
  if (!m_initStatus.isEmpty()) {
    emit statusMessage(m_initStatus);
  }
}

void FileDataSource::setSampleRate(double sr) {
  if (sr <= 0)
    return;

  m_sampleRate = sr;
  // Use a faster timer loop (e.g. 60Hz) and burst samples based on elapsed time
  timer->setInterval(16);
}

void FileDataSource::start() {
  if (m_samples.isEmpty())
    loadFile();

  if (m_samples.isEmpty())
    return;

  m_index = 0;
  m_samplesEmitted = 0;
  m_elapsedTimer.start();
  timer->start();
}

void FileDataSource::stop() { timer->stop(); }

void FileDataSource::generateFromFile() {
  if (m_samples.isEmpty())
    return;

  // Calculate how many samples *should* have been played by now
  qint64 elapsedMs = m_elapsedTimer.elapsed();
  qint64 expectedSamples = elapsedMs * m_sampleRate / 1000;

  while (m_samplesEmitted < expectedSamples) {
    emit newEEGData(m_samples[m_index]);

    m_index++;
    m_samplesEmitted++;

    if (m_index >= m_samples.size()) {
      timer->stop();
      qInfo() << "End of file reached.";
      return;
    }
  }
}
