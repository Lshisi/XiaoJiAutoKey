#include "JsonLite.h"

#include <cctype>
#include <iomanip>
#include <sstream>

namespace jsonlite {

bool Value::IsNull() const { return std::holds_alternative<std::nullptr_t>(data); }
bool Value::IsBool() const { return std::holds_alternative<bool>(data); }
bool Value::IsNumber() const { return std::holds_alternative<double>(data); }
bool Value::IsString() const { return std::holds_alternative<std::string>(data); }
bool Value::IsArray() const { return std::holds_alternative<Array>(data); }
bool Value::IsObject() const { return std::holds_alternative<Object>(data); }

const bool* Value::AsBool() const { return std::get_if<bool>(&data); }
const double* Value::AsNumber() const { return std::get_if<double>(&data); }
const std::string* Value::AsString() const { return std::get_if<std::string>(&data); }
const Array* Value::AsArray() const { return std::get_if<Array>(&data); }
const Object* Value::AsObject() const { return std::get_if<Object>(&data); }

namespace {

class Parser {
public:
    explicit Parser(const std::string& text) : text_(text) {}

    bool ParseRoot(Value& value, std::string& error) {
        SkipWhitespace();
        if (!ParseValue(value, error)) {
            return false;
        }
        SkipWhitespace();
        if (pos_ != text_.size()) {
            error = "Trailing characters after JSON document";
            return false;
        }
        return true;
    }

private:
    bool ParseValue(Value& value, std::string& error) {
        if (pos_ >= text_.size()) {
            error = "Unexpected end of JSON";
            return false;
        }

        switch (text_[pos_]) {
        case 'n':
            return ParseLiteral("null", Value(nullptr), value, error);
        case 't':
            return ParseLiteral("true", Value(true), value, error);
        case 'f':
            return ParseLiteral("false", Value(false), value, error);
        case '"':
            return ParseStringValue(value, error);
        case '[':
            return ParseArray(value, error);
        case '{':
            return ParseObject(value, error);
        default:
            if (text_[pos_] == '-' || std::isdigit(static_cast<unsigned char>(text_[pos_])) != 0) {
                return ParseNumber(value, error);
            }
            error = "Invalid JSON token";
            return false;
        }
    }

    bool ParseLiteral(const char* literal, const Value& replacement, Value& value, std::string& error) {
        const std::size_t length = std::char_traits<char>::length(literal);
        if (text_.compare(pos_, length, literal) != 0) {
            error = "Invalid literal token";
            return false;
        }
        pos_ += length;
        value = replacement;
        return true;
    }

    bool ParseStringValue(Value& value, std::string& error) {
        std::string result;
        if (!ParseString(result, error)) {
            return false;
        }
        value = Value(std::move(result));
        return true;
    }

    bool ParseString(std::string& out, std::string& error) {
        if (text_[pos_] != '"') {
            error = "Expected string";
            return false;
        }
        ++pos_;
        out.clear();

        while (pos_ < text_.size()) {
            const char ch = text_[pos_++];
            if (ch == '"') {
                return true;
            }
            if (ch == '\\') {
                if (pos_ >= text_.size()) {
                    error = "Invalid string escape";
                    return false;
                }
                const char esc = text_[pos_++];
                switch (esc) {
                case '"':
                case '\\':
                case '/':
                    out.push_back(esc);
                    break;
                case 'b':
                    out.push_back('\b');
                    break;
                case 'f':
                    out.push_back('\f');
                    break;
                case 'n':
                    out.push_back('\n');
                    break;
                case 'r':
                    out.push_back('\r');
                    break;
                case 't':
                    out.push_back('\t');
                    break;
                case 'u':
                    error = "\\u escape is not supported";
                    return false;
                default:
                    error = "Unknown string escape";
                    return false;
                }
                continue;
            }
            out.push_back(ch);
        }

        error = "Unterminated string";
        return false;
    }

    bool ParseNumber(Value& value, std::string& error) {
        const std::size_t start = pos_;
        if (text_[pos_] == '-') {
            ++pos_;
        }
        while (pos_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[pos_])) != 0) {
            ++pos_;
        }
        if (pos_ < text_.size() && text_[pos_] == '.') {
            ++pos_;
            while (pos_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[pos_])) != 0) {
                ++pos_;
            }
        }
        if (pos_ < text_.size() && (text_[pos_] == 'e' || text_[pos_] == 'E')) {
            ++pos_;
            if (pos_ < text_.size() && (text_[pos_] == '+' || text_[pos_] == '-')) {
                ++pos_;
            }
            while (pos_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[pos_])) != 0) {
                ++pos_;
            }
        }

        try {
            value = Value(std::stod(text_.substr(start, pos_ - start)));
        } catch (...) {
            error = "Invalid number";
            return false;
        }
        return true;
    }

    bool ParseArray(Value& value, std::string& error) {
        ++pos_;
        Array array;
        SkipWhitespace();
        if (pos_ < text_.size() && text_[pos_] == ']') {
            ++pos_;
            value = Value(std::move(array));
            return true;
        }

        while (pos_ < text_.size()) {
            Value entry;
            SkipWhitespace();
            if (!ParseValue(entry, error)) {
                return false;
            }
            array.push_back(std::move(entry));
            SkipWhitespace();
            if (pos_ >= text_.size()) {
                break;
            }
            if (text_[pos_] == ']') {
                ++pos_;
                value = Value(std::move(array));
                return true;
            }
            if (text_[pos_] != ',') {
                error = "Expected ',' in array";
                return false;
            }
            ++pos_;
            SkipWhitespace();
        }

        error = "Unterminated array";
        return false;
    }

    bool ParseObject(Value& value, std::string& error) {
        ++pos_;
        Object object;
        SkipWhitespace();
        if (pos_ < text_.size() && text_[pos_] == '}') {
            ++pos_;
            value = Value(std::move(object));
            return true;
        }

        while (pos_ < text_.size()) {
            SkipWhitespace();
            std::string key;
            if (!ParseString(key, error)) {
                return false;
            }
            SkipWhitespace();
            if (pos_ >= text_.size() || text_[pos_] != ':') {
                error = "Expected ':' in object";
                return false;
            }
            ++pos_;
            SkipWhitespace();

            Value entry;
            if (!ParseValue(entry, error)) {
                return false;
            }
            object.emplace(std::move(key), std::move(entry));

            SkipWhitespace();
            if (pos_ >= text_.size()) {
                break;
            }
            if (text_[pos_] == '}') {
                ++pos_;
                value = Value(std::move(object));
                return true;
            }
            if (text_[pos_] != ',') {
                error = "Expected ',' in object";
                return false;
            }
            ++pos_;
        }

        error = "Unterminated object";
        return false;
    }

    void SkipWhitespace() {
        while (pos_ < text_.size() && std::isspace(static_cast<unsigned char>(text_[pos_])) != 0) {
            ++pos_;
        }
    }

    const std::string& text_;
    std::size_t pos_ = 0;
};

std::string EscapeString(const std::string& text) {
    std::ostringstream out;
    for (const unsigned char ch : text) {
        switch (ch) {
        case '"':
            out << "\\\"";
            break;
        case '\\':
            out << "\\\\";
            break;
        case '\b':
            out << "\\b";
            break;
        case '\f':
            out << "\\f";
            break;
        case '\n':
            out << "\\n";
            break;
        case '\r':
            out << "\\r";
            break;
        case '\t':
            out << "\\t";
            break;
        default:
            out << static_cast<char>(ch);
            break;
        }
    }
    return out.str();
}

void StringifyImpl(const Value& value, std::ostringstream& out, int indent, int depth) {
    if (const auto* boolean = value.AsBool()) {
        out << (*boolean ? "true" : "false");
        return;
    }
    if (const auto* number = value.AsNumber()) {
        out << std::setprecision(15) << *number;
        return;
    }
    if (const auto* text = value.AsString()) {
        out << '"' << EscapeString(*text) << '"';
        return;
    }
    if (const auto* array = value.AsArray()) {
        out << "[";
        if (!array->empty()) {
            for (std::size_t i = 0; i < array->size(); ++i) {
                out << "\n" << std::string((depth + 1) * indent, ' ');
                StringifyImpl((*array)[i], out, indent, depth + 1);
                if (i + 1 != array->size()) {
                    out << ",";
                }
            }
            out << "\n" << std::string(depth * indent, ' ');
        }
        out << "]";
        return;
    }
    if (const auto* object = value.AsObject()) {
        out << "{";
        if (!object->empty()) {
            std::size_t index = 0;
            for (const auto& [key, child] : *object) {
                out << "\n" << std::string((depth + 1) * indent, ' ')
                    << '"' << EscapeString(key) << "\": ";
                StringifyImpl(child, out, indent, depth + 1);
                if (++index != object->size()) {
                    out << ",";
                }
            }
            out << "\n" << std::string(depth * indent, ' ');
        }
        out << "}";
        return;
    }

    out << "null";
}

}  // namespace

bool Parse(const std::string& text, Value& value, std::string& error) {
    Parser parser(text);
    return parser.ParseRoot(value, error);
}

std::string Stringify(const Value& value, int indent) {
    std::ostringstream out;
    StringifyImpl(value, out, indent, 0);
    return out.str();
}

}  // namespace jsonlite
