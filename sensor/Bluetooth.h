#ifndef ICC316_BLUETOOTH_H
#define ICC316_BLUETOOTH_H

#include <Arduino.h>

bool sendMeasurementBle(
    const String& timestamp,
    float temperature,
    float ph,
    float turbidity
);

#endif
