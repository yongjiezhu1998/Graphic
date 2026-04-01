#include "AudioTypes.h"

#include <QCoreApplication>

QString audioModuleKindToString(AudioModuleKind k) {
    switch (k) {
    case AudioModuleKind::Input:
        return QStringLiteral("Input");
    case AudioModuleKind::Output:
        return QStringLiteral("Output");
    case AudioModuleKind::Gain:
        return QStringLiteral("Gain");
    case AudioModuleKind::BiquadFilter:
        return QStringLiteral("BiquadFilter");
    case AudioModuleKind::ParametricEq:
        return QStringLiteral("ParametricEq");
    case AudioModuleKind::Crossover2Way:
        return QStringLiteral("Crossover2Way");
    case AudioModuleKind::Crossover3Way:
        return QStringLiteral("Crossover3Way");
    }
    return QStringLiteral("Gain");
}

AudioModuleKind audioModuleKindFromString(const QString &s, bool *ok) {
    if (ok) {
        *ok = true;
    }
    if (s == QLatin1String("Input")) {
        return AudioModuleKind::Input;
    }
    if (s == QLatin1String("Output")) {
        return AudioModuleKind::Output;
    }
    if (s == QLatin1String("Gain")) {
        return AudioModuleKind::Gain;
    }
    if (s == QLatin1String("BiquadFilter")) {
        return AudioModuleKind::BiquadFilter;
    }
    if (s == QLatin1String("ParametricEq")) {
        return AudioModuleKind::ParametricEq;
    }
    if (s == QLatin1String("Crossover2Way")) {
        return AudioModuleKind::Crossover2Way;
    }
    if (s == QLatin1String("Crossover3Way")) {
        return AudioModuleKind::Crossover3Way;
    }
    if (ok) {
        *ok = false;
    }
    return AudioModuleKind::Gain;
}

QString audioFilterTypeName(int typeIndex) {
    switch (typeIndex) {
    case 0:
        return QCoreApplication::translate("AudioTypes", "Low-pass");
    case 1:
        return QCoreApplication::translate("AudioTypes", "High-pass");
    case 2:
        return QCoreApplication::translate("AudioTypes", "Band-pass");
    case 3:
        return QCoreApplication::translate("AudioTypes", "Notch");
    default:
        return QCoreApplication::translate("AudioTypes", "Low-pass");
    }
}
