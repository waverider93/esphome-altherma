// Vendored from ESPAltherma (MIT License, Copyright (c) 2020 Raomin). Only daikin_x10a.cpp compiles it: ESPHome
// includes every header of a component in main.cpp, and these headers define functions.
#ifdef DAIKIN_X10A_ESPALTHERMA_IMPL
#pragma once
#ifdef ARDUINO
#include <pgmspace.h>
#endif

class LabelDef
{
public:
    int convid;
    int offset;
    int registryID;
    int dataSize;
    int dataType;
    const char *label;
    const char *name = nullptr; // display name in the chosen language (catalog.h); label when not set
    char *data;
    char asString[30];
    LabelDef(){};
    LabelDef(int registryIDp, int offsetp, int convidp, int dataSizep, int dataTypep, const char *labelp) : convid(convidp), offset(offsetp), registryID(registryIDp), dataSize(dataSizep), dataType(dataTypep), label(labelp){};
};
#endif  // DAIKIN_X10A_ESPALTHERMA_IMPL
