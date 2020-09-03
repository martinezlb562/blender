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

#pragma once

#ifdef __cplusplus

#  include "BLI_span.hh"
#  include "BLI_string_ref.hh"

namespace blender::string_matching {

int levenshtein_distance(StringRef a, StringRef b);
int initial_characters_distance(StringRef query, StringRef text);
int filter_and_sort(StringRef query, MutableSpan<StringRef> results);

}  // namespace blender::string_matching

#endif
