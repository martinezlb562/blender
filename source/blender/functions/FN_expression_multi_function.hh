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
 * \ingroup fn
 */

#ifndef __FN_EXPRESSION_MULTI_FUNCTION_HH__
#define __FN_EXPRESSION_MULTI_FUNCTION_HH__

#include "FN_multi_function_builder.hh"
#include "FN_multi_function_network.hh"

#include "BLI_map.hh"
#include "BLI_resource_collector.hh"

namespace blender::fn {

class SymbolTable {
 private:
  LinearAllocator<> allocator_;
  Map<std::string, Vector<const MultiFunction *>> function_table_;
  Map<std::pair<MFDataType, std::string>, const MultiFunction *> attribute_table_;
  Map<std::pair<MFDataType, std::string>, const MultiFunction *> method_table_;
  Map<std::pair<MFDataType, MFDataType>, const MultiFunction *> conversion_table_;
  Map<std::string, std::pair<const CPPType *, void *>> single_constants_table_;

 public:
  ~SymbolTable()
  {
    for (auto &&[type, buffer] : single_constants_table_.values()) {
      type->destruct(buffer);
    }
  }

  void add_function(StringRef name, const MultiFunction &fn)
  {
    function_table_.lookup_or_add_default_as(name).append(&fn);
  }

  Span<const MultiFunction *> lookup_function_candidates(StringRef name) const
  {
    return function_table_.lookup_default(name, {});
  }

  void add_attribute(MFDataType type, StringRef name, const MultiFunction &fn)
  {
    attribute_table_.add_new({type, name}, &fn);
  }

  const MultiFunction *try_lookup_attribute(MFDataType type, StringRef name) const
  {
    return attribute_table_.lookup_default({type, name}, nullptr);
  }

  void add_method(MFDataType type, StringRef name, const MultiFunction &fn)
  {
    method_table_.add_new({type, name}, &fn);
  }

  const MultiFunction *try_lookup_method(MFDataType type, StringRef name) const
  {
    return method_table_.lookup_default({type, name}, nullptr);
  }

  void add_conversion(MFDataType from, MFDataType to, const MultiFunction &fn)
  {
    conversion_table_.add_new({from, to}, &fn);
  }

  template<typename FromT, typename ToT> void add_conversion(ResourceCollector &resources)
  {
    const MultiFunction &fn = resources.construct<CustomMF_Convert<FromT, ToT>>();
    this->add_conversion(MFDataType::ForSingle<FromT>(), MFDataType::ForSingle<ToT>(), fn);
  }

  const MultiFunction *try_lookup_conversion(MFDataType from, MFDataType to) const
  {
    return conversion_table_.lookup_default({from, to}, nullptr);
  }

  bool can_convert(MFDataType from, MFDataType to) const
  {
    return conversion_table_.contains({from, to});
  }

  void add_single_constant(StringRef name, const CPPType &type, const void *buffer)
  {
    void *own_buffer = allocator_.allocate(type.size(), type.alignment());
    type.copy_to_uninitialized(buffer, own_buffer);
    single_constants_table_.add_new(name, {&type, own_buffer});
  }

  template<typename T> void add_single_constant(StringRef name, const T &value)
  {
    this->add_single_constant(name, CPPType::get<T>(), (const void *)&value);
  }

  const std::pair<const CPPType *, void *> *try_lookup_single_constant(StringRef name) const
  {
    return single_constants_table_.lookup_ptr_as(name);
  }
};

const MultiFunction &expression_to_multi_function(StringRef str,
                                                  MFDataType output_type,
                                                  Span<StringRef> variable_names,
                                                  Span<MFDataType> variable_types,
                                                  ResourceCollector &resources,
                                                  const SymbolTable &symbols);

}  // namespace blender::fn

#endif /* __FN_EXPRESSION_MULTI_FUNCTION_HH__ */
