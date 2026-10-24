// stuntman - benchmark harness. See bench.h.

#include "bench.h"

#include <rex/input/input.h>
#include <rex/logging.h>

#include <windows.h>
#include <tlhelp32.h>

#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace bench {

namespace {

using Clock = std::chrono::steady_clock;
using rex::input::X_INPUT_STATE;

constexpr uint32_t kLtBit = 0x10000;  // the triggers, above the XInput button bits
constexpr uint32_t kRtBit = 0x20000;

struct Step {
  double at;
  std::string what;
};

std::atomic<uint32_t> g_held{0};  // XInput button bits + kLtBit/kRtBit
std::atomic<bool> g_recording{false};
std::mutex g_frames_mutex;
std::vector<float> g_frames;
std::filesystem::path g_out;

uint32_t ButtonBits(const std::string& name) {
  static const std::map<std::string, uint32_t> kNames = {
      {"a", rex::input::X_INPUT_GAMEPAD_A},
      {"b", rex::input::X_INPUT_GAMEPAD_B},
      {"x", rex::input::X_INPUT_GAMEPAD_X},
      {"y", rex::input::X_INPUT_GAMEPAD_Y},
      {"lb", rex::input::X_INPUT_GAMEPAD_LEFT_SHOULDER},
      {"rb", rex::input::X_INPUT_GAMEPAD_RIGHT_SHOULDER},
      {"back", rex::input::X_INPUT_GAMEPAD_BACK},
      {"start", rex::input::X_INPUT_GAMEPAD_START},
      {"up", rex::input::X_INPUT_GAMEPAD_DPAD_UP},
      {"down", rex::input::X_INPUT_GAMEPAD_DPAD_DOWN},
      {"left", rex::input::X_INPUT_GAMEPAD_DPAD_LEFT},
      {"right", rex::input::X_INPUT_GAMEPAD_DPAD_RIGHT},
      {"lt", kLtBit},
      {"rt", kRtBit},
  };
  auto it = kNames.find(name);
  return it == kNames.end() ? 0 : it->second;
}

std::vector<Step> ParseScript(const char* text) {
  std::vector<Step> steps;
  std::istringstream in(text);
  std::string token;
  while (in >> token) {
    const auto colon = token.find(':');
    if (colon == std::string::npos) continue;
    steps.push_back({std::atof(token.substr(0, colon).c_str()), token.substr(colon + 1)});
  }
  return steps;
}

HWND GameWindow() {
  struct Find {
    DWORD pid;
    HWND hwnd;
  } find{GetCurrentProcessId(), nullptr};
  EnumWindows(
      [](HWND hwnd, LPARAM param) -> BOOL {
        auto* f = reinterpret_cast<Find*>(param);
        DWORD pid = 0;
        GetWindowThreadProcessId(hwnd, &pid);
        if (pid != f->pid || !IsWindowVisible(hwnd)) return TRUE;
        f->hwnd = hwnd;
        return FALSE;
      },
      reinterpret_cast<LPARAM>(&find));
  return find.hwnd;
}

void Screenshot(int index) {
  HWND hwnd = GameWindow();
  if (!hwnd) return;
  RECT rc;
  GetWindowRect(hwnd, &rc);
  const int w = rc.right - rc.left, h = rc.bottom - rc.top;
  char cls[128] = {};
  GetClassNameA(hwnd, cls, sizeof(cls));
  REXLOG_INFO("bench: shot of window {} {}x{}", cls, w, h);
  HDC screen = GetDC(hwnd);
  HDC mem = CreateCompatibleDC(screen);
  BITMAPINFO bi{};
  bi.bmiHeader = {sizeof(BITMAPINFOHEADER), w, -h, 1, 32, BI_RGB};
  void* bits = nullptr;
  HBITMAP bmp = CreateDIBSection(mem, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
  HGDIOBJ old = SelectObject(mem, bmp);
  if (!PrintWindow(hwnd, mem, PW_RENDERFULLCONTENT)) REXLOG_WARN("bench: PrintWindow failed");
  GdiFlush();
  const int stride = w * 4;
  BITMAPFILEHEADER fh{0x4D42, DWORD(sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + stride * h),
                      0, 0, sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER)};
  std::ofstream f(g_out / ("shot" + std::to_string(index) + ".bmp"), std::ios::binary);
  f.write(reinterpret_cast<const char*>(&fh), sizeof(fh));
  f.write(reinterpret_cast<const char*>(&bi.bmiHeader), sizeof(bi.bmiHeader));
  f.write(static_cast<const char*>(bits), std::streamsize(stride) * h);
  SelectObject(mem, old);
  DeleteObject(bmp);
  DeleteDC(mem);
  ReleaseDC(hwnd, screen);
}

void WriteFrames() {
  std::lock_guard lock(g_frames_mutex);
  std::ofstream f(g_out / "frames.txt");
  for (float ms : g_frames) f << ms << "\n";
}

constexpr int kStackDepth = 8;
using Stack = std::array<uint64_t, kStackDepth>;

// The sampled thread's instruction pointer and its first callers, walked with
// the x64 unwind data. The thread is suspended, so its stack can be read.
Stack Unwind(CONTEXT ctx) {
  Stack stack{};
  for (int i = 0; i < kStackDepth && ctx.Rip; ++i) {
    stack[i] = ctx.Rip;
    DWORD64 image_base = 0;
    auto* entry = RtlLookupFunctionEntry(ctx.Rip, &image_base, nullptr);
    if (entry) {
      void* handler_data = nullptr;
      DWORD64 establisher = 0;
      RtlVirtualUnwind(UNW_FLAG_NHANDLER, image_base, ctx.Rip, entry, &ctx, &handler_data,
                       &establisher, nullptr);
    } else {
      // A leaf function: the return address is on top of the stack.
      if (IsBadReadPtr(reinterpret_cast<void*>(ctx.Rsp), 8)) break;
      ctx.Rip = *reinterpret_cast<DWORD64*>(ctx.Rsp);
      ctx.Rsp += 8;
    }
  }
  return stack;
}

// Samples the stack of every other thread once per millisecond.
void Profile(double start, double length, bool sample, Clock::time_point t0) {
  std::this_thread::sleep_until(t0 + std::chrono::duration<double>(start));
  REXLOG_INFO("bench: {} for {} s", sample ? "profiling" : "recording frames", length);
  g_recording = true;
  if (!sample) {
    std::this_thread::sleep_for(std::chrono::duration<double>(length));
    g_recording = false;
    WriteFrames();
    return;
  }
  const DWORD self = GetCurrentThreadId();
  const DWORD pid = GetCurrentProcessId();
  std::unordered_map<DWORD, HANDLE> threads;
  std::unordered_map<DWORD, std::map<Stack, uint32_t>> samples;
  const auto end = Clock::now() + std::chrono::duration<double>(length);
  auto next_refresh = Clock::now();
  // A high-resolution timer, so sampling doesn't change the process's timer
  // resolution (which is one of the things being measured).
  HANDLE timer = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
                                        TIMER_ALL_ACCESS);
  while (Clock::now() < end) {
    if (Clock::now() >= next_refresh) {
      next_refresh = Clock::now() + std::chrono::seconds(1);
      HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
      THREADENTRY32 te{sizeof(te)};
      for (BOOL ok = Thread32First(snap, &te); ok; ok = Thread32Next(snap, &te)) {
        if (te.th32OwnerProcessID != pid || te.th32ThreadID == self) continue;
        if (threads.contains(te.th32ThreadID)) continue;
        HANDLE h = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION,
                              FALSE, te.th32ThreadID);
        if (h) threads[te.th32ThreadID] = h;
      }
      CloseHandle(snap);
    }
    for (auto& [tid, h] : threads) {
      if (SuspendThread(h) == DWORD(-1)) continue;
      CONTEXT ctx{};
      ctx.ContextFlags = CONTEXT_FULL;
      if (GetThreadContext(h, &ctx)) ++samples[tid][Unwind(ctx)];
      ResumeThread(h);
    }
    LARGE_INTEGER due{.QuadPart = -10000};  // 1 ms
    SetWaitableTimer(timer, &due, 0, nullptr, nullptr, FALSE);
    WaitForSingleObject(timer, INFINITE);
  }
  CloseHandle(timer);
  g_recording = false;

  auto locate = [&](uint64_t ip) -> std::pair<std::string, uint64_t> {
    HMODULE mod = nullptr;
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            reinterpret_cast<LPCSTR>(ip), &mod)) {
      return {"?", ip};
    }
    char name[MAX_PATH];
    GetModuleFileNameA(mod, name, MAX_PATH);
    return {std::filesystem::path(name).filename().string(), ip - uint64_t(mod)};
  };
  std::ofstream f(g_out / "profile.txt");
  for (auto& [tid, counts] : samples) {
    std::string tname = "?";
    PWSTR desc = nullptr;
    if (SUCCEEDED(GetThreadDescription(threads[tid], &desc)) && desc) {
      tname = std::filesystem::path(desc).string();
      LocalFree(desc);
    }
    uint64_t total = 0;
    for (auto& [stack, n] : counts) total += n;
    f << "thread " << tid << " " << total << " " << (tname.empty() ? "?" : tname) << "\n";
    // "  count module offset [<- module offset ...]", innermost first.
    for (auto& [stack, n] : counts) {
      f << "  " << n;
      for (int i = 0; i < kStackDepth && stack[i]; ++i) {
        auto [mod, off] = locate(stack[i]);
        f << (i ? " <- " : " ") << mod << " 0x" << std::hex << off << std::dec;
      }
      f << "\n";
    }
  }
  for (auto& [tid, h] : threads) CloseHandle(h);
  WriteFrames();
  REXLOG_INFO("bench: profile written to {}", (g_out / "profile.txt").string());
}

void RunScript(std::vector<Step> steps, Clock::time_point t0) {
  int shot = 0;
  for (const auto& s : steps) {
    std::this_thread::sleep_until(t0 + std::chrono::duration<double>(s.at));
    REXLOG_INFO("bench: {} at {} s", s.what, s.at);
    if (s.what == "quit") {
      while (g_recording) std::this_thread::sleep_for(std::chrono::milliseconds(100));
      WriteFrames();
      TerminateProcess(GetCurrentProcess(), 0);
    } else if (s.what == "shot") {
      Screenshot(shot++);
    } else if (s.what[0] == '+') {
      g_held |= ButtonBits(s.what.substr(1));
    } else if (s.what[0] == '-') {
      g_held &= ~ButtonBits(s.what.substr(1));
    } else {
      const uint32_t bits = ButtonBits(s.what);
      g_held |= bits;
      std::this_thread::sleep_for(std::chrono::milliseconds(150));
      g_held &= ~bits;
    }
  }
}

}  // namespace

void Start() {
  const char* script = std::getenv("STUNTMAN_BENCH");
  const char* profile = std::getenv("STUNTMAN_PROFILE");
  const char* frames = std::getenv("STUNTMAN_FRAMES");
  if (!script && !profile && !frames) return;
  const char* out = std::getenv("STUNTMAN_BENCH_OUT");
  g_out = out ? out : "bench";
  std::filesystem::create_directories(g_out);
  const auto t0 = Clock::now();
  if (script) std::thread(RunScript, ParseScript(script), t0).detach();
  if (profile) {
    double start = 0, length = 30;
    std::sscanf(profile, "%lf,%lf", &start, &length);
    std::thread(Profile, start, length, true, t0).detach();
  } else if (frames) {
    double start = 0, length = 30;
    std::sscanf(frames, "%lf,%lf", &start, &length);
    std::thread(Profile, start, length, false, t0).detach();
  }
}

void Inject(X_INPUT_STATE& state) {
  const uint32_t held = g_held.load(std::memory_order_relaxed);
  if (!held) return;
  auto& g = state.gamepad;
  g.buttons = uint16_t(g.buttons) | uint16_t(held & ~(kLtBit | kRtBit));
  if (held & kLtBit) g.left_trigger = 255;
  if (held & kRtBit) g.right_trigger = 255;
}

void OnFrame() {
  static Clock::time_point last;
  const auto now = Clock::now();
  if (g_recording.load(std::memory_order_relaxed) && last != Clock::time_point{}) {
    std::lock_guard lock(g_frames_mutex);
    g_frames.push_back(std::chrono::duration<float, std::milli>(now - last).count());
  }
  last = now;
}

}  // namespace bench
