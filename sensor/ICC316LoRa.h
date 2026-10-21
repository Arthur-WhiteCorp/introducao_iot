#ifndef ICC316_LORA_H
#define ICC316_LORA_H

#include <Arduino.h>

bool sendMeasurementLora(
    const String& timestamp,
    float temperature,
    float ph,
    float turbidity
);

#endif
