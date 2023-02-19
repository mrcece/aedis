/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <iostream>
#include <boost/asio.hpp>
#ifdef BOOST_ASIO_HAS_CO_AWAIT
#include <boost/system/errc.hpp>
#include <boost/asio/experimental/as_tuple.hpp>

#define BOOST_TEST_MODULE low level
#include <boost/test/included/unit_test.hpp>

#include <boost/redis.hpp>
#include <boost/redis/src.hpp>
#include "common.hpp"

namespace net = boost::asio;
namespace resp3 = boost::redis::resp3;

using boost::redis::operation;
using connection = boost::redis::connection;
using error_code = boost::system::error_code;
using net::experimental::as_tuple;
using boost::redis::request;
using boost::redis::response;
using boost::redis::ignore;
using boost::redis::ignore_t;

BOOST_AUTO_TEST_CASE(push_filtered_out)
{
   net::io_context ioc;
   auto const endpoints = resolve();
   connection conn{ioc};
   net::connect(conn.next_layer(), endpoints);

   request req;
   req.push("HELLO", 3);
   req.push("PING");
   req.push("SUBSCRIBE", "channel");
   req.push("QUIT");

   response<ignore_t, std::string, std::string> resp;
   conn.async_exec(req, resp, [](auto ec, auto){
      BOOST_TEST(!ec);
   });

   conn.async_receive(ignore, [](auto ec, auto){
      BOOST_TEST(!ec);
   });

   conn.async_run([](auto ec){
      BOOST_TEST(!ec);
   });

   ioc.run();

   BOOST_CHECK_EQUAL(std::get<1>(resp).value(), "PONG");
   BOOST_CHECK_EQUAL(std::get<2>(resp).value(), "OK");
}

#ifdef BOOST_ASIO_HAS_CO_AWAIT
net::awaitable<void> push_consumer1(connection& conn, bool& push_received)
{
   {
      auto [ec, ev] = co_await conn.async_receive(ignore, as_tuple(net::use_awaitable));
      BOOST_TEST(!ec);
   }

   {
      auto [ec, ev] = co_await conn.async_receive(ignore, as_tuple(net::use_awaitable));
      BOOST_CHECK_EQUAL(ec, net::experimental::channel_errc::channel_cancelled);
   }

   push_received = true;
}

struct response_error_tag{};
response_error_tag error_tag_obj;

struct response_error_adapter {
   void
   operator()(
      std::size_t, boost::redis::resp3::basic_node<std::string_view> const&, boost::system::error_code& ec)
   {
      ec = boost::redis::error::incompatible_size;
   }

   [[nodiscard]]
   auto get_supported_response_size() const noexcept
      { return static_cast<std::size_t>(-1);}
};

auto boost_redis_adapt(response_error_tag&)
{
   return response_error_adapter{};
}

BOOST_AUTO_TEST_CASE(test_push_adapter)
{
   net::io_context ioc;
   auto const endpoints = resolve();
   connection conn{ioc};
   net::connect(conn.next_layer(), endpoints);

   request req;
   req.push("HELLO", 3);
   req.push("PING");
   req.push("SUBSCRIBE", "channel");
   req.push("PING");

   conn.async_receive(error_tag_obj, [](auto ec, auto) {
      BOOST_CHECK_EQUAL(ec, boost::redis::error::incompatible_size);
   });

   conn.async_exec(req, ignore, [](auto ec, auto){
      BOOST_CHECK_EQUAL(ec, net::experimental::error::channel_errors::channel_cancelled);
   });

   conn.async_run([](auto ec){
      BOOST_CHECK_EQUAL(ec, boost::system::errc::errc_t::operation_canceled);
   });

   ioc.run();

   // TODO: Reset the ioc reconnect and send a quit to ensure
   // reconnection is possible after an error.
}

net::awaitable<void> push_consumer3(connection& conn)
{
   for (;;)
      co_await conn.async_receive(ignore, net::use_awaitable);
}

BOOST_AUTO_TEST_CASE(push_received1)
{
   net::io_context ioc;
   auto const endpoints = resolve();
   connection conn{ioc};
   net::connect(conn.next_layer(), endpoints);

   request req;
   req.push("HELLO", 3);
   req.push("SUBSCRIBE", "channel");
   req.push("QUIT");

   conn.async_exec(req, ignore, [](auto ec, auto){
      BOOST_TEST(!ec);
   });

   conn.async_run([&](auto ec){
      BOOST_TEST(!ec);
      conn.cancel(operation::receive);
   });

   bool push_received = false;
   net::co_spawn(
      ioc.get_executor(),
      push_consumer1(conn, push_received),
      net::detached);

   ioc.run();

   BOOST_TEST(push_received);
}

BOOST_AUTO_TEST_CASE(receives_push_waiting_resps)
{
   request req1;
   req1.push("HELLO", 3);
   req1.push("PING", "Message1");

   request req2;
   req2.push("SUBSCRIBE", "channel");

   request req3;
   req3.push("PING", "Message2");
   req3.push("QUIT");

   net::io_context ioc;

   auto const endpoints = resolve();
   connection conn{ioc};
   net::connect(conn.next_layer(), endpoints);

   auto c3 =[](auto ec, auto...)
   {
      BOOST_TEST(!ec);
   };

   auto c2 =[&](auto ec, auto...)
   {
      BOOST_TEST(!ec);
      conn.async_exec(req3, ignore, c3);
   };

   auto c1 =[&](auto ec, auto...)
   {
      BOOST_TEST(!ec);
      conn.async_exec(req2, ignore, c2);
   };

   conn.async_exec(req1, ignore, c1);

   conn.async_run([&](auto ec) {
      BOOST_TEST(!ec);
      conn.cancel(operation::receive);
   });

   bool push_received = false;
   net::co_spawn(
      ioc.get_executor(),
      push_consumer1(conn, push_received),
      net::detached);

   ioc.run();

   BOOST_TEST(push_received);
}

BOOST_AUTO_TEST_CASE(many_subscribers)
{
   request req0;
   req0.get_config().cancel_on_connection_lost = false;
   req0.push("HELLO", 3);

   request req1;
   req1.get_config().cancel_on_connection_lost = false;
   req1.push("PING", "Message1");

   request req2;
   req2.get_config().cancel_on_connection_lost = false;
   req2.push("SUBSCRIBE", "channel");

   request req3;
   req3.get_config().cancel_on_connection_lost = false;
   req3.push("QUIT");

   net::io_context ioc;
   auto const endpoints = resolve();
   connection conn{ioc};
   net::connect(conn.next_layer(), endpoints);

   auto c11 =[&](auto ec, auto...)
   {
      BOOST_TEST(!ec);
   };
   auto c10 =[&](auto ec, auto...)
   {
      BOOST_TEST(!ec);
      conn.async_exec(req3, ignore, c11);
   };
   auto c9 =[&](auto ec, auto...)
   {
      BOOST_TEST(!ec);
      conn.async_exec(req2, ignore, c10);
   };
   auto c8 =[&](auto ec, auto...)
   {
      BOOST_TEST(!ec);
      conn.async_exec(req1, ignore,  c9);
   };
   auto c7 =[&](auto ec, auto...)
   {
      BOOST_TEST(!ec);
      conn.async_exec(req2, ignore,  c8);
   };
   auto c6 =[&](auto ec, auto...)
   {
      BOOST_TEST(!ec);
      conn.async_exec(req2, ignore,  c7);
   };
   auto c5 =[&](auto ec, auto...)
   {
      BOOST_TEST(!ec);
      conn.async_exec(req1, ignore,  c6);
   };
   auto c4 =[&](auto ec, auto...)
   {
      BOOST_TEST(!ec);
      conn.async_exec(req2, ignore,  c5);
   };
   auto c3 =[&](auto ec, auto...)
   {
      BOOST_TEST(!ec);
      conn.async_exec(req1, ignore,  c4);
   };
   auto c2 =[&](auto ec, auto...)
   {
      BOOST_TEST(!ec);
      conn.async_exec(req2, ignore,  c3);
   };
   auto c1 =[&](auto ec, auto...)
   {
      BOOST_TEST(!ec);
      conn.async_exec(req2, ignore,  c2);
   };
   auto c0 =[&](auto ec, auto...)
   {
      BOOST_TEST(!ec);
      conn.async_exec(req1, ignore,  c1);
   };

   conn.async_exec(req0, ignore,  c0);

   conn.async_run([&](auto ec) {
      BOOST_TEST(!ec);
      conn.cancel(operation::receive);
   });

   net::co_spawn(ioc.get_executor(), push_consumer3(conn), net::detached);
   ioc.run();
}
#endif

#else
int main() {}
#endif
