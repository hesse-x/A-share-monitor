#ifndef RING_BUFFER_H
#define RING_BUFFER_H
#include <array>
#include <cstddef>
#include <iterator>
#include <stdexcept>
#include <type_traits>
#include <utility>

template <typename T, size_t Capacity = 240> class ring_buffer {
public:
  // Type aliases following STL naming conventions
  using value_type = T;
  using reference = T &;
  using const_reference = const T &;
  using pointer = T *;
  using const_pointer = const T *;
  using size_type = size_t;
  using difference_type = ptrdiff_t;

private:
  // Templated iterator implementation, distinguishing const and non-const
  // iterators via IsConst parameter
  template <bool IsConst> class iterator_impl {
  public:
    // Iterator type aliases
    using iterator_category = std::random_access_iterator_tag;
    using value_type = T;
    using difference_type = ptrdiff_t;
    using reference = std::conditional_t<IsConst, const T &, T &>;
    using pointer = std::conditional_t<IsConst, const T *, T *>;
    using container_type =
        std::conditional_t<IsConst, const ring_buffer, ring_buffer>;

    // Constructor
    constexpr iterator_impl(container_type *buf, size_type pos) noexcept
        : buffer_(buf), position_(pos) {}

    // Copy constructor (supports conversion from non-const iterator to const
    // iterator)
    template <bool OtherIsConst,
              typename = std::enable_if_t<IsConst && !OtherIsConst>>
    constexpr iterator_impl(const iterator_impl<OtherIsConst> &other) noexcept
        : buffer_(other.buffer_), position_(other.position_) {}

    // Dereference operation
    constexpr reference operator*() const noexcept {
      return buffer_->data_[position_];
    }

    constexpr pointer operator->() const noexcept {
      return &buffer_->data_[position_];
    }

    // Pre-increment
    constexpr iterator_impl &operator++() noexcept {
      position_ = (position_ + 1) % Capacity;
      return *this;
    }

    // Post-increment
    constexpr iterator_impl operator++(int) noexcept {
      auto temp = *this;
      ++(*this);
      return temp;
    }

    // Pre-decrement
    constexpr iterator_impl &operator--() noexcept {
      position_ = (position_ == 0) ? Capacity - 1 : position_ - 1;
      return *this;
    }

    // Post-decrement
    constexpr iterator_impl operator--(int) noexcept {
      auto temp = *this;
      --(*this);
      return temp;
    }

    // Addition operation
    constexpr iterator_impl operator+(difference_type n) const noexcept {
      auto temp = *this;
      temp += n;
      return temp;
    }

    // Addition assignment
    constexpr iterator_impl &operator+=(difference_type n) noexcept {
      n %= static_cast<difference_type>(Capacity);
      if (n < 0)
        n += static_cast<difference_type>(Capacity);
      position_ = (position_ + static_cast<size_type>(n)) % Capacity;
      return *this;
    }

    // Subtraction operation
    constexpr iterator_impl operator-(difference_type n) const noexcept {
      return *this + (-n);
    }

    // Subtraction assignment
    constexpr iterator_impl &operator-=(difference_type n) noexcept {
      return *this += (-n);
    }

    // Iterator difference
    constexpr difference_type
    operator-(const iterator_impl &other) const noexcept {
      return static_cast<difference_type>(position_ - other.position_);
    }

    // Subscript access
    constexpr reference operator[](difference_type n) const noexcept {
      return *(*this + n);
    }

    // Comparison operators
    constexpr bool operator==(const iterator_impl &other) const noexcept {
      return buffer_ == other.buffer_ && position_ == other.position_;
    }

    constexpr bool operator!=(const iterator_impl &other) const noexcept {
      return !(*this == other);
    }

    constexpr bool operator<(const iterator_impl &other) const noexcept {
      return position_ < other.position_;
    }

    constexpr bool operator>(const iterator_impl &other) const noexcept {
      return other < *this;
    }

    constexpr bool operator<=(const iterator_impl &other) const noexcept {
      return !(*this > other);
    }

    constexpr bool operator>=(const iterator_impl &other) const noexcept {
      return !(*this < other);
    }

    // Friend: allow other iterators to access private members
    template <bool> friend class iterator_impl;

  private:
    friend class ring_buffer;
    container_type *buffer_; // Pointer to the owning container
    size_type position_;     // Current position index
  };

public:
  // Public iterator types following STL naming conventions
  using iterator = iterator_impl<false>;
  using const_iterator = iterator_impl<true>;
  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;

  // Constructor
  constexpr ring_buffer() noexcept : head_(0), tail_(0), size_(0) {}

  // Iterator interfaces
  constexpr iterator begin() noexcept { return iterator(this, head_); }
  constexpr iterator end() noexcept { return iterator(this, tail_); }
  constexpr const_iterator begin() const noexcept {
    return const_iterator(this, head_);
  }
  constexpr const_iterator end() const noexcept {
    return const_iterator(this, tail_);
  }
  constexpr const_iterator cbegin() const noexcept {
    return const_iterator(this, head_);
  }
  constexpr const_iterator cend() const noexcept {
    return const_iterator(this, tail_);
  }

  // Reverse iterators
  constexpr reverse_iterator rbegin() noexcept {
    return reverse_iterator(end());
  }
  constexpr reverse_iterator rend() noexcept {
    return reverse_iterator(begin());
  }
  constexpr const_reverse_iterator rbegin() const noexcept {
    return const_reverse_iterator(end());
  }
  constexpr const_reverse_iterator rend() const noexcept {
    return const_reverse_iterator(begin());
  }
  constexpr const_reverse_iterator crbegin() const noexcept {
    return const_reverse_iterator(cend());
  }
  constexpr const_reverse_iterator crend() const noexcept {
    return const_reverse_iterator(cbegin());
  }

  // Capacity-related interfaces
  constexpr size_type size() const noexcept { return size_; }
  constexpr size_type capacity() const noexcept { return Capacity; }
  constexpr bool empty() const noexcept { return size_ == 0; }
  constexpr bool full() const noexcept { return size_ == Capacity; }

  // Element access
  constexpr reference operator[](size_type n) {
    if (n >= size_) {
      throw std::out_of_range("ring_buffer index out of range");
    }
    size_type pos = (head_ + n) % Capacity;
    return data_[pos];
  }

  constexpr const_reference operator[](size_type n) const {
    if (n >= size_) {
      throw std::out_of_range("ring_buffer index out of range");
    }
    size_type pos = (head_ + n) % Capacity;
    return data_[pos];
  }

  constexpr reference front() {
    if (empty())
      throw std::underflow_error("ring_buffer is empty");
    return data_[head_];
  }

  constexpr const_reference front() const {
    if (empty())
      throw std::underflow_error("ring_buffer is empty");
    return data_[head_];
  }

  constexpr reference back() {
    if (empty())
      throw std::underflow_error("ring_buffer is empty");
    return data_[(tail_ - 1 + Capacity) % Capacity];
  }

  constexpr const_reference back() const {
    if (empty())
      throw std::underflow_error("ring_buffer is empty");
    return data_[(tail_ - 1 + Capacity) % Capacity];
  }

  // Insert element at the end (overwrite oldest element when full)
  constexpr void
  push_back(const T &value) noexcept(std::is_nothrow_copy_assignable_v<T>) {
    if (full()) {
      head_ = (head_ + 1) % Capacity; // Remove the oldest element
      --size_;
    }

    data_[tail_] = value;
    tail_ = (tail_ + 1) % Capacity;
    ++size_;
  }
  constexpr void
  push_back(size_type n,
            const T &v) noexcept(std::is_nothrow_copy_assignable_v<T>) {
    if (n == 0)
      return; // Handle boundary case of 0 elements

    // Calculate the number of old elements to be overwritten (if buffer will be
    // full)
    size_type overflow = (size_ + n > Capacity) ? (size_ + n - Capacity) : 0;

    if (overflow > 0) {
      // Move head pointer to overwrite old elements
      head_ = (head_ + overflow) % Capacity;
      size_ -= overflow;
    }

    // Batch insert new elements
    size_type remaining = n;
    size_type current_tail = tail_;

    while (remaining > 0) {
      // Calculate the number of elements that can be inserted in the current
      // batch (space to physical end of buffer)
      size_type batch = std::min(remaining, Capacity - current_tail);

      // Fill the current batch
      for (size_type i = 0; i < batch; ++i) {
        data_[current_tail + i] = v;
      }

      current_tail = (current_tail + batch) % Capacity;
      remaining -= batch;
    }

    // Update tail pointer and size
    tail_ = current_tail;
    size_ += n;
  }

  // Insert element at the end (rvalue reference version)
  constexpr void
  push_back(T &&value) noexcept(std::is_nothrow_move_assignable_v<T>) {
    if (full()) {
      head_ = (head_ + 1) % Capacity; // Remove the oldest element
      --size_;
    }

    data_[tail_] = std::move(value);
    tail_ = (tail_ + 1) % Capacity;
    ++size_;
  }

  // Insert element at specified position
  constexpr iterator insert(const_iterator pos, const T &value) {
    if (full()) {
      throw std::length_error("ring_buffer is full");
    }

    // Get insertion position
    size_type insert_pos = pos.position_;
    size_type current = tail_;

    // Shift elements to make space for new element
    while (current != insert_pos) {
      size_type prev = (current - 1 + Capacity) % Capacity;
      data_[current] = data_[prev];
      current = prev;
    }

    // Insert new element
    data_[insert_pos] = value;
    tail_ = (tail_ + 1) % Capacity;
    ++size_;

    // Adjust head if inserting at head position
    if (insert_pos == head_) {
      head_ = (head_ - 1 + Capacity) % Capacity;
    }

    return iterator(this, insert_pos);
  }

  // Insert rvalue version
  constexpr iterator insert(const_iterator pos, T &&value) {
    if (full()) {
      throw std::length_error("ring_buffer is full");
    }

    size_type insert_pos = pos.position_;
    size_type current = tail_;

    while (current != insert_pos) {
      size_type prev = (current - 1 + Capacity) % Capacity;
      data_[current] = std::move(data_[prev]);
      current = prev;
    }

    data_[insert_pos] = std::move(value);
    tail_ = (tail_ + 1) % Capacity;
    ++size_;

    if (insert_pos == head_) {
      head_ = (head_ - 1 + Capacity) % Capacity;
    }

    return iterator(this, insert_pos);
  }

  // Erase element at specified position
  constexpr iterator erase(const_iterator pos) {
    if (empty()) {
      throw std::underflow_error("ring_buffer is empty");
    }

    size_type erase_pos = pos.position_;
    size_type current = erase_pos;

    // Shift elements to overwrite the erased element
    while (current != tail_) {
      size_type next = (current + 1) % Capacity;
      data_[current] = data_[next];
      current = next;
    }

    // Adjust tail
    tail_ = (tail_ - 1 + Capacity) % Capacity;
    --size_;

    // Adjust head if erasing the head element
    if (erase_pos == head_) {
      head_ = (head_ + 1) % Capacity;
      return iterator(this, head_);
    }

    return iterator(this, erase_pos);
  }

  // Clear the buffer
  constexpr void clear() noexcept {
    head_ = 0;
    tail_ = 0;
    size_ = 0;
  }

private:
  std::array<T, Capacity> data_; // Array storing elements
  size_type head_;               // Points to the first element
  size_type tail_; // Points to the position after the last element
  size_type size_; // Current number of elements
};
#endif // RING_BUFFER_H
