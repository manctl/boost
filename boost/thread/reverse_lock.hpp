// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
// (C) Copyright 2012 Vicente J. Botet Escriba

#ifndef BOOST_THREAD_REVERSE_LOCK_HPP
#define BOOST_THREAD_REVERSE_LOCK_HPP
#include <boost/thread/detail/config.hpp>
#include <boost/thread/locks.hpp>

namespace boost
{

    template<typename Lock>
    class reverse_lock
    {

#ifndef BOOST_NO_DELETED_FUNCTIONS
    public:
        reverse_lock(reverse_lock const&) = delete;
        reverse_lock& operator=(reverse_lock const&) = delete;
#else // BOOST_NO_DELETED_FUNCTIONS
    private:
        reverse_lock(reverse_lock&);
        reverse_lock& operator=(reverse_lock&);
#endif // BOOST_NO_DELETED_FUNCTIONS
    public:
        typedef typename Lock::mutex_type mutex_type;

        explicit reverse_lock(Lock& m_)
        : m(m_), mtx(0)
        {
            if (m.owns_lock())
            {
              m.unlock();
            }
            mtx=m.release();
        }
        ~reverse_lock()
        {
          if (mtx) {
            mtx->lock();
            m = Lock(*mtx, adopt_lock);
          }
        }

    private:
      Lock& m;
      mutex_type* mtx;
    };


#ifdef BOOST_THREAD_NO_AUTO_DETECT_MUTEX_TYPES
    template<typename T>
    struct is_mutex_type<reverse_lock<T> >
    {
        BOOST_STATIC_CONSTANT(bool, value = true);
    };

#endif


}

#endif // header
