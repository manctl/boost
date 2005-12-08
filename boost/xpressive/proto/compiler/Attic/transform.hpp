///////////////////////////////////////////////////////////////////////////////
// transform.hpp
//
//  Copyright 2004 Eric Niebler. Distributed under the Boost
//  Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_PROTO_COMPILER_TRANSFORM_HPP_EAN_04_01_2005
#define BOOST_PROTO_COMPILER_TRANSFORM_HPP_EAN_04_01_2005

#include <boost/xpressive/proto/proto_fwd.hpp>
#include <boost/xpressive/proto/arg_traits.hpp>

namespace boost { namespace proto
{

    ///////////////////////////////////////////////////////////////////////////////
    // transform_compiler
    //   accepts a transformation as a lambda, applies the transformation to any
    //   parse tree passed in, and then compiles the result using the specified
    //   compiler
    template<typename Lambda, typename DomainTag, typename Compiler>
    struct transform_compiler
    {
        template<typename Op, typename State, typename Visitor>
        struct apply
        {
            typedef typename Compiler::BOOST_NESTED_TEMPLATE apply
            <
                typename Lambda::BOOST_NESTED_TEMPLATE apply<Op, State, Visitor>::type
              , State
              , Visitor
            >::type type;
        };

        template<typename Op, typename State, typename Visitor>
        static typename apply<Op, State, Visitor>::type
        call(Op const &op, State const &state, Visitor &visitor)
        {
            return Compiler::call(Lambda::call(op, state, visitor), state, visitor);
        }
    };

    ///////////////////////////////////////////////////////////////////////////////
    // if no compiler is specified, the transform_compiler forwards to the default
    // compiler
    template<typename Lambda, typename DomainTag>
    struct transform_compiler<Lambda, DomainTag, void>
    {
        template<typename Op, typename State, typename Visitor>
        struct apply
        {
            typedef typename Lambda::BOOST_NESTED_TEMPLATE apply
            <
                Op
              , State
              , Visitor
            >::type trans_type;

            typedef proto::compiler<typename tag_type<trans_type>::type, DomainTag> compiler_type;

            typedef typename compiler_type::BOOST_NESTED_TEMPLATE apply
            <
                trans_type
              , State
              , Visitor
            >::type type;
        };

        template<typename Op, typename State, typename Visitor>
        static typename apply<Op, State, Visitor>::type
        call(Op const &op, State const &state, Visitor &visitor)
        {
            return proto::compile(Lambda::call(op, state, visitor), state, visitor, DomainTag());
        }
    };

    ///////////////////////////////////////////////////////////////////////////////
    // arg_transform
    struct arg_transform
    {
        template<typename Op, typename, typename>
        struct apply
        {
            typedef typename arg_type<Op>::type type;
        };

        template<typename Op, typename State, typename Visitor>
        static typename arg_type<Op>::const_reference
        call(Op const &op, State const &, Visitor &)
        {
            return proto::arg(op);
        }
    };

    ///////////////////////////////////////////////////////////////////////////////
    // left_transform
    struct left_transform
    {
        template<typename Op, typename, typename>
        struct apply
        {
            typedef typename left_type<Op>::type type;
        };

        template<typename Op, typename State, typename Visitor>
        static typename left_type<Op>::const_reference
        call(Op const &op, State const &, Visitor &)
        {
            return proto::left(op);
        }
    };

    ///////////////////////////////////////////////////////////////////////////////
    // right_transform
    struct right_transform
    {
        template<typename Op, typename, typename>
        struct apply
        {
            typedef typename right_type<Op>::type type;
        };

        template<typename Op, typename State, typename Visitor>
        static typename right_type<Op>::const_reference
        call(Op const &op, State const &, Visitor &)
        {
            return proto::right(op);
        }
    };

    ///////////////////////////////////////////////////////////////////////////////
    // unary_op_transform
    //   insert a unary operation
    template<typename Tag>
    struct unary_op_transform
    {
        template<typename Op, typename, typename>
        struct apply
        {
            typedef unary_op<Op, Tag> type;
        };

        template<typename Op, typename State, typename Visitor>
        static unary_op<Op, Tag>
        call(Op const &op, State const &, Visitor &)
        {
            return proto::make_op<Tag>(op);
        }
    };

    ///////////////////////////////////////////////////////////////////////////////
    // compose_transforms
    //   execute two transforms in succession
    template<typename First, typename Second>
    struct compose_transforms
    {
        template<typename Op, typename State, typename Visitor>
        struct apply
        {
            typedef typename Second::BOOST_NESTED_TEMPLATE apply
            <
                typename First::BOOST_NESTED_TEMPLATE apply<Op, State, Visitor>::type
              , State
              , Visitor
            >::type type;
        };

        template<typename Op, typename State, typename Visitor>
        static typename apply<Op, State, Visitor>::type
        call(Op const &op, State const &state, Visitor &visitor)
        {
            return Second::call(First::call(op, state, visitor), state, visitor);
        }
    };

}}

#endif
