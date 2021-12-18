/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <aedis/resp3/type.hpp>

#include <string>

namespace aedis {
namespace resp3 {

/** \brief A node in the response tree.
 */
struct node {
   enum class dump_format {raw, clean};

   /// The number of children node is parent of.
   std::size_t size;

   /// The depth of this node in the response tree.
   std::size_t depth;

   /// The RESP3 type  of the data in this node.
   type data_type;

   /// The data. For aggregate data types this is always empty.
   std::string data;

   /// Converts the node to a string and appends to out.
   void dump(dump_format format, int indent, std::string& out) const;
};

/// Compares a node for equality.
bool operator==(node const& a, node const& b);

/// Writes the node to the stream.
std::ostream& operator<<(std::ostream& os, node const& o);

using storage_type = std::vector<node>;

std::string
dump(
   storage_type const& obj,
   node::dump_format format = node::dump_format::clean,
   int indent = 3);

/// Equality comparison for a node.
bool operator==(node const& a, node const& b);

/** Writes the text representation of node to the output stream.
 *  
 *  NOTE: Binary data is not converted to text.
 */
std::ostream& operator<<(std::ostream& os, node const& o);

/** Writes the text representation of the response to the output
 *  stream the response to the output stream.
 */
std::ostream& operator<<(std::ostream& os, storage_type const& r);

std::ostream& operator<<(std::ostream& os, storage_type const& r);

} // resp3
} // aedis