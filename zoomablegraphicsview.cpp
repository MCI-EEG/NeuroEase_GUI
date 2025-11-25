#include "zoomablegraphicsview.h"
#include <QTransform>
#include <QtMath>

ZoomableGraphicsView::ZoomableGraphicsView(QWidget *parent)
    : QGraphicsView(parent)
{
    // Diese Interaktionsflags können auch hier gesetzt werden, oder in MainWindow
    // setDragMode(QGraphicsView::ScrollHandDrag);
    // setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    // setResizeAnchor(QGraphicsView::AnchorViewCenter);
    // setRenderHint(QPainter::Antialiasing); // Für glattere Darstellung beim Skalieren/Drehen
}

void ZoomableGraphicsView::wheelEvent(QWheelEvent *event)
{
    // Zoom-Faktor
    double scaleFactor = 1.15; // Jedes Mausrad-Tick zoomt um 15%

    // Richtung des Mausrads (nach oben oder unten)
    if (event->angleDelta().y() > 0) {
        // Zoom In
        scale(scaleFactor, scaleFactor);
    } else {
        // Zoom Out
        scale(1.0 / scaleFactor, 1.0 / scaleFactor);
    }

    // Optional: Rotation mit gedrückter Strg-Taste
    // If you want to add rotation with Ctrl+Wheel:
    if (event->modifiers() & Qt::ControlModifier) {
        double angleDelta = event->angleDelta().y() / 8.0; // Eine kleinere Rotation pro Tick
        rotate(angleDelta); // Dreht die View
    } else {
        // Zoom-Faktor
        double scaleFactor = 1.15; // Jedes Mausrad-Tick zoomt um 15%

        // Richtung des Mausrads (nach oben oder unten)
        if (event->angleDelta().y() > 0) {
            // Zoom In
            scale(scaleFactor, scaleFactor);
        } else {
            // Zoom Out
            scale(1.0 / scaleFactor, 1.0 / scaleFactor);
        }
    }

    // Ereignis als verarbeitet markieren, damit es nicht an den Parent weitergeleitet wird
    event->accept();
}
