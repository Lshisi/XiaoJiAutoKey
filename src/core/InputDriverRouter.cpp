#include "InputDriverRouter.h"

void InputDriverRouter::SetDriver(InputDriver* driver) noexcept {
    driver_ = driver;
}

InputDriver* InputDriverRouter::CurrentDriver() const noexcept {
    return driver_;
}

bool InputDriverRouter::IsReady() const noexcept {
    return driver_ && driver_->IsReady();
}

std::wstring InputDriverRouter::GetLastError() const {
    return driver_ ? driver_->GetLastError() : L"No input driver selected";
}

bool InputDriverRouter::SendTap(const KeyDescriptor& key, InputSource src) {
    return driver_ && driver_->SendTap(key, src);
}

bool InputDriverRouter::SendDown(const KeyDescriptor& key, InputSource src) {
    return driver_ && driver_->SendDown(key, src);
}

bool InputDriverRouter::SendUp(const KeyDescriptor& key, InputSource src) {
    return driver_ && driver_->SendUp(key, src);
}

void InputDriverRouter::ReleaseAll() {
    if (driver_) driver_->ReleaseAll();
}

void InputDriverRouter::ClearAllStates() {
    if (driver_) driver_->ClearAllStates();
}
