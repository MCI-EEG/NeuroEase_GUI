#include "electrodemap.h"
#include <QGraphicsEllipseItem>
#include <QPainterPath>
#include <QtMath>
#include <QBrush>
#include <QPen>
#include <QFont>
#include <QGraphicsSimpleTextItem>

ElectrodeMap::ElectrodeMap(QObject* parent)
    : QGraphicsScene(parent)
{
    // Beschriftungen für später
    labels = { "Fp1", "Fp2", "F7", "F8", "Fz", "Pz", "T5", "T6", "R" };

    // Positionen 10-20 System (8 Kanäle + Referenz)
    double headR = 80;
    positions = {
        {-headR * 0.25, -headR * 0.9},  // Fp1
        { headR * 0.25, -headR * 0.9},  // Fp2
        {-headR * 0.5,  -headR * 0.7},  // F7
        { headR * 0.5,  -headR * 0.7},  // F8
        { 0,           -headR * 0.3},  // Fz
        { 0,            headR * 0.2},  // Pz
        {-headR * 0.7,  headR * 0.5},  // T5
        { headR * 0.7,  headR * 0.5},  // T6
        { 0,            0}             // Referenz (R)
    };
}

void ElectrodeMap::reset()
{
    clear();
    drawHead();
}

void ElectrodeMap::setActivities(const QVector<double>& activities)
{
    clear();
    drawHead();
    drawHeatmap(activities);
    drawElectrodes(activities);
}

void ElectrodeMap::drawHead()
{
    double headR = 80;
    QPointF c(0, 0);
    QPen pen(Qt::black, 2);

    // Kopfumriss
    addEllipse(c.x() - headR, c.y() - headR * 1.1,
               headR * 2, headR * 2.2,
               pen);

    // Ohren
    addEllipse(c.x() - headR - 15, c.y() - 10, 20, 30, pen);
    addEllipse(c.x() + headR - 5,   c.y() - 10, 20, 30, pen);

    // Nase (Dreieck)
    QPainterPath nose;
    nose.moveTo(c.x(), c.y() - headR * 1.1 + 10);
    nose.lineTo(c.x() - 10, c.y() - headR * 1.1 + 25);
    nose.lineTo(c.x() + 10, c.y() - headR * 1.1 + 25);
    nose.closeSubpath();
    addPath(nose, pen);
}

void ElectrodeMap::drawHeatmap(const QVector<double>& activities)
{
    double headR   = 80;
    double sceneR  = 150;
    double step    = 3.0;

    for (double x = -sceneR; x <= sceneR; x += step) {
        for (double y = -sceneR; y <= sceneR; y += step) {
            // nur innerhalb des Kopfovals
            double e = (x*x)/(headR*headR) + (y*y)/((headR*1.1)*(headR*1.1));
            if (e > 1.0) continue;

            // Inverse Distanzgewichtung (IDW)
            double sum = 0, wsum = 0;
            for (int i = 0; i < positions.size(); ++i) {
                double dx = x - positions[i].x();
                double dy = y - positions[i].y();
                double d  = qSqrt(dx*dx + dy*dy) + 0.01;
                double w  = 1.0 / (d*d);
                sum  += w * activities[i];
                wsum += w;
            }
            double iv = sum / wsum;
            int red   = qBound(0, int(iv * 255), 255);
            int green = 255 - red;
            QColor col(red, green, 0, 150);

            // zeichne als kleines Rechteck/Kreis
            addEllipse(x - step/2.0, y - step/2.0,
                       step, step,
                       Qt::NoPen, QBrush(col));
        }
    }
}

void ElectrodeMap::drawElectrodes(const QVector<double>& activities)
{
    QPen   ePen(Qt::black, 1);
    QBrush eBrush(Qt::gray);
    double eSz = 12;

    for (int i = 0; i < positions.size(); ++i) {
        // Marker
        QPen penEdge   = (i == positions.size()-1 ? QPen(Qt::black,2) : ePen);
        QBrush brushBg = (i == positions.size()-1 ? QBrush(Qt::white) : eBrush);
        addEllipse(positions[i].x() - eSz/2,
                   positions[i].y() - eSz/2,
                   eSz, eSz,
                   penEdge, brushBg);

        // Label zentriert (3 pt für Kanäle, 6 pt für Referenz)
        QGraphicsSimpleTextItem* text = addSimpleText(labels[i]);
        QFont font("Arial", (i == positions.size()-1 ? 6 : 3));
        text->setFont(font);
        QRectF br = text->boundingRect();
        text->setPos(positions[i].x() - br.width()/2,
                     positions[i].y() - br.height()/2);
        text->setBrush(Qt::black);
        text->setZValue(1);
    }
}
