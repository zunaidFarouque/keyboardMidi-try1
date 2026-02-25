#include "CrashLogger.h"

#include <JuceHeader.h>
#include <exception>
#include <mutex>

#if JUCE_WINDOWS
#include <windows.h>
#endif

namespace {

std::atomic<bool> gDebugModeEnabled{false};
std::once_flag gInstallFlag;
std::terminate_handler gPreviousTerminateHandler = nullptr;
juce::String gSessionId;

struct UiContextSnapshot {
  int mainTabIndex = -1;
  juce::String mainTabName;
  bool studioMode = false;
  bool midiMode = false;
  int activeLayer = -1;
  int zonesSelectedIndex = -1;
  int touchpadSelectedRow = -1;
  bool hasData = false;
};

UiContextSnapshot gUiContext;
std::mutex gUiContextMutex;

constexpr int kMaxBreadcrumbs = 32;
juce::String gBreadcrumbs[kMaxBreadcrumbs];
int gBreadcrumbCount = 0;
int gBreadcrumbNextIndex = 0;
std::mutex gBreadcrumbMutex;

#if JUCE_WINDOWS
LPTOP_LEVEL_EXCEPTION_FILTER gPreviousExceptionFilter = nullptr;
#endif

void writeCrashLogImpl(const juce::String &context) {
  if (!gDebugModeEnabled.load())
    return;

  auto exe = juce::File::getSpecialLocation(juce::File::currentExecutableFile);
  auto logFile = exe.getParentDirectory().getChildFile("MIDIQy_crashlog.txt");

  juce::StringArray lines;
  auto now = juce::Time::getCurrentTime();

  lines.add("==== MIDIQy crash ====");
  lines.add("Time: " + now.toISO8601(true));
  lines.add("Context: " + context);

  // Build / version metadata
#if JUCE_DEBUG
  juce::String buildConfig = "Debug";
#else
  juce::String buildConfig = "Release";
#endif
  lines.add("BuildConfig: " + buildConfig);
  lines.add("BuildTime: " + juce::String(__DATE__ " " __TIME__));

  // Session / crash IDs
  if (gSessionId.isNotEmpty())
    lines.add("SessionId: " + gSessionId);
  juce::String crashId = juce::Uuid().toString();
  lines.add("CrashId: " + crashId);

  if (juce::JUCEApplicationBase::isStandaloneApp()) {
    if (auto *app = juce::JUCEApplicationBase::getInstance()) {
      lines.add("App: " + app->getApplicationName() + " " +
                app->getApplicationVersion());
    }
  }

  lines.add("OS: " + juce::SystemStats::getOperatingSystemName());
  lines.add("CPU: " + juce::SystemStats::getCpuVendor() + " " +
            juce::SystemStats::getCpuModel());

  // UI/context snapshot (best-effort)
  {
    std::unique_lock<std::mutex> lock(gUiContextMutex, std::defer_lock);
    if (lock.try_lock() && gUiContext.hasData) {
      lines.add("UIContext:");
      lines.add("  mainTabIndex: " + juce::String(gUiContext.mainTabIndex));
      if (gUiContext.mainTabName.isNotEmpty())
        lines.add("  mainTabName: " + gUiContext.mainTabName);
      lines.add("  studioMode: " + juce::String(gUiContext.studioMode ? "true"
                                                                      : "false"));
      lines.add("  midiMode: " + juce::String(gUiContext.midiMode ? "true"
                                                                  : "false"));
      lines.add("  activeLayer: " + juce::String(gUiContext.activeLayer));
      lines.add("  zonesSelectedIndex: " +
                juce::String(gUiContext.zonesSelectedIndex));
      lines.add("  touchpadSelectedRow: " +
                juce::String(gUiContext.touchpadSelectedRow));
    }
  }

  // Breadcrumbs (best-effort)
  {
    std::unique_lock<std::mutex> lock(gBreadcrumbMutex, std::defer_lock);
    if (lock.try_lock() && gBreadcrumbCount > 0) {
      lines.add("Breadcrumbs (newest first):");
      int count = gBreadcrumbCount;
      for (int i = 0; i < count; ++i) {
        int index = (gBreadcrumbNextIndex - 1 - i + kMaxBreadcrumbs) %
                    kMaxBreadcrumbs;
        const auto &msg = gBreadcrumbs[index];
        if (msg.isNotEmpty())
          lines.add("  " + juce::String(i) + ": " + msg);
      }
    }
  }

  auto stack = juce::SystemStats::getStackBacktrace();
  if (stack.isNotEmpty()) {
    lines.add("Stack trace:");
    lines.add(stack);
  }

  lines.add("");

  auto existing = logFile.loadFileAsString();
  existing << lines.joinIntoString("\n") << "\n";
  logFile.getParentDirectory().createDirectory();
  logFile.replaceWithText(existing);
}

void terminateHandler() {
  writeCrashLogImpl("std::terminate");

  if (gPreviousTerminateHandler) {
    gPreviousTerminateHandler();
  } else {
    std::abort();
  }
}

#if JUCE_WINDOWS
LONG WINAPI sehExceptionFilter(_In_ struct _EXCEPTION_POINTERS *exceptionInfo) {
  juce::String context = "Unhandled SEH exception";
  if (exceptionInfo != nullptr && exceptionInfo->ExceptionRecord != nullptr) {
    auto code = exceptionInfo->ExceptionRecord->ExceptionCode;
    context += " (code 0x" + juce::String::toHexString((int)code) + ")";

    // Add fault address / module info when available.
    void *address = exceptionInfo->ExceptionRecord->ExceptionAddress;
    context += " at 0x" +
               juce::String::toHexString(
                   (juce::int64)reinterpret_cast<std::uintptr_t>(address));

    if (exceptionInfo->ExceptionRecord->NumberParameters >= 2) {
      auto accessType =
          exceptionInfo->ExceptionRecord->ExceptionInformation[0];
      auto faultAddr =
          exceptionInfo->ExceptionRecord->ExceptionInformation[1];
      juce::String accessStr = "unknown";
      if (accessType == 0)
        accessStr = "read";
      else if (accessType == 1)
        accessStr = "write";
      else if (accessType == 8)
        accessStr = "execute";
      context += " (" + accessStr + " at 0x" +
                 juce::String::toHexString((juce::int64)faultAddr) + ")";
    }

    HMODULE module = nullptr;
    WCHAR modulePath[MAX_PATH] = {};
    if (GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(address), &module)) {
      if (GetModuleFileNameW(module, modulePath, MAX_PATH) > 0) {
        context += " in module " + juce::String(modulePath);
      }
    }
  }

  writeCrashLogImpl(context);

  if (gPreviousExceptionFilter)
    return gPreviousExceptionFilter(exceptionInfo);

  return EXCEPTION_EXECUTE_HANDLER;
}
#endif

} // namespace

void CrashLogger::installGlobalHandlers() {
  std::call_once(gInstallFlag, []() {
    gPreviousTerminateHandler = std::set_terminate(terminateHandler);

    // Generate a session identifier for correlating crashes from the same run.
    gSessionId = juce::Uuid().toString();

#if JUCE_WINDOWS
    gPreviousExceptionFilter = SetUnhandledExceptionFilter(sehExceptionFilter);
#endif
  });
}

void CrashLogger::setDebugModeEnabled(bool enabled) {
  gDebugModeEnabled.store(enabled);
}

void CrashLogger::addBreadcrumb(const juce::String &message) {
  std::lock_guard<std::mutex> lock(gBreadcrumbMutex);
  gBreadcrumbs[gBreadcrumbNextIndex] = message;
  gBreadcrumbNextIndex = (gBreadcrumbNextIndex + 1) % kMaxBreadcrumbs;
  if (gBreadcrumbCount < kMaxBreadcrumbs)
    ++gBreadcrumbCount;
}

void CrashLogger::setUiContext(int mainTabIndex, const juce::String &mainTabName,
                               bool studioMode, bool midiMode, int activeLayer,
                               int zonesSelectedIndex, int touchpadSelectedRow) {
  std::lock_guard<std::mutex> lock(gUiContextMutex);
  gUiContext.mainTabIndex = mainTabIndex;
  gUiContext.mainTabName = mainTabName;
  gUiContext.studioMode = studioMode;
  gUiContext.midiMode = midiMode;
  gUiContext.activeLayer = activeLayer;
  gUiContext.zonesSelectedIndex = zonesSelectedIndex;
  gUiContext.touchpadSelectedRow = touchpadSelectedRow;
  gUiContext.hasData = true;
}

void CrashLogger::writeCrashLogForTest(const juce::String &context) {
  writeCrashLogImpl(context);
}

void CrashLogger::writeCrashLog(const juce::String &context) {
  writeCrashLogImpl(context);
}

