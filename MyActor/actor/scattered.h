#ifndef __SCATTERED_H
#define __SCATTERED_H

#include <algorithm>
#include <functional>
#include <memory>
#include <list>
#include "try_move.h"

#define BOND_NAME(__NAMEL__, __NAMER__) __NAMEL__ ## __NAMER__

#ifdef _MSC_VER
#ifdef _WIN64
#pragma comment(lib, BOND_NAME(__FILE__, "./../lib/fcontext_x64.lib"))
#else
#pragma comment(lib, BOND_NAME(__FILE__, "./../lib/fcontext_x86.lib"))
#endif // _WIN64
#endif

#if (WIN32 && __GNUG__)
#ifdef ENABLE_GCC_SEH
#include <seh.h>
#else
#define __seh_try
#define __seh_except(...) if (false)
#define __seh_end_except
#endif
#endif

#ifdef _MSC_VER
#define FRIEND_SHARED_PTR(__T__)\
	friend std::shared_ptr<__T__>;\
	friend std::_Ref_count<__T__>;\
	friend std::_Ref_count_obj<__T__>;
#elif __GNUG__
#define FRIEND_SHARED_PTR(__T__)\
	friend std::shared_ptr<__T__>;\
	friend std::__shared_count<__gnu_cxx::__default_lock_policy>;\
	friend std::_Sp_counted_ptr<__T__*, __gnu_cxx::__default_lock_policy>;\
	friend __gnu_cxx::new_allocator<__T__>;
#endif

#ifdef _MSC_VER
#if (_MSC_VER < 1900)
#define __disable_noexcept
#else
#define __disable_noexcept noexcept(false)
#endif
#elif __GNUG__
#define __disable_noexcept noexcept(false)
#endif

#define _BOND_ID(__P__, __L__) BOND_NAME(__P__, __L__)
#define BOND_LINE(__P__) _BOND_ID(__P__, __LINE__)
#define BOND_COUNT(__P__) _BOND_ID(__P__, __COUNTER__)

#define _param_pck1()
#define _param_pck2(__p1__) __p1__
#define _param_pck3(__p1__, __p2__) __p1__, __p2__
#define _param_pck4(__p1__, __p2__, __p3__) __p1__, __p2__, __p3__
#define _param_pck5(__p1__, __p2__, __p3__, __p4__) __p1__, __p2__, __p3__, __p4__
#define _param_pck6(__p1__, __p2__, __p3__, __p4__, __p5__) __p1__, __p2__, __p3__, __p4__, __p5__
#define _param_pck7(__p1__, __p2__, __p3__, __p4__, __p5__, __p6__) __p1__, __p2__, __p3__, __p4__, __p5__, __p6__

#define _param_right_pck1()
#define _param_right_pck2(__p1__) , __p1__
#define _param_right_pck3(__p1__, __p2__) , __p1__, __p2__
#define _param_right_pck4(__p1__, __p2__, __p3__) , __p1__, __p2__, __p3__
#define _param_right_pck5(__p1__, __p2__, __p3__, __p4__) , __p1__, __p2__, __p3__, __p4__
#define _param_right_pck6(__p1__, __p2__, __p3__, __p4__, __p5__) , __p1__, __p2__, __p3__, __p4__, __p5__
#define _param_right_pck7(__p1__, __p2__, __p3__, __p4__, __p5__, __p6__) , __p1__, __p2__, __p3__, __p4__, __p5__, __p6__

#define _param_left_pck1()
#define _param_left_pck2(__p1__) __p1__,
#define _param_left_pck3(__p1__, __p2__) __p1__, __p2__,
#define _param_left_pck4(__p1__, __p2__, __p3__) __p1__, __p2__, __p3__,
#define _param_left_pck5(__p1__, __p2__, __p3__, __p4__) __p1__, __p2__, __p3__, __p4__,
#define _param_left_pck6(__p1__, __p2__, __p3__, __p4__, __p5__) __p1__, __p2__, __p3__, __p4__, __p5__,
#define _param_left_pck7(__p1__, __p2__, __p3__, __p4__, __p5__, __p6__) __p1__, __p2__, __p3__, __p4__, __p5__, __p6__,

#define _option_pck1()
#define _option_pck2(__p1__) __p1__;
#define _option_pck3(__p1__, __p2__) __p1__; __p2__;
#define _option_pck4(__p1__, __p2__, __p3__) __p1__; __p2__; __p3__;
#define _option_pck5(__p1__, __p2__, __p3__, __p4__) __p1__; __p2__; __p3__; __p4__;
#define _option_pck6(__p1__, __p2__, __p3__, __p4__, __p5__) __p1__; __p2__; __p3__; __p4__; __p5__;
#define _option_pck7(__p1__, __p2__, __p3__, __p4__, __p5__, __p6__) __p1__; __p2__; __p3__; __p4__; __p5__; __p6__;

#ifdef _MSC_VER
#define _param_pck(__pl__, ...) _BOND_LR__(_param_pck, _PP_NARG(__pl__, __VA_ARGS__))(__VA_ARGS__)
#define param_pck(...) _param_pck(__pl__, __VA_ARGS__)
#define _param_right_pck(__pl__, ...) _BOND_LR__(_param_right_pck, _PP_NARG(__pl__, __VA_ARGS__))(__VA_ARGS__)
#define param_right_pck(...) _param_right_pck(__pl__, __VA_ARGS__)
#define _param_left_pck(__pl__, ...) _BOND_LR__(_param_left_pck, _PP_NARG(__pl__, __VA_ARGS__))(__VA_ARGS__)
#define param_left_pck(...) _param_left_pck(__pl__, __VA_ARGS__)
#define _option_pck(__pl__, ...) _BOND_LR__(_option_pck, _PP_NARG(__pl__, __VA_ARGS__))(__VA_ARGS__)
#define option_pck(...) _option_pck(__pl__, __VA_ARGS__)
#elif __GNUG__
#define param_pck(...) _BOND_LR__(_param_pck, _PP_NARG(__pl__, __VA_ARGS__))(__VA_ARGS__)
#define param_right_pck(...) _BOND_LR__(_param_right_pck, _PP_NARG(__pl__, __VA_ARGS__))(__VA_ARGS__)
#define param_left_pck(...) _BOND_LR__(_param_left_pck, _PP_NARG(__pl__, __VA_ARGS__))(__VA_ARGS__)
#define option_pck(...) _BOND_LR__(_option_pck, _PP_NARG(__pl__, __VA_ARGS__))(__VA_ARGS__)
#endif

template <typename Handler>
struct BreakOfScope_
{
	BreakOfScope_(Handler& handler)
	:_handler(handler) {}

	~BreakOfScope_() __disable_noexcept
	{
		_handler();
	}

	Handler& _handler;
private:
	BreakOfScope_(const BreakOfScope_&) = delete;
	void operator =(const BreakOfScope_&) = delete;
};

template <typename Handler>
struct BreakOfScope2_
{
	typedef RM_CREF(Handler) handler_type;

	BreakOfScope2_(Handler& handler)
	:_handler(std::forward<Handler>(handler)) {}

	~BreakOfScope2_() __disable_noexcept
	{
		_handler();
	}

	handler_type _handler;
public:
	BreakOfScope2_(BreakOfScope2_&& s)
		:_handler(s._handler) { assert(false);}
private:
	BreakOfScope2_(const BreakOfScope2_&) = delete;
	void operator =(const BreakOfScope2_&) = delete;
};

struct MakeBreakOfScope2_
{
	template <typename Handler>
	BreakOfScope2_<Handler> operator -(Handler&& handler)
	{
		return BreakOfScope2_<Handler>(handler);
	}
};

//作用域退出时自动调用lambda
#define BREAK_OF_SCOPE_NAME(__name__, __handler__) \
	auto BOND_NAME(__raiiHandler, __name__) = [&]__handler__; \
	BreakOfScope_<decltype(BOND_NAME(__raiiHandler, __name__))> BOND_NAME(__raiiCall, __name__)(BOND_NAME(__raiiHandler, __name__))

#define BREAK_OF_SCOPE(__handler__) BREAK_OF_SCOPE_NAME(__COUNTER__, __handler__)
#define BREAK_OF_SCOPE_EXEC(...) BREAK_OF_SCOPE({ option_pck(__VA_ARGS__) });
#define break_of_scope_call auto BOND_COUNT(__raiiCall) = MakeBreakOfScope2_()-

//在Actor作用域内定时触发某个操作
#define HEARTBEAT_TRACE_NAME(__name__, __self__, __ms__, __handler__)\
	overlap_timer::timer_handle __name__; \
	__self__->self_strand()->over_timer()->interval(__ms__, __name__, [&]__handler__); \
	BREAK_OF_SCOPE_NAME(BOND_NAME(__heartbeatHandler_, __name__), { __self__->self_strand()->over_timer()->cancel(__name__); });

//在Actor作用域内定时触发某个操作
#define HEARTBEAT_TRACE(__self__, __ms__, __handler__) HEARTBEAT_TRACE_NAME(BOND_COUNT(__heartbeatTimer), __self__, __ms__, __handler__)
#define HEARTBEAT_EXEC(__self__, __ms__, ...) HEARTBEAT_TRACE(__self__, __ms__, { option_pck(__VA_ARGS__) });

//在Actor作用域内延时触发某个操作
#define DELAY_TRACE_NAME(__name__, __self__, __ms__, __handler__)\
	async_timer __name__ = __self__->self_strand()->make_timer(); \
	auto BOND_NAME(__delayHandler_, __name__) = [&]__handler__; \
	__name__->timeout(__ms__, wrap_ref_handler(BOND_NAME(__delayHandler_, __name__))); \
	BREAK_OF_SCOPE_NAME(BOND_NAME(__delayHandler_, __name__), { __name__->cancel(); });

//在Actor作用域内延时触发某个操作
#define DELAY_TRACE(__self__, __ms__, __handler__) DELAY_TRACE_NAME(BOND_COUNT(__delayTimer), __self__, __ms__, __handler__)
#define DELAY_EXEC(__self__, __ms__, ...)  DELAY_TRACE(__self__, __ms__, { option_pck(__VA_ARGS__) });

//在Actor作用域内延时触发某个操作
#define SELF_DELAY_TRACE(__self__, __ms__, __handler__)\
	auto BOND_LINE(__selfDelayHandler_) = [&]__handler__; \
	__self__->delay_trig(__ms__, wrap_ref_handler(BOND_LINE(__selfDelayHandler_))); \
	BREAK_OF_SCOPE_NAME(BOND_LINE(__selfDelayHandler_), { __self__->cancel_delay_trig(); });
#define SELF_DELAY_EXEC(__self__, __ms__, ...) SELF_DELAY_TRACE(__self__, __ms__, { option_pck(__VA_ARGS__) });

class ActorTimer_;
class ActorTimerFace_
{
	friend ActorTimer_;
	virtual void timeout_handler() = 0;
};

#if (_DEBUG || DEBUG)
#define DEBUG_OPERATION(__exp__)	__exp__
#else
#define DEBUG_OPERATION(__exp__)
#endif

#if (_DEBUG || DEBUG)

#define CHECK_EXCEPTION(__h, ...) try { (__h)(__VA_ARGS__); } catch (...) { assert(false); }

#define BEGIN_CHECK_EXCEPTION try {
#define END_CHECK_EXCEPTION } catch (...) { assert(false); }

#else

#define CHECK_EXCEPTION(__h, ...) (__h)(__VA_ARGS__)

#define BEGIN_CHECK_EXCEPTION
#define END_CHECK_EXCEPTION

#endif

//内存边界对齐
#define MEM_ALIGN(__o, __a) (((__o) + ((__a)-1)) & (0-(__a)))

//禁用对象拷贝
#define NONE_COPY(__T__) \
	private:\
	__T__(const __T__&) = delete; \
	void operator =(const __T__&) = delete;

//禁用对象赋值拷贝
#define NONE_TARN_COPY(__T__) \
	void operator =(const __T__&) = delete; \
	void operator =(__T__&&) = delete;

#ifdef _MSC_VER

#ifdef _WIN64
#define __space_align _declspec (align(8))
#elif _WIN32
#define __space_align _declspec (align(4))
#endif

#elif __GNUG__

#if (__x86_64__ || _ARM64)
#define __space_align __attribute__((aligned(8)))
#elif (__i386__ || _ARM32)
#define __space_align __attribute__((aligned(4)))
#endif

#endif

#define opt_space __space_align unsigned char

//////////////////////////////////////////////////////////////////////////

#define BEGIN_TRY_ do {\
	bool __catched = false; \
	try {

#define CATCH_FOR(__exp__) }\
	catch (__exp__&) { __catched = true; }\
	DEBUG_OPERATION(catch (...) { assert(false); })\
if (__catched) {

#define END_TRY_ }} while (false)

#define RUN_IN_STRAND(__host__, __strand__, ...) (__host__)->send(__strand__, [&] { option_pck(__VA_ARGS__) })

#define begin_RUN_IN_STRAND(__host__, __strand__) (__host__)->send(__strand__, [&] {
#define end_RUN_IN_STRAND() })

#define begin_ACTOR_RUN_IN_STRAND(__host__, __strand__) do {\
	my_actor::quit_guard __qg(__host__);\
	my_actor* const ___host = __host__;\
	actor_handle ___actor = my_actor::create(__strand__, [&](my_actor* __host__) {

#define end_ACTOR_RUN_IN_STRAND() });\
	___actor->run(); \
	___host->actor_wait_quit(___actor);\
} while (false)

#define RUN_IN_THREAD_STACK(__host__, ...) (__host__)->run_in_thread_stack([&] { option_pck(__VA_ARGS__) })

#define begin_RUN_IN_THREAD_STACK(__host__)	(__host__)->run_in_thread_stack([&] {
#define end_RUN_IN_THREAD_STACK() })

//////////////////////////////////////////////////////////////////////////

#define begin_CODE_GRINDER_AT(__host__, __dispatch_strand__) {\
	my_actor::quit_guard __qg(__host__); \
	shared_strand grinder_dispatcher = __dispatch_strand__; \
	(__host__)->run_child_complete(grinder_dispatcher, [&](my_actor* self) {

#define begin_CODE_GRINDER() begin_CODE_GRINDER_AT(self, self->self_strand())
#define code_grinder_BEGIN self->run_child_complete(grinder_dispatcher, [&](my_actor* self) {
#define code_grinder_END  }, MAX_STACKSIZE);
#define end_CODE_GRINDER() }, MAX_STACKSIZE);}

//////////////////////////////////////////////////////////////////////////

#define begin_CODE_CUT_AT(__host__, __dispatch_strand__) {\
	my_actor::quit_guard __qg(__host__); \
	my_actor* const ___host = __host__; \
	shared_strand cut_dispatcher = __dispatch_strand__; \
	___host->async_send(cut_dispatcher, [&] {

#define begin_CODE_CUT() begin_CODE_CUT_AT(self, self->self_strand())
#define CODE_CUT });___host->async_send(cut_dispatcher, [&] {
#define end_CODE_CUT() });}

/*!
@brief 这个类在测试消息传递时使用
*/
struct move_test
{
	struct count 
	{
		int _id;
		size_t _copyCount;
		size_t _moveCount;
		std::function<void(std::shared_ptr<count>)> _cb;
	};

	explicit move_test(int id);
	move_test();
	~move_test();
	move_test(int id, const std::function<void(std::shared_ptr<count>)>& cb);
	move_test(const move_test& s);
	move_test(move_test&& s);
	void operator=(const move_test& s);
	void operator=(move_test&& s);
	void reset();
	friend std::ostream& operator <<(std::ostream& out, const move_test& s);
	friend std::wostream& operator <<(std::wostream& out, const move_test& s);
	std::shared_ptr<count> _count;
	size_t _generation;
};

/*!
@brief 接受任何回调参数，但不处理
*/
struct any_handler
{
	template <typename... Args>
	void operator()(Args&&...){}
};

/*!
@brief 接受任何传值，但不处理
*/
struct any_accept
{
	template <typename Args>
	void operator=(Args&&){}
};

#if (_DEBUG || DEBUG)
#define debug_mark do{bool __mark = true;}while(0)
#define debug_ref(__ref_obj__) do{auto& __ref = __ref_obj__;}while(0)
#else
#define debug_mark do{}while(0)
#define debug_ref(__ref_obj__) do{}while(0)
#endif

/*!
@brief 指针类型转换
*/
template <typename Type>
Type* as_ptype(void* p)
{
	union { void* _ap_pvoid; Type* _as_ptype; } caster = { p };
	return caster._as_ptype;
}

/*!
@brief 任意指针类型转对象引用
*/
template <typename Type>
Type& as_ref(void* p)
{
	return *as_ptype<Type>(p);
}

/*!
@brief 启用高精度时钟
*/
void enable_high_resolution();

std::string get_time_string_us();
std::string get_time_string_ms();
std::string get_time_string_s();
std::string get_time_string_file_s();
void print_time_us();
void print_time_ms();
void print_time_s();
void print_time_us(std::ostream&);
void print_time_ms(std::ostream&);
void print_time_s(std::ostream&);
void print_time_us(std::wostream&);
void print_time_ms(std::wostream&);
void print_time_s(std::wostream&);

long long get_tick_us();
long long get_tick_ms();
int get_tick_s();
long long rel2abs_tick(int ms);
long long rel2abs_tick(long long us);

#ifdef _MSC_VER
extern "C" void* __fastcall get_sp();
extern "C" unsigned long long __fastcall cpu_tick();
#elif __GNUG__
void* get_sp();
unsigned long long cpu_tick();
#endif

/*!
@brief 根据类型，"计算"一个类型的hash_code，对比typeid(type).hash_code()
*/
template <typename... Types>
struct type_hash
{
#ifdef ENABLE_RTTI_HASH_CODE
	static size_t hash_code()
	{
#if (_WIN64 || __x86_64__ || _ARM64)
		return (typeid(std::tuple<Types...>).hash_code() << 8) >> 8;
#else
		return typeid(std::tuple<Types...>).hash_code();
#endif
	}
};
#else
	static size_t hash_code()
	{
		return (size_t)&_placeholder;
	}
private:
	static bool _placeholder;
};
template <typename... Types> bool type_hash<Types...>::_placeholder;
#endif

template <>
struct type_hash<>
{
	static size_t hash_code()
	{
		return 0;
	}
};

/*!
@brief 固定数组元素个数
*/
template <typename T, size_t N>
size_t fixed_array_length(T(&)[N])
{
	return N;
}

/*!
@brief 固定数组元素字节数
*/
template <typename T, size_t N>
size_t fixed_array_bytes(T(&)[N])
{
	return N*sizeof(T);
}

/*!
@brief 固定数组元素字节数
*/
template <typename T, size_t N>
size_t fixed_array_bytes(T(&)[N], size_t max_bytes)
{
	return N*sizeof(T) < max_bytes ? N*sizeof(T) : max_bytes;
}

/*!
@brief 清空std::function
*/
template <typename R, typename... Args>
inline void clear_function(std::function<R(Args...)>& f)
{
	f = std::function<R(Args...)>();
}

template <typename T>
struct ForwardCopy_
{
	typedef T&& type;
};

template <typename T>
struct ForwardCopy_<T&>
{
	typedef T type;
};

template <typename T>
struct ForwardCopy_<const T&>
{
	typedef T type;
};

/*!
@brief 将左值复制一份，右值不动
*/
template <typename T>
auto forward_copy(T&& p)->typename ForwardCopy_<T>::type
{
	return (T&&)p;
}

#define forward_copys1(p1) forward_copy(p1)
#define forward_copys2(p1,p2) forward_copy(p1),forward_copy(p2)
#define forward_copys3(p1,p2,p3) forward_copy(p1),forward_copy(p2),forward_copy(p3)
#define forward_copys4(p1,p2,p3,p4) forward_copy(p1),forward_copy(p2),forward_copy(p3),forward_copy(p4)
#define forward_copys5(p1,p2,p3,p4,p5) forward_copy(p1),forward_copy(p2),forward_copy(p3),forward_copy(p4),forward_copy(p5)
#define forward_copys6(p1,p2,p3,p4,p5,p6) forward_copy(p1),forward_copy(p2),forward_copy(p3),forward_copy(p4),forward_copy(p5),forward_copy(p6)
#define forward_copys7(p1,p2,p3,p4,p5,p6,p7) forward_copy(p1),forward_copy(p2),forward_copy(p3),forward_copy(p4),forward_copy(p5),forward_copy(p6),forward_copy(p7)
#define forward_copys8(p1,p2,p3,p4,p5,p6,p7,p8) forward_copy(p1),forward_copy(p2),forward_copy(p3),forward_copy(p4),forward_copy(p5),forward_copy(p6),forward_copy(p7),forward_copy(p8)
#define forward_copys9(p1,p2,p3,p4,p5,p6,p7,p8,p9) forward_copy(p1),forward_copy(p2),forward_copy(p3),forward_copy(p4),forward_copy(p5),forward_copy(p6),forward_copy(p7),forward_copy(p8),forward_copy(p9)
#define forward_copys(...) _BOND_LR__(forward_copys, _PP_NARG(__VA_ARGS__))(__VA_ARGS__)

#ifdef _MSC_VER
#ifndef snprintf
#define snprintf sprintf_s
#endif
#endif

#define __1 std::placeholders::_1
#define __2 std::placeholders::_2
#define __3 std::placeholders::_3
#define __4 std::placeholders::_4
#define __5 std::placeholders::_5
#define __6 std::placeholders::_6
#define __7 std::placeholders::_7
#define __8 std::placeholders::_8
#define __9 std::placeholders::_9

struct param_placeholder
{
	template <typename Arg> param_placeholder(Arg&&) {}
private:
	template <typename Arg> void operator=(Arg&&) = delete;
	param_placeholder(const param_placeholder&) = delete;
	param_placeholder() = delete;
};

#define _$ param_placeholder
#define _$1 _$
#define _$2 _$1,_$
#define _$3 _$2,_$
#define _$4 _$3,_$
#define _$5 _$4,_$
#define _$6 _$5,_$
#define _$7 _$6,_$
#define _$8 _$7,_$
#define _$9 _$8,_$

template <typename...>
struct null_handler 
{
	template <typename... Args>
	void operator()(Args&&...){}
};

struct none_functor
{
	template <typename... Args>
	void operator()(Args&&...){}
};

struct void_type {};
struct void_type1 {};
struct void_type2 {};

template <typename T>
struct ValTryRefMove_
{
	static inline T&& move(T& p) { return (T&&)p; }
};

template <typename T>
struct ValTryRefMove_<T&>
{
	static inline T& move(T& p) { return p; }
};

template <typename T>
struct ValTryRefMove_<const T&>
{
	static inline const T& move(const T& p) { return p; }
};

#define _VAL_MOVE(__p__) ValTryRefMove_<decltype(__p__)>::move(__p__)

#define RVALUE_COPY_CONSTRUCTION1(__name__, __val1__) public:\
	RVALUE_MOVE1(__name__, __val1__)\
	LVALUE_COPY1(__name__, __val1__)\
	LVALUE_CONSTRUCT1(__name__, __val1__)

#define RVALUE_COPY_CONSTRUCTION2(__name__, __val1__, __val2__) public:\
	RVALUE_MOVE2(__name__, __val1__, __val2__)\
	LVALUE_COPY2(__name__, __val1__, __val2__)\
	LVALUE_CONSTRUCT2(__name__, __val1__, __val2__)

#define RVALUE_COPY_CONSTRUCTION3(__name__, __val1__, __val2__, __val3__) public:\
	RVALUE_MOVE3(__name__, __val1__, __val2__, __val3__)\
	LVALUE_COPY3(__name__, __val1__, __val2__, __val3__)\
	LVALUE_CONSTRUCT3(__name__, __val1__, __val2__, __val3__)

#define RVALUE_COPY_CONSTRUCTION4(__name__, __val1__, __val2__, __val3__, __val4__) public:\
	RVALUE_MOVE4(__name__, __val1__, __val2__, __val3__, __val4__)\
	LVALUE_COPY4(__name__, __val1__, __val2__, __val3__, __val4__)\
	LVALUE_CONSTRUCT4(__name__, __val1__, __val2__, __val3__, __val4__)

#define RVALUE_COPY_CONSTRUCTION5(__name__, __val1__, __val2__, __val3__, __val4__, __val5__) public:\
	RVALUE_MOVE5(__name__, __val1__, __val2__, __val3__, __val4__, __val5__)\
	LVALUE_COPY5(__name__, __val1__, __val2__, __val3__, __val4__, __val5__)\
	LVALUE_CONSTRUCT5(__name__, __val1__, __val2__, __val3__, __val4__, __val5__)

#define RVALUE_COPY_CONSTRUCTION6(__name__, __val1__, __val2__, __val3__, __val4__, __val5__, __val6__) public:\
	RVALUE_MOVE6(__name__, __val1__, __val2__, __val3__, __val4__, __val5__, __val6__)\
	LVALUE_COPY6(__name__, __val1__, __val2__, __val3__, __val4__, __val5__, __val6__)\
	LVALUE_CONSTRUCT6(__name__, __val1__, __val2__, __val3__, __val4__, __val5__, __val6__)

#define RVALUE_COPY_CONSTRUCTION(__name__, ...) _BOND_LR__(RVALUE_COPY_CONSTRUCTION, _PP_NARG(__VA_ARGS__))(__name__, __VA_ARGS__)
//////////////////////////////////////////////////////////////////////////

#define RVALUE_MOVE1(__name__, __val1__) public:\
	RVALUE_COPY1(__name__, __val1__)\
	RVALUE_CONSTRUCT1(__name__, __val1__)

#define RVALUE_MOVE2(__name__, __val1__, __val2__) public:\
	RVALUE_COPY2(__name__, __val1__, __val2__)\
	RVALUE_CONSTRUCT2(__name__, __val1__, __val2__)

#define RVALUE_MOVE3(__name__, __val1__, __val2__, __val3__) public:\
	RVALUE_COPY3(__name__, __val1__, __val2__, __val3__)\
	RVALUE_CONSTRUCT3(__name__, __val1__, __val2__, __val3__)

#define RVALUE_MOVE4(__name__, __val1__, __val2__, __val3__, __val4__) public:\
	RVALUE_COPY4(__name__, __val1__, __val2__, __val3__, __val4__)\
	RVALUE_CONSTRUCT4(__name__, __val1__, __val2__, __val3__, __val4__)

#define RVALUE_MOVE5(__name__, __val1__, __val2__, __val3__, __val4__, __val5__) public:\
	RVALUE_COPY5(__name__, __val1__, __val2__, __val3__, __val4__, __val5__)\
	RVALUE_CONSTRUCT5(__name__, __val1__, __val2__, __val3__, __val4__, __val5__)

#define RVALUE_MOVE6(__name__, __val1__, __val2__, __val3__, __val4__, __val5__, __val6__) public:\
	RVALUE_COPY6(__name__, __val1__, __val2__, __val3__, __val4__, __val5__, __val6__)\
	RVALUE_CONSTRUCT6(__name__, __val1__, __val2__, __val3__, __val4__, __val5__, __val6__)
//////////////////////////////////////////////////////////////////////////

#define RVALUE_CONSTRUCT1(__name__, __val1__) public:\
	__name__(__name__&& s) : __val1__(_VAL_MOVE(s.__val1__)) {}

#define RVALUE_CONSTRUCT2(__name__, __val1__, __val2__) public:\
	__name__(__name__&& s) : __val1__(_VAL_MOVE(s.__val1__)), __val2__(_VAL_MOVE(s.__val2__)) {}

#define RVALUE_CONSTRUCT3(__name__, __val1__, __val2__, __val3__) public:\
	__name__(__name__&& s) : __val1__(_VAL_MOVE(s.__val1__)), __val2__(_VAL_MOVE(s.__val2__)), __val3__(_VAL_MOVE(s.__val3__)) {}

#define RVALUE_CONSTRUCT4(__name__, __val1__, __val2__, __val3__, __val4__) public:\
	__name__(__name__&& s) : __val1__(_VAL_MOVE(s.__val1__)), __val2__(_VAL_MOVE(s.__val2__)), __val3__(_VAL_MOVE(s.__val3__)), __val4__(_VAL_MOVE(s.__val4__)) {}

#define RVALUE_CONSTRUCT5(__name__, __val1__, __val2__, __val3__, __val4__, __val5__) public:\
	__name__(__name__&& s) : __val1__(_VAL_MOVE(s.__val1__)), __val2__(_VAL_MOVE(s.__val2__)), __val3__(_VAL_MOVE(s.__val3__)), __val4__(_VAL_MOVE(s.__val4__)), \
	__val5__(_VAL_MOVE(s.__val5__)) {}

#define RVALUE_CONSTRUCT6(__name__, __val1__, __val2__, __val3__, __val4__, __val5__, __val6__) public:\
	__name__(__name__&& s) : __val1__(_VAL_MOVE(s.__val1__)), __val2__(_VAL_MOVE(s.__val2__)), __val3__(_VAL_MOVE(s.__val3__)), __val4__(_VAL_MOVE(s.__val4__)), \
	__val5__(_VAL_MOVE(s.__val5__)), __val6__(_VAL_MOVE(s.__val6__)) {}
//////////////////////////////////////////////////////////////////////////

#define LVALUE_CONSTRUCT1(__name__, __val1__) public:\
	__name__(const __name__& s) : __val1__(s.__val1__) {}

#define LVALUE_CONSTRUCT2(__name__, __val1__, __val2__) public:\
	__name__(const __name__& s) : __val1__(s.__val1__), __val2__(s.__val2__) {}

#define LVALUE_CONSTRUCT3(__name__, __val1__, __val2__, __val3__) public:\
	__name__(const __name__& s) : __val1__(s.__val1__), __val2__(s.__val2__), __val3__(s.__val3__) {}

#define LVALUE_CONSTRUCT4(__name__, __val1__, __val2__, __val3__, __val4__) public:\
	__name__(const __name__& s) : __val1__(s.__val1__), __val2__(s.__val2__), __val3__(s.__val3__), __val4__(s.__val4__) {}

#define LVALUE_CONSTRUCT5(__name__, __val1__, __val2__, __val3__, __val4__, __val5__) public:\
	__name__(const __name__& s) : __val1__(s.__val1__), __val2__(s.__val2__), __val3__(s.__val3__), __val4__(s.__val4__), \
	__val5__(s.__val5__) {}

#define LVALUE_CONSTRUCT6(__name__, __val1__, __val2__, __val3__, __val4__, __val5__, __val6__) public:\
	__name__(const __name__& s) : __val1__(s.__val1__), __val2__(s.__val2__), __val3__(s.__val3__), __val4__(s.__val4__), \
	__val5__(s.__val5__), __val6__(s.__val6__) {}
//////////////////////////////////////////////////////////////////////////

#define COPY_CONSTRUCT1(__name__, __val1__)\
	RVALUE_CONSTRUCT1(__name__, __val1__);\
	LVALUE_CONSTRUCT1(__name__, __val1__);

#define COPY_CONSTRUCT2(__name__, __val1__, __val2__)\
	RVALUE_CONSTRUCT2(__name__, __val1__, __val2__);\
	LVALUE_CONSTRUCT2(__name__, __val1__, __val2__);

#define COPY_CONSTRUCT3(__name__, __val1__, __val2__, __val3__)\
	RVALUE_CONSTRUCT3(__name__, __val1__, __val2__, __val3__);\
	LVALUE_CONSTRUCT3(__name__, __val1__, __val2__, __val3__);

#define COPY_CONSTRUCT4(__name__, __val1__, __val2__, __val3__, __val4__)\
	RVALUE_CONSTRUCT4(__name__, __val1__, __val2__, __val3__, __val4__);\
	LVALUE_CONSTRUCT4(__name__, __val1__, __val2__, __val3__, __val4__);

#define COPY_CONSTRUCT5(__name__, __val1__, __val2__, __val3__, __val4__, __val5__)\
	RVALUE_CONSTRUCT5(__name__, __val1__, __val2__, __val3__, __val4__, __val5__);\
	LVALUE_CONSTRUCT5(__name__, __val1__, __val2__, __val3__, __val4__, __val5__);

#define COPY_CONSTRUCT6(__name__, __val1__, __val2__, __val3__, __val4__, __val5__, __val6__)\
	RVALUE_CONSTRUCT6(__name__, __val1__, __val2__, __val3__, __val4__, __val5__, __val6__);\
	LVALUE_CONSTRUCT6(__name__, __val1__, __val2__, __val3__, __val4__, __val5__, __val6__);
//////////////////////////////////////////////////////////////////////////

#define RVALUE_COPY1(__name__, __val1__) public:\
	void operator=(__name__&& s) { if (this != &s) { __val1__ = std::move(s.__val1__); } }

#define RVALUE_COPY2(__name__, __val1__, __val2__) public:\
	void operator=(__name__&& s) { if (this != &s) { __val1__ = std::move(s.__val1__); __val2__ = std::move(s.__val2__); } }

#define RVALUE_COPY3(__name__, __val1__, __val2__, __val3__) public:\
	void operator=(__name__&& s) { if (this != &s) { __val1__ = std::move(s.__val1__); __val2__ = std::move(s.__val2__); __val3__ = std::move(s.__val3__); } }

#define RVALUE_COPY4(__name__, __val1__, __val2__, __val3__, __val4__) public:\
	void operator=(__name__&& s) { if (this != &s) { __val1__ = std::move(s.__val1__); __val2__ = std::move(s.__val2__); __val3__ = std::move(s.__val3__); __val4__ = std::move(s.__val4__); } }

#define RVALUE_COPY5(__name__, __val1__, __val2__, __val3__, __val4__, __val5__) public:\
	void operator=(__name__&& s) { if (this != &s) { __val1__ = std::move(s.__val1__); __val2__ = std::move(s.__val2__); __val3__ = std::move(s.__val3__); __val4__ = std::move(s.__val4__); \
	__val5__ = std::move(s.__val5__); } }

#define RVALUE_COPY6(__name__, __val1__, __val2__, __val3__, __val4__, __val5__, __val6__) public:\
	void operator=(__name__&& s) { if (this != &s) { __val1__ = std::move(s.__val1__); __val2__ = std::move(s.__val2__); __val3__ = std::move(s.__val3__); __val4__ = std::move(s.__val4__); \
	__val5__ = std::move(s.__val5__); __val6__ = std::move(s.__val6__); } }
//////////////////////////////////////////////////////////////////////////

#define LVALUE_COPY1(__name__, __val1__) public:\
	void operator=(const __name__& s) { if (this != &s) { __val1__ = s.__val1__; } }\

#define LVALUE_COPY2(__name__, __val1__, __val2__) public:\
	void operator=(const __name__& s) { if (this != &s) { __val1__ = s.__val1__; __val2__ = s.__val2__; } }\

#define LVALUE_COPY3(__name__, __val1__, __val2__, __val3__) public:\
	void operator=(const __name__& s) { if (this != &s) { __val1__ = s.__val1__; __val2__ = s.__val2__; __val3__ = s.__val3__; } }\

#define LVALUE_COPY4(__name__, __val1__, __val2__, __val3__, __val4__) public:\
	void operator=(const __name__& s) { if (this != &s) { __val1__ = s.__val1__; __val2__ = s.__val2__; __val3__ = s.__val3__; __val4__ = s.__val4__; } }\

#define LVALUE_COPY5(__name__, __val1__, __val2__, __val3__, __val4__, __val5__) public:\
	void operator=(const __name__& s) { if (this != &s) { __val1__ = s.__val1__; __val2__ = s.__val2__; __val3__ = s.__val3__; __val4__ = s.__val4__; \
	__val5__ = s.__val5__; } }\

#define LVALUE_COPY6(__name__, __val1__, __val2__, __val3__, __val4__, __val5__, __val6__) public:\
	void operator=(const __name__& s) { if (this != &s) { __val1__ = s.__val1__; __val2__ = s.__val2__; __val3__ = s.__val3__; __val4__ = s.__val4__; \
	__val5__ = s.__val5__; __val6__ = s.__val6__; } }\


#define RVALUE_MOVE(__name__, ...) _BOND_LR__(RVALUE_MOVE, _PP_NARG(__VA_ARGS__))(__name__, __VA_ARGS__)
#define RVALUE_CONSTRUCT(__name__, ...) _BOND_LR__(RVALUE_CONSTRUCT, _PP_NARG(__VA_ARGS__))(__name__, __VA_ARGS__)
#define LVALUE_CONSTRUCT(__name__, ...) _BOND_LR__(LVALUE_CONSTRUCT, _PP_NARG(__VA_ARGS__))(__name__, __VA_ARGS__)
#define COPY_CONSTRUCT(__name__, ...) _BOND_LR__(COPY_CONSTRUCT, _PP_NARG(__VA_ARGS__))(__name__, __VA_ARGS__)

#ifdef _MSC_VER
#if (_MSC_VER >= 1900)
#define FUNCTION_ALLOCATOR(__dst__, __src__, __alloc__) __dst__(std::allocator_arg, __alloc__, __src__)
#else
#define FUNCTION_ALLOCATOR(__dst__, __src__, __alloc__) __dst__(__src__, __alloc__)
#endif
#elif __GNUG__
#define FUNCTION_ALLOCATOR(__dst__, __src__, __alloc__) __dst__(__src__)
#endif

//检测一个类是否有某个成员函数
#define HAS_MEMBER_FUNC(__member__)\
template<typename T, typename... Args>\
struct has_member_ ## __member__\
{\
private:\
    template<typename U> static auto check(int) -> decltype(std::declval<U>().__member__(std::declval<Args>()...), std::true_type());\
    template<typename U> static auto check(...) -> decltype(std::false_type());\
public:\
	enum { value = std::is_same<decltype(check<T>(0)), std::true_type>::value };\
};

#ifdef __linux__
template <typename Handler>
struct WrapContinuation_
{
	typedef RM_CREF(Handler) handler_type;

	WrapContinuation_(bool pl, Handler& handler)
		:_handler(std::forward<Handler>(handler)) {}

	friend bool asio_handler_is_continuation(WrapContinuation_*)
	{
		return true;
	}

	template <typename... Args>
	void operator()(Args&&... args)
	{
		_handler(std::forward<Args>(args)...);
	}

	handler_type _handler;
	COPY_CONSTRUCT1(WrapContinuation_, _handler);
};

template <typename Handler>
WrapContinuation_<Handler> wrap_continuation(Handler&& handler)
{
	return WrapContinuation_<Handler>(bool(), handler);
}

template <typename Handler>
struct WrapTried_
{
	typedef RM_CREF(Handler) handler_type;

	WrapTried_(bool pl, Handler& handler)
		:_handler(std::forward<Handler>(handler)) {}

	friend bool asio_handler_is_tried(WrapTried_*)
	{
		return true;
	}

	template <typename... Args>
	void operator()(Args&&... args)
	{
		_handler(std::forward<Args>(args)...);
	}

	handler_type _handler;
	COPY_CONSTRUCT1(WrapTried_, _handler);
};

template <typename Handler>
struct WrapNoTried_
{
	typedef RM_CREF(Handler) handler_type;

	WrapNoTried_(bool pl, Handler& handler)
		:_handler(std::forward<Handler>(handler)) {}

	friend bool asio_handler_is_tried(WrapNoTried_*)
	{
		return false;
	}

	template <typename... Args>
	void operator()(Args&&... args)
	{
		_handler(std::forward<Args>(args)...);
	}

	handler_type _handler;
	COPY_CONSTRUCT1(WrapNoTried_, _handler);
};

template <typename Handler>
WrapTried_<Handler> wrap_tried(Handler&& handler)
{
	return WrapTried_<Handler>(bool(), handler);
}

template <typename Handler>
WrapNoTried_<Handler> wrap_no_tried(Handler&& handler)
{
	return WrapNoTried_<Handler>(bool(), handler);
}

#elif WIN32

template <typename Handler>
Handler&& wrap_continuation(Handler&& handler)
{
	return (Handler&&)handler;
}

template <typename Handler>
Handler&& wrap_tried(Handler&& handler)
{
	return (Handler&&)handler;
}

template <typename Handler>
Handler&& wrap_no_tried(Handler&& handler)
{
	return (Handler&&)handler;
}
#endif

#endif