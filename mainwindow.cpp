#include "mainwindow.h"

#include "qcustomplot.h"
#include "zoomablegraphicsview.h"
#include "AbstractDataSource.h"
#include "DummyDataSource.h"
#include "RealDataSource.h"
#include "FileDataSource.h"
#include "DataProcessingQt.h"

#include <QWidget>
#include <QComboBox>
#include <QSpinBox>
#include <QGraphicsScene>
#include <QPainterPath>
#include <QRandomGenerator>
#include <QMessageBox>
#include <QFileDialog>
#include <QPen>
#include <QBrush>
#include <QFont>
#include <QGroupBox>
#include <QCheckBox>
#include <QtMath>
#include <cmath>
#include <algorithm>

// -----------------------------------------------------------------------------
// Konstruktor / Destruktor
// -----------------------------------------------------------------------------

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , time(0.0)
    , placementConfirmed(false)
{
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
    rightColumnLayout  = new QVBoxLayout(); // Theta/Beta + Fokus

    topRowLayout->addLayout(centerColumnLayout, 3);
    topRowLayout->addLayout(rightColumnLayout, 2);

    rightMasterLayout->addLayout(topRowLayout, 4);

    // Unten rechts: FFT-Plot
    fftPlot = new QCustomPlot(this);
    fftPlot->xAxis->setLabel("Frequency (Hz)");
    fftPlot->yAxis->setLabel("Amplitude (a.u.)");
    fftPlot->xAxis->setRange(0, 45);
    fftPlot->yAxis->setRange(0, 1.0);
    rightMasterLayout->addWidget(fftPlot, 2);

    // -------------------------------------------------------------------------
    // Links: EEG-Kanalplots
    // -------------------------------------------------------------------------
    QStringList colors = { "red","green","blue","magenta","cyan","brown","orange","gray" };
    QStringList labels = { "Fp1","Fp2","F7","F8","Fz","Pz","T5","T6" };

    for (int i = 0; i < numChannels; ++i) {
        QCustomPlot *plot = new QCustomPlot(this);
        channelPlots.append(plot);
        leftColumnLayout->addWidget(plot);

        plot->addGraph();
        plot->graph(0)->setPen(QPen(QColor(colors.value(i))));
        plot->xAxis->setLabel("Time (s)");
        plot->yAxis->setLabel(QString("%1 (a.u.)").arg(labels.value(i, QString("Ch%1").arg(i+1))));

        plot->xAxis->setRange(0, 3);      // 3s Fenster
        plot->yAxis->setRange(-1.0, 1.0); // Startbereich
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
    modeCombo->addItem("Simulated");
    modeCombo->addItem("Real (UDP)");
    modeCombo->addItem("File (OpenBCI)");

    udpPortSpinBox = new QSpinBox(this);
    udpPortSpinBox->setRange(1, 65535);
    udpPortSpinBox->setValue(12345);
    udpPortSpinBox->setEnabled(false);  // nur bei Real aktiv

    auto *sourceLayout = new QHBoxLayout();
    sourceLayout->addWidget(new QLabel("Data source:", this));
    sourceLayout->addWidget(modeCombo);
    sourceLayout->addSpacing(10);
    sourceLayout->addWidget(new QLabel("UDP port:", this));
    sourceLayout->addWidget(udpPortSpinBox);

    startButton = new QPushButton("Start", this);
    stopButton  = new QPushButton("Stop",  this);
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

    hpCheckBox    = new QCheckBox(tr("HP 1 Hz"), filterGroup);
    notchCheckBox = new QCheckBox(tr("Notch 50 Hz"), filterGroup);
    bpCheckBox    = new QCheckBox(tr("Bandlimit 1–50 Hz"), filterGroup);

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
        bpTicker->addTick(1, "DELTA");
        bpTicker->addTick(2, "THETA");
        bpTicker->addTick(3, "ALPHA");
        bpTicker->addTick(4, "BETA");
        bpTicker->addTick(5, "GAMMA");
        bandPowerPlot->xAxis->setTicker(bpTicker);
        bandPowerPlot->xAxis->setSubTicks(false);
        bandPowerPlot->xAxis->setTickLength(0, 4);
    }

    bandPowerBars = new QCPBars(bandPowerPlot->xAxis, bandPowerPlot->yAxis);
    bandPowerBars->setWidth(0.6);
    {
        QVector<double> initVals{0,0,0,0,0};
        bandPowerBars->setData(bandPowerTicks, initVals);
    }

    centerColumnLayout->addWidget(bandPowerPlot);

    // Head Plot
    electrodePlacementScene = new QGraphicsScene(this);
    electrodePlacementScene->setSceneRect(-150, -150, 300, 300);

    electrodePlacementView = new ZoomableGraphicsView(this);
    electrodePlacementView->setScene(electrodePlacementScene);
    electrodePlacementView->setRenderHint(QPainter::Antialiasing, true);
    electrodePlacementView->setMinimumHeight(300);
    electrodePlacementView->setMinimumWidth(300);

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
    focusIndicator->setStyleSheet(
        "background-color: green;"
        "border-radius: 50px;"
        "min-width: 100px; min-height: 100px;"
        );

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
        updatePlotDecimation();
        plotSampleCounter = 0;

        if (dataProcessor) {
            delete dataProcessor;
            dataProcessor = nullptr;
        }

        bool hpOn = hpCheckBox    ? hpCheckBox->isChecked()    : true;
        bool ntOn = notchCheckBox ? notchCheckBox->isChecked() : true;
        bool bpOn = bpCheckBox    ? bpCheckBox->isChecked()    : true;

        dataProcessor = new DataProcessingQt(numChannels,
                                             currentSampleRate,
                                             hpOn,
                                             ntOn,
                                             bpOn);

        connect(src, &AbstractDataSource::newEEGData,
                this, &MainWindow::handleNewEEGData);
    };

    // Default: Simulation
    connectDataSource(new DummyDataSource(this));
    modeCombo->setCurrentIndex(0);

    // Checkboxen mit DSP verknüpfen
    connect(hpCheckBox, &QCheckBox::toggled, this, [this](bool on){
        if (dataProcessor) dataProcessor->setEnableHighpass(on);
    });
    connect(notchCheckBox, &QCheckBox::toggled, this, [this](bool on){
        if (dataProcessor) dataProcessor->setEnableNotch(on);
    });
    connect(bpCheckBox, &QCheckBox::toggled, this, [this](bool on){
        if (dataProcessor) dataProcessor->setEnableBandpass(on);
    });

    // Source-Wechsel
    connect(modeCombo,
            static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this,
            [=](int index)
            {
                if (index == 0) {
                    connectDataSource(new DummyDataSource(this));
                    udpPortSpinBox->setEnabled(false);
                } else if (index == 1) {
                    auto *real = new RealDataSource(this);
                    real->setUdpPort(static_cast<quint16>(udpPortSpinBox->value()));
                    connectDataSource(real);
                    udpPortSpinBox->setEnabled(true);

                    connect(real, &RealDataSource::udpError,
                            this, [this](const QString &msg) {
                                QMessageBox::warning(this,
                                                     tr("UDP error"),
                                                     msg + "\nMeasurement stopped.");
                                if (dataSource)
                                    dataSource->stop();
                            });
                } else if (index == 2) {
                    QString fileName = QFileDialog::getOpenFileName(
                        this,
                        tr("Open OpenBCI file"),
                        QString(),
                        tr("OpenBCI Files (*.txt *.csv);;All Files (*.*)")
                        );
                    if (fileName.isEmpty()) {
                        modeCombo->setCurrentIndex(0);
                        connectDataSource(new DummyDataSource(this));
                        udpPortSpinBox->setEnabled(false);
                        return;
                    }
                    connectDataSource(new FileDataSource(fileName, this));
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
                QMessageBox::Yes | QMessageBox::Cancel
                );
            if (reply == QMessageBox::Yes)
                placementConfirmed = true;
            else
                return;
        }

        if (auto *real = qobject_cast<RealDataSource*>(dataSource)) {
            real->setUdpPort(static_cast<quint16>(udpPortSpinBox->value()));
        }

        dataSource->start();
    });

    connect(stopButton, &QPushButton::clicked, this, [=]() {
        if (dataSource)
            dataSource->stop();
    });

    connect(resetButton, &QPushButton::clicked, this, &MainWindow::resetPlots);

    updateElectrodePlacement();
    updateThetaBetaBars();
    updateBandPowerPlot(BandPower{0,0,0,0,0});
    updateFocusIndicator(0.0);
}

MainWindow::~MainWindow()
{
    if (dataProcessor) {
        delete dataProcessor;
        dataProcessor = nullptr;
    }
}

// -----------------------------------------------------------------------------
// Daten-Callback
// -----------------------------------------------------------------------------

void MainWindow::handleNewEEGData(const QVector<double> &values)
{
    if (values.isEmpty())
        return;

    const double dt =
        (currentSampleRate > 0.0) ? (1.0 / currentSampleRate) : 0.02;

    ++plotSampleCounter;
    bool allowPlotSample = false;
    if (plotSampleCounter >= plotDownsampleFactor) {
        allowPlotSample = true;
        plotSampleCounter = 0;
    }

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

        if (allowPlotSample) {
            plot->graph(0)->addData(time, filtered[i]);
            plot->graph(0)->data()->removeBefore(time - windowSec);

            if (doPlotUpdate) {
                plot->xAxis->setRange(time - windowSec, time);
                plot->graph(0)->rescaleValueAxis(false, true);
                plot->replot(QCustomPlot::rpQueuedReplot);
            }
        }
    }
    if (doPlotUpdate) {
        accumPlots = 0.0;
    }

    // ---- Bandpower-Buffer (Kanal 0 / Fp1) ----
    if (!filtered.isEmpty()) {
        bandPowerBuffer.append(filtered[0]);
        int maxSamples = int(currentSampleRate * windowSec);
        if (bandPowerBuffer.size() > maxSamples) {
            bandPowerBuffer.remove(0, bandPowerBuffer.size() - maxSamples);
        }
    }

    // ---- FFT-Buffer & Head-Buffer für alle Kanäle ----
    int maxSamples = int(currentSampleRate * windowSec);
    int headMaxSamples = int(currentSampleRate * 2.0);   // 2 Sekunden Fenster für Head-Plot

    for (int ch = 0; ch < numChannels && ch < filtered.size(); ++ch) {
        // FFT
        fftBuffers[ch].append(filtered[ch]);
        if (fftBuffers[ch].size() > maxSamples) {
            fftBuffers[ch].remove(0, fftBuffers[ch].size() - maxSamples);
        }

        // Head-Plot RMS
        headBuffers[ch].append(filtered[ch]);
        if (headBuffers[ch].size() > headMaxSamples) {
            headBuffers[ch].remove(0, headBuffers[ch].size() - headMaxSamples);
        }
    }

    static double accumBP   = 0.0;
    static double accumFft  = 0.0;
    static double accumHead = 0.0;

    accumBP   += dt;
    accumFft  += dt;
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
}

// -----------------------------------------------------------------------------
// Reset
// -----------------------------------------------------------------------------

void MainWindow::resetPlots()
{
    time = 0.0;
    plotSampleCounter = 0;

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
    updateBandPowerPlot(BandPower{0,0,0,0,0});
    updateFftPlot();
    updateFocusIndicator(0.0);
}

// -----------------------------------------------------------------------------
// Elektroden-Heatmap (RMS-Aktivität aus headBuffers)
// -----------------------------------------------------------------------------

void MainWindow::updateElectrodePlacement()
{
    if (!electrodePlacementScene)
        return;

    electrodePlacementScene->clear();

    const double headR = 80.0;
    const QPointF c(0, 0);
    QPen headPen(Qt::black, 2);

    // Kopf
    electrodePlacementScene->addEllipse(
        c.x() - headR,
        c.y() - headR * 1.1,
        2.0 * headR,
        2.2 * headR,
        headPen
        );

    // Ohren
    electrodePlacementScene->addEllipse(c.x() - headR - 15, c.y() - 10, 20, 30, headPen);
    electrodePlacementScene->addEllipse(c.x() + headR - 5,  c.y() - 10, 20, 30, headPen);

    // Nase
    QPainterPath nose;
    nose.moveTo(c.x(),          c.y() - headR * 1.1 + 10);
    nose.lineTo(c.x() - 10.0,   c.y() - headR * 1.1 + 25);
    nose.lineTo(c.x() + 10.0,   c.y() - headR * 1.1 + 25);
    nose.closeSubpath();
    electrodePlacementScene->addPath(nose, headPen);

    // Elektroden-Positionen
    QStringList labels = { "Fp1","Fp2","F7","F8","Fz","Pz","T5","T6","Ref" };
    QVector<QPointF> pos = {
        { c.x() - headR * 0.25, c.y() - headR * 0.90 }, // Fp1
        { c.x() + headR * 0.25, c.y() - headR * 0.90 }, // Fp2
        { c.x() - headR * 0.55, c.y() - headR * 0.65 }, // F7
        { c.x() + headR * 0.55, c.y() - headR * 0.65 }, // F8
        { c.x(),                c.y() - headR * 0.25 }, // Fz
        { c.x(),                c.y() + headR * 0.20 }, // Pz
        { c.x() - headR * 0.70, c.y() + headR * 0.45 }, // T5
        { c.x() + headR * 0.70, c.y() + headR * 0.45 }, // T6
        { c.x(),                c.y() }                 // Ref
    };

    // Aktivitäten aus Head-Buffern: RMS der letzten ~2s
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

    // Normierung auf 0..1
    if (maxAct > 0.0) {
        for (double &a : activities)
            a /= maxAct;
    }

    // Falls activities noch zu kurz ist, mit Nullen auffüllen
    while (activities.size() < pos.size())
        activities.append(0.0);

    // Heatmap zeichnen (gröberes Raster für Performance)
    const double step = 8.0;
    for (double x = -150; x <= 150; x += step) {
        for (double y = -150; y <= 150; y += step) {

            // nur innerhalb des Kopf-Ellipsoids zeichnen
            double e = (x * x) / (headR * headR)
                       + (y * y) / ((1.1 * headR) * (1.1 * headR));
            if (e > 1.0)
                continue;

            // Inverse Distance Weighting der Kanalaktivitäten
            double sumW = 0.0;
            double sumA = 0.0;

            for (int i = 0; i < activities.size(); ++i) {
                double dx = x - pos[i].x();
                double dy = y - pos[i].y();
                double d  = std::sqrt(dx * dx + dy * dy) + 0.01;
                double w  = 1.0 / (d * d);
                sumW += w;
                sumA += w * activities[i];
            }

            double val = (sumW > 0.0 ? sumA / sumW : 0.0);
            val = qBound(0.0, val, 1.0);

            int r = int(255 * val);
            int g = int(255 * (1.0 - val));
            QColor col(r, g, 0, 140);

            QRectF rect(x, y, step, step);
            electrodePlacementScene->addRect(rect, Qt::NoPen, QBrush(col));
        }
    }

    // Elektrodenkreise + Text
    QPen   penThin(Qt::black, 1);
    QBrush brushGray(Qt::gray);
    const double eSz = 12.0;

    for (int i = 0; i < pos.size(); ++i) {
        bool isRef = (i == pos.size() - 1);
        QPen   p = isRef ? QPen(Qt::black, 2) : penThin;
        QBrush b = isRef ? QBrush(Qt::white) : brushGray;

        electrodePlacementScene->addEllipse(
            pos[i].x() - eSz / 2.0,
            pos[i].y() - eSz / 2.0,
            eSz, eSz, p, b
            );

        auto *txt = electrodePlacementScene->addSimpleText(labels.value(i));
        txt->setFont(QFont("Arial", isRef ? 7 : 5));
        txt->setPos(pos[i].x() - 10, pos[i].y() - 18);
    }
}

// -----------------------------------------------------------------------------
// Theta/Beta Balken – Default nach Reset
// -----------------------------------------------------------------------------

void MainWindow::updateThetaBetaBars()
{
    double t = 0.6;
    double b = 0.8;

    QVector<double> ticks{1.0, 2.0};
    QVector<double> vals {t,   b  };

    thetaBetaBars->setData(ticks, vals);
    thetaBetaBarPlot->replot(QCustomPlot::rpQueuedReplot);

    double ratio = (b > 1e-6) ? t / b : 0.0;
    updateFocusIndicator(ratio);
}

// -----------------------------------------------------------------------------
// Theta/Beta Balken aus Bandpower
// -----------------------------------------------------------------------------

void MainWindow::updateThetaBetaBarsFromBandPower(const BandPower &bp)
{
    double theta = bp.theta;
    double beta  = bp.beta;

    if (theta <= 0.0 && beta <= 0.0) {
        QVector<double> ticks{1.0, 2.0};
        QVector<double> vals {0.0, 0.0};
        thetaBetaBars->setData(ticks, vals);
        thetaBetaBarPlot->replot(QCustomPlot::rpQueuedReplot);
        updateFocusIndicator(0.0);
        return;
    }

    // Relativ normalisieren (Summe = 1)
    double sum   = theta + beta;
    double tNorm = theta / sum;
    double bNorm = beta  / sum;

    QVector<double> ticks{1.0, 2.0};
    QVector<double> vals {tNorm, bNorm};

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

void MainWindow::updateFocusIndicator(double ratio)
{
    QString style;
    if (ratio < 2.0)      style = "background-color:green;";
    else if (ratio < 3.0) style = "background-color:yellow;";
    else                  style = "background-color:red;";

    style += "border-radius:50px;min-width:100px;min-height:100px;";
    focusIndicator->setStyleSheet(style);
}

// -----------------------------------------------------------------------------
// Magnitude-Spektrum (für BandPower & FFT)
// -----------------------------------------------------------------------------

QVector<double> MainWindow::computeMagnitudeSpectrum(
    const QVector<double> &signal,
    double sampleRate)
{
    int Ntotal = signal.size();
    if (Ntotal < 32 || sampleRate <= 0.0)
        return {};

    // Nur die letzten maxN Samples verwenden (z.B. 512)
    const int maxN = 512;
    int N     = qMin(Ntotal, maxN);
    int start = Ntotal - N;

    // Mittelwert über das Fenster entfernen
    double mean = 0.0;
    for (int n = 0; n < N; ++n)
        mean += signal[start + n];
    mean /= double(N);

    int K = N / 2;
    QVector<double> magSpec(K);

    for (int k = 0; k < K; ++k) {
        double real = 0.0;
        double imag = 0.0;

        double angBase = -2.0 * M_PI * k / double(N);

        for (int n = 0; n < N; ++n) {
            // Hahn-Fenster + DC-Entfernung
            double centered = signal[start + n] - mean;
            double w        = 0.5 * (1.0 - qCos(2.0 * M_PI * n / (N - 1)));
            double sample   = centered * w;

            double ang = angBase * n;
            real += sample * qCos(ang);
            imag += sample * qSin(ang);
        }

        magSpec[k] = qSqrt(real * real + imag * imag) / double(N);
    }

    return magSpec;
}

// -----------------------------------------------------------------------------
// Bandpower-Berechnung (nutzt Magnitude-Spektrum)
// -----------------------------------------------------------------------------

MainWindow::BandPower MainWindow::computeBandPower(
    const QVector<double> &signal,
    double sampleRate)
{
    BandPower bp{0,0,0,0,0};

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
        int iLow  = int(std::floor(fLow  / hzPerBin));
        int iHigh = int(std::ceil (fHigh / hzPerBin));
        iLow  = std::max(iLow,  0);
        iHigh = std::min(iHigh, K-1);
        for (int i = iLow; i <= iHigh; ++i) {
            double a = mag[i];
            sum += a * a; // Power ~ Amplitude^2
        }
        return sum;
    };

    bp.delta = band(0.5, 4.0);
    bp.theta = band(4.0, 8.0);
    bp.alpha = band(8.0, 13.0);
    bp.beta  = band(13.0, 32.0);
    bp.gamma = band(32.0, 100.0);

    return bp;
}

// -----------------------------------------------------------------------------
// Bandpower-Plot updaten (relative Darstellung)
// -----------------------------------------------------------------------------

void MainWindow::updateBandPowerPlot(const BandPower &bp)
{
    if (!bandPowerPlot || !bandPowerBars)
        return;

    QVector<double> vals;
    vals << bp.delta << bp.theta << bp.alpha << bp.beta << bp.gamma;

    double total = 0.0;
    for (double v : vals)
        total += v;

    if (total > 0.0) {
        for (double &v : vals)
            v /= total;             // Summe = 1 -> relative power
    }

    bandPowerBars->setData(bandPowerTicks, vals);
    bandPowerPlot->yAxis->setRange(0, 1.2);
    bandPowerPlot->replot(QCustomPlot::rpQueuedReplot);
}

// -----------------------------------------------------------------------------
// FFT-Plot updaten (8 Kanäle)
// -----------------------------------------------------------------------------

void MainWindow::updateFftPlot()
{
    if (!fftPlot || currentSampleRate <= 0.0)
        return;

    fftPlot->clearGraphs();

    if (fftBuffers.isEmpty())
        return;

    int N = fftBuffers[0].size();
    if (N < 32)
        return;

    // Frequenzachse nach aktuellem N
    int K = N / 2;
    double hzPerBin = currentSampleRate / double(N);

    QVector<double> freqs(K);
    for (int k = 0; k < K; ++k)
        freqs[k] = k * hzPerBin;

    QList<QColor> colors = { Qt::red, Qt::green, Qt::blue, Qt::magenta,
                            Qt::cyan, Qt::darkYellow, Qt::darkRed, Qt::gray };

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
            if (a[i] > globalMax) globalMax = a[i];
        }

        fftPlot->addGraph();
        fftPlot->graph(ch)->setPen(QPen(colors.value(ch)));
        fftPlot->graph(ch)->setData(f, a);
    }

    if (globalMax <= 0.0) globalMax = 1.0;

    fftPlot->xAxis->setRange(0, 45);             // 0–45 Hz Fokus
    fftPlot->yAxis->setRange(0, globalMax * 1.2);

    fftPlot->replot(QCustomPlot::rpQueuedReplot);
}

// -----------------------------------------------------------------------------
// Plot-Decimation abhängig von Abtastrate
// -----------------------------------------------------------------------------

void MainWindow::updatePlotDecimation()
{
    const double targetPlotRate = 120.0; // maximale Plot-Samples pro Sekunde
    if (currentSampleRate <= 0.0) {
        plotDownsampleFactor = 1;
        return;
    }

    plotDownsampleFactor = qMax(1, int(std::lround(currentSampleRate / targetPlotRate)));
}
