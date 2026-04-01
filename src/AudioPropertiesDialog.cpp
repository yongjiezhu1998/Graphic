#include "AudioPropertiesDialog.h"

#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>
#include <QCoreApplication>

#include <algorithm>

bool AudioPropertiesDialog::editParams(QWidget *parent, AudioModuleKind kind, AudioModuleParams *params) {
    if (!params) {
        return false;
    }

    if (kind == AudioModuleKind::Input || kind == AudioModuleKind::Output) {
        return false;
    }

    QDialog dlg(parent);
    dlg.setWindowTitle(QCoreApplication::translate("AudioPropertiesDialog", "Block parameters"));

    auto *form = new QFormLayout;

    QDoubleSpinBox *gainSpin = nullptr;
    QComboBox *filterTypeCombo = nullptr;
    QDoubleSpinBox *filterFreqSpin = nullptr;
    QDoubleSpinBox *filterQSpin = nullptr;
    QDoubleSpinBox *xLowSpin = nullptr;
    QDoubleSpinBox *xHighSpin = nullptr;
    QDoubleSpinBox *eqFreqSpin[3] = {};
    QDoubleSpinBox *eqGainSpin[3] = {};
    QDoubleSpinBox *eqQSpin[3] = {};

    switch (kind) {
    case AudioModuleKind::Gain:
        gainSpin = new QDoubleSpinBox;
        gainSpin->setRange(-96.0, 24.0);
        gainSpin->setDecimals(2);
        gainSpin->setSuffix(QCoreApplication::translate("AudioPropertiesDialog", " dB"));
        gainSpin->setValue(params->gainDb);
        form->addRow(QCoreApplication::translate("AudioPropertiesDialog", "Gain:"), gainSpin);
        break;
    case AudioModuleKind::BiquadFilter:
        filterTypeCombo = new QComboBox;
        filterTypeCombo->addItem(audioFilterTypeName(0));
        filterTypeCombo->addItem(audioFilterTypeName(1));
        filterTypeCombo->addItem(audioFilterTypeName(2));
        filterTypeCombo->addItem(audioFilterTypeName(3));
        filterTypeCombo->setCurrentIndex(std::clamp(params->filterType, 0, 3));
        filterFreqSpin = new QDoubleSpinBox;
        filterFreqSpin->setRange(10.0, 96000.0);
        filterFreqSpin->setDecimals(1);
        filterFreqSpin->setSuffix(QCoreApplication::translate("AudioPropertiesDialog", " Hz"));
        filterFreqSpin->setValue(params->filterFreqHz);
        filterQSpin = new QDoubleSpinBox;
        filterQSpin->setRange(0.05, 20.0);
        filterQSpin->setDecimals(3);
        filterQSpin->setValue(params->filterQ);
        form->addRow(QCoreApplication::translate("AudioPropertiesDialog", "Type:"), filterTypeCombo);
        form->addRow(QCoreApplication::translate("AudioPropertiesDialog", "Frequency:"), filterFreqSpin);
        form->addRow(QCoreApplication::translate("AudioPropertiesDialog", "Q:"), filterQSpin);
        break;
    case AudioModuleKind::ParametricEq:
        for (int i = 0; i < 3; ++i) {
            eqFreqSpin[i] = new QDoubleSpinBox;
            eqFreqSpin[i]->setRange(20.0, 96000.0);
            eqFreqSpin[i]->setDecimals(1);
            eqFreqSpin[i]->setSuffix(QCoreApplication::translate("AudioPropertiesDialog", " Hz"));
            eqFreqSpin[i]->setValue(params->eqFreq[i]);
            eqGainSpin[i] = new QDoubleSpinBox;
            eqGainSpin[i]->setRange(-24.0, 24.0);
            eqGainSpin[i]->setDecimals(2);
            eqGainSpin[i]->setSuffix(QCoreApplication::translate("AudioPropertiesDialog", " dB"));
            eqGainSpin[i]->setValue(params->eqGainDb[i]);
            eqQSpin[i] = new QDoubleSpinBox;
            eqQSpin[i]->setRange(0.1, 20.0);
            eqQSpin[i]->setDecimals(2);
            eqQSpin[i]->setValue(params->eqQ[i]);
            const QString band = QCoreApplication::translate("AudioPropertiesDialog", "Band %1").arg(i + 1);
            form->addRow(new QLabel(QStringLiteral("<b>%1</b>").arg(band)));
            form->addRow(QCoreApplication::translate("AudioPropertiesDialog", "Frequency:"), eqFreqSpin[i]);
            form->addRow(QCoreApplication::translate("AudioPropertiesDialog", "Gain:"), eqGainSpin[i]);
            form->addRow(QCoreApplication::translate("AudioPropertiesDialog", "Q:"), eqQSpin[i]);
        }
        break;
    case AudioModuleKind::Crossover2Way:
        xLowSpin = new QDoubleSpinBox;
        xLowSpin->setRange(20.0, 96000.0);
        xLowSpin->setDecimals(1);
        xLowSpin->setSuffix(QCoreApplication::translate("AudioPropertiesDialog", " Hz"));
        xLowSpin->setValue(params->crossoverLowHz);
        form->addRow(QCoreApplication::translate("AudioPropertiesDialog", "Split frequency:"), xLowSpin);
        break;
    case AudioModuleKind::Crossover3Way:
        xLowSpin = new QDoubleSpinBox;
        xLowSpin->setRange(20.0, 96000.0);
        xLowSpin->setDecimals(1);
        xLowSpin->setSuffix(QCoreApplication::translate("AudioPropertiesDialog", " Hz"));
        xLowSpin->setValue(params->crossoverLowHz);
        xHighSpin = new QDoubleSpinBox;
        xHighSpin->setRange(20.0, 96000.0);
        xHighSpin->setDecimals(1);
        xHighSpin->setSuffix(QCoreApplication::translate("AudioPropertiesDialog", " Hz"));
        xHighSpin->setValue(params->crossoverHighHz);
        form->addRow(QCoreApplication::translate("AudioPropertiesDialog", "Low / Mid:"), xLowSpin);
        form->addRow(QCoreApplication::translate("AudioPropertiesDialog", "Mid / High:"), xHighSpin);
        break;
    default:
        break;
    }

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    auto *layout = new QVBoxLayout(&dlg);
    layout->addLayout(form);
    layout->addWidget(buttons);

    if (dlg.exec() != QDialog::Accepted) {
        return false;
    }

    switch (kind) {
    case AudioModuleKind::Gain:
        if (gainSpin) {
            params->gainDb = gainSpin->value();
        }
        break;
    case AudioModuleKind::BiquadFilter:
        if (filterTypeCombo) {
            params->filterType = filterTypeCombo->currentIndex();
        }
        if (filterFreqSpin) {
            params->filterFreqHz = filterFreqSpin->value();
        }
        if (filterQSpin) {
            params->filterQ = filterQSpin->value();
        }
        break;
    case AudioModuleKind::ParametricEq:
        for (int i = 0; i < 3; ++i) {
            if (eqFreqSpin[i]) {
                params->eqFreq[i] = eqFreqSpin[i]->value();
            }
            if (eqGainSpin[i]) {
                params->eqGainDb[i] = eqGainSpin[i]->value();
            }
            if (eqQSpin[i]) {
                params->eqQ[i] = eqQSpin[i]->value();
            }
        }
        break;
    case AudioModuleKind::Crossover2Way:
        if (xLowSpin) {
            params->crossoverLowHz = xLowSpin->value();
        }
        break;
    case AudioModuleKind::Crossover3Way:
        if (xLowSpin) {
            params->crossoverLowHz = xLowSpin->value();
        }
        if (xHighSpin) {
            params->crossoverHighHz = xHighSpin->value();
        }
        break;
    default:
        break;
    }

    return true;
}
