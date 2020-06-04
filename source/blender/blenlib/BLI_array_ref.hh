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

#ifndef __BLI_ARRAY_REF_HH__
#define __BLI_ARRAY_REF_HH__

/** \file
 * \ingroup bli
 *
 * A `BLI::ArrayRef<T>` references an array that is owned by someone else. It is just a pointer and
 * a size. Since the memory is not owned, ArrayRef should not be used to transfer ownership. The
 * array cannot be modified through the ArrayRef. However, if T is a non-const pointer, the
 * pointed-to elements can be modified.
 *
 * There is also `BLI::MutableArrayRef<T>`. It is mostly the same as ArrayRef, but allows the array
 * to be modified.
 *
 * `BLI::ArrayRef<T>` should be your default choice when you have to pass a read-only array into a
 * function. It is better than passing a `const Vector &`, because then the function only works for
 * vectors and not for e.g. arrays. Using ArrayRef as function parameter makes it usable in more
 * contexts, better expresses the intend and does not sacrifice performance. It is also better than
 * passing a raw pointer and size separately, because it is more convenient and safe.
 *
 * `BLI::MutableArrayRef<T>` can be used when a function is supposed to return an array, the size
 * of which is known before the function is called. One advantage of this approach is that the
 * caller is responsible for allocation and deallocation. Furthermore, the function can focus on
 * its task, without having to worry about memory allocation. Alternatively, a function could
 * return an Array or Vector.
 *
 * Note: When a function has a MutableArrayRef<T> output parameter and T is not a trivial type,
 * then the function has to specify whether the referenced array is expected to be initialized or
 * not.
 *
 * Since the arrays are only referenced, it is generally not save to store an ArrayRef. When you
 * store one, you should know who owns the memory.
 *
 * Instances of ArrayRef and MutableArrayRef are small and should be passed by value.
 */

#include <algorithm>
#include <array>
#include <iostream>
#include <string>
#include <vector>

#include "BLI_index_range.hh"
#include "BLI_memory_utils.hh"
#include "BLI_utildefines.h"

namespace BLI {

/**
 * References an array of type T that is owned by someone else. The data in the array cannot be
 * modified.
 */
template<typename T> class ArrayRef {
 private:
  const T *m_start = nullptr;
  uint m_size = 0;

 public:
  /**
   * Create a reference to an empty array.
   */
  ArrayRef() = default;

  ArrayRef(const T *start, uint size) : m_start(start), m_size(size)
  {
  }

  /**
   * Reference an initializer_list. Note that the data in the initializer_list is only valid until
   * the expression containing it is fully computed.
   *
   * Do:
   *  call_function_with_array({1, 2, 3, 4});
   *
   * Don't:
   *  ArrayRef<int> ref = {1, 2, 3, 4};
   *  call_function_with_array(ref);
   */
  ArrayRef(const std::initializer_list<T> &list) : ArrayRef(list.begin(), (uint)list.size())
  {
  }

  ArrayRef(const std::vector<T> &vector) : ArrayRef(vector.data(), (uint)vector.size())
  {
  }

  template<std::size_t N> ArrayRef(const std::array<T, N> &array) : ArrayRef(array.data(), N)
  {
  }

  /**
   * Support implicit conversions like the ones below:
   *   ArrayRef<T *> -> ArrayRef<const T *>
   *   ArrayRef<Derived *> -> ArrayRef<Base *>
   */
  template<typename U,
           typename std::enable_if<std::is_convertible<U *, T>::value>::type * = nullptr>
  ArrayRef(ArrayRef<U *> array) : ArrayRef((T *)array.data(), array.size())
  {
  }

  /**
   * Returns a contiguous part of the array. This fails when the slice does not stay within the
   * bounds of the array.
   */
  ArrayRef slice(uint start, uint size) const
  {
    BLI_assert(start + size <= this->size() || size == 0);
    return ArrayRef(m_start + start, size);
  }

  ArrayRef slice(IndexRange range) const
  {
    return this->slice(range.start(), range.size());
  }

  /**
   * Returns a new ArrayRef with n elements removed from the beginning. This fails when the array
   * is too small.
   */
  ArrayRef drop_front(uint n) const
  {
    BLI_assert(n <= this->size());
    return this->slice(n, this->size() - n);
  }

  /**
   * Returns a new ArrayRef with n elements removed from the beginning. This fails when the array
   * is too small.
   */
  ArrayRef drop_back(uint n) const
  {
    BLI_assert(n <= this->size());
    return this->slice(0, this->size() - n);
  }

  /**
   * Returns a new ArrayRef that only contains the first n elements. This fails when the array
   * is too small.
   */
  ArrayRef take_front(uint n) const
  {
    BLI_assert(n <= this->size());
    return this->slice(0, n);
  }

  /**
   * Returns a new ArrayRef that only contains the last n elements. This fails when the array
   * is too small.
   */
  ArrayRef take_back(uint n) const
  {
    BLI_assert(n <= this->size());
    return this->slice(this->size() - n, n);
  }

  /**
   * Returns the pointer to the beginning of the referenced array. This may be nullptr when the
   * size is zero.
   */
  const T *data() const
  {
    return m_start;
  }

  const T *begin() const
  {
    return m_start;
  }

  const T *end() const
  {
    return m_start + m_size;
  }

  /**
   * Access an element in the array. This fails when the index is out of bounds.
   */
  const T &operator[](uint index) const
  {
    BLI_assert(index < m_size);
    return m_start[index];
  }

  /**
   * Returns the number of elements in the referenced array.
   */
  uint size() const
  {
    return m_size;
  }

  /**
   * Returns true if the size is zero.
   */
  bool is_empty() const
  {
    return m_size == 0;
  }

  /**
   * Returns the number of bytes referenced by this ArrayRef.
   */
  uint size_in_bytes() const
  {
    return sizeof(T) * m_size;
  }

  /**
   * Does a linear search to see of the value is in the array.
   * Returns true if it is, otherwise false.
   */
  bool contains(const T &value) const
  {
    for (const T &element : *this) {
      if (element == value) {
        return true;
      }
    }
    return false;
  }

  /**
   * Does a constant time check to see if the pointer points to a value in the referenced array.
   * Return true if it is, otherwise false.
   */
  bool contains_ptr(const T *ptr) const
  {
    return (this->begin() <= ptr) && (ptr < this->end());
  }

  /**
   * Does a linear search to count how often the value is in the array.
   * Returns the number of occurrences.
   */
  uint count(const T &value) const
  {
    uint counter = 0;
    for (const T &element : *this) {
      if (element == value) {
        counter++;
      }
    }
    return counter;
  }

  /**
   * Return a reference to the first element in the array. This fails when the array is empty.
   */
  const T &first() const
  {
    BLI_assert(m_size > 0);
    return m_start[0];
  }

  /**
   * Returns a reference to the last element in the array. This fails when the array is empty.
   */
  const T &last() const
  {
    BLI_assert(m_size > 0);
    return m_start[m_size - 1];
  }

  /**
   * Get element at the given index. If the index is out of range, return the fallback value.
   */
  T get(uint index, const T &fallback) const
  {
    if (index < m_size) {
      return m_start[index];
    }
    return fallback;
  }

  /**
   * Check if the array contains duplicates. Does a linear search for every element. So the total
   * running time is O(n^2). Only use this for small arrays.
   */
  bool has_duplicates__linear_search() const
  {
    /* The size should really be smaller than that. If it is not, the calling code should be
     * changed. */
    BLI_assert(m_size < 1000);

    for (uint i = 0; i < m_size; i++) {
      const T &value = m_start[i];
      for (uint j = i + 1; j < m_size; j++) {
        if (value == m_start[j]) {
          return true;
        }
      }
    }
    return false;
  }

  /**
   * Returns true when this and the other array have an element in common. This should only be
   * called on small arrays, because it has a running time of O(n*m) where n and m are the sizes of
   * the arrays.
   */
  bool intersects__linear_search(ArrayRef other) const
  {
    /* The size should really be smaller than that. If it is not, the calling code should be
     * changed. */
    BLI_assert(m_size < 1000);

    for (uint i = 0; i < m_size; i++) {
      const T &value = m_start[i];
      if (other.contains(value)) {
        return true;
      }
    }
    return false;
  }

  /**
   * Get the index of the first occurrence of the given value. This fails when the value is not in
   * the array.
   */
  uint first_index(const T &search_value) const
  {
    int index = this->first_index_try(search_value);
    BLI_assert(index >= 0);
    return (uint)index;
  }

  /**
   * Get the index of the first occurrence of the given value or -1 if it does not exist.
   */
  int first_index_try(const T &search_value) const
  {
    for (uint i = 0; i < m_size; i++) {
      if (m_start[i] == search_value) {
        return i;
      }
    }
    return -1;
  }

  /**
   * Utility to make it more convenient to iterate over all indices that can be used with this
   * array.
   */
  IndexRange index_range() const
  {
    return IndexRange(m_size);
  }

  /**
   * Get a new ArrayRef to the same underlying memory buffer. No conversions are done.
   */
  template<typename NewT> ArrayRef<NewT> cast() const
  {
    BLI_assert((m_size * sizeof(T)) % sizeof(NewT) == 0);
    uint new_size = m_size * sizeof(T) / sizeof(NewT);
    return ArrayRef<NewT>(reinterpret_cast<const NewT *>(m_start), new_size);
  }

  /**
   * A debug utility to print the content of the ArrayRef. Every element will be printed on a
   * separate line using the given callback.
   */
  template<typename PrintLineF> void print_as_lines(std::string name, PrintLineF print_line) const
  {
    std::cout << "ArrayRef: " << name << " \tSize:" << m_size << '\n';
    for (const T &value : *this) {
      std::cout << "  ";
      print_line(value);
      std::cout << '\n';
    }
  }

  /**
   * A debug utility to print the content of the array ref. Every element be printed on a separate
   * line.
   */
  void print_as_lines(std::string name) const
  {
    this->print_as_lines(name, [](const T &value) { std::cout << value; });
  }
};

/**
 * Mostly the same as ArrayRef, except that one can change the array elements through a
 * MutableArrayRef.
 */
template<typename T> class MutableArrayRef {
 private:
  T *m_start;
  uint m_size;

 public:
  MutableArrayRef() = default;

  MutableArrayRef(T *start, uint size) : m_start(start), m_size(size)
  {
  }

  /**
   * Reference an initializer_list. Note that the data in the initializer_list is only valid until
   * the expression containing it is fully computed.
   *
   * Do:
   *  call_function_with_array({1, 2, 3, 4});
   *
   * Don't:
   *  MutableArrayRef<int> ref = {1, 2, 3, 4};
   *  call_function_with_array(ref);
   */
  MutableArrayRef(std::initializer_list<T> &list) : MutableArrayRef(list.begin(), list.size())
  {
  }

  MutableArrayRef(std::vector<T> &vector) : MutableArrayRef(vector.data(), vector.size())
  {
  }

  template<std::size_t N>
  MutableArrayRef(std::array<T, N> &array) : MutableArrayRef(array.data(), N)
  {
  }

  operator ArrayRef<T>() const
  {
    return ArrayRef<T>(m_start, m_size);
  }

  /**
   * Get the number of elements in the array.
   */
  uint size() const
  {
    return m_size;
  }

  /**
   * Replace all elements in the referenced array with the given value.
   */
  void fill(const T &value)
  {
    initialized_fill_n(m_start, m_size, value);
  }

  /**
   * Replace a subset of all elements with the given value. This fails when an index is out of
   * bounds.
   */
  void fill_indices(ArrayRef<uint> indices, const T &value)
  {
    for (uint i : indices) {
      BLI_assert(i < m_size);
      m_start[i] = value;
    }
  }

  /**
   * Get a pointer to the beginning of the referenced array. This may be nullptr, when the size is
   * zero.
   */
  T *data() const
  {
    return m_start;
  }

  T *begin() const
  {
    return m_start;
  }

  T *end() const
  {
    return m_start + m_size;
  }

  T &operator[](uint index) const
  {
    BLI_assert(index < this->size());
    return m_start[index];
  }

  /**
   * Returns a contiguous part of the array. This fails when the slice would go out of bounds.
   */
  MutableArrayRef slice(uint start, uint length) const
  {
    BLI_assert(start + length <= this->size());
    return MutableArrayRef(m_start + start, length);
  }

  /**
   * Returns a new MutableArrayRef with n elements removed from the beginning. This fails when the
   * array is too small.
   */
  MutableArrayRef drop_front(uint n) const
  {
    BLI_assert(n <= this->size());
    return this->slice(n, this->size() - n);
  }

  /**
   * Returns a new MutableArrayRef with n elements removed from the end. This fails when the array
   * is too small.
   */
  MutableArrayRef drop_back(uint n) const
  {
    BLI_assert(n <= this->size());
    return this->slice(0, this->size() - n);
  }

  /**
   * Returns a new MutableArrayRef that only contains the first n elements. This fails when the
   * array is too small.
   */
  MutableArrayRef take_front(uint n) const
  {
    BLI_assert(n <= this->size());
    return this->slice(0, n);
  }

  /**
   * Return a new MutableArrayRef that only contains the last n elements. This fails when the array
   * is too small.
   */
  MutableArrayRef take_back(uint n) const
  {
    BLI_assert(n <= this->size());
    return this->slice(this->size() - n, n);
  }

  /**
   * Get an (immutable) ArrayRef that references the same array. This is usually not needed, due to
   * implicit conversions. However, sometimes automatic type deduction needs some help.
   */
  ArrayRef<T> as_ref() const
  {
    return ArrayRef<T>(m_start, m_size);
  }

  /**
   * Utility to make it more convenient to iterate over all indices that can be used with this
   * array.
   */
  IndexRange index_range() const
  {
    return IndexRange(m_size);
  }

  /**
   * Get a reference to the last element. This will fail when the array is empty.
   */
  const T &last() const
  {
    BLI_assert(m_size > 0);
    return m_start[m_size - 1];
  }

  /**
   * Get a new array ref to the same underlying memory buffer. No conversions are done.
   */
  template<typename NewT> MutableArrayRef<NewT> cast() const
  {
    BLI_assert((m_size * sizeof(T)) % sizeof(NewT) == 0);
    uint new_size = m_size * sizeof(T) / sizeof(NewT);
    return MutableArrayRef<NewT>(reinterpret_cast<NewT *>(m_start), new_size);
  }
};

/**
 * Shorthand to make use of automatic template parameter deduction.
 */
template<typename T> ArrayRef<T> ref_c_array(const T *array, uint size)
{
  return ArrayRef<T>(array, size);
}

/**
 * Utilities to check that arrays have the same size in debug builds.
 */
template<typename T1, typename T2> void assert_same_size(const T1 &v1, const T2 &v2)
{
  UNUSED_VARS_NDEBUG(v1, v2);
#ifdef DEBUG
  uint size = v1.size();
  BLI_assert(size == v1.size());
  BLI_assert(size == v2.size());
#endif
}

template<typename T1, typename T2, typename T3>
void assert_same_size(const T1 &v1, const T2 &v2, const T3 &v3)
{
  UNUSED_VARS_NDEBUG(v1, v2, v3);
#ifdef DEBUG
  uint size = v1.size();
  BLI_assert(size == v1.size());
  BLI_assert(size == v2.size());
  BLI_assert(size == v3.size());
#endif
}

} /* namespace BLI */

#endif /* __BLI_ARRAY_REF_HH__ */
