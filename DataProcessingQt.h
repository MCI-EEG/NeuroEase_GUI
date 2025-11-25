#ifndef DATAPROCESSINGQT_H
#define DATAPROCESSINGQT_H

#include <QVector>

/**
 * Kleine DSP-Hilfsklasse für EEG:
 * - 1 Hz Highpass (Drift entfernen)
 * - 50 Hz Notch (Netzbrummen)
 * - 50 Hz Lowpass (zusammen mit HP = Bandpass 1–50 Hz)
 *
 * Filter sind als Biquads (2. Ordnung IIR) implementiert
 * nach den RBJ Audio EQ Cookbook Formeln.
 */
class DataProcessingQt
{
public:
    DataProcessingQt(int numChannels,
                     double sampleRate,
                     bool enableHighpass1Hz      = true,
                     bool enableNotch50Hz        = true,
                     bool enableBandpass_1_50Hz  = true);

    int    channelCount() const { return m_numChannels; }
    double sampleRate()   const { return m_sampleRate; }

    /// Einzelnes Sample eines Kanals durch die Filter schicken
    double processSample(int channelIndex, double x);

    /// Alle Filterzustände zurücksetzen (z.B. bei Reset)
    void reset();

    /// Sample-Rate ändern (z.B. bei neuer DataSource)
    void updateSampleRate(double fs);

    /// Filter live ein-/ausschalten (für GUI-Checkboxen)
    void setEnableHighpass(bool on);
    void setEnableNotch(bool on);
    void setEnableBandpass(bool on);   // Bandpass = Highpass 1 Hz + Lowpass 50 Hz

private:
    struct Biquad {
        double b0 = 1.0, b1 = 0.0, b2 = 0.0;
        double a1 = 0.0, a2 = 0.0;
        double z1 = 0.0, z2 = 0.0;

        double process(double x) {
            // Direct Form II Transposed
            double y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }
        void reset() { z1 = 0.0; z2 = 0.0; }
    };

    void   designFilters();
    static Biquad makeHighpass(double fs, double f0, double Q);
    static Biquad makeNotch   (double fs, double f0, double Q);
    static Biquad makeLowpass (double fs, double f0, double Q);

    int    m_numChannels   = 0;
    double m_sampleRate    = 250.0;

    bool   m_enableHighpass = true;
    bool   m_enableNotch    = true;
    bool   m_enableBandpass = true;  // steuert den Lowpass 50 Hz

    QVector<Biquad> m_hpFilters;    // pro Kanal 1 Hz Highpass
    QVector<Biquad> m_notchFilters; // pro Kanal 50 Hz Notch
    QVector<Biquad> m_lpFilters;    // pro Kanal 50 Hz Lowpass (Bandlimit)
};

#endif // DATAPROCESSINGQT_H
