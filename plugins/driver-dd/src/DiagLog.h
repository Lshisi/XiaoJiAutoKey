#pragma once
#include <xiaoji/plugin/NativeInput.hpp>
namespace DiagLog {
inline void Write(const std::string& message) { xiaoji::plugin::Log(message); }
}
