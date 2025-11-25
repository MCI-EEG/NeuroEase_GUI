#ifndef FILEDATASOURCE_H
#define FILEDATASOURCE_H

#include "AbstractDataSource.h"
#include <QTimer>
#include <QString>
#include <QVector>

class FileDataSource : public AbstractDataSource
{
    Q_OBJECT
public:
    explicit FileDataSource(const QString &filePath, QObject *parent = nullptr);

    void start() override;
    void stop() override;
    double sampleRate() const override { return m_sampleRate; }

    void setSampleRate(double sr);

private slots:
    void generateFromFile();

private:
    void loadFile();

    QTimer *timer = nullptr;
    QString m_filePath;
    QVector<QVector<double>> m_samples;
    int m_index = 0;
    int m_numChannels = 8;
    double m_sampleRate = 250.0;
};

#endif // FILEDATASOURCE_H
