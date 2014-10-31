/*
** $Id: lobject.h,v 2.71.1.1 2013/04/12 18:48:47 roberto Exp $
** Type definitions for Lua objects
** See Copyright Notice in lua.h
*/


#ifndef lobject_h
#define lobject_h


#include <stdarg.h>


#include "llimits.h"
#include "lua.h"


/*
** Extra tags for non-values
*/
/* 一些扩展类型 */
#define LUA_TPROTO	LUA_NUMTAGS
#define LUA_TUPVAL	(LUA_NUMTAGS+1)
#define LUA_TDEADKEY	(LUA_NUMTAGS+2)

/*
** number of all possible tags (including LUA_TNONE but excluding DEADKEY)
*/
/* 不包括DEADKEY类型的类型总计数 */
#define LUA_TOTALTAGS	(LUA_TUPVAL+2)


/*
** tags for Tagged Values have the following use of bits:
** bits 0-3: actual tag (a LUA_T* value)
** bits 4-5: variant bits
** bit 6: whether value is collectable
*/
/* 0-3 真实类型的位
 * 4-5 变量位
 * 6 此变量是否是可回收的
 */
#define VARBITS		(3 << 4)


/*
** LUA_TFUNCTION variants:
** 0 - Lua function
** 1 - light C function
** 2 - regular C function (closure)
*/
/*
 * LUA_TFUNCTION 变量:
 * 0 - Lua 函数
 * 1 - 轻量级C函数
 * 2 - 正常的C函数
 */
/* Variant tags for functions */
#define LUA_TLCL	(LUA_TFUNCTION | (0 << 4))  /* Lua closure */
#define LUA_TLCF	(LUA_TFUNCTION | (1 << 4))  /* light C function */
#define LUA_TCCL	(LUA_TFUNCTION | (2 << 4))  /* C closure */


/* Variant tags for strings */
/* 字符串类型 */
#define LUA_TSHRSTR	(LUA_TSTRING | (0 << 4))  /* short strings */
#define LUA_TLNGSTR	(LUA_TSTRING | (1 << 4))  /* long strings */


/* Bit mark for collectable types */
/* 表明是可回收对象 */
#define BIT_ISCOLLECTABLE	(1 << 6)

/* mark a tag as collectable */
/* 标记一个对象的属性为可回收属性 */
#define ctb(t)			((t) | BIT_ISCOLLECTABLE)


/*
** Union of all collectable objects
*/
/* 可回收对象枚举体定义 */
typedef union GCObject GCObject;


/*
** Common Header for all collectable objects (in macro form, to be
** included in other objects)
*/
/* 定义一组公共的属性为每个可回收的lua内部对象 
 * next 链接到下一个对象
 * tt 对象的类型标志
 * marked 对象标记,一些对象属性
 */
#define CommonHeader	GCObject *next; lu_byte tt; lu_byte marked


/*
** Common header in struct form
*/
typedef struct GCheader {
  CommonHeader;
} GCheader;



/*
** Union of all Lua values
*/
/* 描述了Lua对象值的枚举体 */
typedef union Value Value;

/* 数字区域 */
#define numfield	lua_Number n;    /* numbers */



/*
** Tagged Values. This is the basic representation of values in Lua,
** an actual value plus a tag with its type.
*/
/* 值的一些的区域定义
 * value_ 值
 * tt_ 类型
 */
#define TValuefields	Value value_; int tt_

typedef struct lua_TValue TValue;


/* macro defining a nil value */
/* nil值的定义 */
#define NILCONSTANT	{NULL}, LUA_TNIL

/* 取对象的值相关 */
#define val_(o)		((o)->value_)
#define num_(o)		(val_(o).n)


/* raw type tag of a TValue */
/* 取出对象的标志值 */
#define rttype(o)	((o)->tt_)

/* tag with no variants (bits 0-3) */
/* 取Lua变量标志 */
#define novariant(x)	((x) & 0x0F)

/* type tag of a TValue (bits 0-3 for tags + variant bits 4-5) */
/* 取Lua变量与自定义变量位 */
#define ttype(o)	(rttype(o) & 0x3F)

/* type tag of a TValue with no variants (bits 0-3) */
/* 从对象中取Lua变量标志 */
#define ttypenv(o)	(novariant(rttype(o)))


/* Macros to test type */
/* 一些测试用的宏 */
#define checktag(o,t)		(rttype(o) == (t))                              /* 检查tag标记 */
#define checktype(o,t)		(ttypenv(o) == (t))                           /* 检查类型 */

/*
 * 检查对象类型
 */
#define ttisnumber(o)		checktag((o), LUA_TNUMBER)                      /* 判断对象是否是数字类型 */
#define ttisnil(o)		checktag((o), LUA_TNIL)                           /* 判断对象是否为空对象类型 */
#define ttisboolean(o)		checktag((o), LUA_TBOOLEAN)                   /* 判断对象是否为布尔对象类型 */
#define ttislightuserdata(o)	checktag((o), LUA_TLIGHTUSERDATA)         /* 判断对象是否为轻型用户数据类型 */
#define ttisstring(o)		checktype((o), LUA_TSTRING)                     /* 判断对象是否字符串类型 */
#define ttisshrstring(o)	checktag((o), ctb(LUA_TSHRSTR))               /* 短字符串对象 */
#define ttislngstring(o)	checktag((o), ctb(LUA_TLNGSTR))               /* 长字符串对象 */
#define ttistable(o)		checktag((o), ctb(LUA_TTABLE))                  /* 表对象 */
#define ttisfunction(o)		checktype(o, LUA_TFUNCTION)                   /* 函数对象 */
#define ttisclosure(o)		((rttype(o) & 0x1F) == LUA_TFUNCTION)         /* 闭包函数 */
#define ttisCclosure(o)		checktag((o), ctb(LUA_TCCL))                  /* C闭包函数 */
#define ttisLclosure(o)		checktag((o), ctb(LUA_TLCL))                  /* Lua闭包函数 */
#define ttislcf(o)		checktag((o), LUA_TLCF)                           /* Lua函数 */
#define ttisuserdata(o)		checktag((o), ctb(LUA_TUSERDATA))             /* 用户数据 */
#define ttisthread(o)		checktag((o), ctb(LUA_TTHREAD))                 /* 线程对象 */
#define ttisdeadkey(o)		checktag((o), LUA_TDEADKEY)                   /* 死值 */

/* 两个对象是否相等 */
#define ttisequal(o1,o2)	(rttype(o1) == rttype(o2))

/* Macros to access values */
/* 测试对象类型并且返回这个对象的值 */
#define nvalue(o)	check_exp(ttisnumber(o), num_(o))                     /* 获取数字的值 */
#define gcvalue(o)	check_exp(iscollectable(o), val_(o).gc)             /* 获取可回首对象的值 */
#define pvalue(o)	check_exp(ttislightuserdata(o), val_(o).p)            /* 获取轻型用户数据的值 */
#define rawtsvalue(o)	check_exp(ttisstring(o), &val_(o).gc->ts)         /* 获取原始字符串的值 */
#define tsvalue(o)	(&rawtsvalue(o)->tsv)                               /* 获取原始字符串的值的缓冲指针 */
#define rawuvalue(o)	check_exp(ttisuserdata(o), &val_(o).gc->u)        /* 获取原始值 */
#define uvalue(o)	(&rawuvalue(o)->uv)
#define clvalue(o)	check_exp(ttisclosure(o), &val_(o).gc->cl)          /* 闭包函数的值 */
#define clLvalue(o)	check_exp(ttisLclosure(o), &val_(o).gc->cl.l)       /* lua闭包函数的值 */
#define clCvalue(o)	check_exp(ttisCclosure(o), &val_(o).gc->cl.c)       /* c闭包函数的值 */
#define fvalue(o)	check_exp(ttislcf(o), val_(o).f)                      /* 获取函数 */
#define hvalue(o)	check_exp(ttistable(o), &val_(o).gc->h)               /* 获取哈希表的值 */
#define bvalue(o)	check_exp(ttisboolean(o), val_(o).b)                  /* 获取布尔类型的值 */
#define thvalue(o)	check_exp(ttisthread(o), &val_(o).gc->th)           /* 获取线程 */
/* a dead value may get the 'gc' field, but cannot access its contents */
/* dead值，可以访问 'gc'区域，但是不能访问它的内容 */
#define deadvalue(o)	check_exp(ttisdeadkey(o), cast(void *, val_(o).gc))

/* 对象的值是false */
#define l_isfalse(o)	(ttisnil(o) || (ttisboolean(o) && bvalue(o) == 0))

/* 此对象是否可回收 */
#define iscollectable(o)	(rttype(o) & BIT_ISCOLLECTABLE)

/* Macros for internal tests */
/* 宏的内部测试 */
#define righttt(obj)		(ttype(obj) == gcvalue(obj)->gch.tt)

/* 判断此对象当前是活动对象
 * g 全局状态结构
 * obj 对象指针
 * 判断依据
 * 不是可回收状态
 * 对象属性不为空
 * 非已死对象
 */
#define checkliveness(g,obj) \
	lua_longassert(!iscollectable(obj) || \
			(righttt(obj) && !isdead(g,gcvalue(obj))))


/* Macros to set values */
/* 设置对象属性 */
#define settt_(o,t)	((o)->tt_=(t))

/* 给对象设置数字的值 */
#define setnvalue(obj,x) \
  { TValue *io=(obj); num_(io)=(x); settt_(io, LUA_TNUMBER); }

/* 设置对象为nil值 */
#define setnilvalue(obj) settt_(obj, LUA_TNIL)

/* 设置对象为C轻量级函数 */
#define setfvalue(obj,x) \
  { TValue *io=(obj); val_(io).f=(x); settt_(io, LUA_TLCF); }

/* 设置对象为轻量级用户数据 */
#define setpvalue(obj,x) \
  { TValue *io=(obj); val_(io).p=(x); settt_(io, LUA_TLIGHTUSERDATA); }

/* 设置对象为布尔类型 */
#define setbvalue(obj,x) \
  { TValue *io=(obj); val_(io).b=(x); settt_(io, LUA_TBOOLEAN); }

/* 设置对象为可回收对象 */
#define setgcovalue(L,obj,x) \
  { TValue *io=(obj); GCObject *i_g=(x); \
    val_(io).gc=i_g; settt_(io, ctb(gch(i_g)->tt)); }

/* 设置给字符串对象设置值
 * L 虚拟机状态
 * obj lua对象指针
 * x 要设置给lua对象的值
 */
#define setsvalue(L,obj,x) \
  { TValue *io=(obj); \
    TString *x_ = (x); \
    val_(io).gc=cast(GCObject *, x_); settt_(io, ctb(x_->tsv.tt)); \
    checkliveness(G(L),io); }

/* 设置用户数据给对象 */
#define setuvalue(L,obj,x) \
  { TValue *io=(obj); \
    val_(io).gc=cast(GCObject *, (x)); settt_(io, ctb(LUA_TUSERDATA)); \
    checkliveness(G(L),io); }

/* 设置给对象的线程值 */
#define setthvalue(L,obj,x) \
  { TValue *io=(obj); \
    val_(io).gc=cast(GCObject *, (x)); settt_(io, ctb(LUA_TTHREAD)); \
    checkliveness(G(L),io); }

/* 设置给对象的lua函数 */
#define setclLvalue(L,obj,x) \
  { TValue *io=(obj); \
    val_(io).gc=cast(GCObject *, (x)); settt_(io, ctb(LUA_TLCL)); \
    checkliveness(G(L),io); }

/* 设置给对象的轻量C函数 */
#define setclCvalue(L,obj,x) \
  { TValue *io=(obj); \
    val_(io).gc=cast(GCObject *, (x)); settt_(io, ctb(LUA_TCCL)); \
    checkliveness(G(L),io); }

/* 设置给对象设置哈希表值 */
#define sethvalue(L,obj,x) \
  { TValue *io=(obj); \
    val_(io).gc=cast(GCObject *, (x)); settt_(io, ctb(LUA_TTABLE)); \
    checkliveness(G(L),io); }

/* 设置为对象已死亡 */
#define setdeadvalue(obj)	settt_(obj, LUA_TDEADKEY)

/* 将对象2设置给对象1 */
#define setobj(L,obj1,obj2) \
	{ const TValue *io2=(obj2); TValue *io1=(obj1); \
	  io1->value_ = io2->value_; io1->tt_ = io2->tt_; \
	  checkliveness(G(L),io1); }


/*
** different types of assignments, according to destination
*/

/* from stack to (same) stack */
#define setobjs2s	setobj
/* to stack (not from same stack) */
#define setobj2s	setobj
#define setsvalue2s	setsvalue
#define sethvalue2s	sethvalue
#define setptvalue2s	setptvalue
/* from table to same table */
#define setobjt2t	setobj
/* to table */
#define setobj2t	setobj
/* to new object */
#define setobj2n	setobj
#define setsvalue2n	setsvalue


/* check whether a number is valid (useful only for NaN trick) */
#define luai_checknum(L,o,c)	{ /* empty */ }


/*
** {======================================================
** NaN Trick
** =======================================================
*/
#if defined(LUA_NANTRICK)

/*
** numbers are represented in the 'd_' field. All other values have the
** value (NNMARK | tag) in 'tt__'. A number with such pattern would be
** a "signaled NaN", which is never generated by regular operations by
** the CPU (nor by 'strtod')
*/

/* allows for external implementation for part of the trick */
/* 允许trick部分的扩展继承 */
#if !defined(NNMARK)	/* { */

/* 如果没有定义LUA_IEEEENDIAN IEEE小端字序 */
#if !defined(LUA_IEEEENDIAN)
#error option 'LUA_NANTRICK' needs 'LUA_IEEEENDIAN'
#endif


#define NNMARK		0x7FF7A500
#define NNMASK		0x7FFFFF00

#undef TValuefields
#undef NILCONSTANT

/* 如果不采用IEEEENDIAN字节序则自行定义 */
#if (LUA_IEEEENDIAN == 0)	/* { */

/* little endian */
#define TValuefields  \
	union { struct { Value v__; int tt__; } i; double d__; } u
#define NILCONSTANT	{{{NULL}, tag2tt(LUA_TNIL)}}
/* field-access macros */
#define v_(o)		((o)->u.i.v__)
#define d_(o)		((o)->u.d__)
#define tt_(o)		((o)->u.i.tt__)

#else				/* }{ */

/* big endian */
#define TValuefields  \
	union { struct { int tt__; Value v__; } i; double d__; } u
#define NILCONSTANT	{{tag2tt(LUA_TNIL), {NULL}}}
/* field-access macros */
#define v_(o)		((o)->u.i.v__)
#define d_(o)		((o)->u.d__)
#define tt_(o)		((o)->u.i.tt__)

#endif				/* } */

#endif			/* } */


/* correspondence with standard representation */
#undef val_
#define val_(o)		v_(o)
#undef num_
#define num_(o)		d_(o)


#undef numfield
#define numfield	/* no such field; numbers are the entire struct */

/* basic check to distinguish numbers from non-numbers */
#undef ttisnumber
#define ttisnumber(o)	((tt_(o) & NNMASK) != NNMARK)

#define tag2tt(t)	(NNMARK | (t))

#undef rttype
#define rttype(o)	(ttisnumber(o) ? LUA_TNUMBER : tt_(o) & 0xff)

#undef settt_
#define settt_(o,t)	(tt_(o) = tag2tt(t))

#undef setnvalue
#define setnvalue(obj,x) \
	{ TValue *io_=(obj); num_(io_)=(x); lua_assert(ttisnumber(io_)); }

#undef setobj
#define setobj(L,obj1,obj2) \
	{ const TValue *o2_=(obj2); TValue *o1_=(obj1); \
	  o1_->u = o2_->u; \
	  checkliveness(G(L),o1_); }


/*
** these redefinitions are not mandatory, but these forms are more efficient
*/

#undef checktag
#undef checktype
#define checktag(o,t)	(tt_(o) == tag2tt(t))
#define checktype(o,t)	(ctb(tt_(o) | VARBITS) == ctb(tag2tt(t) | VARBITS))

#undef ttisequal
#define ttisequal(o1,o2)  \
	(ttisnumber(o1) ? ttisnumber(o2) : (tt_(o1) == tt_(o2)))


#undef luai_checknum
#define luai_checknum(L,o,c)	{ if (!ttisnumber(o)) c; }

#endif
/* }====================================================== */



/*
** {======================================================
** types and prototypes
** =======================================================
*/
/* 类型与原型定义 */

/* lua元素值 */
union Value {
	/* 可回收对象指针 */
  GCObject *gc;    /* collectable objects */
	/* 用户定义的数据 */
  void *p;         /* light userdata */
	/* bool值 */
  int b;           /* booleans */
	/* C语言函数 */
  lua_CFunction f; /* light C functions */
	/* 数字 */
  numfield         /* numbers */
};

/* lua元素结构体 
 * 一个Value枚举值
 * 一个tt_表明值的属性
 */
struct lua_TValue {
  TValuefields;
};

/* 指向一个栈元素 
 * 栈中的每一项指向一个元素地址
 */
typedef TValue *StkId;  /* index to stack elements */




/*
** Header for string value; string bytes follow the end of this structure
*/
/* lua内部字符串保存结构 */
typedef union TString {
	/* 字符串缓存最大对齐力度 */
  L_Umaxalign dummy;  /* ensures maximum alignment for strings */
  struct {
    CommonHeader;
    lu_byte extra;  /* reserved words for short strings; "has hash" for longs */
    unsigned int hash;   /* 字符串的hash值 */
		/* 当前字符串长度 */
    size_t len;  /* number of characters in string */
  } tsv;
} TString;


/* get the actual string (array of bytes) from a TString */
/* 获取TString类型的值 */
#define getstr(ts)	cast(const char *, (ts) + 1)

/* get the actual string (array of bytes) from a Lua value */
/* 获取TString值保存的字符串缓冲区数据 */
#define svalue(o)       getstr(rawtsvalue(o))

/*
** Header for userdata; memory area follows the end of this structure
*/
/* 用户数据头 */
typedef union Udata {
	/* 本地用户数据的对齐粒度 */
  L_Umaxalign dummy;  /* ensures maximum alignment for `local' udata */
  struct {
    CommonHeader;
    struct Table *metatable;
    struct Table *env;
    size_t len;  /* number of bytes */
  } uv;
} Udata;



/*
** Description of an upvalue for function prototypes
*/
/* upvalue的描述 */
typedef struct Upvaldesc {
	/* upvalue名称 (调试信息) */
  TString *name;  /* upvalue name (for debug information) */
	/* 当前值是否在栈中 */
  lu_byte instack;  /* whether it is in stack */
	/* upvalue的索引(在栈中或者在函数列表外) */
  lu_byte idx;  /* index of upvalue (in stack or in outer function's list) */
} Upvaldesc;


/*
** Description of a local variable for function prototypes
** (used for debug information)
*/
/* 函数的本地变量 */
typedef struct LocVar {
  TString *varname;
	/* 描述这个变量的生存周期 */
  int startpc;  /* first point where variable is active */
  int endpc;    /* first point where variable is dead */
} LocVar;


/*
** Function Prototypes
*/
/* 函数原型 */
typedef struct Proto {
  CommonHeader;
	/* 函数所使用的常量 */
  TValue *k;  /* constants used by the function */
	/* 函数的指令 */
  Instruction *code;
	/* 在此函数内部定义的函数 */
  struct Proto **p;  /* functions defined inside the function */
	/* 调试信息,opcode对应的源代码行号 */
  int *lineinfo;  /* map from opcodes to source lines (debug information) */
	/* 本地变量信息 */
  LocVar *locvars;  /* information about local variables (debug information) */
	/* upvalue信息 */
  Upvaldesc *upvalues;  /* upvalue information */
	/* 这个函数原型最后创建的closure */
  union Closure *cache;  /* last created closure with this prototype */
	/* 源代码,调试所需 */
  TString  *source;  /* used for debug information */
	/* upvalues的长度 */
  int sizeupvalues;  /* size of 'upvalues' */
  int sizek;  /* size of `k' */
  int sizecode;
  int sizelineinfo;
  int sizep;  /* size of `p' */
  int sizelocvars;
  int linedefined;
  int lastlinedefined;
	/* 可回收对象的列表 */
  GCObject *gclist;
	/* 固定参数的数量 */
  lu_byte numparams;  /* number of fixed parameters */
  lu_byte is_vararg;
	/* 最大栈数量 */
  lu_byte maxstacksize;  /* maximum stack used by this function */
} Proto;



/*
** Lua Upvalues
*/
/* upvalues定义 */
typedef struct UpVal {
  CommonHeader;
	/* 指向栈或者它自己的值 */
  TValue *v;  /* points to stack or to its own value */
  union {
    TValue value;  /* the value (when closed) */
    struct {  /* double linked list (when open) */
      struct UpVal *prev;
      struct UpVal *next;
    } l;
  } u;
} UpVal;


/*
** Closures
*/
/* 三种函数的类型 */
#define ClosureHeader \
	CommonHeader; lu_byte nupvalues; GCObject *gclist

typedef struct CClosure {
  ClosureHeader;
  lua_CFunction f;
  TValue upvalue[1];  /* list of upvalues */
} CClosure;


typedef struct LClosure {
  ClosureHeader;
  struct Proto *p;
  UpVal *upvals[1];  /* list of upvalues */
} LClosure;


typedef union Closure {
  CClosure c;
  LClosure l;
} Closure;


#define isLfunction(o)	ttisLclosure(o)

#define getproto(o)	(clLvalue(o)->p)


/*
** Tables
*/
/* 哈希表的健 */
typedef union TKey {
  struct {
    TValuefields;
    struct Node *next;  /* for chaining */
  } nk;
  TValue tvk;
} TKey;

/* 哈希节点 */
typedef struct Node {
  TValue i_val;           /* 节点的值 */
  TKey i_key;             /* 节点的健 */
} Node;

/* 表 */
typedef struct Table {
  CommonHeader;
  lu_byte flags;  /* 1<<p means tagmethod(p) is not present */
	/* 节点数量,以log2计算 */
  lu_byte lsizenode;  /* log2 of size of `node' array */
	/* 节点原表 */
  struct Table *metatable;
	/* 队列部分 */
  TValue *array;  /* array part */
	/* 表节点链表 */
  Node *node;
	/* 任意空闲的位置在这个位置之前 */
  Node *lastfree;  /* any free position is before this position */
	/* 可回收对象列表 */
  GCObject *gclist;
	/* 哈希队列长度,根长度 */
  int sizearray;  /* size of `array' array */
} Table;


/*
** `module' operation for hashing (size is always a power of 2)
*/
/* size&(size-1))==0 是保证2的次冥
 * s 字符串
 * size 必须是2的次冥
 */
#define lmod(s,size) \
	(check_exp((size&(size-1))==0, (cast(int, (s) & ((size)-1)))))

/* 对节点的数量进行平方操作 */
#define twoto(x)	(1<<(x))
/* 计算节点的真正长度 */
#define sizenode(t)	(twoto((t)->lsizenode))


/*
** (address of) a fixed nil value
*/
/* 对象nil值的地址 */
#define luaO_nilobject		(&luaO_nilobject_)

/* nil值 */
LUAI_DDEC const TValue luaO_nilobject_;


LUAI_FUNC int luaO_int2fb (unsigned int x);
LUAI_FUNC int luaO_fb2int (int x);
LUAI_FUNC int luaO_ceillog2 (unsigned int x);
LUAI_FUNC lua_Number luaO_arith (int op, lua_Number v1, lua_Number v2);
LUAI_FUNC int luaO_str2d (const char *s, size_t len, lua_Number *result);
LUAI_FUNC int luaO_hexavalue (int c);
LUAI_FUNC const char *luaO_pushvfstring (lua_State *L, const char *fmt,
                                                       va_list argp);
LUAI_FUNC const char *luaO_pushfstring (lua_State *L, const char *fmt, ...);
LUAI_FUNC void luaO_chunkid (char *out, const char *source, size_t len);


#endif

