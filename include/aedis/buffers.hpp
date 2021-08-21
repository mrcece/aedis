/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <aedis/type.hpp>

namespace aedis {

struct buffers {
   resp3::transaction transaction;
   resp3::array array;
   resp3::array push;
   resp3::set set;
   resp3::map map;
   resp3::array attribute;
   resp3::simple_string simple_string;
   resp3::simple_error simple_error;
   resp3::number number;
   resp3::doublean doublean;
   resp3::boolean boolean;
   resp3::big_number big_number;
   resp3::blob_string blob_string;
   resp3::blob_error blob_error;
   resp3::verbatim_string verbatim_string;
   resp3::streamed_string_part streamed_string_part;
};

} // aedis

