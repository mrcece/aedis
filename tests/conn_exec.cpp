/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <iostream>
#include <boost/asio.hpp>
#include <boost/system/errc.hpp>

#define BOOST_TEST_MODULE low level
#include <boost/test/included/unit_test.hpp>

#include <aedis.hpp>
#include <aedis/src.hpp>

#include "common.hpp"

// TODO: Test whether HELLO won't be inserted passt commands that have
// been already writen.

namespace net = boost::asio;
namespace resp3 = aedis::resp3;
using error_code = boost::system::error_code;
using connection = aedis::connection;
using aedis::adapt;

BOOST_AUTO_TEST_CASE(hello_priority)
{
   resp3::request req1;
   req1.get_config().coalesce = false;
   req1.push("PING", "req1");

   resp3::request req2;
   req2.get_config().coalesce = false;
   req2.get_config().hello_with_priority = false;
   req2.push("HELLO", 3);
   req2.push("PING", "req2");
   req2.push("QUIT");

   resp3::request req3;
   req3.get_config().coalesce = false;
   req3.get_config().hello_with_priority = true;
   req3.push("HELLO", 3);
   req3.push("PING", "req3");

   net::io_context ioc;

   auto const endpoints = resolve();
   connection conn{ioc};
   net::connect(conn.next_layer(), endpoints);

   bool seen1 = false;
   bool seen2 = false;
   bool seen3 = false;

   conn.async_exec(req1, adapt(), [&](auto ec, auto){
      std::cout << "bbb" << std::endl;
      BOOST_TEST(!ec);
      BOOST_TEST(!seen2);
      BOOST_TEST(seen3);
      seen1 = true;
   });
   conn.async_exec(req2, adapt(), [&](auto ec, auto){
      std::cout << "ccc" << std::endl;
      BOOST_TEST(!ec);
      BOOST_TEST(seen1);
      BOOST_TEST(seen3);
      seen2 = true;
   });
   conn.async_exec(req3, adapt(), [&](auto ec, auto){
      std::cout << "ddd" << std::endl;
      BOOST_TEST(!ec);
      BOOST_TEST(!seen1);
      BOOST_TEST(!seen2);
      seen3 = true;
   });

   conn.async_run([](auto ec){
      BOOST_TEST(!ec);
   });

   ioc.run();
}

BOOST_AUTO_TEST_CASE(wrong_response_data_type)
{
   resp3::request req;
   req.push("HELLO", 3);
   req.push("QUIT");

   // Wrong data type.
   std::tuple<aedis::ignore, int> resp;
   net::io_context ioc;

   auto const endpoints = resolve();
   connection conn{ioc};
   net::connect(conn.next_layer(), endpoints);

   conn.async_exec(req, adapt(resp), [](auto ec, auto){
      BOOST_CHECK_EQUAL(ec, aedis::error::not_a_number);
   });
   conn.async_run([](auto ec){
      BOOST_CHECK_EQUAL(ec, boost::asio::error::basic_errors::operation_aborted);
   });

   ioc.run();
}

BOOST_AUTO_TEST_CASE(cancel_request_if_not_connected)
{
   resp3::request req;
   req.get_config().cancel_if_not_connected = true;
   req.push("HELLO", 3);
   req.push("PING");

   net::io_context ioc;
   auto conn = std::make_shared<connection>(ioc);
   conn->async_exec(req, adapt(), [](auto ec, auto){
      BOOST_CHECK_EQUAL(ec, aedis::error::not_connected);
   });

   ioc.run();
}

// TODO: This test is broken.
BOOST_AUTO_TEST_CASE(request_retry_false)
{
   resp3::request req0;
   req0.get_config().coalesce = false;
   req0.get_config().cancel_on_connection_lost = true;
   req0.push("HELLO", 3);

   resp3::request req1;
   req1.get_config().coalesce = true;
   req1.get_config().cancel_on_connection_lost = true;
   req1.push("BLPOP", "any", 0);

   resp3::request req2;
   req2.get_config().coalesce = true;
   req2.get_config().cancel_on_connection_lost = false;
   req2.get_config().retry_on_connection_lost = false;
   req2.push("PING");

   net::io_context ioc;
   connection conn{ioc};

   net::steady_timer st{ioc};
   st.expires_after(std::chrono::seconds{1});
   st.async_wait([&](auto){
      // Cancels the request before receiving the response. This
      // should cause the second request to complete with error
      // although it has cancel_on_connection_lost = false.
      conn.cancel(aedis::operation::run);
   });

   auto const endpoints = resolve();
   net::connect(conn.next_layer(), endpoints);

   conn.async_exec(req0, adapt(), [](auto ec, auto){
      BOOST_TEST(!ec);
   });

   conn.async_exec(req1, adapt(), [](auto ec, auto){
      BOOST_CHECK_EQUAL(ec, boost::system::errc::errc_t::operation_canceled);
   });

   conn.async_exec(req2, adapt(), [](auto ec, auto){
      BOOST_CHECK_EQUAL(ec, boost::system::errc::errc_t::operation_canceled);
   });

   conn.async_run([](auto ec){
      BOOST_CHECK_EQUAL(ec, boost::system::errc::errc_t::operation_canceled);
   });

   ioc.run();
}

BOOST_AUTO_TEST_CASE(request_retry_true)
{
   resp3::request req0;
   req0.get_config().coalesce = false;
   req0.get_config().cancel_on_connection_lost = true;
   req0.push("HELLO", 3);

   resp3::request req1;
   req1.get_config().coalesce = true;
   req1.get_config().cancel_on_connection_lost = true;
   req1.push("BLPOP", "any", 0);

   resp3::request req2;
   req2.get_config().coalesce = true;
   req2.get_config().cancel_on_connection_lost = false;
   req2.get_config().retry_on_connection_lost = true;
   req2.push("PING");

   resp3::request req3;
   req3.get_config().coalesce = true;
   req3.get_config().cancel_on_connection_lost = true;
   req3.get_config().retry_on_connection_lost = false;
   req3.push("QUIT");

   net::io_context ioc;
   connection conn{ioc};

   net::steady_timer st{ioc};
   st.expires_after(std::chrono::seconds{1});
   st.async_wait([&](auto){
      // Cancels the request before receiving the response. This
      // should cause the second request to complete with error
      // although it has cancel_on_connection_lost = false.
      conn.cancel(aedis::operation::run);
   });

   auto const endpoints = resolve();
   net::connect(conn.next_layer(), endpoints);

   conn.async_exec(req0, adapt(), [](auto ec, auto){
      BOOST_TEST(!ec);
   });

   conn.async_exec(req1, adapt(), [](auto ec, auto){
      BOOST_CHECK_EQUAL(ec, boost::system::errc::errc_t::operation_canceled);
   });

   conn.async_exec(req2, adapt(), [&](auto ec, auto){
      BOOST_TEST(!ec);
      conn.async_exec(req3, adapt(), [&](auto ec, auto){
         BOOST_TEST(!ec);
      });
   });

   conn.async_run([&](auto ec){
      BOOST_CHECK_EQUAL(ec, boost::system::errc::errc_t::operation_canceled);
      conn.reset_stream();
      net::connect(conn.next_layer(), endpoints);
      conn.async_run([&](auto ec){
         std::cout << ec.message() << std::endl;
         BOOST_TEST(!ec);
      });
   });

   ioc.run();
}