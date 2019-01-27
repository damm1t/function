//
// Created by damm1t on 1/26/19.
//

#ifndef FUNCTION_FUNCTION_H
#define FUNCTION_FUNCTION_H

#include <memory>
#include <variant>


template<typename R, typename... Args>
struct callable {
    callable() = default;

    virtual ~callable() = default;

    virtual R call(Args &&... args) = 0;

    virtual std::unique_ptr<callable> copy() = 0;

    virtual void clone(char *to) const = 0;

    virtual void move_by_pointer(char *to) const = 0;

    void operator=(callable const &) = delete;

    callable(callable const &) = delete;
};

template<typename F, typename R, typename... Args>
struct callable_impl : callable<R, Args...> {
    callable_impl(F func) : callable<R, Args...>(), func(std::move(func)) {}

    std::unique_ptr<callable<R, Args...>> copy() override {
        return std::make_unique<callable_impl<F, R, Args...>>(func);
    }

    void clone(char *const to) const override {
        new(to) callable_impl<F, R, Args...>(func);
    }

    void move_by_pointer(char *const to) const override {
        new(to) callable_impl<F, R, Args...>(std::move(func));
    }

    R call(Args &&... args) override {
        return func(std::forward<Args>(args)...);
    }

    virtual ~callable_impl() = default;

private:
    F func;
};


template<typename T>
class function;

const int MAX_SIZE = 18;

template<typename R, typename... Args>
class function<R(Args...)> {
    using smallT =  std::array<char, MAX_SIZE>;
    using bigT = std::unique_ptr<callable<R, Args...>>;
public:
    function() noexcept : call_v(nullptr) {}

    function(std::nullptr_t) noexcept : call_v(nullptr) {}

    function(function const &other) : call_v(nullptr) {
        if (std::holds_alternative<smallT>(other.call_v)) {
            if (std::holds_alternative<bigT>(call_v)) {
                call_v = std::array<char, MAX_SIZE>();
            }
            reinterpret_cast<callable<R, Args...> *>(std::get<smallT>(other.call_v).data())->clone(
                    std::get<smallT>(call_v).data());
        } else if (std::get<bigT>(other.call_v)) {
            call_v = std::get<bigT>(other.call_v)->copy();
        } else {
            call_v = nullptr;
        }
    }

    function(function &&other) noexcept : call_v(nullptr) {
        if (std::holds_alternative<bigT>(other.call_v)) {
            call_v.swap(other.call_v);
        } else {
            call_v = std::array<char, MAX_SIZE>();
            reinterpret_cast<callable<R, Args...> *>(std::get<smallT>(other.call_v).data())->clone(
                    std::get<smallT>(call_v).data());
        }
    }

    template<typename F>
    function(F func) : call_v(nullptr) {
        if (sizeof(callable_impl<F, R, Args...>) <= sizeof(smallT)) {
            if (std::holds_alternative<bigT>(call_v)) {
                call_v = std::array<char, MAX_SIZE>();
            }
            new(std::get<smallT>(call_v).data()) callable_impl<F, R, Args...>(std::move(func));
        } else {
            call_v = std::make_unique<callable_impl<F, R, Args...>>(std::move(func));
        }
    }

    ~function() {
        clear_small();
    }

    function &operator=(function const &other) {
        if (&other == this) {
            return *this;
        }
        function(other).swap(*this);
        return *this;
    }

    function &operator=(function &&other) noexcept {
        if (&other == this) {
            return *this;
        }
        if (std::holds_alternative<bigT>(other.call_v)) {
            bigT tmp(std::move(std::get<bigT>(other.call_v)));
            if (std::holds_alternative<smallT>(call_v)) {
                other.call_v = std::array<char, MAX_SIZE>();
                reinterpret_cast<callable<R, Args...> *>(std::get<smallT>(call_v).data())->move_by_pointer(
                        std::get<smallT>(other.call_v).data());
            } else {
                other.call_v = std::move(std::get<bigT>(call_v));
            }
            call_v = std::move(tmp);
        } else {
            clear_small();
            if (std::holds_alternative<bigT>(call_v)) {
                call_v = std::array<char, MAX_SIZE>();
            }
            reinterpret_cast<callable<R, Args...> *>(std::get<smallT>(other.call_v).data())->move_by_pointer(
                    std::get<smallT>(call_v).data());
        }
        return *this;
    }

    void swap(function &other) {
        if (&other != this) {
            function tmp(std::move(other));
            other = std::move(*this);
            *this = std::move(tmp);
        }
    }

    explicit operator bool() const noexcept {
        return std::holds_alternative<smallT>(call_v) || std::get<bigT>(call_v);
    }

    R operator()(Args &&... args) {
        if (std::holds_alternative<smallT>(call_v))
            return reinterpret_cast<callable<R, Args...> *>(std::get<smallT>(call_v).data())->call(
                    std::forward<Args>(args)...);
        else
            return std::get<bigT>(call_v)->call(std::forward<Args>(args)...);
    }


private:
    mutable std::variant<bigT, smallT> call_v;

    void clear_small() {
        if (std::holds_alternative<smallT>(call_v)) {
            reinterpret_cast<callable<R, Args...> *>(std::get<smallT>(call_v).data())->~callable();
        }
    }
};

#endif //FUNCTION_FUNCTION_H
