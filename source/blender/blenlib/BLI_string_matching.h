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
#  include "BLI_vector.hh"

namespace blender::string_matching {

int damerau_levenshtein_distance(StringRef a,
                                 StringRef b,
                                 int deletion_cost = 1,
                                 int insertion_cost = 1,
                                 int substitution_cost = 1,
                                 int transposition_cost = 1);

bool is_partial_fuzzy_match(StringRef partial, StringRef full);

Vector<int> filter_and_sort(StringRef query, Span<StringRef> possible_results);

}  // namespace blender::string_matching

#endif

#ifdef __cplusplus
extern "C" {
#endif

int BLI_string_matching_filter_and_sort(const char *query,
                                        const char **possible_results,
                                        int possible_results_amount,
                                        int **r_filtered_and_sorted_indices);

#ifdef __cplusplus
}
#endif
