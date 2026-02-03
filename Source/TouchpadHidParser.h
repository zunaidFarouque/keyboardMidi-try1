#pragma once

#include <vector>

#include "TouchpadTypes.h"

// Parses Precision Touchpad contacts from a WM_INPUT (HRAWINPUT) handle.
// rawInputHandle and deviceHandle are passed as void* to keep headers
// Windows-free per project rules.
std::vector<TouchpadContact> parsePrecisionTouchpadReport(void *rawInputHandle,
                                                          void *deviceHandle);
