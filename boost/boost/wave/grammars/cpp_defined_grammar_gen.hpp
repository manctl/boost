/*=============================================================================
    Boost.Wave: A Standard compliant C++ preprocessor library

    http://www.boost.org/

    Copyright (c) 2001-2005 Hartmut Kaiser. Distributed under the Boost
    Software License, Version 1.0. (See accompanying file
    LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/

#if !defined(CPP_DEFINED_GRAMMAR_GEN_HPP_825BE9F5_98A3_400D_A97C_AD76B3B08632_INCLUDED)
#define CPP_DEFINED_GRAMMAR_GEN_HPP_825BE9F5_98A3_400D_A97C_AD76B3B08632_INCLUDED

#include <boost/wave/wave_config.hpp>

#include <list>

#include <boost/spirit/core/parser.hpp>
#include <boost/pool/pool_alloc.hpp>

#include <boost/wave/util/unput_queue_iterator.hpp>

// this must occur after all of the includes and before any code appears
#ifdef BOOST_HAS_ABI_HEADERS
#include BOOST_ABI_PREFIX
#endif

// suppress warnings about dependent classes not being exported from the dll
#ifdef BOOST_MSVC
#pragma warning(push)
#pragma warning(disable : 4251 4231 4660)
#endif

///////////////////////////////////////////////////////////////////////////////
namespace boost {
namespace wave { 
namespace grammars {

template <typename LexIteratorT>
struct BOOST_WAVE_DECL defined_grammar_gen
{
    typedef typename LexIteratorT::token_type token_type;
    typedef std::list<token_type, boost::fast_pool_allocator<token_type> >
        token_sequence_type;

//  The parse_operator_defined function is instantiated manually twice to 
//  simplify the explicit specialization of this template. This way the user 
//  has only to specify one template parameter (the lexer iterator type) to 
//  correctly formulate the required explicit specialization.
//  This results in no code overhead, because otherwise the function would be
//  generated by the compiler twice anyway.

    typedef boost::wave::util::unput_queue_iterator<
            typename token_sequence_type::iterator, token_type, token_sequence_type>
        iterator1_t;

    typedef boost::wave::util::unput_queue_iterator<
            LexIteratorT, token_type, token_sequence_type>
        iterator2_t;
        
//  parse the operator defined and return the found qualified name
    static boost::spirit::parse_info<iterator1_t> 
    parse_operator_defined (iterator1_t const &first, iterator1_t const &last,
        token_sequence_type &found_qualified_name);

    static boost::spirit::parse_info<iterator2_t> 
    parse_operator_defined (iterator2_t const &first, iterator2_t const &last,
        token_sequence_type &found_qualified_name);
};

///////////////////////////////////////////////////////////////////////////////
}   // namespace grammars
}   // namespace wave
}   // namespace boost

// the suffix header occurs after all of the code
#ifdef BOOST_HAS_ABI_HEADERS
#include BOOST_ABI_SUFFIX
#endif

#endif // !defined(CPP_DEFINED_GRAMMAR_GEN_HPP_825BE9F5_98A3_400D_A97C_AD76B3B08632_INCLUDED)
