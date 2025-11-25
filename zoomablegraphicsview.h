#ifndef ZOOMABLEGRAPHICSVIEW_H
#define ZOOMABLEGRAPHICSVIEW_H

#include <QGraphicsView> // WICHTIG: QGraphicsView muss inkludiert sein, da wir davon erben!
#include <QWheelEvent>   // Wird für wheelEvent benötigt (gute Praxis, auch hier zu inkludieren)

class ZoomableGraphicsView : public QGraphicsView // KORREKTUR: Richtiger Klassenname und Vererbung
{
    Q_OBJECT // WICHTIG: Macro für Qt's Meta-Object System (Signals & Slots)

public:
    explicit ZoomableGraphicsView(QWidget *parent = nullptr); // KORREKTUR: Richtiger Konstruktorname und Parameter
    // ~ZoomableGraphicsView() override; // Wenn ein benutzerdefinierter Destruktor nötig wäre

protected:
    void wheelEvent(QWheelEvent *event) override; // Deklaration der überschriebenen Methode
};

#endif // ZOOMABLEGRAPHVIEW_H
