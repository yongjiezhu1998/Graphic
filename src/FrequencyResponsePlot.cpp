#include "FrequencyResponsePlot.h"

#include <QCoreApplication>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>

#include <algorithm>
#include <cmath>
#include <complex>

namespace {

constexpr double kPi = 3.14159265358979323846;

struct BiquadCoeffs {
    double b0 = 1, b1 = 0, b2 = 0;
    double a0 = 1, a1 = 0, a2 = 0;
};

void normalizeA0(BiquadCoeffs &c) {
    if (c.a0 == 0.0) {
        return;
    }
    c.b0 /= c.a0;
    c.b1 /= c.a0;
    c.b2 /= c.a0;
    c.a1 /= c.a0;
    c.a2 /= c.a0;
    c.a0 = 1.0;
}

BiquadCoeffs coeffsLowpass(double fc, double Q, double fs) {
    BiquadCoeffs c;
    const double w0 = 2.0 * kPi * fc / fs;
    const double cosw0 = std::cos(w0);
    const double sinw0 = std::sin(w0);
    const double alpha = sinw0 / (2.0 * std::max(Q, 1e-6));
    c.b0 = (1.0 - cosw0) * 0.5;
    c.b1 = 1.0 - cosw0;
    c.b2 = (1.0 - cosw0) * 0.5;
    c.a0 = 1.0 + alpha;
    c.a1 = -2.0 * cosw0;
    c.a2 = 1.0 - alpha;
    normalizeA0(c);
    return c;
}

BiquadCoeffs coeffsHighpass(double fc, double Q, double fs) {
    BiquadCoeffs c;
    const double w0 = 2.0 * kPi * fc / fs;
    const double cosw0 = std::cos(w0);
    const double sinw0 = std::sin(w0);
    const double alpha = sinw0 / (2.0 * std::max(Q, 1e-6));
    c.b0 = (1.0 + cosw0) * 0.5;
    c.b1 = -(1.0 + cosw0);
    c.b2 = (1.0 + cosw0) * 0.5;
    c.a0 = 1.0 + alpha;
    c.a1 = -2.0 * cosw0;
    c.a2 = 1.0 - alpha;
    normalizeA0(c);
    return c;
}

BiquadCoeffs coeffsBandpass(double fc, double Q, double fs) {
    BiquadCoeffs c;
    const double w0 = 2.0 * kPi * fc / fs;
    const double cosw0 = std::cos(w0);
    const double sinw0 = std::sin(w0);
    const double alpha = sinw0 / (2.0 * std::max(Q, 1e-6));
    c.b0 = sinw0 * 0.5;
    c.b1 = 0.0;
    c.b2 = -sinw0 * 0.5;
    c.a0 = 1.0 + alpha;
    c.a1 = -2.0 * cosw0;
    c.a2 = 1.0 - alpha;
    normalizeA0(c);
    return c;
}

BiquadCoeffs coeffsNotch(double fc, double Q, double fs) {
    BiquadCoeffs c;
    const double w0 = 2.0 * kPi * fc / fs;
    const double cosw0 = std::cos(w0);
    const double sinw0 = std::sin(w0);
    const double alpha = sinw0 / (2.0 * std::max(Q, 1e-6));
    c.b0 = 1.0;
    c.b1 = -2.0 * cosw0;
    c.b2 = 1.0;
    c.a0 = 1.0 + alpha;
    c.a1 = -2.0 * cosw0;
    c.a2 = 1.0 - alpha;
    normalizeA0(c);
    return c;
}

/** RBJ peaking EQ; gainDb is boost/cut at center. */
BiquadCoeffs coeffsPeaking(double fc, double Q, double gainDb, double fs) {
    BiquadCoeffs c;
    const double w0 = 2.0 * kPi * fc / fs;
    const double cosw0 = std::cos(w0);
    const double sinw0 = std::sin(w0);
    const double alpha = sinw0 / (2.0 * std::max(Q, 1e-6));
    const double A = std::pow(10.0, gainDb / 40.0);
    c.b0 = 1.0 + alpha * A;
    c.b1 = -2.0 * cosw0;
    c.b2 = 1.0 - alpha * A;
    c.a0 = 1.0 + alpha / A;
    c.a1 = -2.0 * cosw0;
    c.a2 = 1.0 - alpha / A;
    normalizeA0(c);
    return c;
}

std::complex<double> biquadResponse(const BiquadCoeffs &c, double wRadPerSample) {
    const std::complex<double> zm1(std::cos(wRadPerSample), -std::sin(wRadPerSample));
    const std::complex<double> zm2 = zm1 * zm1;
    const std::complex<double> num = c.b0 + c.b1 * zm1 + c.b2 * zm2;
    const std::complex<double> den = c.a0 + c.a1 * zm1 + c.a2 * zm2;
    return num / den;
}

double magnitudeDb(const BiquadCoeffs &c, double fHz, double fs) {
    if (fHz <= 0.0 || fs <= 0.0) {
        return 0.0;
    }
    const double w = 2.0 * kPi * fHz / fs;
    const std::complex<double> H = biquadResponse(c, w);
    const double mag = std::abs(H);
    if (mag <= 1e-20) {
        return -200.0;
    }
    return 20.0 * std::log10(mag);
}

double parametricEqDb(const AudioModuleParams &p, double fHz, double fs) {
    const BiquadCoeffs c0 = coeffsPeaking(p.eqFreq[0], p.eqQ[0], p.eqGainDb[0], fs);
    const BiquadCoeffs c1 = coeffsPeaking(p.eqFreq[1], p.eqQ[1], p.eqGainDb[1], fs);
    const BiquadCoeffs c2 = coeffsPeaking(p.eqFreq[2], p.eqQ[2], p.eqGainDb[2], fs);
    if (fHz <= 0.0 || fs <= 0.0) {
        return 0.0;
    }
    const double w = 2.0 * kPi * fHz / fs;
    std::complex<double> H(1.0, 0.0);
    H *= biquadResponse(c0, w);
    H *= biquadResponse(c1, w);
    H *= biquadResponse(c2, w);
    const double mag = std::abs(H);
    if (mag <= 1e-20) {
        return -200.0;
    }
    return 20.0 * std::log10(mag);
}

void buildLogFreqs(double fMin, double fMax, int nPoints, QVector<double> *out) {
    out->resize(nPoints);
    if (nPoints <= 1) {
        if (nPoints == 1) {
            (*out)[0] = fMin;
        }
        return;
    }
    const double lo = std::log10(std::max(fMin, 1.0));
    const double hi = std::log10(fMax);
    for (int i = 0; i < nPoints; ++i) {
        const double t = static_cast<double>(i) / static_cast<double>(nPoints - 1);
        const double logf = lo + t * (hi - lo);
        (*out)[i] = std::pow(10.0, logf);
    }
}

} // namespace

FrequencyResponsePlot::FrequencyResponsePlot(QWidget *parent) : QWidget(parent) {
    setMinimumHeight(160);
    setMinimumWidth(320);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    rebuildCurve();
}

void FrequencyResponsePlot::setSampleRate(double hz) {
    if (hz > 0.0 && hz != m_sampleRate) {
        m_sampleRate = hz;
        rebuildCurve();
        update();
    }
}

void FrequencyResponsePlot::setBiquad(int filterType, double fcHz, double q) {
    m_mode = Mode::Biquad;
    m_filterType = filterType;
    m_fcHz = fcHz;
    m_q = q;
    rebuildCurve();
    update();
}

void FrequencyResponsePlot::setParametricEq(const AudioModuleParams &params) {
    m_mode = Mode::ParametricEq;
    m_eqParams = params;
    rebuildCurve();
    update();
}

void FrequencyResponsePlot::rebuildCurve() {
    constexpr int kPoints = 240;
    const double nyquist = m_sampleRate * 0.5;
    const double fMax = std::min(20000.0, nyquist * 0.99);
    const double fMin = 20.0;
    buildLogFreqs(fMin, fMax, kPoints, &m_freqHz);
    m_magDb.resize(kPoints);

    BiquadCoeffs biq;
    switch (m_mode) {
    case Mode::Biquad:
        switch (m_filterType) {
        case 0:
            biq = coeffsLowpass(m_fcHz, m_q, m_sampleRate);
            break;
        case 1:
            biq = coeffsHighpass(m_fcHz, m_q, m_sampleRate);
            break;
        case 2:
            biq = coeffsBandpass(m_fcHz, m_q, m_sampleRate);
            break;
        case 3:
        default:
            biq = coeffsNotch(m_fcHz, m_q, m_sampleRate);
            break;
        }
        for (int i = 0; i < kPoints; ++i) {
            m_magDb[i] = magnitudeDb(biq, m_freqHz[i], m_sampleRate);
        }
        break;
    case Mode::ParametricEq:
        for (int i = 0; i < kPoints; ++i) {
            m_magDb[i] = parametricEqDb(m_eqParams, m_freqHz[i], m_sampleRate);
        }
        break;
    }
}

QSize FrequencyResponsePlot::sizeHint() const {
    return QSize(400, 200);
}

QSize FrequencyResponsePlot::minimumSizeHint() const {
    return QSize(320, 160);
}

void FrequencyResponsePlot::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.fillRect(rect(), palette().color(QPalette::Base));

    const QMargins margins(48, 10, 14, 28);
    const QRect plot = rect().marginsRemoved(margins);

    if (m_magDb.isEmpty() || plot.width() < 4 || plot.height() < 4) {
        return;
    }

    double yMin = *std::min_element(m_magDb.begin(), m_magDb.end());
    double yMax = *std::max_element(m_magDb.begin(), m_magDb.end());
    yMin = std::min(yMin, -48.0);
    yMax = std::max(yMax, 12.0);
    if (yMax - yMin < 6.0) {
        yMax = yMin + 6.0;
    }
    const double pad = (yMax - yMin) * 0.08;
    yMin -= pad;
    yMax += pad;

    const double fMin = m_freqHz.first();
    const double fMax = m_freqHz.last();

    auto xOfFreq = [plot, fMin, fMax](double f) {
        const double lf = std::log10(std::max(f, 1.0));
        const double lf0 = std::log10(fMin);
        const double lf1 = std::log10(fMax);
        const double t = (lf - lf0) / std::max(lf1 - lf0, 1e-12);
        return plot.left() + t * (plot.width() - 1);
    };

    auto yOfDb = [plot, yMin, yMax](double db) {
        const double t = (db - yMin) / (yMax - yMin);
        return plot.bottom() - t * (plot.height() - 1);
    };

    p.setPen(QColor(220, 220, 220));
    const QVector<double> gridDb = {-48, -36, -24, -12, 0, 12, 24};
    for (double gdb : gridDb) {
        if (gdb >= yMin && gdb <= yMax) {
            const int yy = static_cast<int>(std::round(yOfDb(gdb)));
            p.drawLine(plot.left(), yy, plot.right(), yy);
        }
    }
    p.setPen(QColor(180, 200, 255));
    if (0.0 >= yMin && 0.0 <= yMax) {
        const int y0 = static_cast<int>(std::round(yOfDb(0.0)));
        p.drawLine(plot.left(), y0, plot.right(), y0);
    }

    const QVector<double> ticks = {100.0, 1000.0, 10000.0};
    p.setPen(QColor(220, 220, 220));
    for (double ft : ticks) {
        if (ft >= fMin && ft <= fMax) {
            const int xx = static_cast<int>(std::round(xOfFreq(ft)));
            p.drawLine(xx, plot.top(), xx, plot.bottom());
        }
    }

    QPainterPath path;
    for (int i = 0; i < m_freqHz.size(); ++i) {
        const double x = xOfFreq(m_freqHz[i]);
        const double y = yOfDb(m_magDb[i]);
        if (i == 0) {
            path.moveTo(x, y);
        } else {
            path.lineTo(x, y);
        }
    }
    p.setPen(QPen(QColor(40, 100, 200), 2));
    p.drawPath(path);

    p.setPen(palette().color(QPalette::Text));
    QFont f = p.font();
    f.setPointSize(std::max(8, f.pointSize() - 1));
    p.setFont(f);
    const QString title = QCoreApplication::translate("FrequencyResponsePlot", "Magnitude (dB)");
    p.drawText(QRect(plot.left(), 0, plot.width(), margins.top() + 4), Qt::AlignHCenter | Qt::AlignBottom, title);

    const QString fsLabel = QCoreApplication::translate("FrequencyResponsePlot", "Fs = %1 Hz").arg(
        QString::number(static_cast<int>(std::round(m_sampleRate))));
    p.drawText(QRect(plot.right() - 120, plot.top() - 2, 120, 14), Qt::AlignRight | Qt::AlignTop, fsLabel);

    p.drawText(QRect(plot.left(), plot.bottom() + 4, plot.width(), margins.bottom() - 4),
               Qt::AlignHCenter | Qt::AlignTop,
               QCoreApplication::translate("FrequencyResponsePlot", "20 Hz — 20 kHz (log)"));

    const QString y0 = QString::number(static_cast<int>(std::round(yMin)));
    const QString y1 = QString::number(static_cast<int>(std::round(yMax)));
    p.drawText(QRect(4, plot.top(), margins.left() - 8, 14), Qt::AlignRight | Qt::AlignTop, y1 + QLatin1String(" dB"));
    p.drawText(QRect(4, plot.bottom() - 12, margins.left() - 8, 14), Qt::AlignRight | Qt::AlignBottom, y0 + QLatin1String(" dB"));

    const QString lx = QStringLiteral("100");
    const QString mx = QStringLiteral("1k");
    const QString rx = QStringLiteral("10k");
    const int yTick = plot.bottom() + 4;
    p.drawText(QRect(static_cast<int>(xOfFreq(100.0)) - 20, yTick, 40, 18), Qt::AlignHCenter | Qt::AlignTop, lx);
    p.drawText(QRect(static_cast<int>(xOfFreq(1000.0)) - 20, yTick, 40, 18), Qt::AlignHCenter | Qt::AlignTop, mx);
    if (fMax >= 8000.0) {
        p.drawText(QRect(static_cast<int>(xOfFreq(10000.0)) - 24, yTick, 48, 18), Qt::AlignHCenter | Qt::AlignTop, rx);
    }
}
