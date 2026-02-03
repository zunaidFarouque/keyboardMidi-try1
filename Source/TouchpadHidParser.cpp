#include "TouchpadHidParser.h"

#include <algorithm>
#include <map>
#include <vector>

// clang-format off
#include <windows.h>
#include <hidsdi.h>
#include <hidusage.h>
#include <hidpi.h>
// clang-format on

namespace {
struct ContactBuilder {
  int contactId = 0;
  int x = 0;
  int y = 0;
  float normX = 0.0f;
  float normY = 0.0f;
  bool hasId = false;
  bool hasX = false;
  bool hasY = false;
};

float normalizeFromLogical(int value, LONG logicalMin, LONG logicalMax) {
  LONG range = logicalMax - logicalMin;
  if (range <= 0)
    return 0.5f;
  float norm =
      static_cast<float>(value - logicalMin) / static_cast<float>(range);
  return std::clamp(norm, 0.0f, 1.0f);
}

USHORT getUsage(const HIDP_VALUE_CAPS &cap) {
  return cap.IsRange ? cap.Range.UsageMin : cap.NotRange.Usage;
}

bool isUsage(const HIDP_VALUE_CAPS &cap, USHORT page, USHORT usage) {
  return cap.UsagePage == page && getUsage(cap) == usage;
}

std::vector<HIDP_VALUE_CAPS>
sortedByLinkCollection(std::vector<HIDP_VALUE_CAPS> caps) {
  std::sort(caps.begin(), caps.end(),
            [](const HIDP_VALUE_CAPS &a, const HIDP_VALUE_CAPS &b) {
              if (a.LinkCollection != b.LinkCollection)
                return a.LinkCollection < b.LinkCollection;
              if (a.UsagePage != b.UsagePage)
                return a.UsagePage < b.UsagePage;
              return getUsage(a) < getUsage(b);
            });
  return caps;
}
} // namespace

std::vector<TouchpadContact> parsePrecisionTouchpadReport(void *rawInputHandle,
                                                          void *deviceHandle) {
  std::vector<TouchpadContact> result;

  if (rawInputHandle == nullptr || deviceHandle == nullptr)
    return result;

  HRAWINPUT hRawInput = static_cast<HRAWINPUT>(rawInputHandle);
  HANDLE hDevice = static_cast<HANDLE>(deviceHandle);

  UINT rawInputSize = 0;
  if (GetRawInputData(hRawInput, RID_INPUT, nullptr, &rawInputSize,
                      sizeof(RAWINPUTHEADER)) != 0) {
    return result;
  }

  if (rawInputSize == 0)
    return result;

  std::vector<BYTE> rawInputBuffer(rawInputSize);
  if (GetRawInputData(hRawInput, RID_INPUT, rawInputBuffer.data(),
                      &rawInputSize, sizeof(RAWINPUTHEADER)) != rawInputSize) {
    return result;
  }

  auto *rawInput = reinterpret_cast<RAWINPUT *>(rawInputBuffer.data());
  if (rawInput->header.dwType != RIM_TYPEHID)
    return result;

  const UINT rawHidSize =
      rawInput->data.hid.dwSizeHid * rawInput->data.hid.dwCount;
  if (rawHidSize == 0)
    return result;

  BYTE *rawHidData = rawInput->data.hid.bRawData;

  UINT preparsedSize = 0;
  if (GetRawInputDeviceInfo(hDevice, RIDI_PREPARSEDDATA, nullptr,
                            &preparsedSize) != 0) {
    return result;
  }

  if (preparsedSize == 0)
    return result;

  std::vector<BYTE> preparsedBuffer(preparsedSize);
  if (GetRawInputDeviceInfo(hDevice, RIDI_PREPARSEDDATA, preparsedBuffer.data(),
                            &preparsedSize) != preparsedSize) {
    return result;
  }

  HIDP_CAPS caps{};
  if (HidP_GetCaps(
          reinterpret_cast<PHIDP_PREPARSED_DATA>(preparsedBuffer.data()),
          &caps) != HIDP_STATUS_SUCCESS) {
    return result;
  }

  USHORT valueCapsLength = caps.NumberInputValueCaps;
  if (valueCapsLength == 0)
    return result;

  std::vector<HIDP_VALUE_CAPS> valueCaps(valueCapsLength);
  if (HidP_GetValueCaps(HidP_Input, valueCaps.data(), &valueCapsLength,
                        reinterpret_cast<PHIDP_PREPARSED_DATA>(
                            preparsedBuffer.data())) != HIDP_STATUS_SUCCESS) {
    return result;
  }

  valueCaps.resize(valueCapsLength);
  auto orderedCaps = sortedByLinkCollection(std::move(valueCaps));

  ULONG contactCount = 0;
  std::map<USHORT, ContactBuilder> contacts;

  for (const auto &cap : orderedCaps) {
    ULONG value = 0;
    const USAGE usagePage = static_cast<USAGE>(cap.UsagePage);
    const USAGE usage = static_cast<USAGE>(getUsage(cap));
    if (HidP_GetUsageValue(
            HidP_Input, usagePage, cap.LinkCollection, usage, &value,
            reinterpret_cast<PHIDP_PREPARSED_DATA>(preparsedBuffer.data()),
            reinterpret_cast<PCHAR>(rawHidData),
            static_cast<ULONG>(rawHidSize)) != HIDP_STATUS_SUCCESS) {
      continue;
    }

    if (cap.LinkCollection == 0) {
      if (isUsage(cap, 0x0D, 0x54)) { // Contact Count
        contactCount = value;
      }
      continue;
    }

    auto &builder = contacts[cap.LinkCollection];
    if (isUsage(cap, 0x0D, 0x51)) { // Contact ID
      builder.contactId = static_cast<int>(value);
      builder.hasId = true;
    } else if (isUsage(cap, 0x01, 0x30)) { // X
      builder.x = static_cast<int>(value);
      builder.normX =
          normalizeFromLogical(builder.x, cap.LogicalMin, cap.LogicalMax);
      builder.hasX = true;
    } else if (isUsage(cap, 0x01, 0x31)) { // Y
      builder.y = static_cast<int>(value);
      builder.normY =
          normalizeFromLogical(builder.y, cap.LogicalMin, cap.LogicalMax);
      builder.hasY = true;
    }
  }

  for (const auto &entry : contacts) {
    const auto &builder = entry.second;
    if (builder.hasId && builder.hasX && builder.hasY) {
      result.push_back(TouchpadContact{builder.contactId, builder.x, builder.y,
                                       builder.normX, builder.normY});
      if (contactCount > 0 && result.size() >= contactCount)
        break;
    }
  }

  return result;
}
