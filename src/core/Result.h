#pragma once

#include <QString>
#include <variant>
#include <optional>

/// A simple Result<T> type that holds either a success value or an error message.
/// Similar to Rust's Result or tl::expected.
template <typename T>
class Result {
public:
    static Result<T> ok(const T &value) { return Result(value); }
    static Result<T> ok(T &&value) { return Result(std::move(value)); }
    static Result<T> err(const QString &error) { return Result(error, true); }

    bool isOk() const { return std::holds_alternative<T>(m_data); }
    bool isErr() const { return !isOk(); }

    const T &value() const { return std::get<T>(m_data); }
    T &value() { return std::get<T>(m_data); }
    const QString &error() const { return m_error; }

    /// Return value if ok, otherwise return the provided default.
    T valueOr(const T &defaultVal) const
    {
        return isOk() ? value() : defaultVal;
    }

    /// Apply a function to the ok value, propagating errors.
    template <typename F>
    auto map(F &&f) -> Result<decltype(f(std::declval<T>()))>
    {
        using U = decltype(f(std::declval<T>()));
        if (isOk()) return Result<U>::ok(f(value()));
        return Result<U>::err(error());
    }

private:
    Result(const T &value) : m_data(value) {}
    Result(T &&value) : m_data(std::move(value)) {}
    Result(const QString &error, bool) : m_error(error), m_data(std::monostate{}) {}

    std::variant<std::monostate, T> m_data;
    QString m_error;
};

/// Specialization for void (no success value, just ok/error).
template <>
class Result<void> {
public:
    static Result<void> ok() { return Result(); }
    static Result<void> err(const QString &error) { return Result(error); }

    bool isOk() const { return !m_error.has_value(); }
    bool isErr() const { return m_error.has_value(); }
    const QString &error() const { return *m_error; }

private:
    Result() = default;
    Result(const QString &error) : m_error(error) {}
    std::optional<QString> m_error;
};
