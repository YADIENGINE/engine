#pragma once
#include "Input/Internals/InputSample.h"

// The only physical input polling entry; called once per host frame, including transitions.
[[nodiscard]] InputSample ReadInputDevices();
