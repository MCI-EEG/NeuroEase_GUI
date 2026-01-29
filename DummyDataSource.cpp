#include "DummyDataSource.h"

DummyDataSource::DummyDataSource(QObject *parent)
    : AbstractDataSource(parent), time(0), m_samplesGenerated(0) {
  timer = new QTimer(this);
  timer->setInterval(16); // ~60 Hz update loop (generates bursts)
  connect(timer, &QTimer::timeout, this, &DummyDataSource::generateData);

  channelPhases.resize(numChannels);
  for (int i = 0; i < numChannels; ++i) {
    channelPhases[i] = QRandomGenerator::global()->generateDouble() * 2 * M_PI;
  }
}

void DummyDataSource::start() {
  time = 0.0;
  m_samplesGenerated = 0;
  m_elapsedTimer.start();
  timer->start();
}

void DummyDataSource::stop() { timer->stop(); }

void DummyDataSource::generateData() {
  double dt = 1.0 / 250.0; // 0.004
  qint64 elapsedMs = m_elapsedTimer.elapsed();
  qint64 expectedSamples = elapsedMs * 250 / 1000;

  while (m_samplesGenerated < expectedSamples) {
    QVector<double> values;

    for (int i = 0; i < numChannels; ++i) {
      // 1. Base Signal (4-15 Hz depending on channel) - Amplitude ~40 µV
      double baseFreq = 4.0 + i * 1.5;
      double baseSignal =
          40.0 * qSin(2 * M_PI * baseFreq * time + channelPhases[i]);

      // 2. Drift (0.5 Hz) - Amplitude ~30 µV - tests 1 Hz Highpass
      double drift = 30.0 * qSin(2 * M_PI * 0.5 * time);

      // 3. Power Line Hum (50 Hz) - Amplitude ~20 µV - tests 50 Hz Notch
      double hum = 20.0 * qSin(2 * M_PI * 50.0 * time);

      // 4. High Frequency Noise (80 Hz) - Amplitude ~15 µV - tests 50 Hz
      // Lowpass/Bandlimit
      double highFreq = 15.0 * qSin(2 * M_PI * 80.0 * time);

      // 5. White Noise - Amplitude ~5 µV
      double whiteNoise =
          (QRandomGenerator::global()->generateDouble() - 0.5) * 10.0;

      double value = baseSignal + drift + hum + highFreq + whiteNoise;
      values.append(value);
    }

    emit newEEGData(values);
    time += dt;
    m_samplesGenerated++;
  }
}

double DummyDataSource::sampleRate() const { return 250.0; }
