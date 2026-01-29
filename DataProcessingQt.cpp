#include "DataProcessingQt.h"

#include <QtMath>
#include <cmath>

DataProcessingQt::DataProcessingQt(int numChannels,
                                   double sampleRate,
                                   bool enableHighpass1Hz,
                                   bool enableNotch50Hz,
                                   bool enableBandpass_1_50Hz)
    : m_numChannels(numChannels),
    m_sampleRate(sampleRate),
    m_enableHighpass(enableHighpass1Hz),
    m_enableNotch(enableNotch50Hz),
    m_enableBandpass(enableBandpass_1_50Hz)
{
    designFilters();
}

void DataProcessingQt::updateSampleRate(double fs)
{
    if (fs <= 0.0)
        return;
    m_sampleRate = fs;
    designFilters();
}

void DataProcessingQt::reset()
{
    for (auto &b : m_hpFilters)    b.reset();
    for (auto &b : m_notchFilters) b.reset();
    for (auto &b : m_lpFilters)    b.reset();
}

double DataProcessingQt::processSample(int channelIndex, double x)
{
    if (channelIndex < 0 || channelIndex >= m_numChannels)
        return x;

    double y = x;

    // 1 Hz Highpass
    if (m_enableHighpass && channelIndex < m_hpFilters.size()) {
        y = m_hpFilters[channelIndex].process(y);
    }

    // 50 Hz Notch
    if (m_enableNotch && channelIndex < m_notchFilters.size()) {
        y = m_notchFilters[channelIndex].process(y);
    }

    // 50 Hz Lowpass (zusammen mit Highpass ~ Bandpass 1–50 Hz)
    if (m_enableBandpass && channelIndex < m_lpFilters.size()) {
        y = m_lpFilters[channelIndex].process(y);
    }

    return y;
}

void DataProcessingQt::setEnableHighpass(bool on)
{
    m_enableHighpass = on;
    for (auto &b : m_hpFilters) b.reset();
}

void DataProcessingQt::setEnableNotch(bool on)
{
    m_enableNotch = on;
    for (auto &b : m_notchFilters) b.reset();
}

void DataProcessingQt::setEnableBandpass(bool on)
{
    m_enableBandpass = on;
    for (auto &b : m_lpFilters) b.reset();
}

void DataProcessingQt::designFilters()
{
    m_hpFilters.clear();
    m_notchFilters.clear();
    m_lpFilters.clear();

    if (m_numChannels <= 0 || m_sampleRate <= 0.0)
        return;

    m_hpFilters.resize(m_numChannels);
    m_notchFilters.resize(m_numChannels);
    m_lpFilters.resize(m_numChannels);

    // 1 Hz Highpass, Q ~ 0.707 (Butterworth-artig)
    Biquad hp = makeHighpass(m_sampleRate, 1.0, 0.707);

    // 50 Hz Notch, relativ schmal (Q ~ 30)
    Biquad notch = makeNotch(m_sampleRate, 50.0, 30.0);

    // 50 Hz Lowpass, Q ~ 0.707 (2. Ordnung Butterworth)
    Biquad lp = makeLowpass(m_sampleRate, 50.0, 0.707);

    for (int ch = 0; ch < m_numChannels; ++ch) {
        m_hpFilters[ch]    = hp;
        m_notchFilters[ch] = notch;
        m_lpFilters[ch]    = lp;
    }
}

// ------------------------------------------------------------------
// Biquad-Design nach RBJ Audio EQ Cookbook
// ------------------------------------------------------------------

DataProcessingQt::Biquad DataProcessingQt::makeHighpass(double fs,
                                                        double f0,
                                                        double Q)
{
    Biquad biq;

    double w0   = 2.0 * M_PI * f0 / fs;
    double cosw = std::cos(w0);
    double sinw = std::sin(w0);
    double alpha = sinw / (2.0 * Q);

    double b0 =  (1.0 + cosw) / 2.0;
    double b1 = -(1.0 + cosw);
    double b2 =  (1.0 + cosw) / 2.0;
    double a0 =   1.0 + alpha;
    double a1 =  -2.0 * cosw;
    double a2 =   1.0 - alpha;

    biq.b0 = b0 / a0;
    biq.b1 = b1 / a0;
    biq.b2 = b2 / a0;
    biq.a1 = a1 / a0;
    biq.a2 = a2 / a0;
    biq.z1 = biq.z2 = 0.0;

    return biq;
}

DataProcessingQt::Biquad DataProcessingQt::makeNotch(double fs,
                                                     double f0,
                                                     double Q)
{
    Biquad biq;

    double w0   = 2.0 * M_PI * f0 / fs;
    double cosw = std::cos(w0);
    double sinw = std::sin(w0);
    double alpha = sinw / (2.0 * Q);

    double b0 = 1.0;
    double b1 = -2.0 * cosw;
    double b2 = 1.0;
    double a0 = 1.0 + alpha;
    double a1 = -2.0 * cosw;
    double a2 = 1.0 - alpha;

    biq.b0 = b0 / a0;
    biq.b1 = b1 / a0;
    biq.b2 = b2 / a0;
    biq.a1 = a1 / a0;
    biq.a2 = a2 / a0;
    biq.z1 = biq.z2 = 0.0;

    return biq;
}

DataProcessingQt::Biquad DataProcessingQt::makeLowpass(double fs,
                                                       double f0,
                                                       double Q)
{
    Biquad biq;

    double w0   = 2.0 * M_PI * f0 / fs;
    double cosw = std::cos(w0);
    double sinw = std::sin(w0);
    double alpha = sinw / (2.0 * Q);

    double b0 =  (1.0 - cosw) / 2.0;
    double b1 =   1.0 - cosw;
    double b2 =  (1.0 - cosw) / 2.0;
    double a0 =   1.0 + alpha;
    double a1 =  -2.0 * cosw;
    double a2 =   1.0 - alpha;

    biq.b0 = b0 / a0;
    biq.b1 = b1 / a0;
    biq.b2 = b2 / a0;
    biq.a1 = a1 / a0;
    biq.a2 = a2 / a0;
    biq.z1 = biq.z2 = 0.0;

    return biq;
}
