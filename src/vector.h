#pragma once


#include <cstdlib>
#include <new>
#include <utility>
#include <memory>
#include <algorithm>
#include <cassert>

template <typename T>
class RawMemory {
public:
    RawMemory() = default;

    explicit RawMemory(size_t capacity)
        : buffer_(Allocate(capacity))
        , capacity_(capacity) {
    }

    ~RawMemory() {
        Deallocate(buffer_);
    }
    
    RawMemory(const RawMemory&) = delete;
    RawMemory& operator=(const RawMemory& rhs) = delete;

    RawMemory(RawMemory&& other) noexcept 
    {
        Swap(other);
    }

    RawMemory& operator=(RawMemory&& rhs) noexcept
    {
        Swap(rhs);
        return *this;
    }

    T* operator+(size_t offset) noexcept {
        // Разрешается получать адрес ячейки памяти, следующей за последним элементом массива
        assert(offset <= capacity_);
        return buffer_ + offset;
    }

    const T* operator+(size_t offset) const noexcept {
        return const_cast<RawMemory&>(*this) + offset;
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<RawMemory&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < capacity_);
        return buffer_[index];
    }

    void Swap(RawMemory& other) noexcept {
        std::swap(buffer_, other.buffer_);
        std::swap(capacity_, other.capacity_);
    }

    const T* GetAddress() const noexcept {
        return buffer_;
    }

    T* GetAddress() noexcept {
        return buffer_;
    }

    size_t Capacity() const {
        return capacity_;
    }

private:
    // Выделяет сырую память под n элементов и возвращает указатель на неё
    static T* Allocate(size_t n) {
        return n != 0 ? static_cast<T*>(operator new(n * sizeof(T))) : nullptr;
    }

    // Освобождает сырую память, выделенную ранее по адресу buf при помощи Allocate
    static void Deallocate(T* buf) noexcept {
        operator delete(buf);
    }

    T* buffer_ = nullptr;
    size_t capacity_ = 0;
};

template <typename T>
class Vector {
public:

    Vector() = default;

    explicit Vector(size_t size)
        : data_(size)
        , size_(size)  //
    {
        std::uninitialized_value_construct_n(data_.GetAddress(), size);
    }

    Vector(const Vector& other)
        : data_(other.size_)
        , size_(other.size_)
    {
        std::uninitialized_copy_n(other.data_.GetAddress(), other.Size(), data_.GetAddress());
    }

    Vector(Vector&& other) noexcept
    {
        Swap(other);
    }

    using iterator = T*;
    using const_iterator = const T*;
    
    iterator begin() noexcept
    {
        return data_.GetAddress();
    }
    iterator end() noexcept
    {
        return data_ + size_;
    }
    const_iterator begin() const noexcept
    {
        return data_.GetAddress();
    }
    const_iterator end() const noexcept
    {
        return data_ + size_;
    }
    const_iterator cbegin() const noexcept
    {
        return begin();
    }
    const_iterator cend() const noexcept
    {
        return end();
    }


    Vector& operator=(const Vector& rhs)
    {
        if (this == &rhs)
        {
            return *this;
        }

        if (Capacity() >= rhs.size_)
        {
            std::copy_n(rhs.begin(), std::min(rhs.size_, size_), begin());

            if (size_ > rhs.size_)
            {
                std::destroy_n(data_ + rhs.size_, size_ - rhs.size_);
            }
            else
            {
                std::uninitialized_copy_n(rhs.data_ + size_, rhs.size_ - size_, data_ + size_);
            }
            size_ = rhs.size_;
        }
        else
        {
            Vector rhs_copy(rhs);
            Swap(rhs_copy);
        }

        return *this;
    }

    Vector& operator=(Vector&& rhs) noexcept
    {
        Swap(rhs);
        return *this;
    }

    void Swap(Vector& other) noexcept
    {
        data_.Swap(other.data_);
        std::swap(size_, other.size_);
    }

    void Reserve(size_t new_capacity)
    {
        if (new_capacity <= data_.Capacity())
        {
            return;
        }

        RawMemory<T> new_data(new_capacity);

        UninitCopyOrMove(data_.GetAddress(), size_, new_data.GetAddress());

        std::destroy_n(data_.GetAddress(), size_);
        data_.Swap(new_data);
    }

    void Resize(size_t new_size)
    {
        if (new_size > data_.Capacity())
        {
            Reserve(new_size);
            std::uninitialized_value_construct_n(data_ + size_, new_size - size_);
            size_ = new_size;
        }
        else
        {
            new_size < size_ ? std::destroy_n(data_ + new_size, size_ - new_size) : 0;
            size_ = new_size;
        }
    }

    template <typename... Args>
    T& EmplaceBack(Args&&... args)
    {
        return *(Emplace(end(), std::forward<Args>(args)...));
    }

    void PushBack(const T& value)
    {
        EmplaceBack(value);
    }

    void PushBack(T&& value)
    {
        EmplaceBack(std::move(value));
    }

    template <typename... Args>
    iterator Emplace(const_iterator pos, Args&&... args)
    {
        auto place_emplace_indent = std::distance(cbegin(), pos);

        if (data_.Capacity() < size_ + 1 || data_.Capacity() == size_)
        {
            size_t new_capacity = size_ == 0 ? 1 : size_ * 2;
            RawMemory<T> mem_buff{new_capacity};
            auto place_emplace = mem_buff + place_emplace_indent;

            UninitCopyOrMove(data_.GetAddress(), std::distance(cbegin(), pos), mem_buff.GetAddress());
            ConstructElement(place_emplace, std::forward<Args>(args)...);
            UninitCopyOrMove(const_cast<iterator>(pos), std::distance(pos, cend()), mem_buff + (std::distance(cbegin(), pos) + 1));

            std::destroy_n(data_.GetAddress(), size_);
            data_.Swap(mem_buff);
            ++size_;
        }
        else
        {
            auto place_emplace = data_ + place_emplace_indent;
            if (pos < end())
            {
                T buff{std::forward<Args>(args)...};

                ConstructElement(data_ + size_, std::forward<T>(*(end() - 1)));
                std::move_backward(place_emplace, end() - 1, end());

                *place_emplace = std::move(buff);
            }
            else
            {
                ConstructElement(place_emplace, std::forward<Args>(args)...);
            }

            ++size_;
        }
        return data_.GetAddress() + place_emplace_indent;
    }

    iterator Insert(const_iterator pos, const T& value)
    {
        return Emplace(pos, value);
    }

    iterator Insert(const_iterator pos, T&& value)
    {
        return Emplace(pos, std::move(value));
    }

    iterator Erase(const_iterator pos)
    {
        auto place_erase = const_cast<iterator>(cbegin() + std::distance(cbegin(), pos));

        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>)
        {
            std::move(place_erase + 1, end(), place_erase);
        }
        else
        {
            std::copy_n(place_erase + 1, std::distance(pos + 1, end()), place_erase);
        }

        PopBack();

        return place_erase;
    }

    void PopBack() noexcept
    {
        assert(size_ > 0);
        Destroy(data_ + (size_ - 1));
        --size_;
    }

    size_t Size() const noexcept
    {
        return size_;
    }

    size_t Capacity() const noexcept
    {
        return data_.Capacity();
    }

    const T& operator[](size_t index) const noexcept
    {
        return const_cast<Vector&>(*this)[index];
    }

    T& operator[](size_t index) noexcept 
    {
        assert(index < size_);
        return data_[index];
    }

    ~Vector() {
        std::destroy_n(data_.GetAddress(), size_);
    }

private:

    template <typename TFrom, typename TTo>
    void UninitCopyOrMove(TFrom from, size_t count, TTo to)
    {
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>)
        {
            std::uninitialized_move_n(from, count, to);
        } 
        else
        {
            std::uninitialized_copy_n(from, count, to);
        }
    }

    static void DestroyN(T* buf, size_t n) noexcept
    {
        for (size_t i = 0; i != n; ++i) {
            Destroy(buf + i);
        }
    }

    template <typename... T_forward>
    static void ConstructElement(T* buf, T_forward&&... elem)
    {
        new (buf) T(std::forward<T_forward>(elem)...);
    }

    static void Destroy(T* buf) noexcept
    {
        buf->~T();
    }

    RawMemory<T> data_;
    size_t size_ = 0;
};
