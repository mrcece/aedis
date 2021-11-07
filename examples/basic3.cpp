/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <iostream>

#include <aedis/aedis.hpp>

#include "types.hpp"
#include "utils.ipp"

using namespace aedis;

/* A more elaborate way sending requests where a new request is sent
 * only after the last one has arrived. This can be used as a starting
 * point for more complex applications.
 *
 * We also separate the application logic out out the coroutine for
 * clarity.
 */

// Adds a new element in the queue if necessary.
void prepare_next(std::queue<resp3::request>& reqs)
{
   if (std::empty(reqs) || std::size(reqs) == 1)
      reqs.push({});
}

void process_response(
   std::queue<resp3::request>& reqs,
   resp3::response& resp)
{
   std::cout
      << reqs.front().commands.front() << ":\n"
      << resp << std::endl;

   switch (reqs.front().commands.front()) {
      case command::hello:
         prepare_next(reqs);
         reqs.back().push(command::ping);
         break;
      case command::ping:
         prepare_next(reqs);
         reqs.back().push(command::quit);
         break;
      default: {};
   }
}

net::awaitable<void> ping()
{
   auto socket = co_await make_connection();

   std::string buffer;

   std::queue<resp3::request> reqs;
   reqs.push({});
   reqs.back().push(command::hello, 3);

   while (!std::empty(reqs)) {
      co_await async_write(socket, reqs.front());
      while (!std::empty(reqs.front().commands)) {
         resp3::response resp;
	 co_await async_read(socket, buffer, resp);
         process_response(reqs, resp);
         reqs.front().commands.pop();
      }

      reqs.pop();
   }
}

int main()
{
   net::io_context ioc;
   co_spawn(ioc, ping(), net::detached);
   ioc.run();
}