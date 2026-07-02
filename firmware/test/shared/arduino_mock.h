// Compatibility shim: the full native mock now lives in Arduino.h (same
// directory) so production sources that #include <Arduino.h> compile natively.
// Existing tests keep including "arduino_mock.h".
#pragma once
#include "Arduino.h"
