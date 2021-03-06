#ifndef BOOST_THREAD_THREAD_COMMON_HPP
#define BOOST_THREAD_THREAD_COMMON_HPP
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
// (C) Copyright 2007-10 Anthony Williams
// (C) Copyright 20011-12 Vicente J. Botet Escriba

#include <boost/thread/detail/config.hpp>
#include <boost/thread/exceptions.hpp>
#ifndef BOOST_NO_IOSTREAM
#include <ostream>
#endif
#if defined BOOST_THREAD_USES_MOVE
#include <boost/move/move.hpp>
#else
#include <boost/thread/detail/move.hpp>
#endif
#include <boost/thread/mutex.hpp>
#include <boost/thread/xtime.hpp>
#include <boost/thread/detail/thread_heap_alloc.hpp>
#include <boost/utility.hpp>
#include <boost/assert.hpp>
#include <list>
#include <algorithm>
#include <boost/ref.hpp>
#include <boost/cstdint.hpp>
#include <boost/bind.hpp>
#include <stdlib.h>
#include <memory>
#include <boost/utility/enable_if.hpp>
#include <boost/type_traits/remove_reference.hpp>
#include <boost/io/ios_state.hpp>
#include <boost/type_traits/is_same.hpp>
#include <boost/type_traits/decay.hpp>
#include <boost/functional/hash.hpp>
#ifdef BOOST_THREAD_USES_CHRONO
#include <boost/chrono/system_clocks.hpp>
#include <boost/chrono/ceil.hpp>
#endif

#include <boost/config/abi_prefix.hpp>

#ifdef BOOST_MSVC
#pragma warning(push)
#pragma warning(disable:4251)
#endif

namespace boost
{

#ifndef BOOST_NO_RVALUE_REFERENCES
  namespace thread_detail
  {
      template <class T>
      typename decay<T>::type
      decay_copy(T&& t)
      {
          return boost::forward<T>(t);
      }
  }
#endif

    namespace detail
    {
        template<typename F>
        class thread_data:
            public detail::thread_data_base
        {
        public:
#ifndef BOOST_NO_RVALUE_REFERENCES
            thread_data(F&& f_):
              f(boost::forward<F>(f_))
            {}
// This overloading must be removed if we want the packaged_task's tests to pass.
//            thread_data(F& f_):
//                f(f_)
//            {}
#else
            thread_data(F f_):
                f(f_)
            {}
#if defined BOOST_THREAD_USES_MOVE
            thread_data(boost::rv<F>& f_):
                f(boost::move(f_))
            {}
#else
            thread_data(detail::thread_move_t<F> f_):
                f(f_)
            {}
#endif
#endif
            void run()
            {
                f();
            }
        private:
            F f;

            void operator=(thread_data&);
            thread_data(thread_data&);
        };

        template<typename F>
        class thread_data<boost::reference_wrapper<F> >:
            public detail::thread_data_base
        {
        private:
            F& f;

            void operator=(thread_data&);
            thread_data(thread_data&);
        public:
            thread_data(boost::reference_wrapper<F> f_):
                f(f_)
            {}

            void run()
            {
                f();
            }
        };

        template<typename F>
        class thread_data<const boost::reference_wrapper<F> >:
            public detail::thread_data_base
        {
        private:
            F& f;
            void operator=(thread_data&);
            thread_data(thread_data&);
        public:
            thread_data(const boost::reference_wrapper<F> f_):
                f(f_)
            {}

            void run()
            {
                f();
            }
        };
    }

    class BOOST_THREAD_DECL thread
    {
    public:
      typedef thread_attributes attributes;

#ifndef BOOST_NO_DELETED_FUNCTIONS
    public:
      thread(thread const&) = delete;
      thread& operator=(thread const&) = delete;
#else // BOOST_NO_DELETED_FUNCTIONS
    private:
      thread(thread&);
      thread& operator=(thread&);
#endif // BOOST_NO_DELETED_FUNCTIONS
    private:

        void release_handle();

        detail::thread_data_ptr thread_info;

        void start_thread();
        void start_thread(const attributes& attr);

        explicit thread(detail::thread_data_ptr data);

        detail::thread_data_ptr get_thread_info BOOST_PREVENT_MACRO_SUBSTITUTION () const;

#ifndef BOOST_NO_RVALUE_REFERENCES
        template<typename F>
        static inline detail::thread_data_ptr make_thread_info(F&& f)
        {
            return detail::thread_data_ptr(detail::heap_new<detail::thread_data<typename boost::remove_reference<F>::type> >(
                boost::forward<F>(f)));
        }
        static inline detail::thread_data_ptr make_thread_info(void (*f)())
        {
            return detail::thread_data_ptr(detail::heap_new<detail::thread_data<void(*)()> >(
                boost::forward<void(*)()>(f)));
        }
#else
        template<typename F>
        static inline detail::thread_data_ptr make_thread_info(F f)
        {
            return detail::thread_data_ptr(detail::heap_new<detail::thread_data<F> >(f));
        }
#if defined BOOST_THREAD_USES_MOVE
        template<typename F>
        static inline detail::thread_data_ptr make_thread_info(boost::rv<F>& f)
        {
            return detail::thread_data_ptr(detail::heap_new<detail::thread_data<F> >(boost::move(f)));
        }

#else
        template<typename F>
        static inline detail::thread_data_ptr make_thread_info(boost::detail::thread_move_t<F> f)
        {
            return detail::thread_data_ptr(detail::heap_new<detail::thread_data<F> >(f));
        }

#endif
#endif
        struct dummy;
    public:
#if BOOST_WORKAROUND(__SUNPRO_CC, < 0x5100)
        thread(const volatile thread&);
#endif
        thread() BOOST_NOEXCEPT;
        ~thread()
        {
    #if defined BOOST_THREAD_DESTRUCTOR_CALLS_TERMINATE_IF_JOINABLE
          if (joinable()) {
            std::terminate();
          }
    #else
            detach();
    #endif
        }
#ifndef BOOST_NO_RVALUE_REFERENCES
        template <
          class F
        >
        explicit thread(F&& f
        , typename disable_if<is_same<typename decay<F>::type, thread>, dummy* >::type=0
        ):
          thread_info(make_thread_info(thread_detail::decay_copy(boost::forward<F>(f))))
        {
            start_thread();
        }
        template <
          class F
        >
        thread(attributes& attrs, F&& f
        , typename disable_if<is_same<typename decay<F>::type, thread>, dummy* >::type=0
        ):
          thread_info(make_thread_info(thread_detail::decay_copy(boost::forward<F>(f))))
        {
            start_thread(attrs);
        }

        thread(thread&& other) BOOST_NOEXCEPT
        {
            thread_info.swap(other.thread_info);
        }

        thread& operator=(thread&& other) BOOST_NOEXCEPT
        {
#if defined BOOST_THREAD_MOVE_ASSIGN_CALLS_TERMINATE_IF_JOINABLE
            if (joinable()) std::terminate();
#endif
            thread_info=other.thread_info;
            other.thread_info.reset();
            return *this;
        }

//        thread&& move()
//        {
//            return ::boost::move(*this);
//        }

#else
#ifdef BOOST_NO_SFINAE
        template <class F>
        explicit thread(F f):
            thread_info(make_thread_info(f))
        {
            start_thread();
        }
        template <class F>
        thread(attributes& attrs, F f):
            thread_info(make_thread_info(f))
        {
            start_thread(attrs);
        }
#else
#if defined BOOST_THREAD_USES_MOVE
        template <class F>
        explicit thread(F f,typename disable_if<boost::is_convertible<F&,boost::rv<F>& >, dummy* >::type=0):
            thread_info(make_thread_info(f))
        {
            start_thread();
        }
        template <class F>
        thread(attributes& attrs, F f,typename disable_if<boost::is_convertible<F&,boost::rv<F>& >, dummy* >::type=0):
            thread_info(make_thread_info(f))
        {
            start_thread(attrs);
        }
#else
        template <class F>
        explicit thread(F f,typename disable_if<boost::is_convertible<F&,detail::thread_move_t<F> >, dummy* >::type=0):
            thread_info(make_thread_info(f))
        {
            start_thread();
        }
        template <class F>
        thread(attributes& attrs, F f,typename disable_if<boost::is_convertible<F&,detail::thread_move_t<F> >, dummy* >::type=0):
            thread_info(make_thread_info(f))
        {
            start_thread(attrs);
        }
#endif
#endif

#if defined BOOST_THREAD_USES_MOVE
        template <class F>
        explicit thread(boost::rv<F>& f):
            thread_info(make_thread_info(boost::move(f)))
        {
            start_thread();
        }


//        explicit thread(void (*f)()):
//            thread_info(make_thread_info(f))
//        {
//            start_thread();
//        }
//

        template <class F>
        thread(attributes& attrs, boost::rv<F>& f):
            thread_info(make_thread_info(boost::move(f)))
        {
            start_thread(attrs);
        }


        thread(boost::rv<thread>& x)
        {
            thread_info=x.thread_info;
            x.thread_info.reset();
        }
#else
        template <class F>
        explicit thread(detail::thread_move_t<F> f):
            thread_info(make_thread_info(f))
        {
            start_thread();
        }

        template <class F>
        thread(attributes& attrs, detail::thread_move_t<F> f):
            thread_info(make_thread_info(f))
        {
            start_thread(attrs);
        }

        thread(detail::thread_move_t<thread> x)
        {
            thread_info=x->thread_info;
            x->thread_info.reset();
        }
#endif

#if BOOST_WORKAROUND(__SUNPRO_CC, < 0x5100)
        thread& operator=(thread x)
        {
            swap(x);
            return *this;
        }
#else
#if defined BOOST_THREAD_USES_MOVE
        thread& operator=(boost::rv<thread>& x) BOOST_NOEXCEPT
        {
#if defined BOOST_THREAD_MOVE_ASSIGN_CALLS_TERMINATE_IF_JOINABLE
            if (joinable()) std::terminate();
#endif
            thread new_thread(boost::move(x));
            swap(new_thread);
            return *this;
        }
#else
        thread& operator=(detail::thread_move_t<thread> x) BOOST_NOEXCEPT
        {
#if defined BOOST_THREAD_MOVE_ASSIGN_CALLS_TERMINATE_IF_JOINABLE
            if (joinable()) std::terminate();
#endif
            thread new_thread(x);
            swap(new_thread);
            return *this;
        }
#endif
#endif

#if defined BOOST_THREAD_USES_MOVE
        ::boost::rv<thread>& move()  BOOST_NOEXCEPT
        {
          return *static_cast< ::boost::rv<thread>* >(this);
        }
        const ::boost::rv<thread>& move() const BOOST_NOEXCEPT
        {
          return *static_cast<const ::boost::rv<thread>* >(this);
        }

      operator ::boost::rv<thread>&()  BOOST_NOEXCEPT
      {
        return *static_cast< ::boost::rv<thread>* >(this);
      }
      operator const ::boost::rv<thread>&() const BOOST_NOEXCEPT
      {
        return *static_cast<const ::boost::rv<thread>* >(this);
      }
#else
        operator detail::thread_move_t<thread>() BOOST_NOEXCEPT
        {
            return move();
        }

        detail::thread_move_t<thread> move() BOOST_NOEXCEPT
        {
            detail::thread_move_t<thread> x(*this);
            return x;
        }
#endif

#endif

        template <class F,class A1>
        thread(F f,A1 a1,typename disable_if<boost::is_convertible<F&,thread_attributes >, dummy* >::type=0):
            thread_info(make_thread_info(boost::bind(boost::type<void>(),f,a1)))
        {
            start_thread();
        }
        template <class F,class A1,class A2>
        thread(F f,A1 a1,A2 a2):
            thread_info(make_thread_info(boost::bind(boost::type<void>(),f,a1,a2)))
        {
            start_thread();
        }

        template <class F,class A1,class A2,class A3>
        thread(F f,A1 a1,A2 a2,A3 a3):
            thread_info(make_thread_info(boost::bind(boost::type<void>(),f,a1,a2,a3)))
        {
            start_thread();
        }

        template <class F,class A1,class A2,class A3,class A4>
        thread(F f,A1 a1,A2 a2,A3 a3,A4 a4):
            thread_info(make_thread_info(boost::bind(boost::type<void>(),f,a1,a2,a3,a4)))
        {
            start_thread();
        }

        template <class F,class A1,class A2,class A3,class A4,class A5>
        thread(F f,A1 a1,A2 a2,A3 a3,A4 a4,A5 a5):
            thread_info(make_thread_info(boost::bind(boost::type<void>(),f,a1,a2,a3,a4,a5)))
        {
            start_thread();
        }

        template <class F,class A1,class A2,class A3,class A4,class A5,class A6>
        thread(F f,A1 a1,A2 a2,A3 a3,A4 a4,A5 a5,A6 a6):
            thread_info(make_thread_info(boost::bind(boost::type<void>(),f,a1,a2,a3,a4,a5,a6)))
        {
            start_thread();
        }

        template <class F,class A1,class A2,class A3,class A4,class A5,class A6,class A7>
        thread(F f,A1 a1,A2 a2,A3 a3,A4 a4,A5 a5,A6 a6,A7 a7):
            thread_info(make_thread_info(boost::bind(boost::type<void>(),f,a1,a2,a3,a4,a5,a6,a7)))
        {
            start_thread();
        }

        template <class F,class A1,class A2,class A3,class A4,class A5,class A6,class A7,class A8>
        thread(F f,A1 a1,A2 a2,A3 a3,A4 a4,A5 a5,A6 a6,A7 a7,A8 a8):
            thread_info(make_thread_info(boost::bind(boost::type<void>(),f,a1,a2,a3,a4,a5,a6,a7,a8)))
        {
            start_thread();
        }

        template <class F,class A1,class A2,class A3,class A4,class A5,class A6,class A7,class A8,class A9>
        thread(F f,A1 a1,A2 a2,A3 a3,A4 a4,A5 a5,A6 a6,A7 a7,A8 a8,A9 a9):
            thread_info(make_thread_info(boost::bind(boost::type<void>(),f,a1,a2,a3,a4,a5,a6,a7,a8,a9)))
        {
            start_thread();
        }

        void swap(thread& x) BOOST_NOEXCEPT
        {
            thread_info.swap(x.thread_info);
        }

        class BOOST_SYMBOL_VISIBLE id;
        id get_id() const BOOST_NOEXCEPT;


        bool joinable() const BOOST_NOEXCEPT;
        void join();
#if defined(BOOST_THREAD_PLATFORM_WIN32)
        bool timed_join(const system_time& abs_time); // DEPRECATED V2

#ifdef BOOST_THREAD_USES_CHRONO
        template <class Rep, class Period>
        bool try_join_for(const chrono::duration<Rep, Period>& rel_time)
        {
          return try_join_until(chrono::steady_clock::now() + rel_time);
        }
        template <class Clock, class Duration>
        bool try_join_until(const chrono::time_point<Clock, Duration>& t)
        {
          using namespace chrono;
          system_clock::time_point     s_now = system_clock::now();
          typename Clock::time_point  c_now = Clock::now();
          return try_join_until(s_now + ceil<nanoseconds>(t - c_now));
        }
        template <class Duration>
        bool try_join_until(const chrono::time_point<chrono::system_clock, Duration>& t)
        {
          using namespace chrono;
          typedef time_point<system_clock, nanoseconds> nano_sys_tmpt;
          return try_join_until(nano_sys_tmpt(ceil<nanoseconds>(t.time_since_epoch())));
        }
        bool try_join_until(const chrono::time_point<chrono::system_clock, chrono::nanoseconds>& tp);
#endif
    public:

#else
        bool timed_join(const system_time& abs_time) // DEPRECATED V2
        {
          struct timespec const ts=detail::get_timespec(abs_time);
          return do_try_join_until(ts);
        }
#ifdef BOOST_THREAD_USES_CHRONO
        template <class Rep, class Period>
        bool try_join_for(const chrono::duration<Rep, Period>& rel_time)
        {
          return try_join_until(chrono::steady_clock::now() + rel_time);
        }
        template <class Clock, class Duration>
        bool try_join_until(const chrono::time_point<Clock, Duration>& t)
        {
          using namespace chrono;
          system_clock::time_point     s_now = system_clock::now();
          typename Clock::time_point  c_now = Clock::now();
          return try_join_until(s_now + ceil<nanoseconds>(t - c_now));
        }
        template <class Duration>
        bool try_join_until(const chrono::time_point<chrono::system_clock, Duration>& t)
        {
          using namespace chrono;
          typedef time_point<system_clock, nanoseconds> nano_sys_tmpt;
          return try_join_until(nano_sys_tmpt(ceil<nanoseconds>(t.time_since_epoch())));
        }
        bool try_join_until(const chrono::time_point<chrono::system_clock, chrono::nanoseconds>& tp)
        {
          using namespace chrono;
          nanoseconds d = tp.time_since_epoch();
          timespec ts;
          seconds s = duration_cast<seconds>(d);
          ts.tv_sec = static_cast<long>(s.count());
          ts.tv_nsec = static_cast<long>((d - s).count());
          return do_try_join_until(ts);
        }
#endif
      private:
        bool do_try_join_until(struct timespec const &timeout);
      public:

#endif

        template<typename TimeDuration>
        inline bool timed_join(TimeDuration const& rel_time)  // DEPRECATED V2
        {
            return timed_join(get_system_time()+rel_time);
        }

        void detach() BOOST_NOEXCEPT;

        static unsigned hardware_concurrency() BOOST_NOEXCEPT;

#define BOOST_THREAD_DEFINES_THREAD_NATIVE_HANDLE
        typedef detail::thread_data_base::native_handle_type native_handle_type;
        native_handle_type native_handle();

        // backwards compatibility
        bool operator==(const thread& other) const; // DEPRECATED V2
        bool operator!=(const thread& other) const; // DEPRECATED V2

        static inline void yield() BOOST_NOEXCEPT
        {
            this_thread::yield();
        }

        static inline void sleep(const system_time& xt) // DEPRECATED V2
        {
            this_thread::sleep(xt);
        }

        // extensions
        void interrupt();
        bool interruption_requested() const BOOST_NOEXCEPT;
    };

    inline void swap(thread& lhs,thread& rhs) BOOST_NOEXCEPT
    {
        return lhs.swap(rhs);
    }

#ifndef BOOST_NO_RVALUE_REFERENCES
    inline thread&& move(thread& t) BOOST_NOEXCEPT
    {
        return static_cast<thread&&>(t);
    }
    inline thread&& move(thread&& t) BOOST_NOEXCEPT
    {
        return static_cast<thread&&>(t);
    }
#else
#if !defined BOOST_THREAD_USES_MOVE
    inline detail::thread_move_t<thread> move(detail::thread_move_t<thread> t) BOOST_NOEXCEPT
    {
        return t;
    }
#endif
#endif

#ifdef BOOST_NO_RVALUE_REFERENCES
#if !defined BOOST_THREAD_USES_MOVE
    template <>
    struct has_move_emulation_enabled_aux<thread>
      : BOOST_MOVE_BOOST_NS::integral_constant<bool, true>
    {};
#endif
#endif

    namespace this_thread
    {
        thread::id BOOST_THREAD_DECL get_id() BOOST_NOEXCEPT;

        void BOOST_THREAD_DECL interruption_point();
        bool BOOST_THREAD_DECL interruption_enabled() BOOST_NOEXCEPT;
        bool BOOST_THREAD_DECL interruption_requested() BOOST_NOEXCEPT;

        inline BOOST_SYMBOL_VISIBLE void sleep(xtime const& abs_time) // DEPRECATED V2
        {
            sleep(system_time(abs_time));
        }
    }

    class BOOST_SYMBOL_VISIBLE thread::id
    {
    private:
        friend inline
        std::size_t
        hash_value(const thread::id &v)
        {
          return hash_value(v.thread_data.get());
        }

        detail::thread_data_ptr thread_data;

        id(detail::thread_data_ptr thread_data_):
            thread_data(thread_data_)
        {}
        friend class thread;
        friend id BOOST_THREAD_DECL this_thread::get_id() BOOST_NOEXCEPT;
    public:
        id() BOOST_NOEXCEPT:
            thread_data()
        {}

        id(const id& other) BOOST_NOEXCEPT :
            thread_data(other.thread_data)
        {}

        bool operator==(const id& y) const BOOST_NOEXCEPT
        {
            return thread_data==y.thread_data;
        }

        bool operator!=(const id& y) const BOOST_NOEXCEPT
        {
            return thread_data!=y.thread_data;
        }

        bool operator<(const id& y) const BOOST_NOEXCEPT
        {
            return thread_data<y.thread_data;
        }

        bool operator>(const id& y) const BOOST_NOEXCEPT
        {
            return y.thread_data<thread_data;
        }

        bool operator<=(const id& y) const BOOST_NOEXCEPT
        {
            return !(y.thread_data<thread_data);
        }

        bool operator>=(const id& y) const BOOST_NOEXCEPT
        {
            return !(thread_data<y.thread_data);
        }

#ifndef BOOST_NO_IOSTREAM
#ifndef BOOST_NO_MEMBER_TEMPLATE_FRIENDS
        template<class charT, class traits>
        friend BOOST_SYMBOL_VISIBLE
  std::basic_ostream<charT, traits>&
        operator<<(std::basic_ostream<charT, traits>& os, const id& x)
        {
            if(x.thread_data)
            {
                io::ios_flags_saver  ifs( os );
                return os<< std::hex << x.thread_data;
            }
            else
            {
                return os<<"{Not-any-thread}";
            }
        }
#else
        template<class charT, class traits>
        BOOST_SYMBOL_VISIBLE
  std::basic_ostream<charT, traits>&
        print(std::basic_ostream<charT, traits>& os) const
        {
            if(thread_data)
            {
                return os<<thread_data;
            }
            else
            {
                return os<<"{Not-any-thread}";
            }
        }

#endif
#endif
    };

#if !defined(BOOST_NO_IOSTREAM) && defined(BOOST_NO_MEMBER_TEMPLATE_FRIENDS)
    template<class charT, class traits>
    BOOST_SYMBOL_VISIBLE
    std::basic_ostream<charT, traits>&
    operator<<(std::basic_ostream<charT, traits>& os, const thread::id& x)
    {
        return x.print(os);
    }
#endif

    inline bool thread::operator==(const thread& other) const
    {
        return get_id()==other.get_id();
    }

    inline bool thread::operator!=(const thread& other) const
    {
        return get_id()!=other.get_id();
    }

    namespace detail
    {
        struct thread_exit_function_base
        {
            virtual ~thread_exit_function_base()
            {}
            virtual void operator()()=0;
        };

        template<typename F>
        struct thread_exit_function:
            thread_exit_function_base
        {
            F f;

            thread_exit_function(F f_):
                f(f_)
            {}

            void operator()()
            {
                f();
            }
        };

        void BOOST_THREAD_DECL add_thread_exit_function(thread_exit_function_base*);
    }

    namespace this_thread
    {
        template<typename F>
        void at_thread_exit(F f)
        {
            detail::thread_exit_function_base* const thread_exit_func=detail::heap_new<detail::thread_exit_function<F> >(f);
            detail::add_thread_exit_function(thread_exit_func);
        }
    }
}

#ifdef BOOST_MSVC
#pragma warning(pop)
#endif

#include <boost/config/abi_suffix.hpp>

#endif
