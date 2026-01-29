#include "electrodemap.h"
#include <QBrush>
#include <QFont>
#include <QFontMetrics>
#include <QGraphicsEllipseItem>
#include <QGraphicsPixmapItem>
#include <QGraphicsSimpleTextItem>
#include <QImage>
#include <QPainterPath>
#include <QPen>
#include <QPixmap>
#include <QtMath>

ElectrodeMap::ElectrodeMap(QObject *parent) : QGraphicsScene(parent) {
  // Beschriftungen für später
  labels = {"Fp1", "Fp2", "F7", "F8", "Fz", "Pz", "T5", "T6", "Ref"};

  // Positionen 10-20 System (8 Kanäle + Referenz)
  double headR = 80;
  positions = {
      {-headR * 0.25, -headR * 0.9}, // Fp1 (Wiederhergestellt)
      {headR * 0.25, -headR * 0.9},  // Fp2
      {-headR * 0.5, -headR * 0.7},  // F7
      {headR * 0.5, -headR * 0.7},   // F8
      {0, -headR * 0.3},             // Fz
      {0, headR * 0.25},             // Pz
      {-headR * 0.7, headR * 0.5},   // T5
      {headR * 0.7, headR * 0.5},    // T6
      {0, 0}                         // Referenz (R)
  };

  offsets.resize(labels.size());
  for (int i = 0; i < offsets.size(); ++i)
    offsets[i] = QPointF(0, 0);

  // Beispiel: T5 (Index 6) ein wenig nach rechts und unten schieben
  offsets[6] = QPointF(0.5, 1.0);
}

void ElectrodeMap::reset() {
  clear();
  drawHead();
}

void ElectrodeMap::setActivities(const QVector<double> &activities) {
  clear();
  drawHead();
  drawHeatmap(activities);
  drawElectrodes(activities);
}

void ElectrodeMap::drawHead() {
  double headR = 80;
  QPointF c(0, 0);
  QPen pen(Qt::black, 2);

  // Kopfumriss
  addEllipse(c.x() - headR, c.y() - headR * 1.1, headR * 2, headR * 2.2, pen);

  // Ohren
  addEllipse(c.x() - headR - 15, c.y() - 10, 20, 30, pen);
  addEllipse(c.x() + headR - 5, c.y() - 10, 20, 30, pen);

  // Nase (Dreieck)
  QPainterPath nose;
  nose.moveTo(c.x(), c.y() - headR * 1.1 + 10);
  nose.lineTo(c.x() - 10, c.y() - headR * 1.1 + 25);
  nose.lineTo(c.x() + 10, c.y() - headR * 1.1 + 25);
  nose.closeSubpath();
  addPath(nose, pen);
}

void ElectrodeMap::drawHeatmap(const QVector<double> &activities) {
  double headR = 80;
  int imgW = 300;
  int imgH = 300;

  QImage heatmap(imgW, imgH, QImage::Format_ARGB32);
  heatmap.fill(Qt::transparent);

  // Bildmitte bei (150, 150) entspricht Szene (0, 0)
  double offsetX = 150.0;
  double offsetY = 150.0;

  for (int y = 0; y < imgH; ++y) {
    double py = double(y) - offsetY;
    for (int x = 0; x < imgW; ++x) {
      double px = double(x) - offsetX;

      // Innerhalb des Kopfovals?
      double e = (px * px) / (headR * headR) +
                 (py * py) / ((1.1 * headR) * (1.1 * headR));
      if (e > 1.0)
        continue;

      double sumW = 0.0;
      double sumA = 0.0;
      for (int i = 0; i < activities.size() && i < positions.size(); ++i) {
        double dx = px - positions[i].x();
        double dy = py - positions[i].y();
        double d = qSqrt(dx * dx + dy * dy) + 0.01;
        double w = 1.0 / (d * d);
        sumW += w;
        sumA += w * activities[i];
      }
      double val = (sumW > 0.0 ? sumA / sumW : 0.0);
      val = qBound(0.0, val, 1.0);

      int r = int(255 * val);
      int g = int(255 * (1.0 - val));
      heatmap.setPixelColor(x, y, QColor(r, g, 0, 140));
    }
  }

  heatmapItem = addPixmap(QPixmap::fromImage(heatmap));
  heatmapItem->setPos(-150, -150);
  heatmapItem->setZValue(-1);
}

void ElectrodeMap::drawElectrodes(const QVector<double> &activities) {
  QPen ePen(Qt::black, 1);
  QBrush eBrush(Qt::gray);
  double eSz = 14;

  for (int i = 0; i < positions.size(); ++i) {
    double activity = (i < activities.size()) ? activities[i] : 0.0;
    // Marker
    QPen penEdge = (i == positions.size() - 1 ? QPen(Qt::black, 2) : ePen);
    QBrush brushBg = (i == positions.size() - 1 ? QBrush(Qt::white) : eBrush);

    QFont font("Arial");
    font.setPixelSize(6);
    font.setBold(true);

    // Elektrode relativ zum eigenen Ursprung (0,0) erstellen und dann
    // positionieren
    QGraphicsEllipseItem *elItem =
        addEllipse(-eSz / 2.0, -eSz / 2.0, eSz, eSz, penEdge, brushBg);
    elItem->setPos(positions[i]);
    elItem->setZValue(10);

    // Label als Kind der Elektrode -> (0,0) ist jetzt die Mitte der Elektrode
    QGraphicsSimpleTextItem *text =
        new QGraphicsSimpleTextItem(labels[i], elItem);
    text->setFont(font);
    text->setBrush(Qt::black);
    text->setZValue(11);

    // Den Text innerhalb der Elektrode zentrieren + manueller Offset
    QRectF br = text->boundingRect();
    text->setPos(-br.width() / 2.0 + offsets[i].x(),
                 -br.height() / 2.0 + offsets[i].y());
  }
}
