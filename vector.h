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
    using iterator = T*;
    using const_iterator = const T*;

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

    iterator begin() noexcept {
        return data_.GetAddress();
    }
    iterator end() noexcept {
        return data_.GetAddress() + size_;
    }
    const_iterator begin() const noexcept {
        return data_.GetAddress();
    }
    const_iterator end() const noexcept {
        return data_.GetAddress() + size_;
    }
    const_iterator cbegin() const noexcept {
        return data_.GetAddress();
    }
    const_iterator cend() const noexcept {
        return data_.GetAddress() + size_;
    }

    iterator Insert(const_iterator pos, const T& value) {
        return Emplace(pos, value);
    }

    iterator Insert(const_iterator pos, T&& value) {
        return Emplace(pos, std::move(value));
    }

    template <typename... Args>
    iterator Emplace(const_iterator pos, Args&&... args) {
        size_t offset = pos - cbegin();

        if (size_ == Capacity()) {
            RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
            std::construct_at(new_data + offset, std::forward<Args>(args)...);

            try {
                ShiftDataToNewMemory(data_.GetAddress(), offset, new_data.GetAddress());
            } catch (...) {
                std::destroy_n(new_data.GetAddress() + offset, 1);
                throw;
            }

            try {
                ShiftDataToNewMemory(data_.GetAddress() + offset, size_ - offset, new_data.GetAddress() + offset + 1);
            } catch (...) {
                std::destroy_n(new_data.GetAddress(), offset + 1);
                throw;
            }

            std::destroy_n(data_.GetAddress(), size_);

            data_.Swap(new_data);
        } else {
            if (pos == cend()) {
                std::construct_at(end(), std::forward<Args>(args)...);
            } else {
                T temp_val(std::forward<Args>(args)...);

                std::construct_at(end(), std::move(data_[size_ - 1]));
                std::move_backward(begin() + offset, end() - 1, end());
                data_[offset] = std::move(temp_val);
            }
        }

        ++size_;
        return begin() + offset;
    }

    iterator Erase(const_iterator pos) {
        size_t offset = pos - cbegin();

        std::move(begin() + offset + 1, end(), begin() + offset);
        std::destroy_at(data_.GetAddress() + offset);
        --size_;

        return begin() + offset;
    }

    void Reserve(size_t new_capacity) {
        if (new_capacity <= data_.Capacity()) {
            return;
        }

        RawMemory<T> new_data(new_capacity);
        ShiftDataToNewMemory(data_.GetAddress(), size_, new_data.GetAddress());
        std::destroy_n(data_.GetAddress(), size_);

        data_.Swap(new_data);
    }

    void Resize(size_t new_size) {
        if (new_size > Capacity()) {
            Reserve(new_size);
        }

        if (new_size > size_) {
            std::uninitialized_value_construct_n(data_.GetAddress() + size_, new_size - size_);
        } else {
            std::destroy_n(data_.GetAddress() + new_size, size_ - new_size);
        }

        size_ = new_size;
    }

    template <typename Val>
    void PushBack(Val&& value) {
        EmplaceBack(std::forward<Val>(value));
    }

    void PopBack() noexcept {
        assert(size_ > 0);

        std::destroy_at(data_ + size_ - 1);
        --size_;
    }

    template <typename... Args>
    T& EmplaceBack(Args&&... args) {
        iterator it = Emplace(end(), std::forward<Args>(args)...);
        return *it;
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


    void ShiftDataToNewMemory(T* old_buf, size_t count, T* new_buf) {
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(old_buf, count, new_buf);
        } else {
            std::uninitialized_copy_n(old_buf, count, new_buf);
        }
    }
};