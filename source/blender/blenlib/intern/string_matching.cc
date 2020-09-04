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

#include "BLI_array.hh"
#include "BLI_linear_allocator.hh"
#include "BLI_multi_value_map.hh"
#include "BLI_span.hh"
#include "BLI_string.h"
#include "BLI_string_matching.h"
#include "BLI_string_ref.hh"
#include "BLI_string_utf8.h"

namespace blender::string_matching {

/**
 * Computes the cost of transforming string a into b.
 */
int damerau_levenshtein_distance(StringRef a,
                                 StringRef b,
                                 int deletion_cost,
                                 int insertion_cost,
                                 int substitution_cost,
                                 int transposition_cost)
{
  const int len_a = static_cast<int>(BLI_strnlen_utf8(a.data(), static_cast<size_t>(a.size())));
  const int len_b = static_cast<int>(BLI_strnlen_utf8(b.data(), static_cast<size_t>(b.size())));

  /* Instead of keeping the entire table in memory, only keep three rows. The algorithm only
   * accesses these rows and nothing older.
   * All three rows are usually allocated on the stack. At most a single heap allocation is done,
   * if the reserved stack space is too small. */
  const int row_length = len_b + 1;
  Array<int, 64> rows(row_length * 3);

  /* Store rows as spans so that it is cheap to swap them. */
  MutableSpan v0{rows.data() + row_length * 0, row_length};
  MutableSpan v1{rows.data() + row_length * 1, row_length};
  MutableSpan v2{rows.data() + row_length * 2, row_length};

  /* Only v1 needs to be initialized. */
  v0.fill(0);
  v2.fill(0);
  for (const int i : IndexRange(row_length)) {
    v1[i] = i * insertion_cost;
  }

  uint prev_unicode_a;
  const char *current_a = a.data();
  for (const int i : IndexRange(len_a)) {
    v2[0] = (i + 1) * deletion_cost;

    /* Get and step over the next unicode code point from string a. */
    size_t code_point_size_a = 0;
    const uint unicode_a = BLI_str_utf8_as_unicode_and_size(current_a, &code_point_size_a);

    current_a += code_point_size_a;

    uint prev_unicode_b;
    const char *current_b = b.data();
    for (const int j : IndexRange(len_b)) {
      /* Get and step over the next unicode code point from string b. */
      size_t code_point_size_b = 0;
      const uint unicode_b = BLI_str_utf8_as_unicode_and_size(current_b, &code_point_size_b);
      current_b += code_point_size_b;

      /* Check how costly the different operations would be and pick the cheapest - the one with
       * minimal cost. */
      int new_cost = std::min({v1[j + 1] + deletion_cost,
                               v2[j] + insertion_cost,
                               v1[j] + (unicode_a != unicode_b) * substitution_cost});
      if (i > 0 && j > 0) {
        if (unicode_a == prev_unicode_b && prev_unicode_a == unicode_b) {
          new_cost = std::min(new_cost, v0[j - 1] + transposition_cost);
        }
      }

      v2[j + 1] = new_cost;
      prev_unicode_b = unicode_b;
    }

    /* Swap the three rows, so that the next row can be computed. */
    std::tie(v0, v1, v2) = std::tuple<MutableSpan<int>, MutableSpan<int>, MutableSpan<int>>(
        v1, v2, v0);
    prev_unicode_a = unicode_a;
  }

  return v1.last();
}

static bool is_partial_fuzzy_match(StringRef partial, StringRef full)
{
  if (full.find(partial) != StringRef::not_found) {
    return true;
  }
  /* Allow more errors when the size grows larger. */
  const int max_errors = partial.size() <= 1 ? 0 : partial.size() / 8 + 1;
  const int window_count = std::max<int>(0, full.size() - partial.size() - max_errors) + 1;
  const int window_size = partial.size() + max_errors;
  for (const int i : IndexRange(window_count)) {
    StringRef window = full.substr(i, window_size);
    const int extra_chars = window.size() - partial.size();
    const int distance = damerau_levenshtein_distance(partial, window);
    if (distance <= max_errors + extra_chars) {
      return true;
    }
  }
  return false;
}

static void extract_normalized_words(StringRef str,
                                     LinearAllocator<> &allocator,
                                     Vector<StringRef> &r_words)
{
  const size_t str_size = static_cast<size_t>(str.size());
  const int max_word_amount = BLI_string_max_possible_word_count(str.size());
  Array<std::array<int, 2>, 64> word_positions(max_word_amount);
  const int word_amount = BLI_string_find_split_words(
      str.data(),
      str_size,
      ' ',
      reinterpret_cast<int(*)[2]>(word_positions.data()),
      max_word_amount);

  StringRef str_copy = allocator.copy_string(str);
  char *mutable_copy = const_cast<char *>(str_copy.data());
  BLI_str_tolower_ascii(mutable_copy, str_size);
  for (const int i : IndexRange(word_amount)) {
    const int word_start = word_positions[i][0];
    const int word_length = word_positions[i][1];
    r_words.append(str_copy.substr(word_start, word_length));
  }
}

static bool match_word_initials(StringRef query,
                                Span<StringRef> words,
                                Span<bool> word_is_usable,
                                MutableSpan<bool> r_word_is_matched,
                                int start = 0)
{
  if (start >= words.size()) {
    return false;
  }

  r_word_is_matched.fill(false);

  size_t query_index = 0;
  int word_index = start;
  size_t char_index = 0;

  while (query_index < query.size()) {
    const uint query_unicode = BLI_str_utf8_as_unicode_and_size(query.data() + query_index,
                                                                &query_index);
    while (true) {
      if (!word_is_usable[word_index]) {
        word_index++;
        BLI_assert(char_index == 0);
        continue;
      }

      if (word_index >= words.size()) {
        /* TODO: Early recursion stop. */
        return match_word_initials(query, words, word_is_usable, r_word_is_matched, start + 1);
      }

      StringRef word = words[word_index];
      if (static_cast<int>(char_index) < word.size()) {
        const uint char_unicode = BLI_str_utf8_as_unicode_and_size(word.data() + char_index,
                                                                   &char_index);
        if (query_unicode == char_unicode) {
          r_word_is_matched[word_index] = true;
          break;
        }
      }

      word_index += 1;
      char_index = 0;
    }
  }
  return true;
}

static int get_word_index_that_startswith(StringRef query,
                                          Span<StringRef> words,
                                          Span<bool> word_is_usable)
{
  for (const int i : words.index_range()) {
    if (!word_is_usable[i]) {
      continue;
    }
    StringRef word = words[i];
    if (word.startswith(query)) {
      return i;
    }
  }
  return -1;
}

static int get_word_index_that_fuzzy_matches(StringRef query,
                                             Span<StringRef> words,
                                             Span<bool> word_is_usable)
{
  for (const int i : words.index_range()) {
    if (!word_is_usable[i]) {
      continue;
    }
    StringRef word = words[i];
    if (is_partial_fuzzy_match(query, word)) {
      return i;
    }
  }
  return -1;
}

struct ResultsCache {
 public:
  LinearAllocator<> allocator;
  Vector<Span<StringRef>> normalized_words;
};

static void preprocess_possible_results(ResultsCache &cache, Span<StringRef> results)
{
  cache.normalized_words.reserve(cache.normalized_words.size() + results.size());
  Vector<StringRef> words;
  for (const int i : results.index_range()) {
    StringRef full_str = results[i];
    words.clear();
    extract_normalized_words(full_str, cache.allocator, words);
    Span<StringRef> normalized_words = cache.allocator.construct_array_copy(words.as_span());
    cache.normalized_words.append(normalized_words);
  }
}

static int score_query_against_words(StringRef query, Span<StringRef> result_words)
{
  LinearAllocator<> allocator;
  Vector<StringRef> query_words;
  extract_normalized_words(query, allocator, query_words);

  Array<bool, 64> handled_result_words(result_words.size(), false);

  for (StringRef query_word : query_words) {
    {
      /* Check if any result word begins with the query word. */
      const int word_index = get_word_index_that_startswith(
          query_word, result_words, handled_result_words);
      if (word_index >= 0) {
        handled_result_words[word_index] = true;
        continue;
      }
    }
    {
      /* Try to match against word initials. */
      Array<bool, 64> matched_words(result_words.size());
      const bool success = match_word_initials(
          query_word, result_words, handled_result_words, matched_words);
      if (success) {
        for (const int i : result_words.index_range()) {
          handled_result_words[i] = handled_result_words[i] || matched_words[i];
        }
        continue;
      }
    }
    {
      /* Fuzzy match against words. */
      const int word_index = get_word_index_that_fuzzy_matches(
          query_word, result_words, handled_result_words);
      if (word_index >= 0) {
        handled_result_words[word_index] = true;
        continue;
      }
    }

    /* Couldn't match query word with anything. */
    return -1;
  }

  const int handled_word_amount = std::count(
      handled_result_words.begin(), handled_result_words.end(), true);
  return handled_word_amount;
}

Vector<int> filter_and_sort(StringRef query, Span<StringRef> possible_results)
{
  ResultsCache cache;
  preprocess_possible_results(cache, possible_results);

  MultiValueMap<int, int> result_indices_by_score;
  for (const int result_index : possible_results.index_range()) {
    const int score = score_query_against_words(query, cache.normalized_words[result_index]);
    if (score >= 0) {
      result_indices_by_score.add(score, result_index);
    }
  }

  Vector<int> found_scores;
  for (const int score : result_indices_by_score.keys()) {
    found_scores.append_non_duplicates(score);
  }
  std::sort(found_scores.begin(), found_scores.end(), std::greater<int>());

  Vector<int> sorted_result_indices;
  for (const int score : found_scores) {
    Span<int> indices = result_indices_by_score.lookup(score);
    int *old_end = sorted_result_indices.end();
    sorted_result_indices.extend(indices);
    std::sort(old_end, sorted_result_indices.end(), [&](int a, int b) {
      return possible_results[a] < possible_results[b];
    });
  }

  return sorted_result_indices;
}

}  // namespace blender::string_matching

/**
 * Compares the query to all possible results and returns a sorted list of result indices that
 * matched the query.
 */
int BLI_string_matching_filter_and_sort(const char *query,
                                        const char **possible_results,
                                        int possible_results_amount,
                                        int **r_filtered_and_sorted_indices)
{
  using namespace blender;
  Array<StringRef> possible_result_refs(possible_results_amount);
  for (const int i : IndexRange(possible_results_amount)) {
    possible_result_refs[i] = possible_results[i];
  }

  Vector<int> indices = string_matching::filter_and_sort(query, possible_result_refs);
  int *indices_ptr = static_cast<int *>(MEM_malloc_arrayN(indices.size(), sizeof(int), AT));
  memcpy(indices_ptr, indices.data(), sizeof(int) * indices.size());
  *r_filtered_and_sorted_indices = indices_ptr;
  return indices.size();
}
