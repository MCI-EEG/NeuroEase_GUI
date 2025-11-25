#ifndef ABSTRACTDATASOURCE_H
#define ABSTRACTDATASOURCE_H

#include <QObject>
#include <QVector>

class AbstractDataSource : public QObject
{
    Q_OBJECT
public:
    using QObject::QObject;
    ~AbstractDataSource() override = default;

    virtual void start() = 0;
    virtual void stop() = 0;
    virtual double sampleRate() const = 0;

signals:
    void newEEGData(const QVector<double>& values);
};

#endif // ABSTRACTDATASOURCE_H
