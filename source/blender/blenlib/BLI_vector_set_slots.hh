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

#ifndef __BLI_VECTOR_SET_SLOTS_HH__
#define __BLI_VECTOR_SET_SLOTS_HH__

/** \file
 * \ingroup bli
 *
 * TODO:
 * - Small buffer optimization for the keys.
 */

#include "BLI_sys_types.h"

namespace BLI {

template<typename Key> class SimpleVectorSetSlot {
 private:
  static constexpr int32_t s_is_empty = -1;
  static constexpr int32_t s_is_dummy = -2;

  int32_t m_state = s_is_empty;

 public:
  bool is_set() const
  {
    return m_state >= 0;
  }

  bool is_empty() const
  {
    return m_state == s_is_empty;
  }

  uint32_t index() const
  {
    BLI_assert(this->is_set());
    return m_state;
  }

  template<typename ForwardKey>
  bool contains(const ForwardKey &key, uint32_t UNUSED(hash), const Key *keys) const
  {
    if (m_state >= 0) {
      return key == keys[m_state];
    }
    return false;
  }

  void set_and_destruct_other(SimpleVectorSetSlot &other, uint32_t UNUSED(hash))
  {
    BLI_assert(!this->is_set());
    BLI_assert(other.is_set());
    m_state = other.m_state;
  }

  void set(uint32_t index, uint32_t UNUSED(hash))
  {
    BLI_assert(!this->is_set());
    m_state = (int32_t)index;
  }

  void update_index(uint32_t index)
  {
    BLI_assert(this->is_set());
    m_state = (int32_t)index;
  }

  void set_to_dummy()
  {
    m_state = s_is_dummy;
  }

  bool has_index(uint32_t index) const
  {
    return (uint32_t)m_state == index;
  }

  template<typename Hash> uint32_t get_hash(const Key &key, const Hash &hash) const
  {
    BLI_assert(this->is_set());
    return hash(key);
  }
};

template<typename Key> struct DefaultVectorSetSlot {
  using type = SimpleVectorSetSlot<Key>;
};

}  // namespace BLI

#endif /* __BLI_VECTOR_SET_SLOTS_HH__ */
