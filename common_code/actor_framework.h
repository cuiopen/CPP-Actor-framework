/*!
 @header     actor_framework.h
 @abstract   并发逻辑控制框架(Actor Model)，使用"协程(coroutine)"技术，依赖boost_1.55或更新;
 @discussion 一个Actor对象(actor_handle)依赖一个shared_strand(二级调度器，本身依赖于io_service)，多个Actor可以共同依赖同一个shared_strand;
             支持强制结束、挂起/恢复、延时、多子任务(并发控制);
             在Actor中或所依赖的io_service中进行长时间阻塞的操作或重量级运算，会严重影响依赖同一个io_service的Actor响应速度;
             默认Actor栈空间64k字节，远比线程栈小，注意局部变量占用的空间以及调用层次(注意递归).
 @copyright  Copyright (c) 2015 HAM, E-Mail:591170887@qq.com
 */

#ifndef __ACTOR_FRAMEWORK_H
#define __ACTOR_FRAMEWORK_H

#include <list>
#include <xutility>
#include <functional>
#include "ios_proxy.h"
#include "shared_strand.h"
#include "ref_ex.h"
#include "function_type.h"
#include "msg_queue.h"
#include "actor_mutex.h"
#include "scattered.h"
#include "stack_object.h"
#include "check_actor_stack.h"
#include "actor_timer.h"

class my_actor;
typedef std::shared_ptr<my_actor> actor_handle;//Actor句柄

class mutex_trig_notifer;
class mutex_trig_handle;
class _actor_mutex;

using namespace std;

//此函数会进入Actor中断标记，使用时注意逻辑的“连续性”可能会被打破
#define __yield_interrupt

/*!
@brief 用于检测在Actor内调用的函数是否触发了强制退出
*/
#ifdef _DEBUG
#define BEGIN_CHECK_FORCE_QUIT try {
#define END_CHECK_FORCE_QUIT } catch (my_actor::force_quit_exception&) {assert(false);}
#else
#define BEGIN_CHECK_FORCE_QUIT
#define END_CHECK_FORCE_QUIT
#endif

//检测 pump_msg 是否有 pump_disconnected_exception 异常抛出，因为在 catch 内部不能安全的进行coro切换
#define CATCH_PUMP_DISCONNECTED CATCH_FOR(my_actor::pump_disconnected_exception)

template <typename T0 = void, typename T1 = void, typename T2 = void, typename T3 = void>
struct msg_param
{
	typedef ref_ex<T0, T1, T2, T3> ref_type;
	typedef const_ref_ex<T0, T1, T2, T3> const_ref_type;

	template <typename PT0, typename PT1, typename PT2, typename PT3>
	msg_param(PT0&& p0, PT1&& p1, PT2&& p2, PT3&& p3)
		:_res0(TRY_MOVE(p0)), _res1(TRY_MOVE(p1)), _res2(TRY_MOVE(p2)), _res3(TRY_MOVE(p3)) {}

	msg_param(const_ref_type& rp)
		:_res0(rp._p0), _res1(rp._p1), _res2(rp._p2), _res3(rp._p3) {}

	msg_param(ref_type& s)
		:_res0(std::move(s._p0)), _res1(std::move(s._p1)), _res2(std::move(s._p2)), _res3(std::move(s._p3)) {}

	msg_param(msg_param&& s)
		:_res0(std::move(s._res0)), _res1(std::move(s._res1)), _res2(std::move(s._res2)), _res3(std::move(s._res3)) {}

	void operator =(msg_param&& s)
	{
		_res0 = std::move(s._res0);
		_res1 = std::move(s._res1);
		_res2 = std::move(s._res2);
		_res3 = std::move(s._res3);
	}

	void move_out(ref_type& dst)
	{
		dst._p0 = std::move(_res0);
		dst._p1 = std::move(_res1);
		dst._p2 = std::move(_res2);
		dst._p3 = std::move(_res3);
	}

	void save_from(const_ref_type& rp)
	{
		_res0 = rp._p0;
		_res1 = rp._p1;
		_res2 = rp._p2;
		_res3 = rp._p3;
	}

	void move_from(ref_type& src)
	{
		_res0 = std::move(src._p0);
		_res1 = std::move(src._p1);
		_res2 = std::move(src._p2);
		_res3 = std::move(src._p3);
	}

	T0 _res0;
	T1 _res1;
	T2 _res2;
	T3 _res3;
};

template <typename T0, typename T1, typename T2>
struct msg_param<T0, T1, T2, void>
{
	typedef ref_ex<T0, T1, T2> ref_type;
	typedef const_ref_ex<T0, T1, T2> const_ref_type;

	template <typename PT0, typename PT1, typename PT2>
	msg_param(PT0&& p0, PT1&& p1, PT2&& p2)
		:_res0(TRY_MOVE(p0)), _res1(TRY_MOVE(p1)), _res2(TRY_MOVE(p2)) {}

	msg_param(const_ref_type& rp)
		:_res0(rp._p0), _res1(rp._p1), _res2(rp._p2) {}

	msg_param(ref_type& s)
		:_res0(std::move(s._p0)), _res1(std::move(s._p1)), _res2(std::move(s._p2)) {}

	msg_param(msg_param&& s)
		:_res0(std::move(s._res0)), _res1(std::move(s._res1)), _res2(std::move(s._res2)) {}

	void operator =(msg_param&& s)
	{
		_res0 = std::move(s._res0);
		_res1 = std::move(s._res1);
		_res2 = std::move(s._res2);
	}

	void move_out(ref_type& dst)
	{
		dst._p0 = std::move(_res0);
		dst._p1 = std::move(_res1);
		dst._p2 = std::move(_res2);
	}

	void save_from(const_ref_type& rp)
	{
		_res0 = rp._p0;
		_res1 = rp._p1;
		_res2 = rp._p2;
	}

	void move_from(ref_type& src)
	{
		_res0 = std::move(src._p0);
		_res1 = std::move(src._p1);
		_res2 = std::move(src._p2);
	}

	T0 _res0;
	T1 _res1;
	T2 _res2;
};

template <typename T0, typename T1>
struct msg_param<T0, T1, void, void>
{
	typedef ref_ex<T0, T1> ref_type;
	typedef const_ref_ex<T0, T1> const_ref_type;

	template <typename PT0, typename PT1>
	msg_param(PT0&& p0, PT1&& p1)
		:_res0(TRY_MOVE(p0)), _res1(TRY_MOVE(p1)) {}

	msg_param(const_ref_type& rp)
		:_res0(rp._p0), _res1(rp._p1) {}

	msg_param(ref_type& s)
		:_res0(std::move(s._p0)), _res1(std::move(s._p1)) {}

	msg_param(msg_param&& s)
		:_res0(std::move(s._res0)), _res1(std::move(s._res1)) {}

	void operator =(msg_param&& s)
	{
		_res0 = std::move(s._res0);
		_res1 = std::move(s._res1);
	}

	void move_out(ref_type& dst)
	{
		dst._p0 = std::move(_res0);
		dst._p1 = std::move(_res1);
	}

	void save_from(const_ref_type& rp)
	{
		_res0 = rp._p0;
		_res1 = rp._p1;
	}

	void move_from(ref_type& src)
	{
		_res0 = std::move(src._p0);
		_res1 = std::move(src._p1);
	}

	T0 _res0;
	T1 _res1;
};

template <typename T0>
struct msg_param<T0, void, void, void>
{
	typedef ref_ex<T0> ref_type;
	typedef const_ref_ex<T0> const_ref_type;

	msg_param(T0&& p0)
		:_res0(std::move(p0)) {}

	msg_param(const T0& p0)
		:_res0(p0) {}

	msg_param(const_ref_type& rp)
		:_res0(rp._p0) {}

	msg_param(ref_type& s)
		:_res0(std::move(s._p0)) {}

	msg_param(msg_param&& s)
		:_res0(std::move(s._res0)) {}

	void operator =(msg_param&& s)
	{
		_res0 = std::move(s._res0);
	}

	void move_out(ref_type& dst)
	{
		dst._p0 = std::move(_res0);
	}

	void save_from(const_ref_type& rp)
	{
		_res0 = rp._p0;
	}

	void move_from(ref_type& src)
	{
		_res0 = std::move(src._p0);
	}

	T0 _res0;
};

template <>
struct msg_param<void, void, void, void>
{
};
//////////////////////////////////////////////////////////////////////////

template <typename T>
struct type_move_pipe
{
	typedef T type;
};

template <typename T>
struct type_move_pipe<const T&>
{
	typedef std::reference_wrapper<const T> type;
};

template <typename T>
struct type_move_pipe<T&>
{
	typedef std::reference_wrapper<T> type;
};

template <typename T>
struct type_move_pipe<const T&&>
{
	typedef const T&& type;
};

template <typename T>
struct type_move_pipe<T&&>
{
	typedef T&& type;
};

#define TYPE_PIPE(__T__) typename type_move_pipe<__T__>::type
//////////////////////////////////////////////////////////////////////////

template <typename T0, typename T1 = void, typename T2 = void, typename T3 = void>
struct dst_receiver_base
{
	typedef ref_ex<T0, T1, T2, T3> ref_type;
	typedef msg_param<T0, T1, T2, T3> param_type;
	virtual void move_from(ref_type& s) = 0;
	virtual void move_from(param_type& s) = 0;
};

template <typename T0, typename T1 = void, typename T2 = void, typename T3 = void>
struct dst_receiver_ref : public dst_receiver_base<T0, T1, T2, T3>
{
	typedef ref_ex<T0, T1, T2, T3> ref_type;
	typedef msg_param<T0, T1, T2, T3> param_type;

	dst_receiver_ref(ref_type& dst)
		:_dstRef(dst){}

	void move_from(ref_type& s)
	{
		_dstRef.move_from(s);
	}

	void move_from(param_type& s)
	{
		s.move_out(_dstRef);
	}

	ref_type _dstRef;
};

template <typename T0, typename T1 = void, typename T2 = void, typename T3 = void>
struct dst_receiver_buff : public dst_receiver_base<T0, T1, T2, T3>
{
	typedef ref_ex<T0, T1, T2, T3> ref_type;
	typedef msg_param<T0, T1, T2, T3> param_type;

	struct dst_buff 
	{
		dst_buff()
		:_has(false){}

		~dst_buff()
		{
			if (_has)
			{
				((param_type*)_buff)->~param_type();
			}
		}

		param_type& get()
		{
			assert(_has);
			return *(param_type*)_buff;
		}

		bool _has;
		unsigned char _buff[sizeof(param_type)];
	};

	dst_receiver_buff(dst_buff& dstBuff)
		:_dstBuff(dstBuff){}

	void move_from(ref_type& s)
	{
		assert(!_dstBuff._has);
		_dstBuff._has = true;
		new(_dstBuff._buff)param_type(s);
	}

	void move_from(param_type& s)
	{
		assert(!_dstBuff._has);
		_dstBuff._has = true;
		new(_dstBuff._buff)param_type(std::move(s));
	}

	dst_buff& _dstBuff;
};

template <typename T0>
struct stack_dst_receiver : public dst_receiver_base<T0>
{
	typedef ref_ex<T0> ref_type;
	typedef msg_param<T0> param_type;

	stack_dst_receiver(stack_obj<T0>& dstBuff)
		:_has(false), _dstBuff(dstBuff){}

	~stack_dst_receiver()
	{
		if (_has)
		{
			_dstBuff.destroy();
		}
	}

	void move_from(ref_type& s)
	{
		assert(!_has);
		_has = true;
		_dstBuff.create(std::move(s._p0));
	}

	void move_from(param_type& s)
	{
		assert(!_has);
		_has = true;
		_dstBuff.create(std::move(s._res0));
	}

	bool _has;
	stack_obj<T0>& _dstBuff;
};

//////////////////////////////////////////////////////////////////////////

class actor_msg_handle_base
{
protected:
	actor_msg_handle_base();
	virtual ~actor_msg_handle_base(){};
public:
	virtual void close() = 0;
protected:
	void run_one();
	bool is_quited();
	void set_actor(const actor_handle& hostActor);
	static std::shared_ptr<bool> new_bool();
protected:
	bool _waiting;
	my_actor* _hostActor;
	std::shared_ptr<bool> _closed;
	DEBUG_OPERATION(shared_strand _strand);
};

template <typename T0 = void, typename T1 = void, typename T2 = void, typename T3 = void>
class actor_msg_handle;

template <typename T0 = void, typename T1 = void, typename T2 = void, typename T3 = void>
class actor_trig_handle;

template <typename msg_handle, typename T0 = void, typename T1 = void, typename T2 = void, typename T3 = void>
class msg_notifer_base
{
	friend msg_handle;
protected:
	msg_notifer_base()
		:_msgHandle(NULL){}

	msg_notifer_base(msg_handle* msgHandle)
		:_msgHandle(msgHandle),
		_hostActor(_msgHandle->_hostActor->shared_from_this()),
		_closed(msgHandle->_closed)
	{
		assert(msgHandle->_strand == _hostActor->self_strand());
	}

	template <typename T0 = void, typename T1 = void, typename T2 = void, typename T3 = void>
	struct msg_capture
	{
		template <typename TP0, typename TP1, typename TP2, typename TP3>
		msg_capture(msg_handle* msgHandle, const actor_handle& host, const std::shared_ptr<bool>& closed, TP0&& p0, TP1&& p1, TP2&& p2, TP3&& p3)
			:_msgHandle(msgHandle), _hostActor(host), _closed(closed), _p0(TRY_MOVE(p0)), _p1(TRY_MOVE(p1)), _p2(TRY_MOVE(p2)), _p3(TRY_MOVE(p3)) {}

		msg_capture(const msg_capture& s)
			:_msgHandle(s._msgHandle), _hostActor(s._hostActor), _closed(s._closed), _p0(std::move(s._p0)), _p1(std::move(s._p1)), _p2(std::move(s._p2)), _p3(std::move(s._p3)) {}

		msg_capture(msg_capture&& s)
			:_msgHandle(s._msgHandle), _hostActor(s._hostActor), _closed(s._closed), _p0(std::move(s._p0)), _p1(std::move(s._p1)), _p2(std::move(s._p2)), _p3(std::move(s._p3)) {}

		void operator =(const msg_capture& s)
		{
			static_assert(false, "no copy");
			_msgHandle = s._msgHandle;
			_hostActor = s._hostActor;
			_closed = s._closed;
			_p0 = std::move(s._p0);
			_p1 = std::move(s._p1);
			_p2 = std::move(s._p2);
			_p3 = std::move(s._p3);
		}

		void operator =(msg_capture&& s)
		{
			_msgHandle = s._msgHandle;
			_hostActor = s._hostActor;
			_closed = s._closed;
			_p0 = std::move(s._p0);
			_p1 = std::move(s._p1);
			_p2 = std::move(s._p2);
			_p3 = std::move(s._p3);
		}

		void operator ()()
		{
			if (!(*_closed))
			{
				_msgHandle->push_msg(ref_ex<T0, T1, T2, T3>(_p0, _p1, _p2, _p3));
			}
		}

		mutable T0 _p0;
		mutable T1 _p1;
		mutable T2 _p2;
		mutable T3 _p3;
		msg_handle* _msgHandle;
		actor_handle _hostActor;
		std::shared_ptr<bool> _closed;
	};

	template <typename T0, typename T1, typename T2>
	struct msg_capture<T0, T1, T2, void>
	{
		template <typename TP0, typename TP1, typename TP2>
		msg_capture(msg_handle* msgHandle, const actor_handle& host, const std::shared_ptr<bool>& closed, TP0&& p0, TP1&& p1, TP2&& p2)
			:_msgHandle(msgHandle), _hostActor(host), _closed(closed), _p0(TRY_MOVE(p0)), _p1(TRY_MOVE(p1)), _p2(TRY_MOVE(p2)) {}

		msg_capture(const msg_capture& s)
			:_msgHandle(s._msgHandle), _hostActor(s._hostActor), _closed(s._closed), _p0(std::move(s._p0)), _p1(std::move(s._p1)), _p2(std::move(s._p2)) {}

		msg_capture(msg_capture&& s)
			:_msgHandle(s._msgHandle), _hostActor(s._hostActor), _closed(s._closed), _p0(std::move(s._p0)), _p1(std::move(s._p1)), _p2(std::move(s._p2)) {}

		void operator =(const msg_capture& s)
		{
			static_assert(false, "no copy");
			_msgHandle = s._msgHandle;
			_hostActor = s._hostActor;
			_closed = s._closed;
			_p0 = std::move(s._p0);
			_p1 = std::move(s._p1);
			_p2 = std::move(s._p2);
		}

		void operator =(msg_capture&& s)
		{
			_msgHandle = s._msgHandle;
			_hostActor = s._hostActor;
			_closed = s._closed;
			_p0 = std::move(s._p0);
			_p1 = std::move(s._p1);
			_p2 = std::move(s._p2);
		}

		void operator ()()
		{
			if (!(*_closed))
			{
				_msgHandle->push_msg(ref_ex<T0, T1, T2>(_p0, _p1, _p2));
			}
		}

		mutable T0 _p0;
		mutable T1 _p1;
		mutable T2 _p2;
		msg_handle* _msgHandle;
		actor_handle _hostActor;
		std::shared_ptr<bool> _closed;
	};

	template <typename T0, typename T1>
	struct msg_capture<T0, T1, void, void>
	{
		template <typename TP0, typename TP1>
		msg_capture(msg_handle* msgHandle, const actor_handle& host, const std::shared_ptr<bool>& closed, TP0&& p0, TP1&& p1)
			:_msgHandle(msgHandle), _hostActor(host), _closed(closed), _p0(TRY_MOVE(p0)), _p1(TRY_MOVE(p1)) {}

		msg_capture(const msg_capture& s)
			:_msgHandle(s._msgHandle), _hostActor(s._hostActor), _closed(s._closed), _p0(std::move(s._p0)), _p1(std::move(s._p1)) {}

		msg_capture(msg_capture&& s)
			:_msgHandle(s._msgHandle), _hostActor(s._hostActor), _closed(s._closed), _p0(std::move(s._p0)), _p1(std::move(s._p1)) {}

		void operator =(const msg_capture& s)
		{
			static_assert(false, "no copy");
			_msgHandle = s._msgHandle;
			_hostActor = s._hostActor;
			_closed = s._closed;
			_p0 = std::move(s._p0);
			_p1 = std::move(s._p1);
		}

		void operator =(msg_capture&& s)
		{
			_msgHandle = s._msgHandle;
			_hostActor = s._hostActor;
			_closed = s._closed;
			_p0 = std::move(s._p0);
			_p1 = std::move(s._p1);
		}

		void operator ()()
		{
			if (!(*_closed))
			{
				_msgHandle->push_msg(ref_ex<T0, T1>(_p0, _p1));
			}
		}

		mutable T0 _p0;
		mutable T1 _p1;
		msg_handle* _msgHandle;
		actor_handle _hostActor;
		std::shared_ptr<bool> _closed;
	};

	template <typename T0>
	struct msg_capture<T0, void, void, void>
	{
		template <typename TP0>
		msg_capture(msg_handle* msgHandle, const actor_handle& host, const std::shared_ptr<bool>& closed, TP0&& p0)
			:_msgHandle(msgHandle), _hostActor(host), _closed(closed), _p0(TRY_MOVE(p0)) {}

		msg_capture(const msg_capture& s)
			:_msgHandle(s._msgHandle), _hostActor(s._hostActor), _closed(s._closed), _p0(std::move(s._p0)) {}

		msg_capture(msg_capture&& s)
			:_msgHandle(s._msgHandle), _hostActor(s._hostActor), _closed(s._closed), _p0(std::move(s._p0)) {}

		void operator =(const msg_capture s)
		{
			static_assert(false, "no copy");
			_msgHandle = s._msgHandle;
			_hostActor = s._hostActor;
			_closed = s._closed;
			_p0 = std::move(s._p0);
		}

		void operator =(msg_capture&& s)
		{
			_msgHandle = s._msgHandle;
			_hostActor = s._hostActor;
			_closed = s._closed;
			_p0 = std::move(s._p0);
		}

		void operator ()()
		{
			if (!(*_closed))
			{
				_msgHandle->push_msg(ref_ex<T0>(_p0));
			}
		}

		mutable T0 _p0;
		msg_handle* _msgHandle;
		actor_handle _hostActor;
		std::shared_ptr<bool> _closed;
	};

	template <>
	struct msg_capture<void, void, void, void>
	{
		msg_capture(msg_handle* msgHandle, const actor_handle& host, const std::shared_ptr<bool>& closed)
		:_msgHandle(msgHandle), _hostActor(host), _closed(closed) {}

		void operator ()()
		{
			if (!(*_closed))
			{
				_msgHandle->push_msg();
			}
		}

		msg_handle* _msgHandle;
		actor_handle _hostActor;
		std::shared_ptr<bool> _closed;
	};
public:
	template <typename PT0, typename PT1, typename PT2, typename PT3>
	void operator()(PT0&& p0, PT1&& p1, PT2&& p2, PT3&& p3) const
	{
		_hostActor->self_strand()->try_tick(msg_capture<TYPE_PIPE(T0), TYPE_PIPE(T1), TYPE_PIPE(T2), TYPE_PIPE(T3)>
			(_msgHandle, _hostActor, _closed, TRY_MOVE(p0), TRY_MOVE(p1), TRY_MOVE(p2), TRY_MOVE(p3)));
	}

	template <typename PT0, typename PT1, typename PT2>
	void operator()(PT0&& p0, PT1&& p1, PT2&& p2) const
	{
		_hostActor->self_strand()->try_tick(msg_capture<TYPE_PIPE(T0), TYPE_PIPE(T1), TYPE_PIPE(T2)>
			(_msgHandle, _hostActor, _closed, TRY_MOVE(p0), TRY_MOVE(p1), TRY_MOVE(p2)));
	}

	template <typename PT0, typename PT1>
	void operator()(PT0&& p0, PT1&& p1) const
	{
		_hostActor->self_strand()->try_tick(msg_capture<TYPE_PIPE(T0), TYPE_PIPE(T1)>
			(_msgHandle, _hostActor, _closed, TRY_MOVE(p0), TRY_MOVE(p1)));
	}

	template <typename PT0>
	void operator()(PT0&& p0) const
	{
		_hostActor->self_strand()->try_tick(msg_capture<TYPE_PIPE(T0)>
			(_msgHandle, _hostActor, _closed, TRY_MOVE(p0)));
	}

	void operator()() const
	{
		_hostActor->self_strand()->try_tick(msg_capture<>(_msgHandle, _hostActor, _closed));
	}

	typename func_type<T0, T1, T2, T3>::result case_func() const
	{
		return typename func_type<T0, T1, T2, T3>::result(*this);
	}

	actor_handle host_actor() const
	{
		return _hostActor;
	}

	bool empty() const
	{
		return !_msgHandle;
	}

	void clear()
	{
		_msgHandle = NULL;
		_hostActor.reset();
		_closed.reset();
	}

	operator bool() const
	{
		return !empty();
	}
private:
	msg_handle* _msgHandle;
	actor_handle _hostActor;
	std::shared_ptr<bool> _closed;
};

template <typename T0 = void, typename T1 = void, typename T2 = void, typename T3 = void>
class actor_msg_notifer: public msg_notifer_base<actor_msg_handle<T0, T1, T2, T3>, T0, T1, T2, T3>
{
	friend actor_msg_handle<T0, T1, T2, T3>;
public:
	actor_msg_notifer()	{}
private:
	actor_msg_notifer(actor_msg_handle<T0, T1, T2, T3>* msgHandle)
		:msg_notifer_base(msgHandle) {}
};

template <typename T0 = void, typename T1 = void, typename T2 = void, typename T3 = void>
class actor_trig_notifer : public msg_notifer_base<actor_trig_handle<T0, T1, T2, T3>, T0, T1, T2, T3>
{
	friend actor_trig_handle<T0, T1, T2, T3>;
public:
	actor_trig_notifer() {}
private:
	actor_trig_notifer(actor_trig_handle<T0, T1, T2, T3>* msgHandle)
		:msg_notifer_base(msgHandle) {}
};

template <typename T0, typename T1, typename T2, typename T3>
class actor_msg_handle: public actor_msg_handle_base
{
	typedef msg_param<TYPE_PIPE(T0), TYPE_PIPE(T1), TYPE_PIPE(T2), TYPE_PIPE(T3)> msg_type;
	typedef ref_ex<TYPE_PIPE(T0), TYPE_PIPE(T1), TYPE_PIPE(T2), TYPE_PIPE(T3)> ref_type;
	typedef dst_receiver_base<TYPE_PIPE(T0), TYPE_PIPE(T1), TYPE_PIPE(T2), TYPE_PIPE(T3)> dst_receiver;
	typedef actor_msg_notifer<T0, T1, T2, T3> msg_notifer;

	friend msg_notifer_base<actor_msg_handle<T0, T1, T2, T3>, T0, T1, T2, T2>;
	friend my_actor;
public:
	actor_msg_handle(size_t fixedSize = 16)
		:_msgBuff(fixedSize), _dstRec(NULL) {}

	~actor_msg_handle()
	{
		close();
	}
private:
	msg_notifer make_notifer(const actor_handle& hostActor)
	{
		close();
		set_actor(hostActor);
		_closed = new_bool();
		_waiting = false;
		return msg_notifer(this);
	}

	void push_msg(ref_type& msg)
	{
		assert(_strand->running_in_this_thread());
		if (!is_quited())
		{
			if (_waiting)
			{
				_waiting = false;
				assert(_msgBuff.empty());
				assert(_dstRec);
				_dstRec->move_from(msg);
				_dstRec = NULL;
				run_one();
				return;
			}
			_msgBuff.push_back(std::move(msg_type(msg)));
		}
	}

	bool read_msg(dst_receiver& dst)
	{
		assert(_strand->running_in_this_thread());
		assert(_closed);
		assert(!*_closed);
		if (!_msgBuff.empty())
		{
			dst.move_from(_msgBuff.front());
			_msgBuff.pop_front();
			return true;
		}
		_dstRec = &dst;
		_waiting = true;
		return false;
	}

	void close()
	{
		if (_closed)
		{
			assert(_strand->running_in_this_thread());
			*_closed = true;
			_closed.reset();
		}
		_dstRec = NULL;
		_waiting = false;
		_msgBuff.clear();
		_hostActor = NULL;
	}

	size_t size()
	{
		assert(_strand->running_in_this_thread());
		return _msgBuff.size();
	}
private:
	dst_receiver* _dstRec;
	msg_queue<msg_type> _msgBuff;
};


template <>
class actor_msg_handle<void, void, void, void> : public actor_msg_handle_base
{
	typedef actor_msg_notifer<> msg_notifer;

	friend msg_notifer_base<actor_msg_handle<>>;
	friend my_actor;
public:
	~actor_msg_handle()
	{
		close();
	}
private:
	msg_notifer make_notifer(const actor_handle& hostActor)
	{
		close();
		set_actor(hostActor);
		_closed = new_bool();
		_waiting = false;
		return msg_notifer(this);
	}

	void push_msg()
	{
		assert(_strand->running_in_this_thread());
		if (!is_quited())
		{
			if (_waiting)
			{
				_waiting = false;
				run_one();
				return;
			}
			_msgCount++;
		}
	}

	bool read_msg()
	{
		assert(_strand->running_in_this_thread());
		assert(_closed);
		assert(!*_closed);
		if (_msgCount)
		{
			_msgCount--;
			return true;
		}
		_waiting = true;
		return false;
	}

	void close()
	{
		if (_closed)
		{
			assert(_strand->running_in_this_thread());
			*_closed = true;
			_closed.reset();
		}
		_msgCount = 0;
		_waiting = false;
		_hostActor = NULL;
	}

	size_t size()
	{
		assert(_strand->running_in_this_thread());
		return _msgCount;
	}
private:
	size_t _msgCount;
};
//////////////////////////////////////////////////////////////////////////

template <typename T0, typename T1, typename T2, typename T3>
class actor_trig_handle : public actor_msg_handle_base
{
	typedef msg_param<TYPE_PIPE(T0), TYPE_PIPE(T1), TYPE_PIPE(T2), TYPE_PIPE(T3)> msg_type;
	typedef ref_ex<TYPE_PIPE(T0), TYPE_PIPE(T1), TYPE_PIPE(T2), TYPE_PIPE(T3)> ref_type;
	typedef dst_receiver_base<TYPE_PIPE(T0), TYPE_PIPE(T1), TYPE_PIPE(T2), TYPE_PIPE(T3)> dst_receiver;
	typedef actor_trig_notifer<T0, T1, T2, T3> msg_notifer;

	friend msg_notifer_base<actor_trig_handle<T0, T1, T2, T3>, T0, T1, T2, T3>;
	friend my_actor;
public:
	actor_trig_handle()
		:_hasMsg(false), _dstRec(NULL) {}

	~actor_trig_handle()
	{
		close();
	}
private:
	msg_notifer make_notifer(const actor_handle& hostActor)
	{
		close();
		set_actor(hostActor);
		_closed = new_bool();
		_waiting = false;
		_hasMsg = false;
		return msg_notifer(this);
	}

	void push_msg(ref_type& msg)
	{
		assert(_strand->running_in_this_thread());
		*_closed = true;
		if (!is_quited())
		{
			if (_waiting)
			{
				_waiting = false;
				assert(_dstRec);
				_dstRec->move_from(msg);
				_dstRec = NULL;
				run_one();
				return;
			}
			_hasMsg = true;
			new (_msgBuff)msg_type(msg);
		}
	}

	bool read_msg(dst_receiver& dst)
	{
		assert(_strand->running_in_this_thread());
		assert(_closed);
		if (_hasMsg)
		{
			_hasMsg = false;
			dst.move_from(*(msg_type*)_msgBuff);
			((msg_type*)_msgBuff)->~msg_type();
			return true;
		}
		assert(!*_closed);
		_dstRec = &dst;
		_waiting = true;
		return false;
	}

	void close()
	{
		if (_closed)
		{
			assert(_strand->running_in_this_thread());
			*_closed = true;
			_closed.reset();
		}
		if (_hasMsg)
		{
			_hasMsg = false;
			((msg_type*)_msgBuff)->~msg_type();
		}
		_dstRec = NULL;
		_waiting = false;
		_hostActor = NULL;
	}
public:
	bool has()
	{
		assert(_strand->running_in_this_thread());
		return _hasMsg;
	}
private:
	dst_receiver* _dstRec;
	bool _hasMsg;
	unsigned char _msgBuff[sizeof(msg_type)];
};

template <>
class actor_trig_handle<void, void, void, void> : public actor_msg_handle_base
{
	typedef actor_trig_notifer<> msg_notifer;

	friend msg_notifer_base<actor_trig_handle<>>;
	friend my_actor;
public:
	actor_trig_handle()
		:_hasMsg(false){}

	~actor_trig_handle()
	{
		close();
	}
private:
	msg_notifer make_notifer(const actor_handle& hostActor)
	{
		close();
		set_actor(hostActor);
		_closed = new_bool();
		_waiting = false;
		_hasMsg = false;
		return msg_notifer(this);
	}

	void push_msg()
	{
		assert(_strand->running_in_this_thread());
		*_closed = true;
		if (!is_quited())
		{
			if (_waiting)
			{
				_waiting = false;
				run_one();
				return;
			}
			_hasMsg = true;
		}
	}

	bool read_msg()
	{
		assert(_strand->running_in_this_thread());
		assert(_closed);
		if (_hasMsg)
		{
			_hasMsg = false;
			return true;
		}
		assert(!*_closed);
		_waiting = true;
		return false;
	}

	void close()
	{
		if (_closed)
		{
			assert(_strand->running_in_this_thread());
			*_closed = true;
			_closed.reset();
		}
		_hasMsg = false;
		_waiting = false;
		_hostActor = NULL;
	}
public:
	bool has()
	{
		assert(_strand->running_in_this_thread());
		return _hasMsg;
	}
private:
	bool _hasMsg;
};

//////////////////////////////////////////////////////////////////////////
template <typename T0 = void, typename T1 = void, typename T2 = void, typename T3 = void>
class msg_pump;

template <typename T0 = void, typename T1 = void, typename T2 = void, typename T3 = void>
class msg_pool;

template <typename T0 = void, typename T1 = void, typename T2 = void, typename T3 = void>
class post_actor_msg;

class msg_pump_base
{
	friend my_actor;
public:
	virtual ~msg_pump_base() {};
protected:
	bool is_quited();
	void run_one();
	void push_yield();
protected:
	my_actor* _hostActor;
};

class msg_pool_base
{
	friend msg_pump<>;
	friend my_actor;
public:
	virtual ~msg_pool_base() {};
};

template <typename T0, typename T1, typename T2, typename T3>
class msg_pump : public msg_pump_base
{
	typedef msg_param<TYPE_PIPE(T0), TYPE_PIPE(T1), TYPE_PIPE(T2), TYPE_PIPE(T3)> msg_type;
	typedef const_ref_ex<TYPE_PIPE(T0), TYPE_PIPE(T1), TYPE_PIPE(T2), TYPE_PIPE(T3)> const_ref_type;
	typedef dst_receiver_base<TYPE_PIPE(T0), TYPE_PIPE(T1), TYPE_PIPE(T2), TYPE_PIPE(T3)> dst_receiver;
	typedef msg_pool<T0, T1, T2, T3> msg_pool_type;
	typedef typename msg_pool_type::pump_handler pump_handler;

	struct msg_capture
	{
		msg_capture(const actor_handle& hostActor, const shared_ptr<msg_pump>& sharedThis, msg_type&& msg)
		:_hostActor(hostActor), _sharedThis(sharedThis), _msg(std::move(msg)) {}

		msg_capture(const msg_capture& s)
			:_hostActor(s._hostActor), _sharedThis(s._sharedThis), _msg(std::move(s._msg)) {}

		msg_capture(msg_capture&& s)
			:_hostActor(s._hostActor), _sharedThis(s._sharedThis), _msg(std::move(s._msg)) {}

		void operator =(const msg_capture& s)
		{
			static_assert(false, "no copy");
			_hostActor = s._hostActor;
			_sharedThis = s._sharedThis;
			_msg = std::move(s._msg);
		}

		void operator =(msg_capture&& s)
		{
			_hostActor = s._hostActor;
			_sharedThis = s._sharedThis;
			_msg = std::move(s._msg);
		}

		void operator()()
		{
			_sharedThis->receiver(std::move(_msg));
		}

		mutable msg_type _msg;
		actor_handle _hostActor;
		shared_ptr<msg_pump> _sharedThis;
	};

	friend my_actor;
	friend msg_pool<T0, T1, T2, T3>;
	friend pump_handler;
private:
	msg_pump(){}
	~msg_pump(){}
private:
	static std::shared_ptr<msg_pump> make(const actor_handle& hostActor)
	{
		std::shared_ptr<msg_pump> res(new msg_pump(), [](msg_pump* p){delete p; });
		res->_weakThis = res;
		res->_hasMsg = false;
		res->_waiting = false;
		res->_checkDis = false;
		res->_pumpCount = 0;
		res->_dstRec = NULL;
		res->_hostActor = hostActor.get();
		res->_strand = hostActor->self_strand();
		return res;
	}

	void receiver(msg_type&& msg)
	{
		if (!is_quited())
		{
			assert(!_hasMsg);
			_pumpCount++;
			if (_dstRec)
			{
				_dstRec->move_from(msg);
				_dstRec = NULL;
				if (_waiting)
				{
					_waiting = false;
					_checkDis = false;
					run_one();
				}
			}
			else
			{//pump_msg超时结束后才接受到消息
				assert(!_waiting);
				_hasMsg = true;
				new (_msgSpace)msg_type(std::move(msg));
			}
		}
	}

	void receive_msg_tick(msg_type&& msg, const actor_handle& hostActor)
	{
		_strand->try_tick(msg_capture(hostActor, _weakThis.lock(), std::move(msg)));
	}

	void receive_msg(msg_type&& msg, const actor_handle& hostActor)
	{
		if (_strand->running_in_this_thread())
		{
			receiver(std::move(msg));
		} 
		else
		{
			_strand->post(msg_capture(hostActor, _weakThis.lock(), std::move(msg)));
		}
	}

	bool read_msg(dst_receiver& dst)
	{
		assert(_strand->running_in_this_thread());
		assert(!_dstRec);
		assert(!_waiting);
		if (_hasMsg)
		{
			_hasMsg = false;
			dst.move_from(*(msg_type*)_msgSpace);
			((msg_type*)_msgSpace)->~msg_type();
			return true;
		}
		_dstRec = &dst;
		if (!_pumpHandler.empty())
		{
			_pumpHandler(_pumpCount);
			_waiting = !!_dstRec;
			return !_dstRec;
		}
		_waiting = true;
		return false;
	}

	bool try_read(dst_receiver& dst)
	{
		assert(_strand->running_in_this_thread());
		assert(!_dstRec);
		assert(!_waiting);
		if (_hasMsg)
		{
			_hasMsg = false;
			dst.move_from(*(msg_type*)_msgSpace);
			((msg_type*)_msgSpace)->~msg_type();
			return true;
		}
		if (!_pumpHandler.empty())
		{
			bool wait = false;
			if (_pumpHandler.try_pump(_hostActor, dst, _pumpCount, wait))
			{
				if (wait)
				{
					if (!_hasMsg)
					{
						_dstRec = &dst;
						push_yield();
						assert(_hasMsg);
					}
					_hasMsg = false;
					dst.move_from(*(msg_type*)_msgSpace);
					((msg_type*)_msgSpace)->~msg_type();
				}
				return true;
			}
		}
		return false;
	}

	void connect(const pump_handler& pumpHandler)
	{
		assert(_strand->running_in_this_thread());
		assert(_hostActor);
		_pumpHandler = pumpHandler;
		_pumpCount = 0;
		if (_waiting)
		{
			_pumpHandler.post_pump(_pumpCount);
		}
	}

	void clear()
	{
		assert(_strand->running_in_this_thread());
		assert(_hostActor);
		_pumpHandler.clear();
		if (!is_quited() && _checkDis)
		{
			assert(_waiting);
			_waiting = false;
			_dstRec = NULL;
			run_one();
		}
	}

	void close()
	{
		if (_hasMsg)
		{
			((msg_type*)_msgSpace)->~msg_type();
		}
		_hasMsg = false;
		_dstRec = NULL;
		_pumpCount = 0;
		_waiting = false;
		_checkDis = false;
		_pumpHandler.clear();
		_hostActor = NULL;
	}

	bool isDisconnected()
	{
		return _pumpHandler.empty();
	}
private:
	std::weak_ptr<msg_pump> _weakThis;
	unsigned char _msgSpace[sizeof(msg_type)];
	pump_handler _pumpHandler;
	shared_strand _strand;
	dst_receiver* _dstRec;
	unsigned char _pumpCount;
	bool _hasMsg;
	bool _waiting;
	bool _checkDis;
};

template <typename T0, typename T1, typename T2, typename T3>
class msg_pool : public msg_pool_base
{
	typedef msg_param<TYPE_PIPE(T0), TYPE_PIPE(T1), TYPE_PIPE(T2), TYPE_PIPE(T3)> msg_type;
	typedef const_ref_ex<TYPE_PIPE(T0), TYPE_PIPE(T1), TYPE_PIPE(T2), TYPE_PIPE(T3)> const_ref_type;
	typedef dst_receiver_base<TYPE_PIPE(T0), TYPE_PIPE(T1), TYPE_PIPE(T2), TYPE_PIPE(T3)> dst_receiver;
	typedef msg_pump<T0, T1, T2, T3> msg_pump_type;
	typedef post_actor_msg<T0, T1, T2, T3> post_type;

	struct pump_handler
	{
		void pump_msg(unsigned char pumpID, const actor_handle& hostActor)
		{
			assert(_msgPump == _thisPool->_msgPump);
			auto& msgBuff = _thisPool->_msgBuff;
			if (pumpID == _thisPool->_sendCount)
			{
				if (!msgBuff.empty())
				{
					msg_type mt_ = std::move(msgBuff.front());
					msgBuff.pop_front();
					_thisPool->_sendCount++;
					_msgPump->receive_msg(std::move(mt_), hostActor);
				}
				else
				{
					_thisPool->_waiting = true;
				}
			}
			else
			{//上次消息没取到，重新取，但实际中间已经post出去了
				assert(!_thisPool->_waiting);
				assert(pumpID + 1 == _thisPool->_sendCount);
			}
		}

		void operator()(unsigned char pumpID)
		{
			assert(_thisPool);
			if (_thisPool->_strand->running_in_this_thread())
			{
				if (_msgPump == _thisPool->_msgPump)
				{
					pump_msg(pumpID, _msgPump->_hostActor->shared_from_this());
				}
			}
			else
			{
				post_pump(pumpID);
			}
		}

		bool try_pump(my_actor* host, dst_receiver&dst, unsigned char pumpID, bool& wait)
		{
			assert(_thisPool);
			auto& refThis_ = *this;
			my_actor::quit_guard qg(host);
			return host->send<bool>(_thisPool->_strand, [&, refThis_]()->bool
			{
				auto lockThis = refThis_;
				bool ok = false;
				if (_msgPump == _thisPool->_msgPump)
				{
					auto& msgBuff = _thisPool->_msgBuff;
					if (pumpID == _thisPool->_sendCount)
					{
						if (!msgBuff.empty())
						{
							dst.move_from(msgBuff.front());
							msgBuff.pop_front();
							ok = true;
						}
					}
					else
					{//上次消息没取到，重新取，但实际中间已经post出去了
						assert(!_thisPool->_waiting);
						assert(pumpID + 1 == _thisPool->_sendCount);
						wait = true;
						ok = true;
					}
				}
				return ok;
			});
		}

		void post_pump(unsigned char pumpID)
		{
			assert(!empty());
			auto& refThis_ = *this;
			auto hostActor = _msgPump->_hostActor->shared_from_this();
			_thisPool->_strand->post([=]
			{
				if (refThis_._msgPump == refThis_._thisPool->_msgPump)
				{
					((pump_handler&)refThis_).pump_msg(pumpID, hostActor);
				}
			});
		}

		bool empty()
		{
			return !_thisPool;
		}

		void clear()
		{
			_thisPool.reset();
			_msgPump.reset();
		}

		std::shared_ptr<msg_pool> _thisPool;
		std::shared_ptr<msg_pump_type> _msgPump;
	};

	struct msg_capture
	{
		msg_capture(const actor_handle& hostActor, const shared_ptr<msg_pool>& sharedThis, msg_type&& msg)
		:_hostActor(hostActor), _sharedThis(sharedThis), _msg(std::move(msg)) {}

		msg_capture(const msg_capture& s)
			:_hostActor(s._hostActor), _sharedThis(s._sharedThis), _msg(std::move(s._msg)) {}

		msg_capture(msg_capture&& s)
			:_hostActor(s._hostActor), _sharedThis(s._sharedThis), _msg(std::move(s._msg)) {}

		void operator =(const msg_capture& s)
		{
			static_assert(false, "no copy");
			_hostActor = s._hostActor;
			_sharedThis = s._sharedThis;
			_msg = std::move(s._msg);
		}

		void operator =(msg_capture&& s)
		{
			_hostActor = s._hostActor;
			_sharedThis = s._sharedThis;
			_msg = std::move(s._msg);
		}

		void operator()()
		{
			_sharedThis->send_msg(std::move(_msg), _hostActor);
		}

		mutable msg_type _msg;
		actor_handle _hostActor;
		shared_ptr<msg_pool> _sharedThis;
	};

	friend my_actor;
	friend msg_pump_type;
	friend post_type;
private:
	msg_pool(size_t fixedSize)
		:_msgBuff(fixedSize)
	{

	}
	~msg_pool()
	{

	}
private:
	static std::shared_ptr<msg_pool> make(shared_strand strand, size_t fixedSize)
	{
		std::shared_ptr<msg_pool> res(new msg_pool(fixedSize), [](msg_pool* p){delete p; });
		res->_weakThis = res;
		res->_strand = strand;
		res->_waiting = false;
		res->_sendCount = 0;
		return res;
	}

	void send_msg(msg_type&& mt, const actor_handle& hostActor)
	{
		if (_waiting)
		{
			_waiting = false;
			assert(_msgPump);
			assert(_msgBuff.empty());
			_sendCount++;
			_msgPump->receive_msg(std::move(mt), hostActor);
// 			if (_msgBuff.empty())
// 			{
// 				_msgPump->receive_msg(std::move(mt), hostActor);
// 			}
// 			else
// 			{
// 				msg_type mt_ = std::move(_msgBuff.front());
// 				_msgBuff.pop_front();
// 				_msgBuff.push_back(std::move(mt));
// 				_msgPump->receive_msg(std::move(mt_), hostActor);
// 			}
		}
		else
		{
			_msgBuff.push_back(std::move(mt));
		}
	}

	void post_msg(msg_type&& mt, const actor_handle& hostActor)
	{
		if (_waiting)
		{
			_waiting = false;
			assert(_msgPump);
			assert(_msgBuff.empty());
			_sendCount++;
			_msgPump->receive_msg_tick(std::move(mt), hostActor);
// 			if (_msgBuff.empty())
// 			{
// 				_msgPump->receive_msg_tick(std::move(mt), hostActor);
// 			}
// 			else
// 			{
// 				_msgPump->receive_msg_tick(std::move(_msgBuff.front()), hostActor);
// 				_msgBuff.pop_front();
// 				_msgBuff.push_back(std::move(mt));
// 			}
		}
		else
		{
			_msgBuff.push_back(std::move(mt));
		}
	}

	void push_msg(msg_type&& mt, const actor_handle& hostActor)
	{
		if (_strand->running_in_this_thread())
		{
			post_msg(std::move(mt), hostActor);
		}
		else
		{
			_strand->post(msg_capture(hostActor, _weakThis.lock(), std::move(mt)));
		}
	}

	pump_handler connect_pump(const std::shared_ptr<msg_pump_type>& msgPump)
	{
		assert(msgPump);
		assert(_strand->running_in_this_thread());
		_msgPump = msgPump;
		pump_handler compHandler;
		compHandler._thisPool = _weakThis.lock();
		compHandler._msgPump = msgPump;
		_sendCount = 0;
		_waiting = false;
		return compHandler;
	}

	void disconnect()
	{
		assert(_strand->running_in_this_thread());
		_msgPump.reset();
		_waiting = false;
	}

	void expand_fixed(size_t fixedSize)
	{
		assert(_strand->running_in_this_thread());
		_msgBuff.expand_fixed(fixedSize);
	}
private:
	std::weak_ptr<msg_pool> _weakThis;
	std::shared_ptr<msg_pump_type> _msgPump;
	msg_queue<msg_type> _msgBuff;
	shared_strand _strand;
	unsigned char _sendCount;
	bool _waiting;
};

class msg_pump_void;

class msg_pool_void : public msg_pool_base
{
	typedef post_actor_msg<> post_type;
	typedef msg_pump_void msg_pump_type;

	struct pump_handler
	{
		void operator()(unsigned char pumpID);
		void pump_msg(unsigned char pumpID, const actor_handle& hostActor);
		bool try_pump(my_actor* host, unsigned char pumpID, bool& wait);
		void post_pump(unsigned char pumpID);
		bool empty();
		bool same_strand();
		void clear();

		std::shared_ptr<msg_pool_void> _thisPool;
		std::shared_ptr<msg_pump_type> _msgPump;
	};

	friend my_actor;
	friend msg_pump_void;
	friend post_type;
protected:
	msg_pool_void(shared_strand strand);
public:
	virtual ~msg_pool_void();
protected:
	void send_msg(const actor_handle& hostActor);
	void post_msg(const actor_handle& hostActor);
	void push_msg(const actor_handle& hostActor);
	pump_handler connect_pump(const std::shared_ptr<msg_pump_type>& msgPump);
	void disconnect();
	void expand_fixed(size_t fixedSize){};
protected:
	std::weak_ptr<msg_pool_void> _weakThis;
	std::shared_ptr<msg_pump_type> _msgPump;
	size_t _msgBuff;
	shared_strand _strand;
	unsigned char _sendCount;
	bool _waiting;
};

class msg_pump_void : public msg_pump_base
{
	typedef msg_pool_void msg_pool_type;
	typedef msg_pool_void::pump_handler pump_handler;

	friend my_actor;
	friend msg_pool_void;
	friend pump_handler;
protected:
	msg_pump_void(const actor_handle& hostActor);
public:
	virtual ~msg_pump_void();
protected:
	void receiver();
	void receive_msg_tick(const actor_handle& hostActor);
	void receive_msg(const actor_handle& hostActor);
	bool read_msg();
	bool try_read();
	void connect(const pump_handler& pumpHandler);
	void clear();
	void close();
	bool isDisconnected();
protected:
	std::weak_ptr<msg_pump_void> _weakThis;
	pump_handler _pumpHandler;
	shared_strand _strand;
	unsigned char _pumpCount;
	bool _waiting;
	bool _hasMsg;
	bool _checkDis;
};

template <>
class msg_pool<void, void, void, void> : public msg_pool_void
{
	friend my_actor;
private:
	typedef std::shared_ptr<msg_pool> handle;

	msg_pool(shared_strand strand)
		:msg_pool_void(strand){}

	~msg_pool(){}

	static handle make(shared_strand strand, size_t fixedSize)
	{
		handle res(new msg_pool(strand), [](msg_pool* p){delete p; });
		res->_weakThis = res;
		return res;
	}
};

template <>
class msg_pump<void, void, void, void> : public msg_pump_void
{
	friend my_actor;
public:
	typedef msg_pump* handle;
private:
	msg_pump(const actor_handle& hostActor)
		:msg_pump_void(hostActor){}

	~msg_pump(){}

	static std::shared_ptr<msg_pump> make(const actor_handle& hostActor)
	{
		std::shared_ptr<msg_pump> res(new msg_pump(hostActor), [](msg_pump* p){delete p; });
		res->_weakThis = res;
		return res;
	}
};

template <typename T0 = void, typename T1 = void, typename T2 = void, typename T3 = void>
class msg_pump_handle
{
	friend my_actor;
	typedef msg_pump<T0, T1, T2, T3> pump;

	pump* operator ->() const
	{
		return _handle;
	}

	pump* _handle;
};

template <typename T0, typename T1, typename T2, typename T3>
class post_actor_msg
{
	typedef msg_pool<T0, T1, T2, T3> msg_pool_type;
public:
	post_actor_msg(){}
	post_actor_msg(const std::shared_ptr<msg_pool_type>& msgPool, const actor_handle& hostActor)
		:_msgPool(msgPool), _hostActor(hostActor){}
public:
	template <typename PT0, typename PT1, typename PT2, typename PT3>
	void operator()(PT0&& p0, PT1&& p1, PT2&& p2, PT3&& p3) const
	{
		assert(!empty());
		_msgPool->push_msg(std::move(msg_param<TYPE_PIPE(T0), TYPE_PIPE(T1), TYPE_PIPE(T2), TYPE_PIPE(T3)>(TRY_MOVE(p0), TRY_MOVE(p1), TRY_MOVE(p2), TRY_MOVE(p3))), _hostActor);
	}

	template <typename PT0, typename PT1, typename PT2>
	void operator()(PT0&& p0, PT1&& p1, PT2&& p2) const
	{
		assert(!empty());
		_msgPool->push_msg(std::move(msg_param<TYPE_PIPE(T0), TYPE_PIPE(T1), TYPE_PIPE(T2)>(TRY_MOVE(p0), TRY_MOVE(p1), TRY_MOVE(p2))), _hostActor);
	}

	template <typename PT0, typename PT1>
	void operator()(PT0&& p0, PT1&& p1) const
	{
		assert(!empty());
		_msgPool->push_msg(std::move(msg_param<TYPE_PIPE(T0), TYPE_PIPE(T1)>(TRY_MOVE(p0), TRY_MOVE(p1))), _hostActor);
	}

	template <typename PT0>
	void operator()(PT0&& p0) const
	{
		assert(!empty());
		_msgPool->push_msg(std::move(msg_param<TYPE_PIPE(T0)>(TRY_MOVE(p0))), _hostActor);
	}

	void operator()() const
	{
		assert(!empty());
		_msgPool->push_msg(_hostActor);
	}

	typename func_type<T0, T1, T2, T3>::result case_func() const
	{
		return typename func_type<T0, T1, T2, T3>::result(*this);
	}

	bool empty() const
	{
		return !_msgPool;
	}

	void clear()
	{
		_hostActor.reset();
		_msgPool.reset();
	}

	operator bool() const
	{
		return !empty();
	}
private:
	actor_handle _hostActor;
	std::shared_ptr<msg_pool_type> _msgPool;
};
//////////////////////////////////////////////////////////////////////////

class trig_once_base
{
protected:
	trig_once_base()
		DEBUG_OPERATION(:_pIsTrig(new boost::atomic<bool>(false)))
	{
	}

	trig_once_base(const trig_once_base& s)
		:_hostActor(s._hostActor)
	{
		DEBUG_OPERATION(_pIsTrig = s._pIsTrig);
	}
public:
	virtual ~trig_once_base() {};
protected:
	template <typename DST /*dst_receiver*/, typename SRC /*msg_param*/>
	void _trig_handler(DST& dstRec, SRC&& src) const
	{
		assert(!_pIsTrig->exchange(true));
		assert(_hostActor);
		_hostActor->_trig_handler(dstRec, std::move(src));
		_hostActor.reset();
	}

	template <typename DST /*ref_ex*/, typename SRC /*msg_param*/>
	void _trig_handler_ref(DST dstRef, SRC&& src) const
	{
		assert(!_pIsTrig->exchange(true));
		assert(_hostActor);
		_hostActor->_trig_handler_ref(dstRef, std::move(src));
		_hostActor.reset();
	}

	void tick_handler() const;

	void push_yield() const;
protected:
	mutable actor_handle _hostActor;
	DEBUG_OPERATION(std::shared_ptr<boost::atomic<bool> > _pIsTrig);
};

template <typename T0 = void, typename T1 = void, typename T2 = void, typename T3 = void>
class trig_once_notifer: public trig_once_base
{
	typedef msg_param<TYPE_PIPE(T0), TYPE_PIPE(T1), TYPE_PIPE(T2), TYPE_PIPE(T3)> msg_type;
	typedef dst_receiver_base<TYPE_PIPE(T0), TYPE_PIPE(T1), TYPE_PIPE(T2), TYPE_PIPE(T3)> dst_receiver;

	friend my_actor;
public:
	trig_once_notifer() :_dstRec(0) {};
private:
	trig_once_notifer(const actor_handle& hostActor, dst_receiver* dstRec)
		:_dstRec(dstRec) {
		_hostActor = hostActor;
	}
public:
	template <typename PT0, typename PT1, typename PT2, typename PT3>
	void operator()(PT0&& p0, PT1&& p1, PT2&& p2, PT3&& p3) const
	{
		_trig_handler(*_dstRec, std::move(msg_type(TRY_MOVE(p0), TRY_MOVE(p1), TRY_MOVE(p2), TRY_MOVE(p3))));
	}

	template <typename PT0, typename PT1, typename PT2>
	void operator()(PT0&& p0, PT1&& p1, PT2&& p2) const
	{
		_trig_handler(*_dstRec, std::move(msg_type(TRY_MOVE(p0), TRY_MOVE(p1), TRY_MOVE(p2))));
	}

	template <typename PT0, typename PT1>
	void operator()(PT0&& p0, PT1&& p1) const
	{
		_trig_handler(*_dstRec, std::move(msg_type(TRY_MOVE(p0), TRY_MOVE(p1))));
	}

	template <typename PT0>
	void operator()(PT0&& p0) const
	{
		_trig_handler(*_dstRec, std::move(msg_type(TRY_MOVE(p0))));
	}

	void operator()() const
	{
		tick_handler();
	}

	typename func_type<T0, T1, T2, T3>::result case_func() const
	{
		return typename func_type<T0, T1, T2, T3>::result(*this);
	}
private:
	dst_receiver* _dstRec;
};
//////////////////////////////////////////////////////////////////////////
/*!
@brief 异步回调器，作为回调函数参数传入，回调后，自动返回到下一行语句继续执行
*/
template <typename T0 = void, typename T1 = void, typename T2 = void, typename T3 = void>
class callback_handler: public trig_once_base
{
	typedef msg_param<T0, T1, T2, T3> msg_type;
	typedef ref_ex<T0, T1, T2, T3> ref_type;
	typedef dst_receiver_ref<T0, T1, T2, T3> dst_receiver;

	friend my_actor;
public:
	template <typename PT0, typename PT1, typename PT2, typename PT3>
	callback_handler(my_actor* host, PT0& r0, PT1& r1, PT2& r2, PT3& r3)
		:_early(true), _dstRef(r0, r1, r2, r3)
	{
		_hostActor = host->shared_from_this();
	}

	template <typename PT0, typename PT1, typename PT2>
	callback_handler(my_actor* host, PT0& r0, PT1& r1, PT2& r2)
		: _early(true), _dstRef(r0, r1, r2)
	{
		_hostActor = host->shared_from_this();
	}

	template <typename PT0, typename PT1>
	callback_handler(my_actor* host, PT0& r0, PT1& r1)
		: _early(true), _dstRef(r0, r1)
	{
		_hostActor = host->shared_from_this();
	}

	template <typename PT0>
	callback_handler(my_actor* host, PT0& r0)
		: _early(true), _dstRef(r0)
	{
		_hostActor = host->shared_from_this();
	}

	callback_handler(my_actor* host)
		:_early(true)
	{
		_hostActor = host->shared_from_this();
	}

	~callback_handler()
	{
		if (_early)
		{
			//可能在此析构函数内抛出 force_quit_exception 异常，但在 push_yield 已经切换出堆栈，在切换回来后会安全的释放资源
			push_yield();
		}
	}

	callback_handler(const callback_handler& s)
		:trig_once_base(s), _early(false), _dstRef(s._dstRef)
	{
	}
public:
	template <typename PT0, typename PT1, typename PT2, typename PT3>
	void operator()(PT0&& p0, PT1&& p1, PT2&& p2, PT3&& p3) const
	{
		_trig_handler_ref(_dstRef, std::move(msg_type(TRY_MOVE(p0), TRY_MOVE(p1), TRY_MOVE(p2), TRY_MOVE(p3))));
	}

	template <typename PT0, typename PT1, typename PT2>
	void operator()(PT0&& p0, PT1&& p1, PT2&& p2) const
	{
		_trig_handler_ref(_dstRef, std::move(msg_type(TRY_MOVE(p0), TRY_MOVE(p1), TRY_MOVE(p2))));
	}

	template <typename PT0, typename PT1>
	void operator()(PT0&& p0, PT1&& p1) const
	{
		_trig_handler_ref(_dstRef, std::move(msg_type(TRY_MOVE(p0), TRY_MOVE(p1))));
	}

	template <typename PT0>
	void operator()(PT0&& p0) const
	{
		_trig_handler_ref(_dstRef, std::move(msg_type(TRY_MOVE(p0))));
	}

	void operator()() const
	{
		tick_handler();
	}
private:
	callback_handler& operator=(const callback_handler&)
	{
		return *this;
	}
private:
	bool _early;
	ref_type _dstRef;
};

//////////////////////////////////////////////////////////////////////////
/*!
@brief 子Actor句柄，不可拷贝
*/
class child_actor_handle 
{
public:
	typedef std::shared_ptr<child_actor_handle> ptr;
private:
	friend my_actor;
	/*!
	@brief 子Actor句柄参数，child_actor_handle内使用
	*/
	struct child_actor_param
	{
#ifdef _DEBUG
		child_actor_param();
		child_actor_param(child_actor_param& s);
		~child_actor_param();
		child_actor_param& operator =(child_actor_param& s);
		bool _isCopy;
#endif
		actor_handle _actor;///<本Actor
		msg_list_shared_alloc<actor_handle>::iterator _actorIt;///<保存在父Actor集合中的节点
	};
private:
	child_actor_handle(child_actor_handle&);
	child_actor_handle& operator =(child_actor_handle&);
	void peel();
public:
	child_actor_handle();
	child_actor_handle(child_actor_param& s);
	~child_actor_handle();
	child_actor_handle& operator =(child_actor_param& s);
	actor_handle get_actor();
	static ptr make_ptr();
	bool empty();
private:
	DEBUG_OPERATION(msg_list_shared_alloc<std::function<void()> >::iterator _qh);
	bool _quited;///<检测是否已经关闭
	actor_trig_handle<> _quiteAth;
	child_actor_param _param;
};
//////////////////////////////////////////////////////////////////////////

/*!
@brief Actor对象
*/
class my_actor
{
	struct suspend_resume_option 
	{
		bool _isSuspend;
		std::function<void ()> _h;
	};

	struct msg_pool_status 
	{
		msg_pool_status()
		:_msgTypeMap(_msgTypeMapAll) {}

		~msg_pool_status() {}

		struct pck_base 
		{
			pck_base() :_amutex(_strand) { assert(false); }

			pck_base(my_actor* hostActor)
			:_strand(hostActor->_strand), _amutex(_strand), _isHead(true), _hostActor(hostActor){}

			virtual ~pck_base(){};

			virtual void close() = 0;

			bool is_closed()
			{
				return !_hostActor;
			}

			void lock(my_actor* self)
			{
				_amutex.lock(self);
			}

			void unlock(my_actor* self)
			{
				_amutex.unlock(self);
			}

			shared_strand _strand;
			actor_mutex _amutex;
			bool _isHead;
			my_actor* _hostActor;
		};

		template <typename T0, typename T1, typename T2, typename T3>
		struct pck: public pck_base
		{
			pck(my_actor* hostActor)
			:pck_base(hostActor){}

			void close()
			{
				if (_msgPump)
				{
					_msgPump->close();
				}
				_hostActor = NULL;
			}

			std::shared_ptr<msg_pool<T0, T1, T2, T3> > _msgPool;
			std::shared_ptr<msg_pump<T0, T1, T2, T3> > _msgPump;
			std::shared_ptr<pck> _next;
		};

		void clear(my_actor* self)
		{
			for (auto it = _msgTypeMap.begin(); it != _msgTypeMap.end(); it++) { it->second->_amutex.quited_lock(self); }
			for (auto it = _msgTypeMap.begin(); it != _msgTypeMap.end(); it++) { it->second->close(); }
			for (auto it = _msgTypeMap.begin(); it != _msgTypeMap.end(); it++) { it->second->_amutex.quited_unlock(self); }
			_msgTypeMap.clear();
		}

		msg_map_shared_alloc<size_t, std::shared_ptr<pck_base> > _msgTypeMap;
		static msg_map_shared_alloc<size_t, std::shared_ptr<pck_base> >::shared_node_alloc _msgTypeMapAll;
	};

	struct timer_state 
	{
		bool _timerSuspend;
		bool _timerCompleted;
		long long _timerTime;
		long long _timerStampBegin;
		long long _timerStampEnd;
		std::function<void()> _timerCb;
		actor_timer::timer_handle _timerHandle;
	};

	class boost_actor_run;
	friend boost_actor_run;
	friend child_actor_handle;
	friend msg_pump_base;
	friend actor_msg_handle_base;
	friend trig_once_base;
	friend mutex_trig_notifer;
	friend mutex_trig_handle;
	friend actor_timer;
	friend _actor_mutex;
public:
	/*!
	@brief 在{}一定范围内锁定当前Actor不被强制退出，如果锁定期间被挂起，将无法等待其退出
	*/
	class quit_guard
	{
	public:
		quit_guard(my_actor* self)
		:_self(self)
		{
			_locked = true;
			_self->lock_quit();
		}

		~quit_guard()
		{
			if (_locked)
			{
				//可能在此析构函数内抛出 force_quit_exception 异常，但在 unlock_quit 已经切换出堆栈，在切换回来后会安全的释放资源
				_self->unlock_quit();
			}
		}

		void lock()
		{
			assert(!_locked);
			_locked = true;
			_self->lock_quit();
		}

		void unlock()
		{
			assert(_locked);
			_locked = false;
			_self->unlock_quit();
		}
	private:
		quit_guard(const quit_guard&){};
		void operator=(const quit_guard&){};
		my_actor* _self;
		bool _locked;
	};

	/*!
	@brief Actor被强制退出的异常类型
	*/
	struct force_quit_exception { };

	/*!
	@brief 消息泵被断开
	*/
	struct pump_disconnected_exception { };

	/*!
	@brief Actor入口函数体
	*/
	typedef std::function<void (my_actor*)> main_func;

	/*!
	@brief actor id
	*/
	typedef long long id;
private:
	my_actor();
	my_actor(const my_actor&);
	my_actor& operator =(const my_actor&);
	~my_actor();
public:
	/*!
	@brief 创建一个Actor
	@param actorStrand Actor所依赖的strand
	@param mainFunc Actor执行入口
	@param stackSize Actor栈大小，默认64k字节，必须是4k的整数倍，最小4k，最大1M
	*/
	static actor_handle create(shared_strand actorStrand, const main_func& mainFunc, size_t stackSize = DEFAULT_STACKSIZE);

	/*!
	@brief 启用堆栈内存池
	*/
	static void enable_stack_pool();
public:
	/*!
	@brief 创建一个子Actor，父Actor终止时，子Actor也终止（在子Actor都完全退出后，父Actor才结束）
	@param actorStrand 子Actor依赖的strand
	@param mainFunc 子Actor入口函数
	@param stackSize Actor栈大小，4k的整数倍（最大1MB）
	@return 子Actor句柄，使用 child_actor_handle 接收返回值
	*/
	child_actor_handle::child_actor_param create_child_actor(shared_strand actorStrand, const main_func& mainFunc, size_t stackSize = DEFAULT_STACKSIZE);
	child_actor_handle::child_actor_param create_child_actor(const main_func& mainFunc, size_t stackSize = DEFAULT_STACKSIZE);

	/*!
	@brief 开始运行子Actor，只能调用一次
	*/
	void child_actor_run(child_actor_handle& actorHandle);
	void child_actor_run(const list<child_actor_handle::ptr>& actorHandles);

	/*!
	@brief 强制终止一个子Actor
	*/
	__yield_interrupt void child_actor_force_quit(child_actor_handle& actorHandle);
	__yield_interrupt void child_actors_force_quit(const list<child_actor_handle::ptr>& actorHandles);

	/*!
	@brief 等待一个子Actor完成后返回
	@return 正常退出的返回true，否则false
	*/
	__yield_interrupt void child_actor_wait_quit(child_actor_handle& actorHandle);

	__yield_interrupt bool timed_child_actor_wait_quit(int tm, child_actor_handle& actorHandle);

	/*!
	@brief 等待一组子Actor完成后返回
	@return 都正常退出的返回true，否则false
	*/
	__yield_interrupt void child_actors_wait_quit(const list<child_actor_handle::ptr>& actorHandles);

	/*!
	@brief 挂起子Actor
	*/
	__yield_interrupt void child_actor_suspend(child_actor_handle& actorHandle);
	
	/*!
	@brief 挂起一组子Actor
	*/
	__yield_interrupt void child_actors_suspend(const list<child_actor_handle::ptr>& actorHandles);

	/*!
	@brief 恢复子Actor
	*/
	__yield_interrupt void child_actor_resume(child_actor_handle& actorHandle);
	
	/*!
	@brief 恢复一组子Actor
	*/
	__yield_interrupt void child_actors_resume(const list<child_actor_handle::ptr>& actorHandles);

	/*!
	@brief 创建另一个Actor，Actor执行完成后返回
	*/
	__yield_interrupt void run_child_actor_complete(shared_strand actorStrand, const main_func& h, size_t stackSize = DEFAULT_STACKSIZE);
	__yield_interrupt void run_child_actor_complete(const main_func& h, size_t stackSize = DEFAULT_STACKSIZE);

	/*!
	@brief 延时等待，Actor内部禁止使用操作系统API Sleep()
	@param ms 等待毫秒数，等于0时暂时放弃Actor执行，直到下次被调度器触发
	*/
	__yield_interrupt void sleep(int ms);
	__yield_interrupt void sleep_guard(int ms);

	/*!
	@brief 中断当前时间片，等到下次被调度(因为Actor是非抢占式调度，当有占用时间片较长的逻辑时，适当使用yield分割时间片)
	*/
	__yield_interrupt void yield();

	/*!
	@brief 中断时间片，当前Actor句柄在别的地方必须被持有
	*/
	__yield_interrupt void yield_guard();

	/*!
	@brief 获取父Actor
	*/
	actor_handle parent_actor();

	/*!
	@brief 获取子Actor
	*/
	const msg_list_shared_alloc<actor_handle>& child_actors();
public:
	typedef msg_list_shared_alloc<std::function<void()> >::iterator quit_iterator;

	/*!
	@brief 注册一个资源释放函数，在强制退出Actor时调用
	*/
	quit_iterator regist_quit_handler(const std::function<void ()>& quitHandler);

	/*!
	@brief 注销资源释放函数
	*/
	void cancel_quit_handler(quit_iterator qh);
public:
	/*!
	@brief 使用内部定时器延时触发某个函数，在触发完成之前不能多次调用
	@param ms 触发延时(毫秒)
	@param h 触发函数
	*/
	template <typename H>
	void delay_trig(int ms, H&& h)
	{
		assert_enter();
		if (ms > 0)
		{
			assert(_timer);
			timeout(ms, TRY_MOVE(h));
		} 
		else if (0 == ms)
		{
			_strand->post(TRY_MOVE(h));
		}
		else
		{
			assert(false);
		}
	}

	/*!
	@brief 取消内部定时器触发
	*/
	void cancel_delay_trig();
public:
	/*!
	@brief 发送一个异步函数到shared_strand中执行（如果是和self一样的shared_strand直接执行），配合quit_guard使用防止引用失效，完成后返回
	*/
	template <typename H>
	__yield_interrupt void send(shared_strand exeStrand, H&& h)
	{
		assert_enter();
		if (exeStrand != _strand)
		{
			actor_handle shared_this = shared_from_this();
			exeStrand->asyncInvokeVoid(TRY_MOVE(h), [shared_this]{shared_this->post_handler(); });
			push_yield();
			return;
		}
		h();
	}

	template <typename T0, typename H>
	__yield_interrupt T0 send(shared_strand exeStrand, H&& h)
	{
		assert_enter();
		if (exeStrand != _strand)
		{
			dst_receiver_buff<TYPE_PIPE(T0)>::dst_buff dstBuff;
			dst_receiver_buff<TYPE_PIPE(T0)> dstRec(dstBuff);
			actor_handle shared_this = shared_from_this();
			exeStrand->asyncInvoke(TRY_MOVE(h), [shared_this, &dstRec](const T0& p0){shared_this->_trig_handler(dstRec, std::move(msg_param<TYPE_PIPE(T0)>(p0))); });
			push_yield();
			return std::move((RM_REF(T0)&)dstBuff.get()._res0);
		} 
		return h();
	}

	/*!
	@brief 往当前"系统线程"堆栈中抛出一个任务，完成后返回，（用于消耗堆栈高的函数）
	*/
	template <typename H>
	__yield_interrupt void run_in_thread_stack(H&& h)
	{
		assert_enter();
		actor_handle shared_this = shared_from_this();
		_strand->asyncInvokeVoid(TRY_MOVE(h), [shared_this]{shared_this->next_tick_handler(); });
		push_yield();
	}

	template <typename T0, typename H>
	__yield_interrupt T0 run_in_thread_stack(H&& h)
	{
		return async_send<T0>(_strand, TRY_MOVE(h));
	}

	/*!
	@brief 强制将一个函数发送到一个shared_strand中执行（比如某个API会进行很多层次的堆栈调用，而当前Actor堆栈不够，可以用此切换到线程堆栈中直接执行），
		配合quit_guard使用防止引用失效，完成后返回
	*/
	template <typename H>
	__yield_interrupt void async_send(shared_strand exeStrand, H&& h)
	{
		assert_enter();
		actor_handle shared_this = shared_from_this();
		exeStrand->asyncInvokeVoid(TRY_MOVE(h), [shared_this]{shared_this->tick_handler(); });
		push_yield();
	}

	template <typename T0, typename H>
	__yield_interrupt T0 async_send(shared_strand exeStrand, H&& h)
	{
		assert_enter();
		dst_receiver_buff<TYPE_PIPE(T0)>::dst_buff dstBuff;
		dst_receiver_buff<TYPE_PIPE(T0)> dstRec(dstBuff);
		actor_handle shared_this = shared_from_this();
		exeStrand->asyncInvoke(TRY_MOVE(h), [shared_this, &dstRec](const T0& p0){shared_this->_trig_handler(dstRec, std::move(msg_param<TYPE_PIPE(T0)>(p0))); });
		push_yield();
		return std::move((RM_REF(T0)&)dstBuff.get()._res0);
	}

	template <typename H>
	__yield_interrupt void async_send_self(H&& h)
	{
		async_send(_strand, TRY_MOVE(h));
	}

	template <typename T0, typename H>
	__yield_interrupt T0 async_send_self(H&& h)
	{
		return async_send<T0>(_strand, TRY_MOVE(h));
	}

	/*!
	@brief 调用一个异步函数，异步回调完成后返回
	*/
	template <typename H>
	__yield_interrupt void trig(const H& h)
	{
		assert_enter();
		h(trig_once_notifer<>(shared_from_this(), NULL));
		push_yield();
	}

	template <typename T0, typename H>
	__yield_interrupt void trig(__out T0& r0, const H& h)
	{
		assert_enter();
		ref_ex<T0> dstRef(r0);
		dst_receiver_ref<T0> dstRec(dstRef);
		h(trig_once_notifer<T0>(shared_from_this(), &dstRec));
		push_yield();
	}

	template <typename T0, typename H>
	__yield_interrupt T0 trig(const H& h)
	{
		T0 r0;
		trig(r0, h);
		return r0;
	}

	template <typename T0, typename T1, typename H>
	__yield_interrupt void trig(__out T0& r0, __out T1& r1, const H& h)
	{
		assert_enter();
		ref_ex<T0, T1> dstRef(r0, r1);
		dst_receiver_ref<T0, T1> dstRec(dstRef);
		h(trig_once_notifer<T0, T1>(shared_from_this(), &dstRec));
		push_yield();
	}

	template <typename T0, typename T1, typename T2, typename H>
	__yield_interrupt void trig(__out T0& r0, __out T1& r1, __out T2& r2, const H& h)
	{
		assert_enter();
		ref_ex<T0, T1, T2> dstRef(r0, r1, r2);
		dst_receiver_ref<T0, T1, T2> dstRec(dstRef);
		h(trig_once_notifer<T0, T1, T2>(shared_from_this(), &dstRec));
		push_yield();
	}

	template <typename T0, typename T1, typename T2, typename T3, typename H>
	__yield_interrupt void trig(__out T0& r0, __out T1& r1, __out T2& r2, __out T3& r3, const H& h)
	{
		assert_enter();
		ref_ex<T0, T1, T2, T3> dstRef(r0, r1, r2, r3);
		dst_receiver_ref<T0, T1, T2, T3> dstRec(dstRef);
		h(trig_once_notifer<T0, T1, T2, T3>(shared_from_this(), &dstRec));
		push_yield();
	}
private:
	void post_handler();
	void tick_handler();
	void next_tick_handler();

	template <typename DST /*dst_receiver*/, typename SRC /*msg_param*/>
	void _trig_handler(DST& dstRec, SRC&& src)
	{
		if (_strand->running_in_this_thread())
		{
			if (!_quited)
			{
				dstRec.move_from(src);
				next_tick_handler();
			}
		} 
		else
		{
			actor_handle shared_this = shared_from_this();
			_strand->post([=, &dstRec]
			{
				if (!shared_this->_quited)
				{
					dstRec.move_from((SRC&)src);
					shared_this->pull_yield();
				}
			});
		}
	}

	template <typename DST /*ref_ex*/, typename SRC /*msg_param*/>
	void _trig_handler_ref(DST dstRef, SRC&& src)
	{
		if (_strand->running_in_this_thread())
		{
			if (!_quited)
			{
				src.move_out(dstRef);
				next_tick_handler();
			}
		}
		else
		{
			actor_handle shared_this = shared_from_this();
			_strand->post([=]
			{
				if (!shared_this->_quited)
				{
					((SRC&)src).move_out((DST&)dstRef);
					shared_this->pull_yield();
				}
			});
		}
	}
public:
	/*!
	@brief 创建一个消息通知函数
	*/
	template <typename T0, typename T1, typename T2, typename T3>
	actor_msg_notifer<T0, T1, T2, T3> make_msg_notifer(actor_msg_handle<T0, T1, T2, T3>& amh)
	{
		return amh.make_notifer(shared_from_this());
	}

	template <typename T0, typename T1, typename T2>
	actor_msg_notifer<T0, T1, T2> make_msg_notifer(actor_msg_handle<T0, T1, T2>& amh)
	{
		return amh.make_notifer(shared_from_this());
	}

	template <typename T0, typename T1>
	actor_msg_notifer<T0, T1> make_msg_notifer(actor_msg_handle<T0, T1>& amh)
	{
		return amh.make_notifer(shared_from_this());
	}

	template <typename T0>
	actor_msg_notifer<T0> make_msg_notifer(actor_msg_handle<T0>& amh)
	{
		return amh.make_notifer(shared_from_this());
	}

	actor_msg_notifer<> make_msg_notifer(actor_msg_handle<>& amh);

	/*!
	@brief 关闭消息通知句柄
	*/
	void close_msg_notifer(actor_msg_handle_base& amh);
private:
	template <typename AMH, typename DST>
	bool _timed_wait_msg(AMH& amh, DST& dstRec, int tm)
	{
		assert(amh._hostActor && amh._hostActor->self_id() == self_id());
		if (!amh.read_msg(dstRec))
		{
			if (0 != tm)
			{
				bool timed = false;
				if (tm > 0)
				{
					delay_trig(tm, [this, &timed]
					{
						if (!_quited)
						{
							timed = true;
							pull_yield();
						}
					});
				}
				push_yield();
				if (!timed)
				{
					if (tm > 0)
					{
						cancel_delay_trig();
					}
					return true;
				}
			}
			amh._dstRec = NULL;
			amh._waiting = false;
			return false;
		}
		return true;
	}

	template <typename AMH, typename DST>
	void _wait_msg(AMH& amh, DST& dstRec)
	{
		assert(amh._hostActor && amh._hostActor->self_id() == self_id());
		if (!amh.read_msg(dstRec))
		{
			push_yield();
		}
	}
public:
	/*!
	@brief 从消息句柄中提取消息
	@param tm 超时时间
	@return 超时完成返回false，成功提取消息返回true
	*/
	template <typename T0, typename T1, typename T2, typename T3, typename DT0, typename DT1, typename DT2, typename DT3>
	__yield_interrupt bool timed_wait_msg(int tm, actor_msg_handle<T0, T1, T2, T3>& amh, DT0& r0, DT1& r1, DT2& r2, DT3& r3)
	{
		assert_enter();
		dst_receiver_buff<TYPE_PIPE(T0), TYPE_PIPE(T1), TYPE_PIPE(T2), TYPE_PIPE(T3)>::dst_buff dstBuff;
		dst_receiver_buff<TYPE_PIPE(T0), TYPE_PIPE(T1), TYPE_PIPE(T2), TYPE_PIPE(T3)> dstRec(dstBuff);
		if (_timed_wait_msg(amh, dstRec, tm))
		{
			msg_param<TYPE_PIPE(T0), TYPE_PIPE(T1), TYPE_PIPE(T2), TYPE_PIPE(T3)>& msg = dstBuff.get();
			r0 = std::move(msg._res0);
			r1 = std::move(msg._res1);
			r2 = std::move(msg._res2);
			r3 = std::move(msg._res3);
			return true;
		}
		return false;
	}

	template <typename T0, typename T1, typename T2, typename DT0, typename DT1, typename DT2>
	__yield_interrupt bool timed_wait_msg(int tm, actor_msg_handle<T0, T1, T2>& amh, DT0& r0, DT1& r1, DT2& r2)
	{
		assert_enter();
		dst_receiver_buff<TYPE_PIPE(T0), TYPE_PIPE(T1), TYPE_PIPE(T2)>::dst_buff dstBuff;
		dst_receiver_buff<TYPE_PIPE(T0), TYPE_PIPE(T1), TYPE_PIPE(T2)> dstRec(dstBuff);
		if (_timed_wait_msg(amh, dstRec, tm))
		{
			msg_param<TYPE_PIPE(T0), TYPE_PIPE(T1), TYPE_PIPE(T2)>& msg = dstBuff.get();
			r0 = std::move(msg._res0);
			r1 = std::move(msg._res1);
			r2 = std::move(msg._res2);
			return true;
		}
		return false;
	}

	template <typename T0, typename T1, typename DT0, typename DT1>
	__yield_interrupt bool timed_wait_msg(int tm, actor_msg_handle<T0, T1>& amh, DT0& r0, DT1& r1)
	{
		assert_enter();
		dst_receiver_buff<TYPE_PIPE(T0), TYPE_PIPE(T1)>::dst_buff dstBuff;
		dst_receiver_buff<TYPE_PIPE(T0), TYPE_PIPE(T1)> dstRec(dstBuff);
		if (_timed_wait_msg(amh, dstRec, tm))
		{
			msg_param<TYPE_PIPE(T0), TYPE_PIPE(T1)>& msg = dstBuff.get();
			r0 = std::move(msg._res0);
			r1 = std::move(msg._res1);
			return true;
		}
		return false;
	}

	template <typename T0, typename DT0>
	__yield_interrupt bool timed_wait_msg(int tm, actor_msg_handle<T0>& amh, DT0& r0)
	{
		assert_enter();
		dst_receiver_buff<TYPE_PIPE(T0)>::dst_buff dstBuff;
		dst_receiver_buff<TYPE_PIPE(T0)> dstRec(dstBuff);
		if (_timed_wait_msg(amh, dstRec, tm))
		{
			msg_param<TYPE_PIPE(T0)>& msg = dstBuff.get();
			r0 = std::move(msg._res0);
			return true;
		}
		return false;
	}

	template <typename T0>
	__yield_interrupt bool timed_wait_msg(int tm, actor_msg_handle<T0>& ath, stack_obj<T0>& r0)
	{
		assert_enter();
		stack_dst_receiver<T0> dstRec(r0);
		if (_timed_wait_msg(ath, dstRec, tm))
		{
			dstRec._has = false;
			return true;
		}
		return false;
	}

	__yield_interrupt bool timed_wait_msg(int tm, actor_msg_handle<>& amh);

	template <typename T0, typename T1, typename T2, typename T3, typename Handler>
	__yield_interrupt bool timed_wait_msg(int tm, actor_msg_handle<T0, T1, T2, T3>& amh, const Handler& h)
	{
		assert_enter();
		dst_receiver_buff<TYPE_PIPE(T0), TYPE_PIPE(T1), TYPE_PIPE(T2), TYPE_PIPE(T3)>::dst_buff dstBuff;
		dst_receiver_buff<TYPE_PIPE(T0), TYPE_PIPE(T1), TYPE_PIPE(T2), TYPE_PIPE(T3)> dstRec(dstBuff);
		if (_timed_wait_msg(amh, dstRec, tm))
		{
			msg_param<TYPE_PIPE(T0), TYPE_PIPE(T1), TYPE_PIPE(T2), TYPE_PIPE(T3)>& msg = dstBuff.get();
			h(msg._res0, msg._res1, msg._res2, msg._res3);
			return true;
		}
		return false;
	}

	template <typename T0, typename T1, typename T2, typename Handler>
	__yield_interrupt bool timed_wait_msg(int tm, actor_msg_handle<T0, T1, T2>& amh, const Handler& h)
	{
		assert_enter();
		dst_receiver_buff<TYPE_PIPE(T0), TYPE_PIPE(T1), TYPE_PIPE(T2)>::dst_buff dstBuff;
		dst_receiver_buff<TYPE_PIPE(T0), TYPE_PIPE(T1), TYPE_PIPE(T2)> dstRec(dstBuff);
		if (_timed_wait_msg(amh, dstRec, tm))
		{
			msg_param<TYPE_PIPE(T0), TYPE_PIPE(T1), TYPE_PIPE(T2)>& msg = dstBuff.get();
			h(msg._res0, msg._res1, msg._res2);
			return true;
		}
		return false;
	}

	template <typename T0, typename T1, typename Handler>
	__yield_interrupt bool timed_wait_msg(int tm, actor_msg_handle<T0, T1>& amh, const Handler& h)
	{
		assert_enter();
		dst_receiver_buff<TYPE_PIPE(T0), TYPE_PIPE(T1)>::dst_buff dstBuff;
		dst_receiver_buff<TYPE_PIPE(T0), TYPE_PIPE(T1)> dstRec(dstBuff);
		if (_timed_wait_msg(amh, dstRec, tm))
		{
			msg_param<TYPE_PIPE(T0), TYPE_PIPE(T1)>& msg = dstBuff.get();
			h(msg._res0, msg._res1);
			return true;
		}
		return false;
	}

	template <typename T0, typename Handler>
	__yield_interrupt bool timed_wait_msg(int tm, actor_msg_handle<T0>& amh, const Handler& h)
	{
		assert_enter();
		dst_receiver_buff<TYPE_PIPE(T0)>::dst_buff dstBuff;
		dst_receiver_buff<TYPE_PIPE(T0)> dstRec(dstBuff);
		if (_timed_wait_msg(amh, dstRec, tm))
		{
			msg_param<TYPE_PIPE(T0)>& msg = dstBuff.get();
			h(msg._res0);
			return true;
		}
		return false;
	}

	template <typename Handler>
	__yield_interrupt bool timed_wait_msg(int tm, actor_msg_handle<>& amh, const Handler& h)
	{
		assert_enter();
		if (timed_wait_msg(tm, amh))
		{
			h();
			return true;
		}
		return false;
	}


	/*!
	@brief 从消息句柄中提取消息
	*/
	template <typename T0, typename T1, typename T2, typename T3, typename DT0, typename DT1, typename DT2, typename DT3>
	__yield_interrupt void wait_msg(actor_msg_handle<T0, T1, T2, T3>& amh, DT0& r0, DT1& r1, DT2& r2, DT3& r3)
	{
		timed_wait_msg(-1, amh, r0, r1, r2, r3);
	}

	template <typename T0, typename T1, typename T2, typename DT0, typename DT1, typename DT2>
	__yield_interrupt void wait_msg(actor_msg_handle<T0, T1, T2>& amh, DT0& r0, DT1& r1, DT2& r2)
	{
		timed_wait_msg(-1, amh, r0, r1, r2);
	}

	template <typename T0, typename T1, typename DT0, typename DT1>
	__yield_interrupt void wait_msg(actor_msg_handle<T0, T1>& amh, DT0& r0, DT1& r1)
	{
		timed_wait_msg(-1, amh, r0, r1);
	}

	template <typename T0, typename DT0>
	__yield_interrupt void wait_msg(actor_msg_handle<T0>& amh, DT0& r0)
	{
		timed_wait_msg(-1, amh, r0);
	}

	template <typename T0>
	__yield_interrupt T0 wait_msg(actor_msg_handle<T0>& amh)
	{
		assert_enter();
		dst_receiver_buff<TYPE_PIPE(T0)>::dst_buff dstBuff;
		dst_receiver_buff<TYPE_PIPE(T0)> dstRec(dstBuff);
		_wait_msg(amh, dstRec);
		return std::move((RM_REF(T0)&)dstBuff.get()._res0);
	}

	__yield_interrupt void wait_msg(actor_msg_handle<>& amh);

	template <typename T0, typename T1, typename T2, typename T3, typename Handler>
	__yield_interrupt void wait_msg(actor_msg_handle<T0, T1, T2, T3>& amh, const Handler& h)
	{
		timed_wait_msg<T0, T1, T2, T3>(-1, amh, h);
	}

	template <typename T0, typename T1, typename T2, typename Handler>
	__yield_interrupt void wait_msg(actor_msg_handle<T0, T1, T2>& amh, const Handler& h)
	{
		timed_wait_msg<T0, T1, T2>(-1, amh, h);
	}

	template <typename T0, typename T1, typename Handler>
	__yield_interrupt void wait_msg(actor_msg_handle<T0, T1>& amh, const Handler& h)
	{
		timed_wait_msg<T0, T1>(-1, amh, h);
	}

	template <typename T0, typename Handler>
	__yield_interrupt void wait_msg(actor_msg_handle<T0>& amh, const Handler& h)
	{
		timed_wait_msg<T0>(-1, amh, h);
	}

	template <typename Handler>
	__yield_interrupt void wait_msg(actor_msg_handle<>& amh, const Handler& h)
	{
		timed_wait_msg(-1, amh, h);
	}
public:
	/*!
	@brief 创建一个消息触发函数，只有一次触发有效
	*/
	template <typename T0, typename T1, typename T2, typename T3>
	actor_trig_notifer<T0, T1, T2, T3> make_trig_notifer(actor_trig_handle<T0, T1, T2, T3>& ath)
	{
		return ath.make_notifer(shared_from_this());
	}

	template <typename T0, typename T1, typename T2>
	actor_trig_notifer<T0, T1, T2> make_trig_notifer(actor_trig_handle<T0, T1, T2>& ath)
	{
		return ath.make_notifer(shared_from_this());
	}

	template <typename T0, typename T1>
	actor_trig_notifer<T0, T1> make_trig_notifer(actor_trig_handle<T0, T1>& ath)
	{
		return ath.make_notifer(shared_from_this());
	}

	template <typename T0>
	actor_trig_notifer<T0> make_trig_notifer(actor_trig_handle<T0>& ath)
	{
		return ath.make_notifer(shared_from_this());
	}

	actor_trig_notifer<> make_trig_notifer(actor_trig_handle<>& ath);

	/*!
	@brief 关闭消息触发句柄
	*/
	void close_trig_notifer(actor_msg_handle_base& ath);

	/*!
	@brief 创建回调出发函数，直接作为回调参数使用，async_func(..., Handler self->make_callback())
	*/
	template <typename T0, typename T1, typename T2, typename T3>
	callback_handler<T0, T1, T2, T3> make_callback(T0& r0, T1& r1, T2& r2, T3& r3)
	{
		assert_enter();
		return callback_handler<T0, T1, T2, T3>(this, r0, r1, r2, r3);
	}

	template <typename T0, typename T1, typename T2>
	callback_handler<T0, T1, T2> make_callback(T0& r0, T1& r1, T2& r2)
	{
		assert_enter();
		return callback_handler<T0, T1, T2>(this, r0, r1, r2);
	}

	template <typename T0, typename T1>
	callback_handler<T0, T1> make_callback(T0& r0, T1& r1)
	{
		assert_enter();
		return callback_handler<T0, T1>(this, r0, r1);
	}

	template <typename T0>
	callback_handler<T0> make_callback(T0& r0)
	{
		assert_enter();
		return callback_handler<T0>(this, r0);
	}

	callback_handler<> make_callback();
public:
	/*!
	@brief 从触发句柄中提取消息
	@param tm 超时时间
	@return 超时完成返回false，成功提取消息返回true
	*/
	template <typename T0, typename T1, typename T2, typename T3, typename DT0, typename DT1, typename DT2, typename DT3>
	__yield_interrupt bool timed_wait_trig(int tm, actor_trig_handle<T0, T1, T2, T3>& ath, DT0& r0, DT1& r1, DT2& r2, DT3& r3)
	{
		assert_enter();
		dst_receiver_buff<TYPE_PIPE(T0), TYPE_PIPE(T1), TYPE_PIPE(T2), TYPE_PIPE(T3)>::dst_buff dstBuff;
		dst_receiver_buff<TYPE_PIPE(T0), TYPE_PIPE(T1), TYPE_PIPE(T2), TYPE_PIPE(T3)> dstRec(dstBuff);
		if (_timed_wait_msg(ath, dstRec, tm))
		{
			msg_param<TYPE_PIPE(T0), TYPE_PIPE(T1), TYPE_PIPE(T2), TYPE_PIPE(T3)>& msg = dstBuff.get();
			r0 = std::move(msg._res0);
			r1 = std::move(msg._res1);
			r2 = std::move(msg._res2);
			r3 = std::move(msg._res3);
			return true;
		}
		return false;
	}

	template <typename T0, typename T1, typename T2, typename DT0, typename DT1, typename DT2>
	__yield_interrupt bool timed_wait_trig(int tm, actor_trig_handle<T0, T1, T2>& ath, DT0& r0, DT1& r1, DT2& r2)
	{
		assert_enter();
		dst_receiver_buff<TYPE_PIPE(T0), TYPE_PIPE(T1), TYPE_PIPE(T2)>::dst_buff dstBuff;
		dst_receiver_buff<TYPE_PIPE(T0), TYPE_PIPE(T1), TYPE_PIPE(T2)> dstRec(dstBuff);
		if (_timed_wait_msg(ath, dstRec, tm))
		{
			msg_param<TYPE_PIPE(T0), TYPE_PIPE(T1), TYPE_PIPE(T2)>& msg = dstBuff.get();
			r0 = std::move(msg._res0);
			r1 = std::move(msg._res1);
			r2 = std::move(msg._res2);
			return true;
		}
		return false;
	}

	template <typename T0, typename T1, typename DT0, typename DT1>
	__yield_interrupt bool timed_wait_trig(int tm, actor_trig_handle<T0, T1>& ath, DT0& r0, DT1& r1)
	{
		assert_enter();
		dst_receiver_buff<TYPE_PIPE(T0), TYPE_PIPE(T1)>::dst_buff dstBuff;
		dst_receiver_buff<TYPE_PIPE(T0), TYPE_PIPE(T1)> dstRec(dstBuff);
		if (_timed_wait_msg(ath, dstRec, tm))
		{
			msg_param<TYPE_PIPE(T0), TYPE_PIPE(T1)>& msg = dstBuff.get();
			r0 = std::move(msg._res0);
			r1 = std::move(msg._res1);
			return true;
		}
		return false;
	}

	template <typename T0, typename DT0>
	__yield_interrupt bool timed_wait_trig(int tm, actor_trig_handle<T0>& ath, DT0& r0)
	{
		assert_enter();
		dst_receiver_buff<TYPE_PIPE(T0)>::dst_buff dstBuff;
		dst_receiver_buff<TYPE_PIPE(T0)> dstRec(dstBuff);
		if (_timed_wait_msg(ath, dstRec, tm))
		{
			msg_param<TYPE_PIPE(T0)>& msg = dstBuff.get();
			r0 = std::move(msg._res0);
			return true;
		}
		return false;
	}

	template <typename T0>
	__yield_interrupt bool timed_wait_trig(int tm, actor_trig_handle<T0>& ath, stack_obj<T0>& r0)
	{
		assert_enter();
		stack_dst_receiver<T0> dstRec(r0);
		if (_timed_wait_msg(ath, dstRec, tm))
		{
			dstRec._has = false;
			return true;
		}
		return false;
	}

	__yield_interrupt bool timed_wait_trig(int tm, actor_trig_handle<>& ath);

	template <typename T0, typename T1, typename T2, typename T3, typename Handler>
	__yield_interrupt bool timed_wait_trig(int tm, actor_trig_handle<T0, T1, T2, T3>& ath, const Handler& h)
	{
		assert_enter();
		dst_receiver_buff<TYPE_PIPE(T0), TYPE_PIPE(T1), TYPE_PIPE(T2), TYPE_PIPE(T3)>::dst_buff dstBuff;
		dst_receiver_buff<TYPE_PIPE(T0), TYPE_PIPE(T1), TYPE_PIPE(T2), TYPE_PIPE(T3)> dstRec(dstBuff);
		if (_timed_wait_msg(ath, dstRec, tm))
		{
			msg_param<TYPE_PIPE(T0), TYPE_PIPE(T1), TYPE_PIPE(T2), TYPE_PIPE(T3)>& msg = dstBuff.get();
			h(msg._res0, msg._res1, msg._res2, msg._res3);
			return true;
		}
		return false;
	}

	template <typename T0, typename T1, typename T2, typename Handler>
	__yield_interrupt bool timed_wait_trig(int tm, actor_trig_handle<T0, T1, T2>& ath, const Handler& h)
	{
		assert_enter();
		dst_receiver_buff<TYPE_PIPE(T0), TYPE_PIPE(T1), TYPE_PIPE(T2)>::dst_buff dstBuff;
		dst_receiver_buff<TYPE_PIPE(T0), TYPE_PIPE(T1), TYPE_PIPE(T2)> dstRec(dstBuff);
		if (_timed_wait_msg(ath, dstRec, tm))
		{
			msg_param<TYPE_PIPE(T0), TYPE_PIPE(T1), TYPE_PIPE(T2)>& msg = dstBuff.get();
			h(msg._res0, msg._res1, msg._res2);
			return true;
		}
		return false;
	}

	template <typename T0, typename T1, typename Handler>
	__yield_interrupt bool timed_wait_trig(int tm, actor_trig_handle<T0, T1>& ath, const Handler& h)
	{
		assert_enter();
		dst_receiver_buff<TYPE_PIPE(T0), TYPE_PIPE(T1)>::dst_buff dstBuff;
		dst_receiver_buff<TYPE_PIPE(T0), TYPE_PIPE(T1)> dstRec(dstBuff);
		if (_timed_wait_msg(ath, dstRec, tm))
		{
			msg_param<TYPE_PIPE(T0), TYPE_PIPE(T1)>& msg = dstBuff.get();
			h(msg._res0, msg._res1);
			return true;
		}
		return false;
	}

	template <typename T0, typename Handler>
	__yield_interrupt bool timed_wait_trig(int tm, actor_trig_handle<T0>& ath, const Handler& h)
	{
		assert_enter();
		dst_receiver_buff<TYPE_PIPE(T0)>::dst_buff dstBuff;
		dst_receiver_buff<TYPE_PIPE(T0)> dstRec(dstBuff);
		if (_timed_wait_msg(ath, dstRec, tm))
		{
			msg_param<TYPE_PIPE(T0)>& msg = dstBuff.get();
			h(msg._res0);
			return true;
		}
		return false;
	}

	template <typename Handler>
	__yield_interrupt bool timed_wait_trig(int tm, actor_trig_handle<>& ath, const Handler& h)
	{
		assert_enter();
		if (timed_wait_trig(tm, ath))
		{
			h();
			return true;
		}
		return false;
	}

	/*!
	@brief 从触发句柄中提取消息
	*/
	template <typename T0, typename T1, typename T2, typename T3, typename DT0, typename DT1, typename DT2, typename DT3>
	__yield_interrupt void wait_trig(actor_trig_handle<T0, T1, T2, T3>& ath, DT0& r0, DT1& r1, DT2& r2, DT3& r3)
	{
		timed_wait_trig(-1, ath, r0, r1, r2, r3);
	}

	template <typename T0, typename T1, typename T2, typename DT0, typename DT1, typename DT2>
	__yield_interrupt void wait_trig(actor_trig_handle<T0, T1, T2>& ath, DT0& r0, DT1& r1, DT2& r2)
	{
		timed_wait_trig(-1, ath, r0, r1, r2);
	}

	template <typename T0, typename T1, typename DT0, typename DT1>
	__yield_interrupt void wait_trig(actor_trig_handle<T0, T1>& ath, DT0& r0, DT1& r1)
	{
		timed_wait_trig(-1, ath, r0, r1);
	}

	template <typename T0, typename DT0>
	__yield_interrupt void wait_trig(actor_trig_handle<T0>& ath, DT0& r0)
	{
		timed_wait_trig(-1, ath, r0);
	}

	template <typename T0>
	__yield_interrupt T0 wait_trig(actor_trig_handle<T0>& ath)
	{
		assert_enter();
		dst_receiver_buff<TYPE_PIPE(T0)>::dst_buff dstBuff;
		dst_receiver_buff<TYPE_PIPE(T0)> dstRec(dstBuff);
		_wait_msg(ath, dstRec);
		return std::move((RM_REF(T0)&)dstBuff.get()._res0);
	}

	__yield_interrupt void wait_trig(actor_trig_handle<>& ath);

	template <typename T0, typename T1, typename T2, typename T3, typename Handler>
	__yield_interrupt void wait_trig(actor_trig_handle<T0, T1, T2, T3>& ath, const Handler& h)
	{
		timed_wait_trig<T0, T1, T2, T3>(-1, ath, h);
	}

	template <typename T0, typename T1, typename T2, typename Handler>
	__yield_interrupt void wait_trig(actor_trig_handle<T0, T1, T2>& ath, const Handler& h)
	{
		timed_wait_trig<T0, T1, T2>(-1, ath, h);
	}

	template <typename T0, typename T1, typename Handler>
	__yield_interrupt void wait_trig(actor_trig_handle<T0, T1>& ath, const Handler& h)
	{
		timed_wait_trig<T0, T1>(-1, ath, h);
	}

	template <typename T0, typename Handler>
	__yield_interrupt void wait_trig(actor_trig_handle<T0>& ath, const Handler& h)
	{
		timed_wait_trig<T0>(-1, ath, h);
	}

	template <typename Handler>
	__yield_interrupt void wait_trig(actor_trig_handle<>& ath, const Handler& h)
	{
		timed_wait_trig(-1, ath, h);
	}
private:
	/*!
	@brief 寻找出与模板参数类型匹配的消息池
	*/
	template <typename T0, typename T1, typename T2, typename T3>
	std::shared_ptr<msg_pool_status::pck<T0, T1, T2, T3> > msg_pool_pck(bool make = true)
	{
		typedef msg_pool_status::pck<T0, T1, T2, T3> pck_type;
		size_t typeID = func_type<T0, T1, T2, T3>::number != 0 ? typeid(pck_type).hash_code() : 0;
		if (make)
		{
			auto& res = _msgPoolStatus._msgTypeMap.insert(make_pair(typeID, std::shared_ptr<pck_type>())).first->second;
			if (!res)
			{
				res = std::shared_ptr<pck_type>(new pck_type(this));
			}
			//assert(std::dynamic_pointer_cast<pck_type>(res));
			return std::static_pointer_cast<pck_type>(res);
		}
		auto it = _msgPoolStatus._msgTypeMap.find(typeID);
		if (it != _msgPoolStatus._msgTypeMap.end())
		{
			//assert(std::dynamic_pointer_cast<pck_type>(it->second));
			return std::static_pointer_cast<pck_type>(it->second);
		}
		return std::shared_ptr<pck_type>();
	}

	/*!
	@brief 清除消息代理链
	*/
	template <typename T0, typename T1, typename T2, typename T3>
	void clear_msg_list(const std::shared_ptr<msg_pool_status::pck<T0, T1, T2, T3>>& msgPck, bool poolDis = true)
	{
		std::shared_ptr<msg_pool_status::pck<T0, T1, T2, T3>> uStack[16];
		size_t stackl = 0;
		auto pckIt = msgPck;
		while (true)
		{
			if (pckIt->_next)
			{
				pckIt->_msgPool.reset();
				pckIt = pckIt->_next;
				pckIt->lock(this);
				assert(stackl < 15);
				uStack[stackl++] = pckIt;
			}
			else
			{
				if (!pckIt->is_closed())
				{
					if (pckIt->_msgPool)
					{
						if (poolDis)
						{
							auto& msgPool_ = pckIt->_msgPool;
							auto& msgPump_ = pckIt->_msgPump;
							send(msgPool_->_strand, [&msgPool_, &msgPump_]
							{
								assert(msgPool_->_msgPump == msgPump_);
								msgPool_->disconnect();
							});
						}
						pckIt->_msgPool.reset();
					}
					if (pckIt->_msgPump)
					{
						auto& msgPump_ = pckIt->_msgPump;
						send(msgPump_->_strand, [&msgPump_]
						{
							msgPump_->clear();
						});
					}
				}
				while (stackl)
				{
					uStack[--stackl]->unlock(this);
				}
				return;
			}
		}
	}

	/*!
	@brief 更新消息代理链
	*/
	template <typename T0, typename T1, typename T2, typename T3>
	actor_handle update_msg_list(const std::shared_ptr<msg_pool_status::pck<T0, T1, T2, T3>>& msgPck, const std::shared_ptr<msg_pool<T0, T1, T2, T3>>& newPool)
	{
		typedef typename msg_pool<T0, T1, T2, T3>::pump_handler pump_handler;

		std::shared_ptr<msg_pool_status::pck<T0, T1, T2, T3>> uStack[16];
		size_t stackl = 0;
		auto pckIt = msgPck;
		while (true)
		{
			if (pckIt->_next)
			{
				pckIt->_msgPool = newPool;
				pckIt = pckIt->_next;
				pckIt->lock(this);
				assert(stackl < 15);
				uStack[stackl++] = pckIt;
			} 
			else
			{
				if (!pckIt->is_closed())
				{
					if (pckIt->_msgPool)
					{
						auto& msgPool_ = pckIt->_msgPool;
						send(msgPool_->_strand, [&msgPool_]
						{
							msgPool_->disconnect();
						});
					}
					pckIt->_msgPool = newPool;
					if (pckIt->_msgPump)
					{
						auto& msgPump_ = pckIt->_msgPump;
						if (newPool)
						{
							auto ph = send<pump_handler>(newPool->_strand, [&newPool, &msgPump_]()->pump_handler
							{
								return newPool->connect_pump(msgPump_);
							});
							send(msgPump_->_strand, [&msgPump_, &ph]
							{
								msgPump_->connect(ph);
							});
						}
						else
						{
							send(msgPump_->_strand, [&msgPump_]
							{
								msgPump_->clear();
							});
						}
					}
					while (stackl)
					{
						uStack[--stackl]->unlock(this);
					}
					return pckIt->_hostActor->shared_from_this();
				}
				while (stackl)
				{
					uStack[--stackl]->unlock(this);
				}
				return actor_handle();
			}
		}
	}
private:
	/*!
	@brief 把本Actor内消息由伙伴Actor代理处理
	*/
	template <typename T0, typename T1, typename T2, typename T3>
	__yield_interrupt bool msg_agent_to(actor_handle childActor)
	{
		typedef std::shared_ptr<msg_pool_status::pck<T0, T1, T2, T3>> pck_type;

		assert_enter();
		assert(childActor);
		{
			auto isSelf = childActor->parent_actor();
			if (!isSelf || isSelf->self_id() != self_id())
			{
				assert(false);
				return false;
			}
		}
		quit_guard qg(this);
		auto childPck = send<pck_type>(childActor->self_strand(), [&childActor]()->pck_type
		{
			if (!childActor->is_quited())
			{
				return childActor->msg_pool_pck<T0, T1, T2, T3>();
			}
			return pck_type();
		});
		if (childPck)
		{
			auto msgPck = msg_pool_pck<T0, T1, T2, T3>();
			msgPck->lock(this);
			childPck->lock(this);
			childPck->_isHead = false;
			actor_handle hostActor = update_msg_list<T0, T1, T2, T3>(childPck, msgPck->_msgPool);
			if (hostActor)
			{
				if (msgPck->_next && msgPck->_next != childPck)
				{
					msgPck->_next->lock(this);
					clear_msg_list<T0, T1, T2, T3>(msgPck->_next, false);
					msgPck->_next->unlock(this);
				}
				msgPck->_next = childPck;
				childPck->unlock(this);
				msgPck->unlock(this);
				return true;
			}
			childPck->unlock(this);
			msgPck->unlock(this);
		}
		return false;
	}
public:
	template <typename T0, typename T1, typename T2, typename T3>
	__yield_interrupt bool msg_agent_to(child_actor_handle& childActor)
	{
		return msg_agent_to<T0, T1, T2, T3>(childActor.get_actor());
	}

	template <typename T0, typename T1, typename T2>
	__yield_interrupt bool msg_agent_to(child_actor_handle& childActor)
	{
		return msg_agent_to<T0, T1, T2, void>(childActor.get_actor());
	}

	template <typename T0, typename T1>
	__yield_interrupt bool msg_agent_to(child_actor_handle& childActor)
	{
		return msg_agent_to<T0, T1, void, void>(childActor.get_actor());
	}

	template <typename T0>
	__yield_interrupt bool msg_agent_to(child_actor_handle& childActor)
	{
		return msg_agent_to<T0, void, void, void>(childActor.get_actor());
	}

	__yield_interrupt bool msg_agent_to(child_actor_handle& childActor);

public:
	/*!
	@brief 把消息指定到一个特定Actor函数体去处理
	@return 返回处理该消息的子Actor句柄
	*/
	template <typename T0, typename T1, typename T2, typename T3, typename Handler>
	__yield_interrupt child_actor_handle::child_actor_param msg_agent_to_actor(shared_strand strand, bool autoRun, const Handler& agentActor, size_t stackSize = DEFAULT_STACKSIZE)
	{
		child_actor_handle::child_actor_param childActor = create_child_actor(strand, [agentActor](my_actor* self)
		{
			agentActor(self, self->connect_msg_pump<T0, T1, T2, T3>());
		}, stackSize);
		msg_agent_to<T0, T1, T2, T3>(childActor._actor);
		if (autoRun)
		{
			childActor._actor->notify_run();
		}
		return childActor;
	}

	template <typename T0, typename T1, typename T2, typename Handler>
	__yield_interrupt child_actor_handle::child_actor_param msg_agent_to_actor(shared_strand strand, bool autoRun, const Handler& agentActor, size_t stackSize = DEFAULT_STACKSIZE)
	{
		return msg_agent_to_actor<T0, T1, T2, void>(strand, autoRun, agentActor, stackSize);
	}

	template <typename T0, typename T1, typename Handler>
	__yield_interrupt child_actor_handle::child_actor_param msg_agent_to_actor(shared_strand strand, bool autoRun, const Handler& agentActor, size_t stackSize = DEFAULT_STACKSIZE)
	{
		return msg_agent_to_actor<T0, T1, void, void>(strand, autoRun, agentActor, stackSize);
	}

	template <typename T0, typename Handler>
	__yield_interrupt child_actor_handle::child_actor_param msg_agent_to_actor(shared_strand strand, bool autoRun, const Handler& agentActor, size_t stackSize = DEFAULT_STACKSIZE)
	{
		return msg_agent_to_actor<T0, void, void, void>(strand, autoRun, agentActor, stackSize);
	}

	template <typename Handler>
	__yield_interrupt child_actor_handle::child_actor_param msg_agent_to_actor(shared_strand strand, bool autoRun, const Handler& agentActor, size_t stackSize = DEFAULT_STACKSIZE)
	{
		return msg_agent_to_actor<void, void, void, void>(strand, autoRun, agentActor, stackSize);
	}

	template <typename T0, typename T1, typename T2, typename T3, typename Handler>
	__yield_interrupt child_actor_handle::child_actor_param msg_agent_to_actor(bool autoRun, const Handler& agentActor, size_t stackSize = DEFAULT_STACKSIZE)
	{
		return msg_agent_to_actor<T0, T1, T2, T3>(self_strand(), autoRun, agentActor, stackSize);
	}

	template <typename T0, typename T1, typename T2, typename Handler>
	__yield_interrupt child_actor_handle::child_actor_param msg_agent_to_actor(bool autoRun, const Handler& agentActor, size_t stackSize = DEFAULT_STACKSIZE)
	{
		return msg_agent_to_actor<T0, T1, T2, void>(self_strand(), autoRun, agentActor, stackSize);
	}

	template <typename T0, typename T1, typename Handler>
	__yield_interrupt child_actor_handle::child_actor_param msg_agent_to_actor(bool autoRun, const Handler& agentActor, size_t stackSize = DEFAULT_STACKSIZE)
	{
		return msg_agent_to_actor<T0, T1, void, void>(self_strand(), autoRun, agentActor, stackSize);
	}

	template <typename T0, typename Handler>
	__yield_interrupt child_actor_handle::child_actor_param msg_agent_to_actor(bool autoRun, const Handler& agentActor, size_t stackSize = DEFAULT_STACKSIZE)
	{
		return msg_agent_to_actor<T0, void, void, void>(self_strand(), autoRun, agentActor, stackSize);
	}

	template <typename Handler>
	__yield_interrupt child_actor_handle::child_actor_param msg_agent_to_actor(bool autoRun, const Handler& agentActor, size_t stackSize = DEFAULT_STACKSIZE)
	{
		return msg_agent_to_actor<void, void, void, void>(self_strand(), autoRun, agentActor, stackSize);
	}
public:
	/*!
	@brief 断开伙伴代理该消息
	*/
	template <typename T0, typename T1, typename T2, typename T3>
	__yield_interrupt void msg_agent_off()
	{
		assert_enter();
		auto msgPck = msg_pool_pck<T0, T1, T2, T3>(false);
		if (msgPck)
		{
			quit_guard qg(this);
			msgPck->lock(this);
			if (msgPck->_next)
			{
				msgPck->_next->lock(this);
				clear_msg_list<T0, T1, T2, T3>(msgPck->_next);
				msgPck->_next->_isHead = true;
				msgPck->_next->unlock(this);
				msgPck->_next.reset();
			}
			msgPck->unlock(this);
		}
	}

	template <typename T0, typename T1, typename T2>
	__yield_interrupt void msg_agent_off()
	{
		msg_agent_off<T0, T1, T2, void>();
	}

	template <typename T0, typename T1>
	__yield_interrupt void msg_agent_off()
	{
		msg_agent_off<T0, T1, void, void>();
	}

	template <typename T0>
	__yield_interrupt void msg_agent_off()
	{
		msg_agent_off<T0, void, void, void>();
	}

	__yield_interrupt void msg_agent_off();
public:
	/*!
	@brief 连接消息通知到一个伙伴Actor，该Actor必须是子Actor或没有父Actor
	@param makeNew false 如果存在返回之前，否则创建新的通知；true 强制创建新的通知，之前的将失效，且断开与buddyActor的关联
	@param fixedSize 消息队列内存池长度
	@warning 如果 makeNew = false 且该节点为父的代理，将创建失败
	@return 消息通知函数
	*/
	template <typename T0, typename T1, typename T2, typename T3>
	__yield_interrupt post_actor_msg<T0, T1, T2, T3> connect_msg_notifer_to(actor_handle buddyActor, bool makeNew = false, size_t fixedSize = 16)
	{
		typedef msg_pool<T0, T1, T2, T3> pool_type;
		typedef typename pool_type::pump_handler pump_handler;
		typedef std::shared_ptr<msg_pool_status::pck<T0, T1, T2, T3>> pck_type;

		assert_enter();
		{
			auto buddyParent = buddyActor->parent_actor();
			if (!(buddyActor && (!buddyParent || buddyParent->self_id() == self_id())))
			{
				assert(false);
				return post_actor_msg<T0, T1, T2, T3>();
			}
		}
#ifdef _DEBUG
		{
			auto pa = parent_actor();
			while (pa)
			{
				assert(pa->self_id() != buddyActor->self_id());
				pa = pa->parent_actor();
			}
		}
#endif
		auto msgPck = msg_pool_pck<T0, T1, T2, T3>();
		quit_guard qg(this);
		msgPck->lock(this);
		auto childPck = send<pck_type>(buddyActor->self_strand(), [&buddyActor]()->pck_type
		{
			if (!buddyActor->is_quited())
			{
				return buddyActor->msg_pool_pck<T0, T1, T2, T3>();
			}
			return pck_type();
		});
		if (!childPck)
		{
			msgPck->unlock(this);
			return post_actor_msg<T0, T1, T2, T3>();
		}
		if (makeNew)
		{
			auto newPool = pool_type::make(buddyActor->self_strand(), fixedSize);
			childPck->lock(this);
			childPck->_isHead = true;
			actor_handle hostActor = update_msg_list<T0, T1, T2, T3>(childPck, newPool);
			childPck->unlock(this);
			if (hostActor)
			{
				//如果当前消息节点在更新的消息链中，则断开，然后连接到本地消息泵
				if (msgPck->_next == childPck)
				{
					msgPck->_next.reset();
					if (msgPck->_msgPump)
					{
						auto& msgPump_ = msgPck->_msgPump;
						if (msgPck->_msgPool)
						{
							auto& msgPool_ = msgPck->_msgPool;
							msgPump_->connect(this->send<pump_handler>(msgPool_->_strand, [&msgPool_, &msgPump_]()->pump_handler
							{
								return msgPool_->connect_pump(msgPump_);
							}));
						}
						else
						{
							msgPump_->clear();
						}
					}
				}
				msgPck->unlock(this);
				return post_actor_msg<T0, T1, T2, T3>(newPool, hostActor);
			}
			msgPck->unlock(this);
			//消息链的代理人已经退出，失败
			return post_actor_msg<T0, T1, T2, T3>();
		}
		childPck->lock(this);
		if (childPck->_isHead)
		{
			assert(msgPck->_next != childPck);
			auto childPool = childPck->_msgPool;
			if (!childPool)
			{
				childPool = pool_type::make(buddyActor->self_strand(), fixedSize);
			}
			actor_handle hostActor = update_msg_list<T0, T1, T2, T3>(childPck, childPool);
			if (hostActor)
			{
				childPck->unlock(this);
				msgPck->unlock(this);
				return post_actor_msg<T0, T1, T2, T3>(childPool, hostActor);
			}
			//消息链的代理人已经退出，失败
		}
		childPck->unlock(this);
		msgPck->unlock(this);
		return post_actor_msg<T0, T1, T2, T3>();
	}

	template <typename T0, typename T1, typename T2, typename T3>
	__yield_interrupt post_actor_msg<T0, T1, T2, T3> connect_msg_notifer_to(child_actor_handle& childActor, bool makeNew = false, size_t fixedSize = 16)
	{
		return connect_msg_notifer_to<T0, T1, T2, T3>(childActor.get_actor(), makeNew, fixedSize);
	}

	template <typename T0, typename T1, typename T2>
	__yield_interrupt post_actor_msg<T0, T1, T2> connect_msg_notifer_to(const actor_handle& buddyActor, bool makeNew = false, size_t fixedSize = 16)
	{
		return connect_msg_notifer_to<T0, T1, T2, void>(buddyActor, makeNew, fixedSize);
	}

	template <typename T0, typename T1, typename T2>
	__yield_interrupt post_actor_msg<T0, T1, T2> connect_msg_notifer_to(child_actor_handle& childActor, bool makeNew = false, size_t fixedSize = 16)
	{
		return connect_msg_notifer_to<T0, T1, T2, void>(childActor.get_actor(), makeNew, fixedSize);
	}

	template <typename T0, typename T1>
	__yield_interrupt post_actor_msg<T0, T1> connect_msg_notifer_to(const actor_handle& buddyActor, bool makeNew = false, size_t fixedSize = 16)
	{
		return connect_msg_notifer_to<T0, T1, void, void>(buddyActor, makeNew, fixedSize);
	}

	template <typename T0, typename T1>
	__yield_interrupt post_actor_msg<T0, T1> connect_msg_notifer_to(child_actor_handle& childActor, bool makeNew = false, size_t fixedSize = 16)
	{
		return connect_msg_notifer_to<T0, T1, void, void>(childActor.get_actor(), makeNew, fixedSize);
	}

	template <typename T0>
	__yield_interrupt post_actor_msg<T0> connect_msg_notifer_to(const actor_handle& buddyActor, bool makeNew = false, size_t fixedSize = 16)
	{
		return connect_msg_notifer_to<T0, void, void, void>(buddyActor, makeNew, fixedSize);
	}

	template <typename T0>
	__yield_interrupt post_actor_msg<T0> connect_msg_notifer_to(child_actor_handle& childActor, bool makeNew = false, size_t fixedSize = 16)
	{
		return connect_msg_notifer_to<T0, void, void, void>(childActor.get_actor(), makeNew, fixedSize);
	}

	__yield_interrupt post_actor_msg<> connect_msg_notifer_to(const actor_handle& buddyActor, bool makeNew = false);
	__yield_interrupt post_actor_msg<> connect_msg_notifer_to(child_actor_handle& childActor, bool makeNew = false);

	/*!
	@brief 连接消息通知到自己的Actor
	@param makeNew false 如果存在返回之前，否则创建新的通知；true 强制创建新的通知，之前的将失效，且断开与buddyActor的关联
	@param fixedSize 消息队列内存池长度
	@warning 如果该节点为父的代理，那么将创建失败
	@return 消息通知函数
	*/
	template <typename T0, typename T1, typename T2, typename T3>
	__yield_interrupt post_actor_msg<T0, T1, T2, T3> connect_msg_notifer_to_self(bool makeNew = false, size_t fixedSize = 16)
	{
		typedef msg_pool<T0, T1, T2, T3> pool_type;

		assert_enter();
		auto msgPck = msg_pool_pck<T0, T1, T2, T3>();
		quit_guard qg(this);
		msgPck->lock(this);
		if (msgPck->_isHead)
		{
			std::shared_ptr<pool_type> msgPool;
			if (makeNew || !msgPck->_msgPool)
			{
				msgPool = pool_type::make(self_strand(), fixedSize);
			}
			else
			{
				msgPool = msgPck->_msgPool;
			}
			actor_handle hostActor = update_msg_list<T0, T1, T2, T3>(msgPck, msgPool);
			if (hostActor)
			{
				msgPck->unlock(this);
				return post_actor_msg<T0, T1, T2, T3>(msgPool, hostActor);
			}
		}
		msgPck->unlock(this);
		return post_actor_msg<T0, T1, T2, T3>();
	}

	template <typename T0, typename T1, typename T2>
	__yield_interrupt post_actor_msg<T0, T1, T2> connect_msg_notifer_to_self(bool makeNew = false, size_t fixedSize = 16)
	{
		return connect_msg_notifer_to_self<T0, T1, T2, void>(makeNew, fixedSize);
	}

	template <typename T0, typename T1>
	__yield_interrupt post_actor_msg<T0, T1> connect_msg_notifer_to_self(bool makeNew = false, size_t fixedSize = 16)
	{
		return connect_msg_notifer_to_self<T0, T1, void, void>(makeNew, fixedSize);
	}

	template <typename T0>
	__yield_interrupt post_actor_msg<T0> connect_msg_notifer_to_self(bool makeNew = false, size_t fixedSize = 16)
	{
		return connect_msg_notifer_to_self<T0, void, void, void>(makeNew, fixedSize);
	}

	__yield_interrupt post_actor_msg<> connect_msg_notifer_to_self(bool makeNew = false);

	/*!
	@brief 创建一个消息通知函数，在该Actor所依赖的ios无关线程中使用，且在该Actor调用 notify_run() 之前
	@param fixedSize 消息队列内存池长度
	@return 消息通知函数
	*/
	template <typename T0, typename T1, typename T2, typename T3>
	post_actor_msg<T0, T1, T2, T3> connect_msg_notifer(size_t fixedSize = 16)
	{
		typedef post_actor_msg<T0, T1, T2, T3> post_type;

		return _strand->syncInvoke<post_type>([this, fixedSize]()->post_type
		{
			typedef msg_pool<T0, T1, T2, T3> pool_type;
			if (!this->parent_actor() && !this->is_started() && !this->is_quited())
			{
				auto msgPck = this->msg_pool_pck<T0, T1, T2, T3>();
				msgPck->_msgPool = pool_type::make(this->self_strand(), fixedSize);
				return post_type(msgPck->_msgPool, shared_from_this());
			}
			assert(false);
			return post_type();
		});
	}

	template <typename T0, typename T1, typename T2>
	post_actor_msg<T0, T1, T2> connect_msg_notifer(size_t fixedSize = 16)
	{
		return connect_msg_notifer<T0, T1, T2, void>(fixedSize);
	}

	template <typename T0, typename T1>
	post_actor_msg<T0, T1> connect_msg_notifer(size_t fixedSize = 16)
	{
		return connect_msg_notifer<T0, T1, void, void>(fixedSize);
	}

	template <typename T0>
	post_actor_msg<T0> connect_msg_notifer(size_t fixedSize = 16)
	{
		return connect_msg_notifer<T0, void, void, void>(fixedSize);
	}

	post_actor_msg<> connect_msg_notifer();
	//////////////////////////////////////////////////////////////////////////

	/*!
	@brief 连接消息泵到消息池
	@return 返回消息泵句柄
	*/
	template <typename T0, typename T1, typename T2, typename T3>
	msg_pump_handle<T0, T1, T2, T3> connect_msg_pump()
	{
		typedef msg_pump<T0, T1, T2, T3> pump_type;
		typedef msg_pool<T0, T1, T2, T3> pool_type;
		typedef typename pool_type::pump_handler pump_handler;

		assert_enter();
		auto msgPck = msg_pool_pck<T0, T1, T2, T3>();
		quit_guard qg(this);
		msgPck->lock(this);
		if (msgPck->_next)
		{
			msgPck->_next->lock(this);
			clear_msg_list<T0, T1, T2, T3>(msgPck->_next);
			msgPck->_next->unlock(this);
			msgPck->_next.reset();
		}
		if (!msgPck->_msgPump)
		{
			msgPck->_msgPump = pump_type::make(shared_from_this());
		}
		auto msgPump = msgPck->_msgPump;
		auto msgPool = msgPck->_msgPool;
		if (msgPool)
		{
			msgPump->connect(send<pump_handler>(msgPool->_strand, [&msgPck]()->pump_handler
			{
				return msgPck->_msgPool->connect_pump(msgPck->_msgPump);
			}));
		}
		else
		{
			msgPump->clear();
		}
		msgPck->unlock(this);
		msg_pump_handle<T0, T1, T2, T3> mh;
		mh._handle = msgPump.get();
		return mh;
	}

	template <typename T0, typename T1, typename T2>
	msg_pump_handle<T0, T1, T2> connect_msg_pump()
	{
		return connect_msg_pump<T0, T1, T2, void>();
	}

	template <typename T0, typename T1>
	msg_pump_handle<T0, T1> connect_msg_pump()
	{
		return connect_msg_pump<T0, T1, void, void>();
	}

	template <typename T0>
	msg_pump_handle<T0> connect_msg_pump()
	{
		return connect_msg_pump<T0, void, void, void>();
	}

	msg_pump_handle<> connect_msg_pump();
private:
	template <typename PUMP, typename DST>
	bool _timed_pump_msg(const PUMP& pump, DST& dstRec, int tm, bool checkDis)
	{
		assert(pump->_hostActor && pump->_hostActor->self_id() == self_id());
		if (!pump->read_msg(dstRec))
		{
			if (checkDis && pump->isDisconnected())
			{
				pump->_waiting = false;
				pump->_dstRec = NULL;
				throw pump_disconnected_exception();
			}
			if (0 != tm)
			{
				pump->_checkDis = checkDis;
				bool timed = false;
				if (tm > 0)
				{
					delay_trig(tm, [this, &timed]
					{
						if (!_quited)
						{
							timed = true;
							pull_yield();
						}
					});
				}
				push_yield();
				if (!timed)
				{
					if (tm > 0)
					{
						cancel_delay_trig();
					}
					if (pump->_checkDis)
					{
						assert(checkDis);
						pump->_checkDis = false;
						throw pump_disconnected_exception();
					}
					return true;
				}
			}
			pump->_checkDis = false;
			pump->_waiting = false;
			pump->_dstRec = NULL;
			return false;
		}
		return true;
	}

	template <typename PUMP, typename DST>
	bool _try_pump_msg(const PUMP& pump, DST& dstRec, bool checkDis)
	{
		assert(pump->_hostActor && pump->_hostActor->self_id() == self_id());
		if (!pump->try_read(dstRec))
		{
			if (checkDis && pump->isDisconnected())
			{
				throw pump_disconnected_exception();
			}
			return false;
		}
		return true;
	}

	template <typename PUMP, typename DST>
	void _pump_msg(const PUMP& pump, DST& dstRec, bool checkDis)
	{
		assert(pump->_hostActor && pump->_hostActor->self_id() == self_id());
		if (!pump->read_msg(dstRec))
		{
			if (checkDis && pump->isDisconnected())
			{
				pump->_waiting = false;
				pump->_dstRec = NULL;
				throw pump_disconnected_exception();
			}
			pump->_checkDis = checkDis;
			push_yield();
			if (pump->_checkDis)
			{
				assert(checkDis);
				pump->_checkDis = false;
				throw pump_disconnected_exception();
			}
		}
	}
public:

	/*!
	@brief 从消息泵中提取消息
	@param tm 超时时间
	@param checkDis 检测是否被断开连接，是就抛出 pump_disconnected_exception 异常
	@return 超时完成返回false，成功取到消息返回true
	*/
	template <typename T0, typename T1, typename T2, typename T3, typename DT0, typename DT1, typename DT2, typename DT3>
	__yield_interrupt bool timed_pump_msg(int tm, const msg_pump_handle<T0, T1, T2, T3>& pump, DT0& r0, DT1& r1, DT2& r2, DT3& r3, bool checkDis = false)
	{
		assert_enter();
		dst_receiver_buff<TYPE_PIPE(T0), TYPE_PIPE(T1), TYPE_PIPE(T2), TYPE_PIPE(T3)>::dst_buff dstBuff;
		dst_receiver_buff<TYPE_PIPE(T0), TYPE_PIPE(T1), TYPE_PIPE(T2), TYPE_PIPE(T3)> dstRec(dstBuff);
		if (_timed_pump_msg(pump, dstRec, tm, checkDis))
		{
			msg_param<TYPE_PIPE(T0), TYPE_PIPE(T1), TYPE_PIPE(T2), TYPE_PIPE(T3)>& msg = dstBuff.get();
			r0 = std::move(msg._res0);
			r1 = std::move(msg._res1);
			r2 = std::move(msg._res2);
			r3 = std::move(msg._res3);
			return true;
		}
		return false;
	}

	template <typename T0, typename T1, typename T2, typename DT0, typename DT1, typename DT2>
	__yield_interrupt bool timed_pump_msg(int tm, const msg_pump_handle<T0, T1, T2>& pump, DT0& r0, DT1& r1, DT2& r2, bool checkDis = false)
	{
		assert_enter();
		dst_receiver_buff<TYPE_PIPE(T0), TYPE_PIPE(T1), TYPE_PIPE(T2)>::dst_buff dstBuff;
		dst_receiver_buff<TYPE_PIPE(T0), TYPE_PIPE(T1), TYPE_PIPE(T2)> dstRec(dstBuff);
		if (_timed_pump_msg(pump, dstRec, tm, checkDis))
		{
			msg_param<TYPE_PIPE(T0), TYPE_PIPE(T1), TYPE_PIPE(T2)>& msg = dstBuff.get();
			r0 = std::move(msg._res0);
			r1 = std::move(msg._res1);
			r2 = std::move(msg._res2);
			return true;
		}
		return false;
	}

	template <typename T0, typename T1, typename DT0, typename DT1>
	__yield_interrupt bool timed_pump_msg(int tm, const msg_pump_handle<T0, T1>& pump, DT0& r0, DT1& r1, bool checkDis = false)
	{
		assert_enter();
		dst_receiver_buff<TYPE_PIPE(T0), TYPE_PIPE(T1)>::dst_buff dstBuff;
		dst_receiver_buff<TYPE_PIPE(T0), TYPE_PIPE(T1)> dstRec(dstBuff);
		if (_timed_pump_msg(pump, dstRec, tm, checkDis))
		{
			msg_param<TYPE_PIPE(T0), TYPE_PIPE(T1)>& msg = dstBuff.get();
			r0 = std::move(msg._res0);
			r1 = std::move(msg._res1);
			return true;
		}
		return false;
	}

	template <typename T0, typename DT0>
	__yield_interrupt bool timed_pump_msg(int tm, const msg_pump_handle<T0>& pump, DT0& r0, bool checkDis = false)
	{
		assert_enter();
		dst_receiver_buff<TYPE_PIPE(T0)>::dst_buff dstBuff;
		dst_receiver_buff<TYPE_PIPE(T0)> dstRec(dstBuff);
		if (_timed_pump_msg(pump, dstRec, tm, checkDis))
		{
			msg_param<TYPE_PIPE(T0)>& msg = dstBuff.get();
			r0 = std::move(msg._res0);
			return true;
		}
		return false;
	}

	template <typename T0>
	__yield_interrupt bool timed_pump_msg(int tm, const msg_pump_handle<T0>& pump, stack_obj<T0>& r0, bool checkDis = false)
	{
		assert_enter();
		stack_dst_receiver<T0> dstRec(r0);
		if (_timed_pump_msg(pump, dstRec, tm, checkDis))
		{
			dstRec._has = false;
			return true;
		}
		return false;
	}

	__yield_interrupt bool timed_pump_msg(int tm, const msg_pump_handle<>& pump, bool checkDis = false);

	template <typename T0, typename T1, typename T2, typename T3, typename Handler>
	__yield_interrupt bool timed_pump_msg(int tm, const msg_pump_handle<T0, T1, T2, T3>& pump, const Handler& h, bool checkDis = false)
	{
		assert_enter();
		dst_receiver_buff<TYPE_PIPE(T0), TYPE_PIPE(T1), TYPE_PIPE(T2), TYPE_PIPE(T3)>::dst_buff dstBuff;
		dst_receiver_buff<TYPE_PIPE(T0), TYPE_PIPE(T1), TYPE_PIPE(T2), TYPE_PIPE(T3)> dstRec(dstBuff);
		if (_timed_pump_msg(pump, dstRec, tm, checkDis))
		{
			msg_param<TYPE_PIPE(T0), TYPE_PIPE(T1), TYPE_PIPE(T2), TYPE_PIPE(T3)>& msg = dstBuff.get();
			h(msg._res0, msg._res1, msg._res2, msg._res3);
			return true;
		}
		return false;
	}

	template <typename T0, typename T1, typename T2, typename Handler>
	__yield_interrupt bool timed_pump_msg(int tm, const msg_pump_handle<T0, T1, T2>& pump, const Handler& h, bool checkDis = false)
	{
		assert_enter();
		dst_receiver_buff<TYPE_PIPE(T0), TYPE_PIPE(T1), TYPE_PIPE(T2)>::dst_buff dstBuff;
		dst_receiver_buff<TYPE_PIPE(T0), TYPE_PIPE(T1), TYPE_PIPE(T2)> dstRec(dstBuff);
		if (_timed_pump_msg(pump, dstRec, tm, checkDis))
		{
			msg_param<TYPE_PIPE(T0), TYPE_PIPE(T1), TYPE_PIPE(T2)>& msg = dstBuff.get();
			h(msg._res0, msg._res1, msg._res2);
			return true;
		}
		return false;
	}

	template <typename T0, typename T1, typename Handler>
	__yield_interrupt bool timed_pump_msg(int tm, const msg_pump_handle<T0, T1>& pump, const Handler& h, bool checkDis = false)
	{
		assert_enter();
		dst_receiver_buff<TYPE_PIPE(T0), TYPE_PIPE(T1)>::dst_buff dstBuff;
		dst_receiver_buff<TYPE_PIPE(T0), TYPE_PIPE(T1)> dstRec(dstBuff);
		if (_timed_pump_msg(pump, dstRec, tm, checkDis))
		{
			msg_param<TYPE_PIPE(T0), TYPE_PIPE(T1)>& msg = dstBuff.get();
			h(msg._res0, msg._res1);
			return true;
		}
		return false;
	}

	template <typename T0, typename Handler>
	__yield_interrupt bool timed_pump_msg(int tm, const msg_pump_handle<T0>& pump, const Handler& h, bool checkDis = false)
	{
		assert_enter();
		dst_receiver_buff<TYPE_PIPE(T0)>::dst_buff dstBuff;
		dst_receiver_buff<TYPE_PIPE(T0)> dstRec(dstBuff);
		if (_timed_pump_msg(pump, dstRec, tm, checkDis))
		{
			msg_param<TYPE_PIPE(T0)>& msg = dstBuff.get();
			h(msg._res0);
			return true;
		}
		return false;
	}

	template <typename Handler>
	__yield_interrupt bool timed_pump_msg(int tm, const msg_pump_handle<>& pump, const Handler& h, bool checkDis = false)
	{
		assert_enter();
		if (timed_pump_msg(tm, pump, checkDis))
		{
			h();
			return true;
		}
		return false;
	}

	/*!
	@brief 从消息泵中提取消息
	*/
	template <typename T0, typename T1, typename T2, typename T3, typename DT0, typename DT1, typename DT2, typename DT3>
	__yield_interrupt void pump_msg(const msg_pump_handle<T0, T1, T2, T3>& pump, DT0& r0, DT1& r1, DT2& r2, DT3& r3, bool checkDis = false)
	{
		timed_pump_msg(-1, pump, r0, r1, r2, r3, checkDis);
	}

	template <typename T0, typename T1, typename T2, typename DT0, typename DT1, typename DT2>
	__yield_interrupt void pump_msg(const msg_pump_handle<T0, T1, T2>& pump, DT0& r0, DT1& r1, DT2& r2, bool checkDis = false)
	{
		timed_pump_msg(-1, pump, r0, r1, r2, checkDis);
	}

	template <typename T0, typename T1, typename DT0, typename DT1>
	__yield_interrupt void pump_msg(const msg_pump_handle<T0, T1>& pump, DT0& r0, DT1& r1, bool checkDis = false)
	{
		timed_pump_msg(-1, pump, r0, r1, checkDis);
	}

	template <typename T0, typename DT0>
	__yield_interrupt void pump_msg(const msg_pump_handle<T0>& pump, DT0& r0, bool checkDis = false)
	{
		timed_pump_msg(-1, pump, r0, checkDis);
	}

	template <typename T0>
	__yield_interrupt T0 pump_msg(const msg_pump_handle<T0>& pump, bool checkDis = false)
	{
		assert_enter();
		dst_receiver_buff<TYPE_PIPE(T0)>::dst_buff dstBuff;
		dst_receiver_buff<TYPE_PIPE(T0)> dstRec(dstBuff);
		_pump_msg(pump, dstRec, checkDis);
		return std::move((RM_REF(T0)&)dstBuff.get()._res0);
	}

	__yield_interrupt void pump_msg(const msg_pump_handle<>& pump, bool checkDis = false);

	template <typename T0, typename T1, typename T2, typename T3, typename Handler>
	__yield_interrupt void pump_msg(const msg_pump_handle<T0, T1, T2, T3>& pump, const Handler& h, bool checkDis = false)
	{
		timed_pump_msg<T0, T1, T2, T3>(-1, pump, h, checkDis);
	}

	template <typename T0, typename T1, typename T2, typename Handler>
	__yield_interrupt void pump_msg(const msg_pump_handle<T0, T1, T2>& pump, const Handler& h, bool checkDis = false)
	{
		timed_pump_msg<T0, T1, T2>(-1, pump, h, checkDis);
	}

	template <typename T0, typename T1, typename Handler>
	__yield_interrupt void pump_msg(const msg_pump_handle<T0, T1>& pump, const Handler& h, bool checkDis = false)
	{
		timed_pump_msg<T0, T1>(-1, pump, h, checkDis);
	}

	template <typename T0, typename Handler>
	__yield_interrupt void pump_msg(const msg_pump_handle<T0>& pump, const Handler& h, bool checkDis = false)
	{
		timed_pump_msg<T0>(-1, pump, h, checkDis);
	}

	template <typename Handler>
	__yield_interrupt void pump_msg(const msg_pump_handle<>& pump, const Handler& h, bool checkDis = false)
	{
		timed_pump_msg(-1, pump, h, checkDis);
	}

	/*!
	@brief 尝试从消息泵中提取消息
	*/
	template <typename T0, typename T1, typename T2, typename T3, typename DT0, typename DT1, typename DT2, typename DT3>
	__yield_interrupt bool try_pump_msg(const msg_pump_handle<T0, T1, T2, T3>& pump, DT0& r0, DT1& r1, DT2& r2, DT3& r3, bool checkDis = false)
	{
		assert_enter();
		dst_receiver_buff<TYPE_PIPE(T0), TYPE_PIPE(T1), TYPE_PIPE(T2), TYPE_PIPE(T3)>::dst_buff dstBuff;
		dst_receiver_buff<TYPE_PIPE(T0), TYPE_PIPE(T1), TYPE_PIPE(T2), TYPE_PIPE(T3)> dstRec(dstBuff);
		if (_try_pump_msg(pump, dstRec, checkDis))
		{
			msg_param<TYPE_PIPE(T0), TYPE_PIPE(T1), TYPE_PIPE(T2), TYPE_PIPE(T3)>& msg = dstBuff.get();
			r0 = std::move(msg._res0);
			r1 = std::move(msg._res1);
			r2 = std::move(msg._res2);
			r3 = std::move(msg._res3);
			return true;
		}
		return false;
	}

	template <typename T0, typename T1, typename T2, typename DT0, typename DT1, typename DT2>
	__yield_interrupt bool try_pump_msg(const msg_pump_handle<T0, T1, T2>& pump, DT0& r0, DT1& r1, DT2& r2, bool checkDis = false)
	{
		assert_enter();
		dst_receiver_buff<TYPE_PIPE(T0), TYPE_PIPE(T1), TYPE_PIPE(T2)>::dst_buff dstBuff;
		dst_receiver_buff<TYPE_PIPE(T0), TYPE_PIPE(T1), TYPE_PIPE(T2)> dstRec(dstBuff);
		if (_try_pump_msg(pump, dstRec, checkDis))
		{
			msg_param<TYPE_PIPE(T0), TYPE_PIPE(T1), TYPE_PIPE(T2)>& msg = dstBuff.get();
			r0 = std::move(msg._res0);
			r1 = std::move(msg._res1);
			r2 = std::move(msg._res2);
			return true;
		}
		return false;
	}

	template <typename T0, typename T1, typename DT0, typename DT1>
	__yield_interrupt bool try_pump_msg(const msg_pump_handle<T0, T1>& pump, DT0& r0, DT1& r1, bool checkDis = false)
	{
		assert_enter();
		dst_receiver_buff<TYPE_PIPE(T0), TYPE_PIPE(T1)>::dst_buff dstBuff;
		dst_receiver_buff<TYPE_PIPE(T0), TYPE_PIPE(T1)> dstRec(dstBuff);
		if (_try_pump_msg(pump, dstRec, checkDis))
		{
			msg_param<TYPE_PIPE(T0), TYPE_PIPE(T1)>& msg = dstBuff.get();
			r0 = std::move(msg._res0);
			r1 = std::move(msg._res1);
			return true;
		}
		return false;
	}

	template <typename T0, typename DT0>
	__yield_interrupt bool try_pump_msg(const msg_pump_handle<T0>& pump, DT0& r0, bool checkDis = false)
	{
		assert_enter();
		dst_receiver_buff<TYPE_PIPE(T0)>::dst_buff dstBuff;
		dst_receiver_buff<TYPE_PIPE(T0)> dstRec(dstBuff);
		if (_try_pump_msg(pump, dstRec, checkDis))
		{
			msg_param<TYPE_PIPE(T0)>& msg = dstBuff.get();
			r0 = std::move(msg._res0);
			return true;
		}
		return false;
	}

	template <typename T0>
	__yield_interrupt bool try_pump_msg(const msg_pump_handle<T0>& pump, stack_obj<T0>& r0, bool checkDis = false)
	{
		assert_enter();
		stack_dst_receiver<T0> dstRec(r0);
		if (_try_pump_msg(pump, dstRec, checkDis))
		{
			dstRec._has = false;
			return true;
		}
		return false;
	}

	__yield_interrupt bool try_pump_msg(const msg_pump_handle<>& pump, bool checkDis = false);

	template <typename T0, typename T1, typename T2, typename T3, typename Handler>
	__yield_interrupt bool try_pump_msg(const msg_pump_handle<T0, T1, T2, T3>& pump, const Handler& h, bool checkDis = false)
	{
		assert_enter();
		dst_receiver_buff<TYPE_PIPE(T0), TYPE_PIPE(T1), TYPE_PIPE(T2), TYPE_PIPE(T3)>::dst_buff dstBuff;
		dst_receiver_buff<TYPE_PIPE(T0), TYPE_PIPE(T1), TYPE_PIPE(T2), TYPE_PIPE(T3)> dstRec(dstBuff);
		if (_try_pump_msg(pump, dstRec, checkDis))
		{
			msg_param<TYPE_PIPE(T0), TYPE_PIPE(T1), TYPE_PIPE(T2), TYPE_PIPE(T3)>& msg = dstBuff.get();
			h(msg._res0, msg._res1, msg._res2, msg._res3);
			return true;
		}
		return false;
	}

	template <typename T0, typename T1, typename T2, typename Handler>
	__yield_interrupt bool try_pump_msg(const msg_pump_handle<T0, T1, T2>& pump, const Handler& h, bool checkDis = false)
	{
		assert_enter();
		dst_receiver_buff<TYPE_PIPE(T0), TYPE_PIPE(T1), TYPE_PIPE(T2)>::dst_buff dstBuff;
		dst_receiver_buff<TYPE_PIPE(T0), TYPE_PIPE(T1), TYPE_PIPE(T2)> dstRec(dstBuff);
		if (_try_pump_msg(pump, dstRec, checkDis))
		{
			msg_param<TYPE_PIPE(T0), TYPE_PIPE(T1), TYPE_PIPE(T2)>& msg = dstBuff.get();
			h(msg._res0, msg._res1, msg._res2);
			return true;
		}
		return false;
	}

	template <typename T0, typename T1, typename Handler>
	__yield_interrupt bool try_pump_msg(const msg_pump_handle<T0, T1>& pump, const Handler& h, bool checkDis = false)
	{
		assert_enter();
		dst_receiver_buff<TYPE_PIPE(T0), TYPE_PIPE(T1)>::dst_buff dstBuff;
		dst_receiver_buff<TYPE_PIPE(T0), TYPE_PIPE(T1)> dstRec(dstBuff);
		if (_try_pump_msg(pump, dstRec, checkDis))
		{
			msg_param<TYPE_PIPE(T0), TYPE_PIPE(T1)>& msg = dstBuff.get();
			h(msg._res0, msg._res1);
			return true;
		}
		return false;
	}

	template <typename T0, typename Handler>
	__yield_interrupt bool try_pump_msg(const msg_pump_handle<T0>& pump, const Handler& h, bool checkDis = false)
	{
		assert_enter();
		dst_receiver_buff<TYPE_PIPE(T0)>::dst_buff dstBuff;
		dst_receiver_buff<TYPE_PIPE(T0)> dstRec(dstBuff);
		if (_try_pump_msg(pump, dstRec, checkDis))
		{
			msg_param<TYPE_PIPE(T0)>& msg = dstBuff.get();
			h(msg._res0);
			return true;
		}
		return false;
	}
public:
	/*!
	@brief 查询当前消息由谁代理
	*/
	template <typename T0, typename T1, typename T2, typename T3>
	actor_handle msg_agent_handle(actor_handle buddyActor)
	{
		typedef std::shared_ptr<msg_pool_status::pck<T0, T1, T2, T3>> pck_type;

		quit_guard qg(this);
		auto msgPck = send<pck_type>(buddyActor->self_strand(), [&buddyActor]()->pck_type
		{
			if (!buddyActor->is_quited())
			{
				return buddyActor->msg_pool_pck<T0, T1, T2, T3>(false);
			}
			return pck_type();
		});
		if (msgPck)
		{
			std::shared_ptr<msg_pool_status::pck<T0, T1, T2, T3>> uStack[16];
			size_t stackl = 0;
			msgPck->lock(this);
			auto pckIt = msgPck;
			while (true)
			{
				if (pckIt->_next)
				{
					assert(stackl < 15);
					pckIt = pckIt->_next;
					uStack[stackl++] = pckIt;
					pckIt->lock(this);
				}
				else
				{
					actor_handle r;
					if (!pckIt->is_closed())
					{
						r = pckIt->_hostActor->shared_from_this();
					}
					while (stackl)
					{
						uStack[--stackl]->unlock(this);
					}
					msgPck->unlock(this);
					return r;
				}
			}
		}
		return actor_handle();
	}

	template <typename T0, typename T1, typename T2>
	actor_handle msg_agent_handle(const actor_handle& buddyActor)
	{
		return msg_agent_handle<T0, T1, T2, void>(buddyActor);
	}

	template <typename T0, typename T1>
	actor_handle msg_agent_handle(const actor_handle& buddyActor)
	{
		return msg_agent_handle<T0, T1, void, void>(buddyActor);
	}

	template <typename T0>
	actor_handle msg_agent_handle(const actor_handle& buddyActor)
	{
		return msg_agent_handle<T0, void, void, void>(buddyActor);
	}

	actor_handle msg_agent_handle(const actor_handle& buddyActor);
public:
	/*!
	@brief 获取当前代码运行在哪个Actor下
	@return 不运行在Actor下或没启动 CHECK_SELF 返回NULL
	*/
	static my_actor* self_actor();

	/*!
	@brief 检测当前是否在该Actor内运行
	*/
	void check_self();

	/*!
	@brief 测试当前下的Actor栈是否安全
	*/
	void check_stack();

	/*!
	@brief 获取当前Actor剩余安全栈空间
	*/
	size_t stack_free_space();

	/*!
	@brief 获取当前Actor调度器
	*/
	shared_strand self_strand();

	/*!
	@brief 返回本对象的智能指针
	*/
	actor_handle shared_from_this();

	/*!
	@brief 获取当前ActorID号
	*/
	id self_id();

	/*!
	@brief 获取Actor切换计数
	*/
	size_t yield_count();

	/*!
	@brief Actor切换计数清零
	*/
	void reset_yield();

	/*!
	@brief 开始运行建立好的Actor
	*/
	void notify_run();

	/*!
	@brief 强制退出该Actor，不可滥用，有可能会造成资源泄漏
	*/
	void notify_quit();

	/*!
	@brief 强制退出该Actor，完成后回调
	*/
	void notify_quit(const std::function<void ()>& h);

	/*!
	@brief Actor是否已经开始运行
	*/
	bool is_started();

	/*!
	@brief Actor是否已经退出
	*/
	bool is_quited();

	/*!
	@brief 是否是强制退出（在Actor确认退出后调用）
	*/
	bool is_force();

	/*!
	@brief 是否在Actor中
	*/
	bool in_actor();

	/*!
	@brief lock_quit后检测是否收到退出消息
	*/
	bool quit_msg();

	/*!
	@brief 锁定当前Actor，暂时不让强制退出
	*/
	void lock_quit();

	/*!
	@brief 解除退出锁定
	*/
	void unlock_quit();

	/*!
	@brief 是否锁定了退出
	*/
	bool is_locked_quit();

	/*!
	@brief 暂停Actor
	*/
	void notify_suspend();
	void notify_suspend(const std::function<void ()>& h);

	/*!
	@brief 恢复已暂停Actor
	*/
	void notify_resume();
	void notify_resume(const std::function<void ()>& h);

	/*!
	@brief 切换挂起/非挂起状态
	*/
	void switch_pause_play();
	void switch_pause_play(const std::function<void (bool)>& h);

	/*!
	@brief 等待Actor退出，在Actor所依赖的ios无关线程中使用
	*/
	void outside_wait_quit();

	/*!
	@brief 添加一个Actor结束回调
	*/
	void append_quit_callback(const std::function<void ()>& h);

	/*!
	@brief 启动一堆Actor
	*/
	void actors_start_run(const list<actor_handle>& anotherActors);

	/*!
	@brief 强制退出另一个Actor，并且等待完成
	*/
	__yield_interrupt void actor_force_quit(actor_handle anotherActor);
	__yield_interrupt void actors_force_quit(const list<actor_handle>& anotherActors);

	/*!
	@brief 等待另一个Actor结束后返回
	*/
	__yield_interrupt void actor_wait_quit(actor_handle anotherActor);
	__yield_interrupt void actors_wait_quit(const list<actor_handle>& anotherActors);
	__yield_interrupt bool timed_actor_wait_quit(int tm, actor_handle anotherActor);

	/*!
	@brief 挂起另一个Actor，等待其所有子Actor都调用后才返回
	*/
	__yield_interrupt void actor_suspend(actor_handle anotherActor);
	__yield_interrupt void actors_suspend(const list<actor_handle>& anotherActors);

	/*!
	@brief 恢复另一个Actor，等待其所有子Actor都调用后才返回
	*/
	__yield_interrupt void actor_resume(actor_handle anotherActor);
	__yield_interrupt void actors_resume(const list<actor_handle>& anotherActors);

	/*!
	@brief 对另一个Actor进行挂起/恢复状态切换
	@return 都已挂起返回true，否则false
	*/
	__yield_interrupt bool actor_switch(actor_handle anotherActor);
	__yield_interrupt bool actors_switch(const list<actor_handle>& anotherActors);

	void assert_enter();
private:
	template <typename H>
	void timeout(int ms, H&& h)
	{
		assert_enter();
		assert(ms > 0);
		assert(_timerState._timerCompleted);
		_timerState._timerTime = (long long)ms * 1000;
		_timerState._timerCb = TRY_MOVE(h);
		_timerState._timerStampBegin = get_tick_us();
		_timerState._timerCompleted = false;
		_timerState._timerHandle = _timer->timeout(_timerState._timerTime, shared_from_this());
	}

	void timeout_handler();
	void cancel_timer();
	void suspend_timer();
	void resume_timer();
	void start_run();
	void force_quit(const std::function<void ()>& h);
	void suspend(const std::function<void ()>& h);
	void resume(const std::function<void ()>& h);
	void suspend();
	void resume();
	void run_one();
	void pull_yield();
	void pull_yield_as_mutex();
	void push_yield();
	void push_yield_as_mutex();
	void force_quit_cb_handler();
	void exit_callback();
	void child_suspend_cb_handler();
	void child_resume_cb_handler();
public:
#ifdef CHECK_ACTOR_STACK
	bool _checkStackFree;//是否检测堆栈过多
	std::shared_ptr<list<stack_line_info>> _createStack;//当前Actor创建时的调用堆栈
#endif
private:
	void* _actorPull;///<Actor中断点恢复
	void* _actorPush;///<Actor中断点
	void* _stackTop;///<Actor栈顶
	id _selfID;///<ActorID
	size_t _stackSize;///<Actor栈大小
	shared_strand _strand;///<Actor调度器
	bool _inActor;///<当前正在Actor内部执行标记
	bool _started;///<已经开始运行的标记
	bool _quited;///<_mainFunc已经执行完毕
	bool _exited;///<完全退出
	bool _suspended;///<Actor挂起标记
	bool _hasNotify;///<当前Actor挂起，有外部触发准备进入Actor标记
	bool _isForce;///<是否是强制退出的标记，成功调用了force_quit
	bool _notifyQuited;///<当前Actor被锁定后，收到退出消息
	size_t _lockQuit;///<锁定当前Actor，如果当前接收到退出消息，暂时不退，等到解锁后退出
	size_t _yieldCount;//yield计数
	size_t _childOverCount;///<子Actor退出时计数
	size_t _childSuspendResumeCount;///<子Actor挂起/恢复计数
	main_func _mainFunc;///<Actor入口
	msg_list_shared_alloc<suspend_resume_option> _suspendResumeQueue;///<挂起/恢复操作队列
	msg_list_shared_alloc<std::function<void()> > _exitCallback;///<Actor结束后的回调函数，强制退出返回false，正常退出返回true
	msg_list_shared_alloc<std::function<void()> > _quitHandlerList;///<Actor退出时强制调用的函数，后注册的先执行
	msg_list_shared_alloc<actor_handle> _childActorList;///<子Actor集合，子Actor都退出后，父Actor才能退出
	msg_pool_status _msgPoolStatus;//消息池列表
	actor_handle _parentActor;///<父Actor，子Actor都析构后，父Actor才能析构
	timer_state _timerState;///<定时器状态
	actor_timer* _timer;///<定时器
	std::weak_ptr<my_actor> _weakThis;
#ifdef CHECK_SELF
	msg_map<void*, my_actor*>::iterator _btIt;
	msg_map<void*, my_actor*>::iterator _topIt;
#endif
	static msg_list_shared_alloc<my_actor::suspend_resume_option>::shared_node_alloc _suspendResumeQueueAll;
	static msg_list_shared_alloc<std::function<void()> >::shared_node_alloc _quitExitCallbackAll;
	static msg_list_shared_alloc<actor_handle>::shared_node_alloc _childActorListAll;
};

#endif