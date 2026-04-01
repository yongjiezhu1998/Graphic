#pragma once

#include "AudioTypes.h"

#include <QWidget>

/** Log-frequency magnitude plot (dB) for biquad / cascaded peaking EQ. */
class FrequencyResponsePlot : public QWidget {
public:
    explicit FrequencyResponsePlot(QWidget *parent = nullptr);

    void setSampleRate(double hz);
    void setBiquad(int filterType, double fcHz, double q);
    void setParametricEq(const AudioModuleParams &params);

protected:
    void paintEvent(QPaintEvent *event) override;
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

private:
    void rebuildCurve();

    double m_sampleRate = 48000.0;
    enum class Mode { Biquad, ParametricEq } m_mode = Mode::Biquad;
    int m_filterType = 0;
    double m_fcHz = 1000.0;
    double m_q = 0.707;
    AudioModuleParams m_eqParams;

    QVector<double> m_freqHz;
    QVector<double> m_magDb;
};
