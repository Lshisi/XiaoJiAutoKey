#pragma once

#include <Windows.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iterator>
#include <optional>
#include <thread>

class PrecisionWaiter final {
public:
    PrecisionWaiter() {
        wakeEvent_ = ::CreateEventW(nullptr, FALSE, FALSE, nullptr);
        timer_ = ::CreateWaitableTimerExW(
            nullptr,
            nullptr,
            CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
            TIMER_ALL_ACCESS);
        if (!timer_) {
            // Older Windows releases do not support the high-resolution flag.
            timer_ = ::CreateWaitableTimerW(nullptr, FALSE, nullptr);
        }
    }

    ~PrecisionWaiter() {
        if (timer_) ::CloseHandle(timer_);
        if (wakeEvent_) ::CloseHandle(wakeEvent_);
    }

    PrecisionWaiter(const PrecisionWaiter&) = delete;
    PrecisionWaiter& operator=(const PrecisionWaiter&) = delete;

    void Wake() const noexcept {
        if (wakeEvent_) ::SetEvent(wakeEvent_);
    }

    void WaitUntil(std::optional<std::chrono::steady_clock::time_point> deadline) const {
        if (!wakeEvent_) {
            if (deadline) std::this_thread::sleep_until(*deadline);
            return;
        }

        if (!deadline) {
            ::WaitForSingleObject(wakeEvent_, INFINITE);
            return;
        }

        const auto now = std::chrono::steady_clock::now();
        if (*deadline <= now) return;

        if (!timer_) {
            const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(*deadline - now);
            ::WaitForSingleObject(wakeEvent_, static_cast<DWORD>(std::max<std::int64_t>(1, remaining.count())));
            return;
        }

        const auto remaining = std::chrono::duration_cast<std::chrono::nanoseconds>(*deadline - now).count();
        LARGE_INTEGER dueTime{};
        dueTime.QuadPart = -std::max<LONGLONG>(1, (remaining + 99) / 100); // Relative 100 ns units.
        if (!::SetWaitableTimer(timer_, &dueTime, 0, nullptr, nullptr, FALSE)) {
            ::WaitForSingleObject(wakeEvent_, 1);
            return;
        }

        HANDLE handles[] = {wakeEvent_, timer_};
        ::WaitForMultipleObjects(static_cast<DWORD>(std::size(handles)), handles, FALSE, INFINITE);
    }

private:
    HANDLE wakeEvent_ = nullptr;
    HANDLE timer_ = nullptr;
};
