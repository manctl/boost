//
// basic_socket_iostream.hpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2006 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_ASIO_BASIC_SOCKET_IOSTREAM_HPP
#define BOOST_ASIO_BASIC_SOCKET_IOSTREAM_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include <boost/asio/detail/push_options.hpp>

#include <boost/asio/detail/push_options.hpp>
#include <boost/preprocessor/arithmetic/inc.hpp>
#include <boost/preprocessor/repetition/enum_binary_params.hpp>
#include <boost/preprocessor/repetition/enum_params.hpp>
#include <boost/preprocessor/repetition/repeat_from_to.hpp>
#include <boost/utility/base_from_member.hpp>
#include <boost/asio/detail/pop_options.hpp>

#include <boost/asio/basic_socketbuf.hpp>
#include <boost/asio/stream_socket_service.hpp>

#if !defined(BOOST_ASIO_SOCKET_IOSTREAM_MAX_ARITY)
#define BOOST_ASIO_SOCKET_IOSTREAM_MAX_ARITY 5
#endif // !defined(BOOST_ASIO_SOCKET_IOSTREAM_MAX_ARITY)

// A macro that should expand to:
//   template < typename T1, ..., typename Tn >
//   explicit basic_socket_iostream( T1 x1, ..., Tn xn )
//     : basic_iostream<char>(&this->boost::base_from_member<
//         basic_socketbuf<Protocol, Service> >::member)
//   {
//     try
//     {
//       rdbuf()->connect ( x1, ..., xn );
//     }
//     catch (boost::asio::error&)
//     {
//       this->setstate(std::ios_base::failbit);
//       if (this->exceptions() & std::ios_base::failbit)
//         throw;
//     }
//   }
// This macro should only persist within this file.

#define BOOST_ASIO_PRIVATE_CTR_DEF( z, n, data ) \
  template < BOOST_PP_ENUM_PARAMS(n, typename T) > \
  explicit basic_socket_iostream( BOOST_PP_ENUM_BINARY_PARAMS(n, T, x) ) \
    : std::basic_iostream<char>(&this->boost::base_from_member< \
        basic_socketbuf<Protocol, Service> >::member) \
  { \
    try \
    { \
      rdbuf()->connect( BOOST_PP_ENUM_PARAMS(n, x) ); \
    } \
    catch (boost::asio::error&) \
    { \
      this->setstate(std::ios_base::failbit); \
      if (this->exceptions() & std::ios_base::failbit) \
        throw; \
    } \
  } \
  /**/

// A macro that should expand to:
//   template < typename T1, ..., typename Tn >
//   void connect( T1 x1, ..., Tn xn )
//   {
//     try
//     {
//       rdbuf()->connect ( x1, ..., xn );
//     }
//     catch (boost::asio::error&)
//     {
//       this->setstate(std::ios_base::failbit);
//       if (this->exceptions() & std::ios_base::failbit)
//         throw;
//     }
//   }
// This macro should only persist within this file.

#define BOOST_ASIO_PRIVATE_CONNECT_DEF( z, n, data ) \
  template < BOOST_PP_ENUM_PARAMS(n, typename T) > \
  void connect( BOOST_PP_ENUM_BINARY_PARAMS(n, T, x) ) \
  { \
    try \
    { \
      rdbuf()->connect( BOOST_PP_ENUM_PARAMS(n, x) ); \
    } \
    catch (boost::asio::error&) \
    { \
      this->setstate(std::ios_base::failbit); \
      if (this->exceptions() & std::ios_base::failbit) \
        throw; \
    } \
  } \
  /**/

namespace boost {
namespace asio {

/// Iostream interface for a socket.
template <typename Protocol,
    typename Service = stream_socket_service<Protocol> >
class basic_socket_iostream
  : public boost::base_from_member<basic_socketbuf<Protocol, Service> >,
    public std::basic_iostream<char>
{
public:
  /// Construct a basic_socket_iostream without establishing a connection.
  basic_socket_iostream()
    : std::basic_iostream<char>(&this->boost::base_from_member<
        basic_socketbuf<Protocol, Service> >::member)
  {
  }

  BOOST_PP_REPEAT_FROM_TO(
      1, BOOST_PP_INC(BOOST_ASIO_SOCKET_IOSTREAM_MAX_ARITY),
      BOOST_ASIO_PRIVATE_CTR_DEF, _ )

  BOOST_PP_REPEAT_FROM_TO(
      1, BOOST_PP_INC(BOOST_ASIO_SOCKET_IOSTREAM_MAX_ARITY),
      BOOST_ASIO_PRIVATE_CONNECT_DEF, _ )

  /// Close the connection.
  void close()
  {
    rdbuf()->close();
  }

  /// Return a pointer to the underlying streambuf.
  basic_socketbuf<Protocol, Service>* rdbuf() const
  {
    return const_cast<basic_socketbuf<Protocol, Service>*>(
        &this->boost::base_from_member<
          basic_socketbuf<Protocol, Service> >::member);
  }
};

} // namespace asio
} // namespace boost

#undef BOOST_ASIO_PRIVATE_CTR_DEF
#undef BOOST_ASIO_PRIVATE_CONNECT_DEF

#include <boost/asio/detail/pop_options.hpp>

#endif // BOOST_ASIO_BASIC_SOCKET_IOSTREAM_HPP
