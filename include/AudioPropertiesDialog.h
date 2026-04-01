#pragma once

#include "AudioTypes.h"

class QWidget;

class AudioPropertiesDialog {
public:
    static bool editParams(QWidget *parent, AudioModuleKind kind, AudioModuleParams *params);
};
