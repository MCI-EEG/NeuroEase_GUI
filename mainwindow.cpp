#include "mainwindow.h"

#include "AbstractDataSource.h"
#include "BleDataSource.h"
#include "DataProcessingQt.h"
#include "DummyDataSource.h"
#include "FileDataSource.h"
#include "RealDataSource.h"
#include "electrodemap.h"
#include "qcustomplot.h"
#include "zoomablegraphicsview.h"

#include <QBrush>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QFont>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QGroupBox>
#include <QMessageBox>
#include <QPainterPath>
#include <QPen>
#include <QRandomGenerator>
#include <QSpinBox>
#include <QStandardPaths>
#include <QWidget>
#include <QtMath>
#include <algorithm>
#include <cmath>

// -----------------------------------------------------------------------------
// Konstruktor / Destruktor
// -----------------------------------------------------------------------------

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), time(0.0), placementConfirmed(false) {
  resize(1400, 800);

  QWidget *central = new QWidget(this);
  setCentralWidget(central);

  mainHorizontalLayout = new QHBoxLayout(central);

  // Linke Spalte: Time Series + Controls
  leftColumnLayout = new QVBoxLayout();
  mainHorizontalLayout->addLayout(leftColumnLayout, 8);

  // Rechte Seite: Master-Layout
  QVBoxLayout *rightMasterLayout = new QVBoxLayout();
  mainHorizontalLayout->addLayout(rightMasterLayout, 7);

  // Oben rechts: Mitte (BandPower + Head) & Rechts (Theta/Beta + Fokus)
  QHBoxLayout *topRowLayout = new QHBoxLayout();
  centerColumnLayout = new QVBoxLayout(); // BandPower + Head
  rightColumnLayout = new QVBoxLayout();  // Theta/Beta + Fokus

  topRowLayout->addLayout(centerColumnLayout, 3);
  topRowLayout->addLayout(rightColumnLayout, 2);

  rightMasterLayout->addLayout(topRowLayout, 4);

  // Unten rechts: FFT-Plot
  fftPlot = new QCustomPlot(this);
  fftPlot->xAxis->setLabel("Frequency (Hz)");
  fftPlot->yAxis->setLabel("Amplitude (a.u.)");
  fftPlot->xAxis->setRange(0, 100);
  fftPlot->yAxis->setRange(0, 1.0);

  // Graphen vorab anlegen (für Performance)
  QList<QColor> fftColors = {Qt::red,  Qt::green,      Qt::blue,    Qt::magenta,
                             Qt::cyan, Qt::darkYellow, Qt::darkRed, Qt::gray};
  for (int i = 0; i < numChannels; ++i) {
    fftPlot->addGraph();
    fftPlot->graph(i)->setPen(QPen(fftColors.value(i % fftColors.size())));
  }

  // FFT Plot Tools
  auto *fftToolsLayout = new QHBoxLayout();
  fftRangeCombo = new QComboBox(this);
  fftRangeCombo->addItem("Theta-Beta Analysis (30 Hz)", 30.0);
  fftRangeCombo->addItem("Bandlimit Analysis (60 Hz)", 60.0);
  fftRangeCombo->addItem("Full Spectrum (100 Hz)", 100.0);
  fftRangeCombo->setCurrentIndex(2); // Default to 100 Hz

  fftToolsLayout->addWidget(new QLabel("FFT Range:", this));
  fftToolsLayout->addWidget(fftRangeCombo);
  fftToolsLayout->addStretch();

  rightMasterLayout->addLayout(fftToolsLayout);
  rightMasterLayout->addWidget(fftPlot, 2);

  connect(fftRangeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          [this](int index) {
            if (fftPlot) {
              double range = fftRangeCombo->itemData(index).toDouble();
              fftPlot->xAxis->setRange(0, range);
              fftPlot->replot(QCustomPlot::rpQueuedReplot);
            }
          });

  // -------------------------------------------------------------------------
  // Links: EEG-Kanalplots
  // -------------------------------------------------------------------------
  QStringList colors = {"red",  "green", "blue",   "magenta",
                        "cyan", "brown", "orange", "gray"};
  QStringList labels = {"Fp1", "Fp2", "F7", "F8", "Fz", "Pz", "T5", "T6"};

  for (int i = 0; i < numChannels; ++i) {
    QCustomPlot *plot = new QCustomPlot(this);
    channelPlots.append(plot);
    leftColumnLayout->addWidget(plot);

    plot->addGraph();
    plot->graph(0)->setPen(QPen(QColor(colors.value(i))));
    plot->xAxis->setLabel("Time (s)");
    plot->yAxis->setLabel(
        QString("%1 (µV)").arg(labels.value(i, QString("Ch%1").arg(i + 1))));

    plot->xAxis->setRange(0, 3);          // 3s Fenster
    plot->yAxis->setRange(-100.0, 100.0); // Startbereich ±100 µV
    plot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
    plot->axisRect()->setRangeZoom(Qt::Vertical);
  }

  channelPhases.resize(numChannels);
  for (int i = 0; i < numChannels; ++i)
    channelPhases[i] = QRandomGenerator::global()->generateDouble() * 2 * M_PI;

  fftBuffers.resize(numChannels);
  headBuffers.resize(numChannels);

  // -------------------------------------------------------------------------
  // Links unten: Data source + UDP-Port + Start/Stop/Reset
  // -------------------------------------------------------------------------
  modeCombo = new QComboBox(this);
  modeCombo->addItem("Simulated (Filter Testing)");
  modeCombo->addItem("Wireless (UDP)");
  modeCombo->addItem("Bluetooth (BLE)");
  modeCombo->addItem("File (CSV)");

  udpPortSpinBox = new QSpinBox(this);
  udpPortSpinBox->setRange(1, 65535);
  udpPortSpinBox->setValue(12345);
  udpPortSpinBox->setEnabled(false); // nur bei Real aktiv

  auto *sourceLayout = new QHBoxLayout();
  sourceLayout->addWidget(new QLabel("Data source:", this));
  sourceLayout->addWidget(modeCombo);
  sourceLayout->addSpacing(10);
  sourceLayout->addWidget(new QLabel("UDP port:", this));
  sourceLayout->addWidget(udpPortSpinBox);

  recordCheckBox = new QCheckBox("Record (CSV)", this);
  sourceLayout->addSpacing(10);
  sourceLayout->addWidget(recordCheckBox);

  startButton = new QPushButton("Start", this);
  stopButton = new QPushButton("Stop", this);
  resetButton = new QPushButton("Reset", this);

  auto *btnLayout = new QHBoxLayout();
  btnLayout->addWidget(startButton);
  btnLayout->addWidget(stopButton);

  leftColumnLayout->addLayout(sourceLayout);
  leftColumnLayout->addLayout(btnLayout);
  leftColumnLayout->addWidget(resetButton);

  // -------------------------------------------------------------------------
  // Filter-Checkboxen (Highpass, Notch, Bandlimit)
  // -------------------------------------------------------------------------
  QGroupBox *filterGroup = new QGroupBox(tr("Filters"), this);
  QVBoxLayout *filterLayout = new QVBoxLayout(filterGroup);

  hpCheckBox = new QCheckBox(tr("HP 1 Hz"), filterGroup);
  notchCheckBox = new QCheckBox(tr("Notch 50 Hz"), filterGroup);
  bpCheckBox = new QCheckBox(tr("Bandlimit 1–50 Hz"), filterGroup);

  hpCheckBox->setChecked(true);
  notchCheckBox->setChecked(true);
  bpCheckBox->setChecked(true);

  filterLayout->addWidget(hpCheckBox);
  filterLayout->addWidget(notchCheckBox);
  filterLayout->addWidget(bpCheckBox);

  leftColumnLayout->addWidget(filterGroup);

  // -------------------------------------------------------------------------
  // Mitte: BandPower (oben) + Head Plot (darunter)
  // -------------------------------------------------------------------------

  // Band Power
  bandPowerPlot = new QCustomPlot(this);
  bandPowerPlot->setMinimumHeight(220);

  bandPowerPlot->yAxis->setLabel("Relative power");
  bandPowerPlot->xAxis->setLabel("EEG Power Bands");
  bandPowerPlot->yAxis->setRange(0, 1.2);

  bandPowerTicks.clear();
  bandPowerTicks << 1 << 2 << 3 << 4 << 5;

  {
    QSharedPointer<QCPAxisTickerText> bpTicker(new QCPAxisTickerText);
    bpTicker->addTick(1, "DELTA\n0.5-4Hz");
    bpTicker->addTick(2, "THETA\n4-8Hz");
    bpTicker->addTick(3, "ALPHA\n8-13Hz");
    bpTicker->addTick(4, "BETA\n13-32Hz");
    bpTicker->addTick(5, "GAMMA\n32-100Hz");
    bandPowerPlot->xAxis->setTicker(bpTicker);
    bandPowerPlot->xAxis->setSubTicks(false);
    bandPowerPlot->xAxis->setTickLength(0, 4);
    bandPowerPlot->xAxis->setLabelFont(QFont("sans serif", 10, QFont::Bold));
    bandPowerPlot->xAxis->setTickLabelFont(QFont("sans serif", 8));
  }

  // OpenBCI Colors (approximate from image)
  QList<QColor> bpColors = {
      QColor(89, 112, 163),  // Delta: Bluish
      QColor(162, 133, 177), // Theta: Purple
      QColor(139, 161, 146), // Alpha: Greenish-grey
      QColor(223, 199, 118), // Beta: Yellowish
      QColor(216, 143, 131)  // Gamma: Reddish
  };

  bandPowerBarsList.clear();
  for (int i = 0; i < 5; ++i) {
    QCPBars *bars = new QCPBars(bandPowerPlot->xAxis, bandPowerPlot->yAxis);
    bars->setWidth(0.7);
    bars->setPen(Qt::NoPen);
    bars->setBrush(QBrush(bpColors.value(i)));

    QVector<double> tick{bandPowerTicks[i]};
    QVector<double> val{0.0};
    bars->setData(tick, val);

    bandPowerBarsList.append(bars);
  }
  bandPowerPlot->xAxis->setRange(0.5, 5.5);

  centerColumnLayout->addWidget(bandPowerPlot);

  // Head Plot
  electrodePlacementScene = new ElectrodeMap(this);

  electrodePlacementView = new ZoomableGraphicsView(this);
  electrodePlacementView->setScene(electrodePlacementScene);
  electrodePlacementView->setRenderHint(QPainter::Antialiasing, true);
  electrodePlacementView->setMinimumHeight(300);
  electrodePlacementView->setMinimumWidth(300);
  electrodePlacementView->setStyleSheet(
      "background-color: white; border: none;");

  centerColumnLayout->addWidget(electrodePlacementView, 0, Qt::AlignCenter);

  // -------------------------------------------------------------------------
  // Rechts: Theta/Beta-Balkendiagramm + Fokus-Ampel
  // -------------------------------------------------------------------------
  thetaBetaBarPlot = new QCustomPlot(this);
  thetaBetaBarPlot->setMinimumHeight(180);

  thetaBetaBarPlot->yAxis->setLabel("Normalized amplitude");
  thetaBetaBarPlot->xAxis->setRange(0, 3);
  thetaBetaBarPlot->yAxis->setRange(0, 1.5);

  QVector<double> tbTicks;
  tbTicks << 1 << 2;
  QSharedPointer<QCPAxisTickerText> textTicker(new QCPAxisTickerText);
  textTicker->addTick(1, "Theta");
  textTicker->addTick(2, "Beta");
  thetaBetaBarPlot->xAxis->setTicker(textTicker);
  thetaBetaBarPlot->xAxis->setSubTicks(false);
  thetaBetaBarPlot->xAxis->setTickLength(0, 4);

  thetaBetaBars = new QCPBars(thetaBetaBarPlot->xAxis, thetaBetaBarPlot->yAxis);
  thetaBetaBars->setWidth(0.4);

  rightColumnLayout->addWidget(thetaBetaBarPlot);

  // Fokus-Ampel
  focusIndicator = new QLabel(this);
  focusIndicator->setAlignment(Qt::AlignCenter);
  focusIndicator->setText("Focus");
  focusIndicator->setMinimumSize(120, 120);
  focusIndicator->setMaximumSize(160, 160);
  focusIndicator->setStyleSheet("background-color: green;"
                                "border-radius: 50px;"
                                "min-width: 100px; min-height: 100px;");

  rightColumnLayout->addWidget(focusIndicator, 0, Qt::AlignHCenter);

  // -------------------------------------------------------------------------
  // Datenquelle + DataProcessingQt
  // -------------------------------------------------------------------------
  auto connectDataSource = [this](AbstractDataSource *src) {
    if (!src)
      return;

    if (dataSource) {
      dataSource->stop();
      dataSource->disconnect(this);
      dataSource->deleteLater();
      dataSource = nullptr;
    }

    dataSource = src;
    currentSampleRate = src->sampleRate();

    if (dataProcessor) {
      delete dataProcessor;
      dataProcessor = nullptr;
    }

    bool hpOn = hpCheckBox ? hpCheckBox->isChecked() : true;
    bool ntOn = notchCheckBox ? notchCheckBox->isChecked() : true;
    bool bpOn = bpCheckBox ? bpCheckBox->isChecked() : true;

    dataProcessor =
        new DataProcessingQt(numChannels, currentSampleRate, hpOn, ntOn, bpOn);

    connect(src, &AbstractDataSource::newEEGData, this,
            &MainWindow::handleNewEEGData);
    connect(
        src, &AbstractDataSource::statusMessage, this,
        [this](const QString &msg) { this->statusBar()->showMessage(msg); });
  };

  // Default: Simulation
  connectDataSource(new DummyDataSource(this));
  modeCombo->setCurrentIndex(0);

  // Checkboxen mit DSP verknüpfen
  connect(hpCheckBox, &QCheckBox::toggled, this, [this](bool on) {
    if (dataProcessor)
      dataProcessor->setEnableHighpass(on);
  });
  connect(notchCheckBox, &QCheckBox::toggled, this, [this](bool on) {
    if (dataProcessor)
      dataProcessor->setEnableNotch(on);
  });
  connect(bpCheckBox, &QCheckBox::toggled, this, [this](bool on) {
    if (dataProcessor)
      dataProcessor->setEnableBandpass(on);
  });

  // Source-Wechsel
  connect(
      modeCombo,
      static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
      this, [=](int index) {
        if (index == 0) {
          connectDataSource(new DummyDataSource(this));
          udpPortSpinBox->setEnabled(false);
        } else if (index == 1) {
          auto *real = new RealDataSource(this);
          real->setUdpPort(static_cast<quint16>(udpPortSpinBox->value()));
          connectDataSource(real);
          udpPortSpinBox->setEnabled(true);

          connect(real, &RealDataSource::udpError, this,
                  [this](const QString &msg) {
                    QMessageBox::warning(this, tr("UDP error"),
                                         msg + "\nMeasurement stopped.");
                  });
        } else if (index == 2) {
          // BLE
          auto *ble = new BleDataSource(this);
          connectDataSource(ble);
          udpPortSpinBox->setEnabled(false);
          connect(ble, &BleDataSource::statusMessage, this,
                  [this](const QString &msg) {
                    this->statusBar()->showMessage(msg);
                    qDebug() << "[BLE Status]" << msg;
                  });

          connect(ble, &BleDataSource::criticalError, this,
                  [this](const QString &title, const QString &msg) {
                    QMessageBox::critical(this, title, msg);
                    this->modeCombo->setCurrentIndex(
                        0); // Switch back to simulated
                  });
        } else if (index == 3) {
          QString fileName = QFileDialog::getOpenFileName(
              this, tr("Open OpenBCI file"), QString(),
              tr("OpenBCI Files (*.txt *.csv);;All Files (*.*)"));
          if (fileName.isEmpty()) {
            modeCombo->setCurrentIndex(0);
            connectDataSource(new DummyDataSource(this));
            udpPortSpinBox->setEnabled(false);
            return;
          }
          auto *fileDs = new FileDataSource(fileName, this);
          connectDataSource(fileDs);
          fileDs->reportInitStatus(); // Re-emit the status now that we are
                                      // connected

          udpPortSpinBox->setEnabled(false);
        }

        resetPlots();
      });

  // Start / Stop
  connect(startButton, &QPushButton::clicked, this, [=]() {
    if (!dataSource)
      return;

    if (!placementConfirmed) {
      auto reply = QMessageBox::question(
          this, tr("Check electrodes"),
          tr("Please verify electrode placement and contact.\n"
             "Start measurement now?"),
          QMessageBox::Yes | QMessageBox::Cancel);
      if (reply == QMessageBox::Yes)
        placementConfirmed = true;
      else
        return;
    }

    if (auto *real = qobject_cast<RealDataSource *>(dataSource)) {
      real->setUdpPort(static_cast<quint16>(udpPortSpinBox->value()));
    }

    if (recordCheckBox->isChecked()) {
      if (!startRecording()) {
        return; // Abort if user cancelled file selection
      }
    }

    dataSource->start();
  });

  connect(stopButton, &QPushButton::clicked, this, [=]() {
    if (dataSource) {
      dataSource->stop();
    }
    stopRecording();
  });

  connect(resetButton, &QPushButton::clicked, this, &MainWindow::resetPlots);

  updateElectrodePlacement();
  updateThetaBetaBars();
  updateBandPowerPlot(BandPower{0, 0, 0, 0, 0});
  updateFocusIndicator(0.0);
}

MainWindow::~MainWindow() {
  if (dataProcessor) {
    delete dataProcessor;
    dataProcessor = nullptr;
  }
}

// -----------------------------------------------------------------------------
// Daten-Callback
// -----------------------------------------------------------------------------

void MainWindow::handleNewEEGData(const QVector<double> &values) {
  if (values.isEmpty())
    return;

  const double dt =
      (currentSampleRate > 0.0) ? (1.0 / currentSampleRate) : 0.02;

  const double windowSec = 3.0;

  // Plot-Updates drosseln (~30 Hz Redraw)
  static double accumPlots = 0.0;
  accumPlots += dt;
  bool doPlotUpdate = (accumPlots >= 1.0 / 30.0);

  // ---- erst filtern (Highpass + Notch + Bandlimit) ----
  QVector<double> filtered = values;
  if (dataProcessor) {
    for (int ch = 0; ch < numChannels && ch < filtered.size(); ++ch) {
      filtered[ch] = dataProcessor->processSample(ch, filtered[ch]);
    }
  }

  // ---- Time-Series Plots mit gefilterten Daten ----
  for (int i = 0; i < numChannels && i < filtered.size(); ++i) {
    QCustomPlot *plot = channelPlots[i];
    if (!plot || plot->graphCount() == 0)
      continue;

    plot->graph(0)->addData(time, filtered[i]);
    plot->graph(0)->data()->removeBefore(time - windowSec);

    if (doPlotUpdate) {
      plot->xAxis->setRange(time - windowSec, time);
      plot->graph(0)->rescaleValueAxis(false, true);
      plot->replot(QCustomPlot::rpQueuedReplot);
    }
  }
  if (doPlotUpdate) {
    accumPlots = 0.0;
  }

  // ---- Bandpower-Buffer (Average of all channels for Global Field Power) ----
  if (!filtered.isEmpty()) {
    double sum = 0.0;
    for (double v : filtered) {
      sum += v;
    }
    double avg = sum / double(filtered.size());
    bandPowerBuffer.append(avg);

    int maxSamples = int(currentSampleRate * windowSec);
    // Erst aufräumen, wenn deutlich zu groß (Amortisierung)
    if (bandPowerBuffer.size() > maxSamples * 1.5) {
      bandPowerBuffer.remove(0, bandPowerBuffer.size() - maxSamples);
    }
  }

  // ---- FFT-Buffer & Head-Buffer für alle Kanäle ----
  int maxSamples = int(currentSampleRate * windowSec);
  int headMaxSamples = int(currentSampleRate * 2.0);

  for (int ch = 0; ch < numChannels && ch < filtered.size(); ++ch) {
    // FFT
    fftBuffers[ch].append(filtered[ch]);
    if (fftBuffers[ch].size() > maxSamples * 1.5) {
      fftBuffers[ch].remove(0, fftBuffers[ch].size() - maxSamples);
    }

    // Head-Plot RMS
    headBuffers[ch].append(filtered[ch]);
    if (headBuffers[ch].size() > headMaxSamples * 1.5) {
      headBuffers[ch].remove(0, headBuffers[ch].size() - headMaxSamples);
    }
  }

  static double accumBP = 0.0;
  static double accumFft = 0.0;
  static double accumHead = 0.0;

  accumBP += dt;
  accumFft += dt;
  accumHead += dt;

  // Bandpower + Theta/Beta: 1x pro Sekunde
  if (accumBP > 1.0 && bandPowerBuffer.size() >= maxSamples) {
    BandPower bp = computeBandPower(bandPowerBuffer, currentSampleRate);
    updateBandPowerPlot(bp);
    updateThetaBetaBarsFromBandPower(bp);
    accumBP = 0.0;
  }

  // FFT: 1x pro Sekunde
  bool fftReady = true;
  for (int ch = 0; ch < numChannels; ++ch) {
    if (fftBuffers[ch].size() < maxSamples) {
      fftReady = false;
      break;
    }
  }
  if (accumFft > 1.0 && fftReady) {
    updateFftPlot();
    accumFft = 0.0;
  }

  // Head-Plot: 2x pro Sekunde
  if (accumHead > 0.5) {
    updateElectrodePlacement();
    accumHead = 0.0;
  }

  time += dt;

  if (isRecording) {
    // Write data to CSV: Index, Ch1, Ch2, ...
    for (int i = 0; i < values.size() / numChannels; ++i) {
      // Handle multi-sample packets if necessary, though handleNewEEGData
      // usually gets one sample vector of size numChannels.
      // BleDataSource sends 8 values. Dummy sends 8.

      recordingStream << recordingIndex++ << ",";
      for (int ch = 0; ch < numChannels; ++ch) {
        recordingStream << values[ch];
        if (ch < numChannels - 1)
          recordingStream << ",";
      }
      recordingStream << "\n";
    }
  }
}

bool MainWindow::startRecording() {
  QString docPath =
      QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
  QString userFile = QFileDialog::getSaveFileName(
      this, tr("Save EEG Recording"),
      docPath + "/NeuroEase_Recordings/EEG_Record.csv",
      tr("CSV Files (*.csv)"));

  if (userFile.isEmpty())
    return false;

  recordingFile.setFileName(userFile);
  if (recordingFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
    recordingStream.setDevice(&recordingFile);

    // Write Header
    recordingStream << "% Format = NeuroEaseCSV\n";
    recordingStream << "% Sample Rate = " << currentSampleRate << "\n";
    recordingStream << "% Created by NeuroEase GUI\n";
    recordingStream << "% File Path = " << userFile << "\n";
    recordingStream
        << "Index, Fp1, Fp2, F7, F8, Fz, Pz, T5, T6\n"; // Hardcoded labels for
                                                        // now

    isRecording = true;
    recordingIndex = 0;
    statusBar()->showMessage("Recording to " + userFile);
    return true;
  } else {
    QMessageBox::warning(this, "Recording Error",
                         "Could not create file: " + userFile);
    recordCheckBox->setChecked(false);
    return false;
  }
}

void MainWindow::stopRecording() {
  if (isRecording) {
    if (recordingFile.isOpen()) {
      recordingFile.close();
    }
    isRecording = false;
    statusBar()->showMessage("Recording saved.");
  }
}

// -----------------------------------------------------------------------------
// Reset
// -----------------------------------------------------------------------------

void MainWindow::resetPlots() {
  time = 0.0;

  for (auto *plot : channelPlots) {
    if (!plot || plot->graphCount() == 0)
      continue;
    plot->graph(0)->data()->clear();
    plot->xAxis->setRange(0, 3);
    plot->replot();
  }

  placementConfirmed = false;
  bandPowerBuffer.clear();
  for (auto &buf : fftBuffers)
    buf.clear();
  for (auto &buf : headBuffers)
    buf.clear();

  if (dataProcessor)
    dataProcessor->reset();

  updateElectrodePlacement();
  updateThetaBetaBars();
  updateBandPowerPlot(BandPower{0, 0, 0, 0, 0});
  updateFftPlot();
  updateFocusIndicator(0.0);
}

// -----------------------------------------------------------------------------
// Elektroden-Heatmap (RMS-Aktivität aus headBuffers)
// -----------------------------------------------------------------------------

void MainWindow::updateElectrodePlacement() {
  if (!electrodePlacementScene || headBuffers.isEmpty())
    return;

  // Aktivitäten extrahieren
  QVector<double> activities(numChannels, 0.0);
  double maxAct = 0.0;
  for (int ch = 0; ch < numChannels; ++ch) {
    const auto &buf = headBuffers[ch];
    if (buf.isEmpty())
      continue;
    double sumSq = 0.0;
    for (double v : buf)
      sumSq += v * v;
    double rms = std::sqrt(sumSq / double(buf.size()));
    activities[ch] = rms;
    if (rms > maxAct)
      maxAct = rms;
  }

  // Normalisierung
  if (maxAct > 0.0) {
    for (double &a : activities)
      a /= maxAct;
  }

  // Nutze die spezialisierte Klasse für das Zeichnen
  electrodePlacementScene->setActivities(activities);
}

// -----------------------------------------------------------------------------
// Theta/Beta Balken – Default nach Reset
// -----------------------------------------------------------------------------

void MainWindow::updateThetaBetaBars() {
  double t = 0.6;
  double b = 0.8;

  QVector<double> ticks{1.0, 2.0};
  QVector<double> vals{t, b};

  thetaBetaBars->setData(ticks, vals);
  thetaBetaBarPlot->replot(QCustomPlot::rpQueuedReplot);

  double ratio = (b > 1e-6) ? t / b : 0.0;
  updateFocusIndicator(ratio);
}

// -----------------------------------------------------------------------------
// Theta/Beta Balken aus Bandpower
// -----------------------------------------------------------------------------

void MainWindow::updateThetaBetaBarsFromBandPower(const BandPower &bp) {
  double theta = bp.theta;
  double beta = bp.beta;

  if (theta <= 0.0 && beta <= 0.0) {
    QVector<double> ticks{1.0, 2.0};
    QVector<double> vals{0.0, 0.0};
    thetaBetaBars->setData(ticks, vals);
    thetaBetaBarPlot->replot(QCustomPlot::rpQueuedReplot);
    updateFocusIndicator(0.0);
    return;
  }

  // Relativ normalisieren (Summe = 1)
  double sum = theta + beta;
  double tNorm = theta / sum;
  double bNorm = beta / sum;

  QVector<double> ticks{1.0, 2.0};
  QVector<double> vals{tNorm, bNorm};

  thetaBetaBars->setData(ticks, vals);
  thetaBetaBarPlot->yAxis->setRange(0, 1.2);
  thetaBetaBarPlot->replot(QCustomPlot::rpQueuedReplot);

  // Fokus: echtes Theta/Beta Verhältnis
  double ratio = (beta > 1e-9) ? theta / beta : 0.0;
  updateFocusIndicator(ratio);
}

// -----------------------------------------------------------------------------
// Fokus-Ampel
// -----------------------------------------------------------------------------

void MainWindow::updateFocusIndicator(double ratio) {
  QString style;
  if (ratio < 2.0)
    style = "background-color:green;";
  else if (ratio < 3.0)
    style = "background-color:yellow;";
  else
    style = "background-color:red;";

  style += "border-radius:50px;min-width:100px;min-height:100px;";
  focusIndicator->setStyleSheet(style);
}

// -----------------------------------------------------------------------------
// Magnitude-Spektrum (für BandPower & FFT)
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// FFT-Algorithmus (Radix-2)
// -----------------------------------------------------------------------------

void MainWindow::fft(QVector<std::complex<double>> &a, bool invert) {
  int n = a.size();
  for (int i = 1, j = 0; i < n; i++) {
    int bit = n >> 1;
    for (; j & bit; bit >>= 1)
      j ^= bit;
    j ^= bit;
    if (i < j)
      std::swap(a[i], a[j]);
  }
  for (int len = 2; len <= n; len <<= 1) {
    double ang = 2 * M_PI / len * (invert ? -1 : 1);
    std::complex<double> wlen(std::cos(ang), std::sin(ang));
    for (int i = 0; i < n; i += len) {
      std::complex<double> w(1);
      for (int j = 0; j < len / 2; j++) {
        std::complex<double> u = a[i + j], v = a[i + j + len / 2] * w;
        a[i + j] = u + v;
        a[i + j + len / 2] = u - v;
        w *= wlen;
      }
    }
  }
  if (invert) {
    for (auto &x : a)
      x /= n;
  }
}

QVector<double>
MainWindow::computeMagnitudeSpectrum(const QVector<double> &signal,
                                     double sampleRate) {
  int Ntotal = signal.size();
  if (Ntotal < 32 || sampleRate <= 0.0)
    return {};

  // FFT braucht Power-of-2.
  const int N = 1024;
  QVector<std::complex<double>> fa(N);
  int start = std::max(0, Ntotal - N);
  int actualN = std::min(Ntotal, N);

  // DC-Entfernung & Fensterung
  double mean = 0.0;
  for (int n = 0; n < actualN; ++n)
    mean += signal[start + n];
  mean /= double(actualN);

  for (int i = 0; i < N; i++) {
    if (i < actualN) {
      double w = 0.5 * (1.0 - std::cos(2.0 * M_PI * i / (actualN - 1)));
      fa[i] = std::complex<double>((signal[start + i] - mean) * w, 0);
    } else {
      fa[i] = std::complex<double>(0, 0);
    }
  }

  fft(fa, false);

  int K = N / 2;
  QVector<double> magSpec(K);
  for (int k = 0; k < K; ++k) {
    magSpec[k] = std::abs(fa[k]) / double(N);
  }

  return magSpec;
}

// -----------------------------------------------------------------------------
// Bandpower-Berechnung (nutzt Magnitude-Spektrum)
// -----------------------------------------------------------------------------

MainWindow::BandPower
MainWindow::computeBandPower(const QVector<double> &signal, double sampleRate) {
  BandPower bp{0, 0, 0, 0, 0};

  int N = signal.size();
  if (N < 32 || sampleRate <= 0.0)
    return bp;

  QVector<double> mag = computeMagnitudeSpectrum(signal, sampleRate);
  if (mag.isEmpty())
    return bp;

  int K = mag.size();
  double hzPerBin = sampleRate / double(2 * K);

  auto band = [&](double fLow, double fHigh) {
    double sum = 0.0;
    int iLow = int(std::floor(fLow / hzPerBin));
    int iHigh = int(std::ceil(fHigh / hzPerBin));
    iLow = std::max(iLow, 0);
    iHigh = std::min(iHigh, K - 1);
    for (int i = iLow; i <= iHigh; ++i) {
      double a = mag[i];
      sum += a * a; // Power ~ Amplitude^2
    }
    return sum;
  };

  bp.delta = band(0.5, 4.0);
  bp.theta = band(4.0, 8.0);
  bp.alpha = band(8.0, 13.0);
  bp.beta = band(13.0, 32.0);
  bp.gamma = band(32.0, 100.0);

  return bp;
}

// -----------------------------------------------------------------------------
// Bandpower-Plot updaten (relative Darstellung)
// -----------------------------------------------------------------------------

void MainWindow::updateBandPowerPlot(const BandPower &bp) {
  if (!bandPowerPlot || bandPowerBarsList.size() < 5)
    return;

  QVector<double> vals;
  vals << bp.delta << bp.theta << bp.alpha << bp.beta << bp.gamma;

  double total = 0.0;
  for (double v : vals)
    total += v;

  if (total > 0.0) {
    for (double &v : vals)
      v /= total; // Summe = 1 -> relative power
  }

  for (int i = 0; i < 5; ++i) {
    QVector<double> tick{bandPowerTicks[i]};
    QVector<double> val{vals[i]};
    bandPowerBarsList[i]->setData(tick, val);
  }

  bandPowerPlot->yAxis->setRange(0, 1.2);
  bandPowerPlot->replot(QCustomPlot::rpQueuedReplot);
}

// -----------------------------------------------------------------------------
// FFT-Plot updaten (8 Kanäle)
// -----------------------------------------------------------------------------

void MainWindow::updateFftPlot() {
  if (!fftPlot || currentSampleRate <= 0.0)
    return;

  if (fftBuffers.isEmpty())
    return;

  int N = fftBuffers[0].size();
  if (N < 32)
    return;

  // Frequenzachse nach FFT-Größe (fest 1024)
  const int FFT_N = 1024;
  int K = FFT_N / 2;
  double hzPerBin = currentSampleRate / double(FFT_N);

  QVector<double> freqs(K);
  for (int k = 0; k < K; ++k)
    freqs[k] = k * hzPerBin;

  QList<QColor> colors = {Qt::red,  Qt::green,      Qt::blue,    Qt::magenta,
                          Qt::cyan, Qt::darkYellow, Qt::darkRed, Qt::gray};

  double globalMax = 0.0;

  for (int ch = 0; ch < numChannels; ++ch) {
    const auto &buf = fftBuffers[ch];
    if (buf.size() < N)
      continue;

    QVector<double> spec = computeMagnitudeSpectrum(buf, currentSampleRate);
    if (spec.isEmpty())
      continue;

    int len = std::min(K, int(spec.size()));

    QVector<double> f(len), a(len);
    for (int i = 0; i < len; ++i) {
      f[i] = freqs[i];
      a[i] = spec[i];
      if (a[i] > globalMax)
        globalMax = a[i];
    }

    if (fftPlot->graphCount() > ch) {
      fftPlot->graph(ch)->setData(f, a);
    }
  }

  if (globalMax <= 0.0)
    globalMax = 1.0;

  double xRange = 100.0;
  if (fftRangeCombo) {
    xRange = fftRangeCombo->currentData().toDouble();
  }

  fftPlot->xAxis->setRange(0, xRange);
  fftPlot->yAxis->setRange(0, globalMax * 1.2);

  fftPlot->replot(QCustomPlot::rpQueuedReplot);
}
