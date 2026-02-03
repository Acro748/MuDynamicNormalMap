#pragma once

namespace Mus {
    class lString {
        std::string str;

    public:
        lString() = default;
        lString(const char* s) : str(lower(s)) {}
        lString(const std::string& s) : str(lower(s)) {}
        lString(const lString& s) : str(s.str) {}

        lString& operator=(const char* s) {
            str = lower(s);
            return *this;
        }
        lString& operator=(const std::string& s) {
            str = lower(s);
            return *this;
        }
        lString& operator=(const lString& s) {
            str = s.str;
            return *this;
        }

        friend lString operator+(const char* left, const lString& right) {
            return lString(left).str + right.str;
        }
        friend lString operator+(const lString& left, const char* right) {
            return left.str + lString(right).str;
        }
        friend lString operator+(const std::string& left, const lString& right) {
            return lString(left).str + right.str;
        }
        friend lString operator+(const lString& left, const std::string& right) {
            return left.str + lString(right).str;
        }

        lString& operator+=(const char* other) {
            str += lower(other);
            return *this;
        }
        lString& operator+=(const std::string& other) {
            str += lower(other);
            return *this;
        }
        lString& operator+=(const lString& other) {
            str += other.str;
            return *this;
        }

        bool operator==(const char* other) const {
            return str == lower(other);
        }
        bool operator==(const std::string& other) const {
            return str == lower(other);
        }
        bool operator==(const lString& other) const {
            return str == other.str;
        }

        operator const std::string&() const {
            return str;
        }
        operator std::string&() {
            return str;
        }
        operator std::string_view() const {
            return std::string_view(str);
        }

        const char* c_str() const {
            return str.c_str();
        }
        std::string string() const {
            return str;
        }
        bool empty() const {
            return str.empty();
        }

        bool contains(const char* other) const {
            return str.contains(lower(other));
        }
        bool contains(const std::string& other) const {
            return str.contains(lower(other));
        }
        bool contains(const lString& other) const {
            return str.contains(other.str);
        }

        bool starts_with(const char* prefix) const {
            return str.starts_with(lower(prefix));
        }
        bool starts_with(const std::string& prefix) const {
            return str.starts_with(lower(prefix));
        }
        bool starts_with(const lString& prefix) const {
            return str.starts_with(prefix.str);
        }

        lString& remove_starts_with(const char* prefix) {
            if (starts_with(prefix))
                str = std::string(str.begin() + std::strlen(prefix), str.end());
            return *this;
        }
        lString& remove_starts_with(const std::string& prefix) {
            if (starts_with(prefix))
                str = std::string(str.begin() + prefix.size(), str.end());
            return *this;
        }
        lString& remove_starts_with(const lString& prefix) {
            if (starts_with(prefix))
                str = std::string(str.begin() + prefix.str.size(), str.end());
            return *this;
        }

        bool ends_with(const char* suffix) const {
            return str.ends_with(lower(suffix));
        }
        bool ends_with(const std::string& suffix) const {
            return str.ends_with(lower(suffix));
        }
        bool ends_with(const lString& suffix) const {
            return str.ends_with(suffix.str);
        }

        lString& remove_ends_with(const char* suffix) {
            if (starts_with(suffix))
                str = std::string(str.begin(), str.end() - std::strlen(suffix));
            return *this;
        }
        lString& remove_ends_with(const std::string& suffix) {
            if (starts_with(suffix))
                str = std::string(str.begin(), str.end() - suffix.size());
            return *this;
        }
        lString& remove_ends_with(const lString& suffix) {
            if (starts_with(suffix))
                str = std::string(str.begin(), str.end() - suffix.str.size());
            return *this;
        }

    private:
        std::string lower(std::string s) const {
            std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
            return s;
        }
    };
} // namespace Mus

namespace std {
    template <>
    struct hash<Mus::lString> {
        size_t operator()(const Mus::lString& k) const {
            return std::hash<std::string>()(k.string());
        }
    };
} // namespace std

namespace fmt {
    template <>
    struct formatter<Mus::lString> : formatter<std::string> {
        auto format(const Mus::lString& obj, format_context& ctx) const {
            return formatter<std::string>::format(obj.string(), ctx);
        }
    };
} // namespace fmt