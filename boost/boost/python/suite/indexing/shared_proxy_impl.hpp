// -*- mode:c++; switch-modules-target: "container_proxy.hpp" -*-
//
// Header file shared_proxy_impl.hpp
//
// The shared implementation of the element proxy (const and non-const
// versions).  Multiple proxy pointers can refer to the same shared
// implementation, meaning that only one object requires updating to
// affect all proxy pointers that ultimately refer to the same index
// in the container.
//
// Copyright (c) 2003 Raoul M. Gough
//
// Use, modification and distribution is subject to the Boost Software
// License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy
// at http://www.boost.org/LICENSE_1_0.txt)
//
// History
// =======
// 2003/ 8/29   rmg     File creation from container_proxy.hh
//
// $Id$
//

#ifndef BOOST_PYTHON_INDEXING_SHARED_PROXY_IMPL_HPP
#define BOOST_PYTHON_INDEXING_SHARED_PROXY_IMPL_HPP

#include <memory>

namespace boost { namespace python { namespace indexing {
  template<class ContainerProxy>
  class shared_proxy_impl
  {
  public:
    typedef typename ContainerProxy::raw_value_type value_type;
    typedef value_type &reference;
    typedef value_type *pointer;
    typedef void iterator_category;  // Fudge for boost::indirect_iterator
    typedef void difference_type;    // Fudge for boost::indirect_iterator

    reference operator*() const;
    pointer operator->() const { return &(**this); }

    ContainerProxy *owner() const { return m_owner_ptr; }
    size_t index() const { return m_index; }

    shared_proxy_impl (value_type const &copy);
    // Creates value-only (detached) proxy

  private:
    template<class C, class A> friend class container_proxy;

    shared_proxy_impl (ContainerProxy *, size_t);
    void detach ();

  private:
    ContainerProxy *m_owner_ptr;             // When attached
    size_t m_index;                          // When attached
    std::auto_ptr<value_type> m_element_ptr; // When detached

  private:
    // Not implemented
    shared_proxy_impl (shared_proxy_impl const &);
    shared_proxy_impl &operator= (shared_proxy_impl const &);
  };

  template<class ContainerProxy>
  shared_proxy_impl<ContainerProxy>::shared_proxy_impl (ContainerProxy *owner
                                                        , size_t index)
    : m_owner_ptr (owner)
    , m_index (index)
    , m_element_ptr ()
  {
  }

  template<class ContainerProxy>
  shared_proxy_impl<ContainerProxy>::shared_proxy_impl (value_type const &val)
    : m_owner_ptr (0)
    , m_index (static_cast<size_t>(-1))
    , m_element_ptr (new value_type (val))
  {
  }

  template<class ContainerProxy>
  typename shared_proxy_impl<ContainerProxy>::reference
  shared_proxy_impl<ContainerProxy>::operator* () const
  {
    return m_owner_ptr
      ? m_owner_ptr->raw_container().at (m_index)
      : *m_element_ptr;
  }

  template<class ContainerProxy>
  void shared_proxy_impl<ContainerProxy>::detach ()
  {
    m_element_ptr.reset (new value_type (**this));
    m_owner_ptr = 0;
    m_index = static_cast<size_t>(-1);
  }
} } }

#endif // BOOST_PYTHON_INDEXING_SHARED_PROXY_IMPL_HPP
