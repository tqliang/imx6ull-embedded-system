#ifndef SPECTRUMWIDGET_H
#define SPECTRUMWIDGET_H

#include <QWidget>
#include <QTimer>

class SpectrumWidget : public QWidget
{
    Q_OBJECT
public:
    explicit SpectrumWidget(QWidget *parent = nullptr);

    void setSampleRate(int rate);
    void feedPcm(const short *data, int samples);
    void start();
    void stop();
    void reset();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void computeSpectrum();
    void fft(float *real, float *imag, int n);

    static const int FFT_SIZE = 256;
    static const int NUM_BARS = 16;
    static const int RING_SIZE = 4096;

    short  m_ringBuf[RING_SIZE];
    int    m_ringWrite;
    int    m_sampleRate;

    float  m_fftReal[FFT_SIZE];
    float  m_fftImag[FFT_SIZE];
    float  m_spectrum[NUM_BARS];
    float  m_peakHold[NUM_BARS];
    float  m_peakDecay[NUM_BARS];

    QTimer *m_animTimer;
    bool    m_active;
};

#endif