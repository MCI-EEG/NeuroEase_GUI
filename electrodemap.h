#pragma once
#include <QGraphicsScene>
#include <QVector>
#include <QPointF>
#include <QStringList>

class ElectrodeMap : public QGraphicsScene {
    Q_OBJECT
public:
    explicit ElectrodeMap(QObject* parent = nullptr);
    void setActivities(const QVector<double>& activities);
    void reset();
private:
    QStringList labels{"Fp1","Fp2","F7","F8","Fz","Pz","T5","T6","R"};
    QVector<QPointF> positions;
    void drawHead();
    void drawElectrodes(const QVector<double>& activities);
    void drawHeatmap(const QVector<double>& activities);
};
