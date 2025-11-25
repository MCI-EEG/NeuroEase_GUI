#include "FileDataSource.h"

#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QDebug>

FileDataSource::FileDataSource(const QString &filePath, QObject *parent)
    : AbstractDataSource(parent)
    , m_filePath(filePath)
{
    timer = new QTimer(this);
    connect(timer, &QTimer::timeout,
            this, &FileDataSource::generateFromFile);

    loadFile();
}

void FileDataSource::loadFile()
{
    m_samples.clear();
    m_index = 0;

    QFile file(m_filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Could not open EEG sample file:" << m_filePath;
        return;
    }

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

        // Qt6-kompatible Regular Expression:
        QRegularExpression rx("Sample Rate\\s*=\\s*([0-9\\.]+)");
        auto match = rx.match(line);
        if (match.hasMatch()) {
            double sr = match.captured(1).toDouble();
            if (sr > 0) {
                m_sampleRate = sr;
                qInfo() << "Detected sample rate from file:" << m_sampleRate;
            }
        }
    }

    setSampleRate(m_sampleRate);

    // Daten lesen
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty()) continue;
        if (line.startsWith('%')) continue;

        QStringList parts = line.split(',');
        if (parts.size() < 1 + m_numChannels)
            continue;

        QVector<double> values;
        values.reserve(m_numChannels);

        for (int ch = 0; ch < m_numChannels; ++ch) {
            bool ok = false;
            double raw = parts[1 + ch].trimmed().toDouble(&ok);
            if (!ok) raw = 0.0;

            double value = raw / 50000.0; // Normierung
            values.append(value);
        }

        m_samples.append(values);
    }

    qInfo() << "Loaded" << m_samples.size()
            << "EEG samples from" << m_filePath
            << "with SR =" << m_sampleRate << "Hz";
}

void FileDataSource::setSampleRate(double sr)
{
    if (sr <= 0) return;

    m_sampleRate = sr;
    int intervalMs = qMax(1, int(1000.0 / m_sampleRate));
    timer->setInterval(intervalMs);
}

void FileDataSource::start()
{
    if (m_samples.isEmpty())
        loadFile();

    if (m_samples.isEmpty())
        return;

    m_index = 0;
    timer->start();
}

void FileDataSource::stop()
{
    timer->stop();
}

void FileDataSource::generateFromFile()
{
    if (m_samples.isEmpty())
        return;

    emit newEEGData(m_samples[m_index]);

    ++m_index;
    if (m_index >= m_samples.size())
        m_index = 0; // Loop
}
