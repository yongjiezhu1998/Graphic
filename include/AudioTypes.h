#pragma once

#include <QString>

enum class AudioModuleKind {
    Input,
    Output,
    Gain,
    BiquadFilter,
    ParametricEq,
    Crossover2Way,
    Crossover3Way,
};

struct AudioModuleParams {
    double gainDb = 0.0;

    double crossoverLowHz = 250.0;
    double crossoverHighHz = 4000.0;

    double filterFreqHz = 1000.0;
    double filterQ = 0.707;
    int filterType = 0;

    double eqFreq[3] = {125.0, 1000.0, 8000.0};
    double eqGainDb[3] = {0.0, 0.0, 0.0};
    double eqQ[3] = {1.0, 1.0, 1.0};
};

QString audioModuleKindToString(AudioModuleKind k);
AudioModuleKind audioModuleKindFromString(const QString &s, bool *ok = nullptr);

QString audioFilterTypeName(int typeIndex);
