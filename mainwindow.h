#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QGraphicsScene>
#include <QHBoxLayout>
#include <QLabel>
#include <QMainWindow>
#include <QPushButton>
#include <QVBoxLayout>
#include <QVector>
#include <complex>

class QComboBox;
class QSpinBox;
class QCheckBox;
class QCustomPlot;
class QCPBars;
class ZoomableGraphicsView;
#include "AbstractDataSource.h"
#include <QCheckBox>
#include <QFile>
#include <QTextStream>

class DataProcessingQt;

class MainWindow : public QMainWindow {
  Q_OBJECT

public:
  explicit MainWindow(QWidget *parent = nullptr);
  ~MainWindow() override;

  // Recording
  QCheckBox *recordCheckBox = nullptr;
  QFile recordingFile;
  QTextStream recordingStream;
  bool isRecording = false;
  int recordingIndex = 0;

  bool startRecording();
  void stopRecording();

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
  void updateThetaBetaBars(); // Default Balken
  void updateThetaBetaBarsFromBandPower(const BandPower &bp);
  void updateFocusIndicator(double ratio);

  BandPower computeBandPower(const QVector<double> &signal, double sampleRate);
  QVector<double> computeMagnitudeSpectrum(const QVector<double> &signal,
                                           double sampleRate);
  void updateBandPowerPlot(const BandPower &bp);
  void updateFftPlot();

  // Zeitachse
  double time = 0.0;

  // EEG-Kanäle
  static constexpr int numChannels = 8;
  QVector<QCustomPlot *> channelPlots;
  QVector<double> channelPhases;

  // Buttons
  QPushButton *startButton = nullptr;
  QPushButton *stopButton = nullptr;
  QPushButton *resetButton = nullptr;

  // Layouts
  QHBoxLayout *mainHorizontalLayout = nullptr;
  QVBoxLayout *leftColumnLayout = nullptr;
  QVBoxLayout *centerColumnLayout = nullptr;
  QVBoxLayout *rightColumnLayout = nullptr;

  // Heatmap / Topographie
  ZoomableGraphicsView *electrodePlacementView = nullptr;
  class ElectrodeMap *electrodePlacementScene = nullptr;
  QGraphicsPixmapItem *heatmapPixmapItem = nullptr;

  // Theta/Beta-Balkendiagramm
  QCustomPlot *thetaBetaBarPlot = nullptr;
  QCPBars *thetaBetaBars = nullptr;

  // Band-Power (Delta/Theta/Alpha/Beta/Gamma)
  QCustomPlot *bandPowerPlot = nullptr;
  QCPBars *bandPowerBars = nullptr;
  QVector<double> bandPowerTicks;

  // FFT-Plot (unten, über Mitte+Rechts)
  QCustomPlot *fftPlot = nullptr;
  QVector<QVector<double>> fftBuffers; // [channel][samples]

  // Fokus-Ampel
  QLabel *focusIndicator = nullptr;

  // Aktuelle Datenquelle (Sim / Real / File)
  AbstractDataSource *dataSource = nullptr;

  // Auswahlmodus (Sim/Real/File) + UDP-Port
  QComboBox *modeCombo = nullptr;
  QSpinBox *udpPortSpinBox = nullptr;
  QComboBox *fftRangeCombo = nullptr;

  // Filter-Checkboxen
  QCheckBox *hpCheckBox = nullptr;
  QCheckBox *notchCheckBox = nullptr;
  QCheckBox *bpCheckBox = nullptr;

  // aktuelle Abtastrate für Zeitachse
  double currentSampleRate = 50.0;

  // Kontroll-Flag für Elektroden-Check
  bool placementConfirmed = false;

  // Buffer für Bandpower (z.B. Kanal Fp1)
  QVector<double> bandPowerBuffer;

  // Buffer für Head-Plot (RMS-Aktivität pro Kanal)
  QVector<QVector<double>> headBuffers;

  // DSP (Highpass + Notch + Bandlimit)
  DataProcessingQt *dataProcessor = nullptr;

  // FFT Optimization
  void fft(QVector<std::complex<double>> &a, bool invert);
};

#endif // MAINWINDOW_H
