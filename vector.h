#pragma once

#include <cassert>
#include <cstdlib>
#include <new>
#include <utility>
#include <memory>
#include <algorithm>

#include "rawmemory.h"

template <typename T>
class Vector {
public:
    Vector() = default;

    explicit Vector(size_t size)
            : data_(size)
            , size_(size) {
        std::uninitialized_value_construct_n(data_.GetAddress(), size);
    }

    Vector(const Vector& other)
            : data_(other.size_)
            , size_(other.size_) {
        std::uninitialized_copy_n(other.data_.GetAddress(), other.size_, data_.GetAddress());
    }

    Vector(Vector&& other) noexcept
            : data_(std::move(other.data_))
            , size_(other.size_) {
        other.size_ = 0;
    }

    Vector& operator=(const Vector& other) {
        if (this != &other) {
            if (other.size_ > data_.Capacity()) {
                Vector temp(other);
                Swap(temp);
            } else {
                size_t min_size = std::min(size_, other.size_);
                std::copy_n(other.data_.GetAddress(), min_size, data_.GetAddress());
                if (size_ < other.size_) {
                    std::uninitialized_copy_n(other.data_.GetAddress() + min_size, other.size_ - min_size,
                                              data_.GetAddress() + min_size);
                } else {
                    std::destroy_n(data_.GetAddress() + other.size_, size_ - other.size_);
                }
                size_ = other.size_;
            }
        }
        return *this;
    }

    Vector& operator=(Vector&& other) noexcept {
        if (this != &other) {
            data_.Swap(other.data_);
            std::swap(size_, other.size_);
        }
        return *this;
    }

    ~Vector() {
        std::destroy_n(data_.GetAddress(), size_);
    }

    void Reserve(size_t new_capacity) {
        if (new_capacity <= data_.Capacity()) {
            return;
        }

        RawMemory<T> new_data(new_capacity);
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
        } else {
            std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
        }
        std::destroy_n(data_.GetAddress(), size_);
        data_.Swap(new_data);
    }

    void Resize(size_t new_size) {
        if (new_size < size_) {
            std::destroy_n(data_.GetAddress() + new_size, size_ - new_size);
            size_ = new_size;
        } else if (new_size > size_) {
            Reserve(new_size);
            std::uninitialized_value_construct_n(data_.GetAddress() + size_, new_size - size_);
            size_ = new_size;
        }
    }

    void PushBack(const T& value) {
        if (size_ < Capacity()) {
            ::new (data_.GetAddress() + size_) T(value);
            ++size_;
            return;
        }
        ReallocateAndPush(value);
    }

    void PushBack(T&& value) {
        if (size_ < Capacity()) {
            ::new (data_.GetAddress() + size_) T(std::move(value));
            ++size_;
            return;
        }
        ReallocateAndPush(std::move(value));
    }

    void PopBack() noexcept {
        assert(size_ > 0);
        std::destroy_at(data_ + size_ - 1);
        --size_;
    }

    template <typename... Args>
    T& EmplaceBack(Args&&... args) {
        if (size_ < Capacity()) {
            ::new (data_.GetAddress() + size_) T(std::forward<Args>(args)...);
            ++size_;
            return data_[size_ - 1];
        }
        return ReallocateAndEmplace(std::forward<Args>(args)...);
    }

    void Swap(Vector& other) noexcept {
        data_.Swap(other.data_);
        std::swap(size_, other.size_);
    }

    [[nodiscard]] size_t Size() const noexcept {
        return size_;
    }

    [[nodiscard]] size_t Capacity() const noexcept {
        return data_.Capacity();
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<Vector&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < size_);
        return data_[index];
    }

private:
    RawMemory<T> data_;
    size_t size_ = 0;

private:
    template <class U>
    void ReallocateAndPush(U&& value) {
        RawMemory<T> data_copy = RawMemory<T>(size_ == 0 ? 1 : size_ * 2);
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(data_.GetAddress(), size_, data_copy.GetAddress());
        } else {
            std::uninitialized_copy_n(data_.GetAddress(), size_, data_copy.GetAddress());
        }

        new (data_copy + size_) T(std::forward<U>(value));

        std::destroy_n(data_.GetAddress(), size_);
        data_.Swap(data_copy);
        ++size_;
    }

    template <typename... Args>
    T& ReallocateAndEmplace(Args&&... args) {
        RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
        T* const new_begin = new_data.GetAddress();
        T* const dest      = new_begin + size_;

        ::new (dest) T(std::forward<Args>(args)...);

        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            size_t i = 0;
            try {
                for (; i < size_; ++i) {
                    ::new (new_begin + i) T(std::move(data_[i]));
                }
            } catch (...) {
                std::destroy_n(new_begin, i);
                std::destroy_at(dest);
                throw;
            }
        } else {
            size_t i = 0;
            try {
                for (; i < size_; ++i) {
                    ::new (new_begin + i) T(data_[i]);
                }
            } catch (...) {
                std::destroy_n(new_begin, i);
                std::destroy_at(dest);
                throw;
            }
        }

        std::destroy_n(data_.GetAddress(), size_);
        data_.Swap(new_data);
        ++size_;
        return data_[size_ - 1];
    }
};