#pragma once

#include <functional>
#include <string>

namespace xiaoji {
void CheckPackageVersion(const std::wstring& infPath);
std::wstring StageDriverPackage(const std::wstring& infPath);
std::wstring BoundOem(int index, bool requireStarted, const std::wstring& allowedInfs, bool includeAbsent = false);
void EnsureDevices(const std::wstring& allowedInfs, const std::wstring& priorInfs, bool forceRecreate = false);
void RemoveXiaojiDevices(const std::wstring& allowedInfs);
std::wstring SavedRemovalPackage(int index);
void RemoveXiaojiConfiguration();
void WithOperationLock(const std::function<void()>& operation);
}
