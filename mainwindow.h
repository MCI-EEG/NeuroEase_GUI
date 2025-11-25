#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QVector>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QGraphicsScene>
#include <complex>

class QComboBox;
class QSpinBox;
class QCustomPlot;
class QCPBars;
class ZoomableGraphicsView;
class AbstractDataSource;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void handleNewEEGData(const QVector<double> &values);
    void resetPlots();

private:
    struct BandPower {
        double delta;
        double theta;
        double alpha;
        double beta;
        double gamma;
    };

    void updateElectrodePlacement();
    void updateThetaBetaBars();
    void updateThetaBetaBarsFromEEG(const QVector<double> &values);
    void updateFocusIndicator(double ratio);

    BandPower      computeBandPower(const QVector<double> &signal, double sampleRate);
    QVector<double> computeMagnitudeSpectrum(const QVector<double> &signal, double sampleRate);
    void           updateBandPowerPlot(const BandPower &bp);
    void           updateFftPlot();

    // Zeitachse
    double                      time = 0.0;

    // EEG-Kanäle
    static constexpr int        numChannels = 8;
    QVector<QCustomPlot*>       channelPlots;
    QVector<double>             channelPhases;

    // Buttons
    QPushButton                *startButton = nullptr;
    QPushButton                *stopButton  = nullptr;
    QPushButton                *resetButton = nullptr;

    // Layouts
    QHBoxLayout               *mainHorizontalLayout = nullptr;
    QVBoxLayout               *leftColumnLayout     = nullptr;
    QVBoxLayout               *centerColumnLayout   = nullptr;
    QVBoxLayout               *rightColumnLayout    = nullptr;

    // Heatmap / Topographie
    ZoomableGraphicsView      *electrodePlacementView  = nullptr;
    QGraphicsScene            *electrodePlacementScene = nullptr;

    // Theta/Beta-Balkendiagramm
    QCustomPlot               *thetaBetaBarPlot = nullptr;
    QCPBars                   *thetaBetaBars    = nullptr;

    // Band-Power (Delta/Theta/Alpha/Beta/Gamma)
    QCustomPlot               *bandPowerPlot    = nullptr;
    QCPBars                   *bandPowerBars    = nullptr;
    QVector<double>            bandPowerTicks;

    // FFT-Plot (unten, über Mitte+Rechts)
    QCustomPlot               *fftPlot          = nullptr;
    QVector<QVector<double>>   fftBuffers;   // [channel][samples]

    // Fokus-Ampel
    QLabel                    *focusIndicator   = nullptr;

    // Aktuelle Datenquelle (Sim / Real / File)
    AbstractDataSource        *dataSource       = nullptr;

    // Auswahlmodus (Sim/Real/File) + UDP-Port
    QComboBox                 *modeCombo        = nullptr;
    QSpinBox                  *udpPortSpinBox   = nullptr;

    // aktuelle Abtastrate für Zeitachse
    double                     currentSampleRate = 50.0;

    // Kontroll-Flag für Elektroden-Check
    bool                       placementConfirmed = false;

    // Buffer für Bandpower (z.B. Kanal Fp1)
    QVector<double>            bandPowerBuffer;
};

#endif // MAINWINDOW_H
