#pragma once
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QPointF>
#include <QStringList>
#include <QVector>

class ElectrodeMap : public QGraphicsScene {
  Q_OBJECT
public:
  explicit ElectrodeMap(QObject *parent = nullptr);
  void setActivities(const QVector<double> &activities);
  void reset();

private:
  QStringList labels{"Fp1", "Fp2", "F7", "F8", "Fz", "Pz", "T5", "T6", "Ref"};
  QVector<QPointF> positions;
  QVector<QPointF> offsets;
  QGraphicsPixmapItem *heatmapItem = nullptr;
  void drawHead();
  void drawElectrodes(const QVector<double> &activities);
  void drawHeatmap(const QVector<double> &activities);
};
