#pragma once

#include <string>

// 核心层注入诊断日志：写入 exe 同级 logs/dd-inject.log（与 DdRuntimeService 的日志同目录）。
// 只应记录状态变化或低频事件（如仲裁跳过、句柄打开失败），禁止在每次注入时无条件写入。
namespace DiagLog {

// 追加一行（自动带本地时间戳前缀），line 为 UTF-8/ASCII。
void Write(const std::string& line);

}  // namespace DiagLog
