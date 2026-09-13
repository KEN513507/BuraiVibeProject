#pragma once
#include <cstddef>
#include <new>
#include <utility>
#include <algorithm>

namespace boss {

template <typename T, size_t N = 4>
class SmallVector {
private:
    alignas(T) unsigned char inline_buf_[sizeof(T) * N];
    T* data_ = reinterpret_cast<T*>(inline_buf_);
    size_t size_ = 0;
    size_t cap_ = N;
    T* heap_ = nullptr;

    bool is_small() const { return data_ == reinterpret_cast<const T*>(inline_buf_); }

public:
    SmallVector() = default;

    ~SmallVector() {
        clear();
        if (!is_small()) {
            delete[] reinterpret_cast<unsigned char*>(heap_);
        }
    }

    SmallVector(const SmallVector& other) {
        reserve(other.size_);
        for (size_t i = 0; i < other.size_; ++i) {
            push_back(other[i]);
        }
    }

    SmallVector& operator=(const SmallVector& other) {
        if (this != &other) {
            clear();
            reserve(other.size_);
            for (size_t i = 0; i < other.size_; ++i) {
                push_back(other[i]);
            }
        }
        return *this;
    }

    void push_back(const T& val) {
        if (size_ >= cap_) {
            reserve(cap_ == 0 ? N : cap_ * 2);
        }
        new (&data_[size_++]) T(val);
    }

    void reserve(size_t new_cap) {
        if (new_cap <= cap_) return;
        T* new_buf = reinterpret_cast<T*>(new unsigned char[sizeof(T) * new_cap]);
        for (size_t i = 0; i < size_; ++i) {
            new (&new_buf[i]) T(std::move(data_[i]));
            data_[i].~T();
        }
        if (!is_small()) {
            delete[] reinterpret_cast<unsigned char*>(heap_);
        }
        heap_ = new_buf;
        data_ = new_buf;
        cap_ = new_cap;
    }

    void clear() {
        for (size_t i = 0; i < size_; ++i) {
            data_[i].~T();
        }
        size_ = 0;
    }

    size_t size() const { return size_; }
    T& operator[](size_t idx) { return data_[idx]; }
    const T& operator[](size_t idx) const { return data_[idx]; }
    T* begin() { return data_; }
    T* end() { return data_ + size_; }
    const T* begin() const { return data_; }
    const T* end() const { return data_ + size_; }
};

}
