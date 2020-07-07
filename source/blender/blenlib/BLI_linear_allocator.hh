/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/** \file
 * \ingroup bli
 *
 * A linear allocator is the simplest form of an allocator. It never reuses any memory, and
 * therefore does not need a deallocation method. It simply hands out consecutive buffers of
 * memory. When the current buffer is full, it reallocates a new larger buffer and continues.
 */

#ifndef __BLI_LINEAR_ALLOCATOR_HH__
#define __BLI_LINEAR_ALLOCATOR_HH__

#include "BLI_string_ref.hh"
#include "BLI_utility_mixins.hh"
#include "BLI_vector.hh"

namespace blender {

template<typename Allocator = GuardedAllocator> class LinearAllocator : NonCopyable, NonMovable {
 private:
  Allocator allocator_;
  Vector<void *> owned_buffers_;
  Vector<Span<char>> unused_borrowed_buffers_;

  uintptr_t current_begin_;
  uintptr_t current_end_;
  uint next_min_alloc_size_;

#ifdef DEBUG
  uint debug_allocated_amount_ = 0;
#endif

 public:
  LinearAllocator()
  {
    current_begin_ = 0;
    current_end_ = 0;
    next_min_alloc_size_ = 64;
  }

  ~LinearAllocator()
  {
    for (void *ptr : owned_buffers_) {
      allocator_.deallocate(ptr);
    }
  }

  /**
   * Get a pointer to a memory buffer with the given size an alignment. The memory buffer will be
   * freed when this LinearAllocator is destructed.
   *
   * The alignment has to be a power of 2.
   */
  void *allocate(const uint size, const uint alignment)
  {
    BLI_assert(alignment >= 1);
    BLI_assert(is_power_of_2_i(alignment));

#ifdef DEBUG
    debug_allocated_amount_ += size;
#endif

    const uintptr_t alignment_mask = alignment - 1;
    const uintptr_t potential_allocation_begin = (current_begin_ + alignment_mask) &
                                                 ~alignment_mask;
    const uintptr_t potential_allocation_end = potential_allocation_begin + size;

    if (potential_allocation_end <= current_end_) {
      current_begin_ = potential_allocation_end;
      return (void *)potential_allocation_begin;
    }
    else {
      this->allocate_new_buffer(size + alignment);
      return this->allocate(size, alignment);
    }
  };

  /**
   * Allocate a memory buffer that can hold an instance of T.
   *
   * This method only allocates memory and does not construct the instance.
   */
  template<typename T> T *allocate()
  {
    return (T *)this->allocate(sizeof(T), alignof(T));
  }

  /**
   * Allocate a memory buffer that can hold T array with the given size.
   *
   * This method only allocates memory and does not construct the instance.
   */
  template<typename T> MutableSpan<T> allocate_array(uint size)
  {
    return MutableSpan<T>((T *)this->allocate(sizeof(T) * size, alignof(T)), size);
  }

  /**
   * Construct an instance of T in memory provided by this allocator.
   *
   * Arguments passed to this method will be forwarded to the constructor of T.
   *
   * You must not call `delete` on the returned pointer.
   * Instead, the destruct has to be called explicitly.
   */
  template<typename T, typename... Args> T *construct(Args &&... args)
  {
    void *buffer = this->allocate(sizeof(T), alignof(T));
    T *value = new (buffer) T(std::forward<Args>(args)...);
    return value;
  }

  /**
   * Copy the given array into a memory buffer provided by this allocator.
   */
  template<typename T> MutableSpan<T> construct_array_copy(Span<T> src)
  {
    MutableSpan<T> dst = this->allocate_array<T>(src.size());
    uninitialized_copy_n(src.data(), src.size(), dst.data());
    return dst;
  }

  /**
   * Copy the given string into a memory buffer provided by this allocator. The returned string is
   * always null terminated.
   */
  StringRefNull copy_string(StringRef str)
  {
    const uint alloc_size = str.size() + 1;
    char *buffer = (char *)this->allocate(alloc_size, 1);
    str.copy(buffer, alloc_size);
    return StringRefNull((const char *)buffer);
  }

  MutableSpan<void *> allocate_elements_and_pointer_array(uint element_amount,
                                                          uint element_size,
                                                          uint element_alignment)
  {
    void *pointer_buffer = this->allocate(element_amount * sizeof(void *), alignof(void *));
    void *elements_buffer = this->allocate(element_amount * element_size, element_alignment);

    MutableSpan<void *> pointers((void **)pointer_buffer, element_amount);
    void *next_element_buffer = elements_buffer;
    for (uint i : IndexRange(element_amount)) {
      pointers[i] = next_element_buffer;
      next_element_buffer = POINTER_OFFSET(next_element_buffer, element_size);
    }

    return pointers;
  }

  template<typename T, typename... Args>
  Span<T *> construct_elements_and_pointer_array(uint n, Args &&... args)
  {
    MutableSpan<void *> void_pointers = this->allocate_elements_and_pointer_array(
        n, sizeof(T), alignof(T));
    MutableSpan<T *> pointers = void_pointers.cast<T *>();

    for (uint i : IndexRange(n)) {
      new ((void *)pointers[i]) T(std::forward<Args>(args)...);
    }

    return pointers;
  }

  /**
   * Tell the allocator to use up the given memory buffer, before allocating new memory from the
   * system.
   */
  void provide_buffer(void *buffer, uint size)
  {
    unused_borrowed_buffers_.append(Span<char>((char *)buffer, size));
  }

  template<size_t Size, size_t Alignment>
  void provide_buffer(AlignedBuffer<Size, Alignment> &aligned_buffer)
  {
    this->provide_buffer(aligned_buffer.ptr(), Size);
  }

 private:
  void allocate_new_buffer(uint min_allocation_size)
  {
    for (uint i : unused_borrowed_buffers_.index_range()) {
      Span<char> buffer = unused_borrowed_buffers_[i];
      if (buffer.size() >= min_allocation_size) {
        unused_borrowed_buffers_.remove_and_reorder(i);
        current_begin_ = (uintptr_t)buffer.begin();
        current_end_ = (uintptr_t)buffer.end();
        return;
      }
    }

    const uint size_in_bytes = power_of_2_min_u(
        std::max(min_allocation_size, next_min_alloc_size_));
    next_min_alloc_size_ = size_in_bytes * 2;

    void *buffer = allocator_.allocate(size_in_bytes, 8, AT);
    owned_buffers_.append(buffer);
    current_begin_ = (uintptr_t)buffer;
    current_end_ = current_begin_ + size_in_bytes;
  }
};

}  // namespace blender

#endif /* __BLI_LINEAR_ALLOCATOR_HH__ */
