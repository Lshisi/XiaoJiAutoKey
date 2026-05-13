#pragma once

#include <map>
#include <string>
#include <variant>
#include <vector>

namespace jsonlite {

struct Value;

using Object = std::map<std::string, Value>;
using Array = std::vector<Value>;

struct Value {
    using Storage = std::variant<std::nullptr_t, bool, double, std::string, Array, Object>;

    Storage data = nullptr;

    Value() = default;
    Value(std::nullptr_t) : data(nullptr) {}
    Value(bool value) : data(value) {}
    Value(double value) : data(value) {}
    Value(int value) : data(static_cast<double>(value)) {}
    Value(std::string value) : data(std::move(value)) {}
    Value(Array value) : data(std::move(value)) {}
    Value(Object value) : data(std::move(value)) {}

    [[nodiscard]] bool IsNull() const;
    [[nodiscard]] bool IsBool() const;
    [[nodiscard]] bool IsNumber() const;
    [[nodiscard]] bool IsString() const;
    [[nodiscard]] bool IsArray() const;
    [[nodiscard]] bool IsObject() const;

    [[nodiscard]] const bool* AsBool() const;
    [[nodiscard]] const double* AsNumber() const;
    [[nodiscard]] const std::string* AsString() const;
    [[nodiscard]] const Array* AsArray() const;
    [[nodiscard]] const Object* AsObject() const;
};

bool Parse(const std::string& text, Value& value, std::string& error);
std::string Stringify(const Value& value, int indent = 2);

}  // namespace jsonlite
