/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <string>
#include <string_view>
#include <utility>

namespace aedis {
namespace resp3 {
namespace detail {

template <class>
struct needs_to_string {
  static constexpr auto value = true;
};

template <>
struct needs_to_string<std::string> {
  static constexpr auto value = false;
};

template <>
struct needs_to_string<std::string_view> {
  static constexpr auto value = false;
};

template <>
struct needs_to_string<char const*> {
  static constexpr auto value = false;
};

template <>
struct needs_to_string<char*> {
  static constexpr auto value = false;
};

template <std::size_t N>
struct needs_to_string<char[N]> {
  static constexpr auto value = false;
};

template <std::size_t N>
struct needs_to_string<char const[N]> {
  static constexpr auto value = false;
};

void add_header(std::string& to, int size);
void add_bulk(std::string& to, std::string_view param);

template <class T>
void add_bulk(std::string& to, T const& data, typename std::enable_if<needs_to_string<T>::value, bool>::type = false)
{
  using std::to_string;
  auto const s = to_string(data);
  add_bulk(to, s);
}

// Overload for pairs.
template <class T1, class T2>
void add_bulk(std::string& to, std::pair<T1, T2> const& pair)
{
  add_bulk(to, pair.first);
  add_bulk(to, pair.second);
}

template <class>
struct value_type_size {
  static constexpr auto size = 1U;
};

template <class T, class U>
struct value_type_size<std::pair<T, U>> {
  static constexpr auto size = 2U;
};

bool has_push_response(command cmd);

} // detail
} // resp3
} // aedis
