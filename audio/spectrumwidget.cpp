#include "spectrumwidget.h"
#include <QPainter>
#include <QtMath>
#include <cstring>
#include <cmath>

SpectrumWidget::SpectrumWidget(QWidget *parent)
    : QWidget(parent)
    , m_ringWrite(0)
    , m_sampleRate(44100)
    , m_active(false)
{
    memset(m_ringBuf, 0, sizeof(m_ringBuf));
    memset(m_spectrum, 0, sizeof(m_spectrum));
    memset(m_peakHold, 0, sizeof(m_peakHold));
    memset(m_peakDecay, 0, sizeof(m_peakDecay));

    setMinimumHeight(100);
    setMaximumHeight(120);

    m_animTimer = new QTimer(this);
    m_animTimer->setInterval(40);
    connect(m_animTimer, &QTimer::timeout, this, [this]() {
        computeSpectrum();
        update();
    });
}

void SpectrumWidget::setSampleRate(int rate)
{
    m_sampleRate = rate;
}

void SpectrumWidget::feedPcm(const short *data, int samples)
{
    if (!m_active || samples <= 0)
        return;

    for (int i = 0; i < samples; i++) {
        m_ringBuf[m_ringWrite] = data[i];
        m_ringWrite = (m_ringWrite + 1) % RING_SIZE;
    }
}

void SpectrumWidget::start()
{
    m_active = true;
    m_animTimer->start();
}

void SpectrumWidget::stop()
{
    m_active = false;
    m_animTimer->stop();
    memset(m_ringBuf, 0, sizeof(m_ringBuf));
    m_ringWrite = 0;
    memset(m_spectrum, 0, sizeof(m_spectrum));
    memset(m_peakHold, 0, sizeof(m_peakHold));
    memset(m_peakDecay, 0, sizeof(m_peakDecay));
    update();
}

void SpectrumWidget::reset()
{
    memset(m_ringBuf, 0, sizeof(m_ringBuf));
    m_ringWrite = 0;
    memset(m_spectrum, 0, sizeof(m_spectrum));
    memset(m_peakHold, 0, sizeof(m_peakHold));
    memset(m_peakDecay, 0, sizeof(m_peakDecay));
    update();
}

void SpectrumWidget::computeSpectrum()
{
    memset(m_fftReal, 0, sizeof(m_fftReal));
    memset(m_fftImag, 0, sizeof(m_fftImag));

    int readPos = (m_ringWrite - FFT_SIZE + RING_SIZE) % RING_SIZE;
    for (int i = 0; i < FFT_SIZE; i++) {
        m_fftReal[i] = (float)m_ringBuf[readPos] / 32768.0f;
        m_fftImag[i] = 0.0f;
        readPos = (readPos + 1) % RING_SIZE;
    }

    float window[FFT_SIZE];
    for (int i = 0; i < FFT_SIZE; i++) {
        window[i] = 0.5f - 0.5f * cosf(2.0f * M_PI * i / (FFT_SIZE - 1));
    }
    for (int i = 0; i < FFT_SIZE; i++) {
        m_fftReal[i] *= window[i];
    }

    fft(m_fftReal, m_fftImag, FFT_SIZE);

    float maxMag = 0.0f;
    float rawMag[NUM_BARS];
    memset(rawMag, 0, sizeof(rawMag));

    int maxBin = (int)(FFT_SIZE / 2);
    float binFreq = (float)m_sampleRate / (float)FFT_SIZE;
    float maxFreq = 5000.0f;
    int useBins = (int)(maxFreq / binFreq);
    if (useBins > maxBin) useBins = maxBin;

    float binsPerBar = (float)useBins / (float)NUM_BARS;

    for (int i = 1; i < useBins; i++) {
        float mag = sqrtf(m_fftReal[i] * m_fftReal[i] + m_fftImag[i] * m_fftImag[i]);
        int bar = (int)((float)i / binsPerBar);
        if (bar < NUM_BARS)
            rawMag[bar] += mag;
    }

    for (int i = 0; i < NUM_BARS; i++) {
        rawMag[i] /= binsPerBar;
        if (rawMag[i] > maxMag) maxMag = rawMag[i];
    }

    float scale = (maxMag > 0.001f) ? (1.0f / maxMag) : 1.0f;
    for (int i = 0; i < NUM_BARS; i++) {
        float target = rawMag[i] * scale;
        target = (target < 1.0f) ? target : 1.0f;
        target = sqrtf(target);
        m_spectrum[i] += (target - m_spectrum[i]) * 0.45f;
        if (m_spectrum[i] < 0.001f) m_spectrum[i] = 0.0f;

        if (target > m_peakHold[i]) {
            m_peakHold[i] = target;
            m_peakDecay[i] = 0;
        } else {
            m_peakDecay[i]++;
            if (m_peakDecay[i] > 20) {
                m_peakHold[i] -= 0.02f;
                if (m_peakHold[i] < 0.0f) m_peakHold[i] = 0.0f;
            }
        }
    }
}

void SpectrumWidget::fft(float *real, float *imag, int n)
{
    int j = 0;
    for (int i = 0; i < n; i++)
    {
        if (i < j) {
            float tr = real[i], ti = imag[i];
            real[i] = real[j]; imag[i] = imag[j];
            real[j] = tr;      imag[j] = ti;
        }
        int m = n >> 1;
        while (m >= 1 && j >= m) { j -= m; m >>= 1; }
        j += m;
    }

    for (int len = 2; len <= n; len <<= 1) 
    {
        float angle = -2.0f * M_PI / (float)len;
        float wReal = cosf(angle);
        float wImag = sinf(angle);
        for (int i = 0; i < n; i += len) 
        {
            float curReal = 1.0f, curImag = 0.0f;
            int half = len >> 1;
            for (int k = 0; k < half; k++) 
            {
                int even = i + k;
                int odd  = i + k + half;
                float tr = curReal * real[odd] - curImag * imag[odd];
                float ti = curReal * imag[odd] + curImag * real[odd];
                real[odd] = real[even] - tr;
                imag[odd] = imag[even] - ti;
                real[even] += tr;
                imag[even] += ti;
                float nextReal = curReal * wReal - curImag * wImag;
                curImag = curReal * wImag + curImag * wReal;
                curReal = nextReal;
            }
        }
    }
}

void SpectrumWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false);

    int w = width();
    int h = height();
    int barCount = NUM_BARS;
    int barWidth = (w - (barCount + 1) * 2) / barCount;
    int gap = 2;
    if (barWidth < 2) barWidth = 2;

    painter.fillRect(0, 0, w, h, QColor(20, 20, 20));

    for (int i = 0; i < barCount; i++) 
    {
        float val = m_spectrum[i];
        int barH = (int)(val * (h - 8));
        if (barH < 1) barH = 1;

        int r, g, b;
        if (val < 0.33f) {
            float s = val / 0.33f;
            r = (int)(76 * s);
            g = (int)(175 + (80 * s));
            b = (int)(80 * (1.0f - s));
        } else if (val < 0.66f) {
            float s = (val - 0.33f) / 0.33f;
            r = (int)(76 + (179 * s));
            g = (int)(255 - (60 * s));
            b = 0;
        } else {
            float s = (val - 0.66f) / 0.34f;
            if (s > 1.0f) s = 1.0f;
            r = (int)(255);
            g = (int)(195 * (1.0f - s));
            b = 0;
        }

        QColor barColor(r, g, b);
        int x = 2 + i * (barWidth + gap);
        int y = h - barH - 2;
        painter.fillRect(x, y, barWidth, barH, barColor);

        float peak = m_peakHold[i];
        if (peak > 0.01f) {
            int peakY = h - (int)(peak * (h - 8)) - 2;
            int peakH = 3;
            if (peakY < 2) peakY = 2;
            painter.fillRect(x, peakY, barWidth, peakH, QColor(255, 255, 255, 180));
        }
    }
}