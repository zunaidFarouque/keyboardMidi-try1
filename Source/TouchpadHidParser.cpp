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

// For value caps with ReportCount > 1, extract all values via
// HidP_GetUsageValueArray. Returns empty on failure.
std::vector<ULONG> getUsageValueArray(const HIDP_VALUE_CAPS &cap,
                                      PHIDP_PREPARSED_DATA preparsed,
                                      PCHAR report, ULONG reportLen) {
  std::vector<ULONG> out;
  if (cap.ReportCount <= 1)
    return out;
  // Buffer size in bytes: (BitSize * ReportCount + 7) / 8
  USHORT bitSize = cap.BitSize;
  USHORT reportCount = cap.ReportCount;
  if (bitSize == 0 || bitSize > 32)
    return out;
  size_t byteLen = (static_cast<size_t>(bitSize) * reportCount + 7) / 8;
  if (byteLen > 256)
    return out;
  std::vector<BYTE> buf(byteLen, 0);
  USAGE usagePage = cap.UsagePage;
  USAGE usage = getUsage(cap);
  if (HidP_GetUsageValueArray(HidP_Input, usagePage, cap.LinkCollection, usage,
                              reinterpret_cast<PCHAR>(buf.data()),
                              static_cast<USHORT>(byteLen), preparsed, report,
                              reportLen) != HIDP_STATUS_SUCCESS) {
    return out;
  }
  out.reserve(reportCount);
  size_t bitOffset = 0;
  for (USHORT i = 0; i < reportCount; ++i) {
    ULONG val = 0;
    for (USHORT b = 0; b < bitSize; ++b) {
      size_t byteIdx = bitOffset / 8;
      int bitInByte = static_cast<int>(bitOffset % 8);
      if (byteIdx < buf.size())
        val |= (static_cast<ULONG>((buf[byteIdx] >> bitInByte) & 1) << b);
      ++bitOffset;
    }
    out.push_back(val);
  }
  return out;
}

// Parses a single HID input report (one report = dwSizeHid bytes).
// Used when multiple reports are packed in one WM_INPUT (dwCount > 1).
std::vector<TouchpadContact>
parseOneReport(PCHAR report, ULONG reportLen, PHIDP_PREPARSED_DATA preparsed,
               const std::vector<HIDP_VALUE_CAPS> &orderedCaps) {
  std::vector<TouchpadContact> result;

  ULONG contactCount = 0;
  for (const auto &cap : orderedCaps) {
    if (cap.LinkCollection == 0 && isUsage(cap, 0x0D, 0x54)) {
      ULONG v = 0;
      if (HidP_GetUsageValue(HidP_Input, cap.UsagePage, 0,
                             static_cast<USAGE>(getUsage(cap)), &v, preparsed,
                             report, reportLen) == HIDP_STATUS_SUCCESS)
        contactCount = v;
      break;
    }
  }

  const HIDP_VALUE_CAPS *capContactId = nullptr;
  const HIDP_VALUE_CAPS *capX = nullptr;
  const HIDP_VALUE_CAPS *capY = nullptr;
  for (const auto &cap : orderedCaps) {
    if (cap.LinkCollection == 0 && isUsage(cap, 0x0D, 0x54))
      continue;
    if (cap.ReportCount > 1) {
      if (isUsage(cap, 0x0D, 0x51))
        capContactId = &cap;
      else if (isUsage(cap, 0x01, 0x30))
        capX = &cap;
      else if (isUsage(cap, 0x01, 0x31))
        capY = &cap;
    }
  }

  if (capContactId || capX || capY) {
    std::vector<ULONG> ids, xs, ys;
    if (capContactId)
      ids = getUsageValueArray(*capContactId, preparsed, report, reportLen);
    if (capX)
      xs = getUsageValueArray(*capX, preparsed, report, reportLen);
    if (capY)
      ys = getUsageValueArray(*capY, preparsed, report, reportLen);
    size_t n = std::max({ids.size(), xs.size(), ys.size()});
    if (contactCount > 0 && n > contactCount)
      n = contactCount;
    for (size_t i = 0; i < n; ++i) {
      int cid = (i < ids.size()) ? static_cast<int>(ids[i]) : 0;
      int xval = (i < xs.size()) ? static_cast<int>(xs[i]) : 0;
      int yval = (i < ys.size()) ? static_cast<int>(ys[i]) : 0;
      float nx = 0.5f, ny = 0.5f;
      if (capX && i < xs.size())
        nx = normalizeFromLogical(xval, capX->LogicalMin, capX->LogicalMax);
      if (capY && i < ys.size())
        ny = normalizeFromLogical(yval, capY->LogicalMin, capY->LogicalMax);
      result.push_back(TouchpadContact{cid, xval, yval, nx, ny});
    }
    return result;
  }

  std::map<USHORT, ContactBuilder> contacts;
  for (const auto &cap : orderedCaps) {
    ULONG value = 0;
    const USAGE usagePage = static_cast<USAGE>(cap.UsagePage);
    const USAGE usage = static_cast<USAGE>(getUsage(cap));
    if (HidP_GetUsageValue(HidP_Input, usagePage, cap.LinkCollection, usage,
                           &value, preparsed, report,
                           reportLen) != HIDP_STATUS_SUCCESS) {
      continue;
    }

    if (cap.LinkCollection == 0) {
      if (isUsage(cap, 0x0D, 0x54))
        contactCount = value;
      continue;
    }

    auto &builder = contacts[cap.LinkCollection];
    if (isUsage(cap, 0x0D, 0x51)) {
      builder.contactId = static_cast<int>(value);
      builder.hasId = true;
    } else if (isUsage(cap, 0x01, 0x30)) {
      builder.x = static_cast<int>(value);
      builder.normX =
          normalizeFromLogical(builder.x, cap.LogicalMin, cap.LogicalMax);
      builder.hasX = true;
    } else if (isUsage(cap, 0x01, 0x31)) {
      builder.y = static_cast<int>(value);
      builder.normY =
          normalizeFromLogical(builder.y, cap.LogicalMin, cap.LogicalMax);
      builder.hasY = true;
    }
  }

  for (const auto &entry : contacts) {
    const auto &builder = entry.second;
    if ((builder.hasId || builder.hasX || builder.hasY) &&
        (builder.hasX && builder.hasY)) {
      int cid =
          builder.hasId ? builder.contactId : static_cast<int>(entry.first);
      result.push_back(TouchpadContact{cid, builder.x, builder.y, builder.normX,
                                       builder.normY});
      if (contactCount > 0 && result.size() >= contactCount)
        break;
    }
  }

  return result;
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

  const UINT dwSizeHid = rawInput->data.hid.dwSizeHid;
  const UINT dwCount = rawInput->data.hid.dwCount;
  if (dwSizeHid == 0 || dwCount == 0)
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
  auto *preparsed =
      reinterpret_cast<PHIDP_PREPARSED_DATA>(preparsedBuffer.data());

  // Parse each HID report separately. Windows may pack multiple reports in one
  // WM_INPUT (dwCount > 1); HidP_* expects one report at a time.
  for (UINT i = 0; i < dwCount; ++i) {
    PCHAR report = reinterpret_cast<PCHAR>(rawHidData +
                                           static_cast<size_t>(i) * dwSizeHid);
    ULONG reportLen = dwSizeHid;
    auto contacts = parseOneReport(report, reportLen, preparsed, orderedCaps);
    result.insert(result.end(), contacts.begin(), contacts.end());
  }

  return result;
}
