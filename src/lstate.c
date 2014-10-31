/*
** $Id: lstate.c,v 2.99.1.2 2013/11/08 17:45:31 roberto Exp $
** Global State
** See Copyright Notice in lua.h
*/


#include <stddef.h>
#include <string.h>

#define lstate_c
#define LUA_CORE

#include "lua.h"

#include "lapi.h"
#include "ldebug.h"
#include "ldo.h"
#include "lfunc.h"
#include "lgc.h"
#include "llex.h"
#include "lmem.h"
#include "lstate.h"
#include "lstring.h"
#include "ltable.h"
#include "ltm.h"


#if !defined(LUAI_GCPAUSE)
#define LUAI_GCPAUSE	200  /* 200% */
#endif

#if !defined(LUAI_GCMAJOR)
#define LUAI_GCMAJOR	200  /* 200% */
#endif

#if !defined(LUAI_GCMUL)
#define LUAI_GCMUL	200 /* GC runs 'twice the speed' of memory allocation */
#endif

/* 没有足够的内存 */
#define MEMERRMSG	"not enough memory"


/*
** a macro to help the creation of a unique random seed when a state is
** created; the seed is used to randomize hashes.
*/
/* 生成时间种子 */
#if !defined(luai_makeseed)
#include <time.h>
#define luai_makeseed()		cast(unsigned int, time(NULL))
#endif



/*
** thread state + extra space
*/
/* 线程状态 + 扩展空间 */
typedef struct LX {
/* 如果定义了扩展空间则定义 */
#if defined(LUAI_EXTRASPACE)     /* 默认为0 */
  char buff[LUAI_EXTRASPACE];
#endif
  lua_State l;    /* 线程状态 */
} LX;


/*
** Main thread combines a thread state and the global state
*/
/* 主线程保护一个线程状态与一个全局状态 */
typedef struct LG {
  LX l;                   /* 线程状态 */
  global_State g;         /* 全局状态，供所有线程共享 */
} LG;


/* 找到线程状态 */
#define fromstate(L)	(cast(LX *, cast(lu_byte *, (L)) - offsetof(LX, l)))


/*
** Compute an initial seed as random as possible. In ANSI, rely on
** Address Space Layout Randomization (if present) to increase
** randomness..
*/
/* b 缓存指针
 * p 是位置
 * e 指针的值 (4个字节)
 */
#define addbuff(b,p,e) \
  { size_t t = cast(size_t, e); \
    memcpy(buff + p, &t, sizeof(t)); p += sizeof(t); }

/* 生成种子
 * L 虚拟机状态
 * 
 * 缓存的结构为
 * 虚拟机状态指针
 * 一个随机数
 * nil对象指针
 * lua_newstate的函数指针
 */
static unsigned int makeseed (lua_State *L) {
  char buff[4 * sizeof(size_t)];
  unsigned int h = luai_makeseed();      /* 生成时间种子 */
  int p = 0;
	/* 堆变量 */
  addbuff(buff, p, L);  /* heap variable */
	/* 本地变量 */
  addbuff(buff, p, &h);  /* local variable */
	/* 全局变量 */
  addbuff(buff, p, luaO_nilobject);  /* global variable */
	/* lua_newstate函数指针 */
  addbuff(buff, p, &lua_newstate);  /* public function */
	
	/* 最后p必须同buf的长度 */
  lua_assert(p == sizeof(buff));
	
	/* 返回哈希数 */
  return luaS_hash(buff, p, h);
}


/*
** set GCdebt to a new value keeping the value (totalbytes + GCdebt)
** invariant
*/
void luaE_setdebt (global_State *g, l_mem debt) {
  g->totalbytes -= (debt - g->GCdebt);
  g->GCdebt = debt;
}


CallInfo *luaE_extendCI (lua_State *L) {
  CallInfo *ci = luaM_new(L, CallInfo);
  lua_assert(L->ci->next == NULL);
  L->ci->next = ci;
  ci->previous = L->ci;
  ci->next = NULL;
  return ci;
}

/* 释放调用栈
 * L 虚拟机状态指针
 */
void luaE_freeCI (lua_State *L) {
  CallInfo *ci = L->ci;
  CallInfo *next = ci->next;
  ci->next = NULL;
	/* 循环遍历 */
  while ((ci = next) != NULL) {
    next = ci->next;
    luaM_free(L, ci);
  }
}

/* 栈初始化
 * L1 虚拟机状态
 * L 虚拟机状态
 */
static void stack_init (lua_State *L1, lua_State *L) {
  int i; CallInfo *ci;
  /* initialize stack array */
	/* 分配数据栈空间 */
  L1->stack = luaM_newvector(L, BASIC_STACK_SIZE, TValue);
	/* 数据栈大小 */
  L1->stacksize = BASIC_STACK_SIZE;
	/* 初始化栈空间 */
  for (i = 0; i < BASIC_STACK_SIZE; i++)
    setnilvalue(L1->stack + i);  /* erase new stack */
	/* 设置栈顶 */
  L1->top = L1->stack;
	/* 末尾留出EXTRA_STACK大小 */
  L1->stack_last = L1->stack + L1->stacksize - EXTRA_STACK;
  /* initialize first ci */
	/* 初始化调用栈 */
  ci = &L1->base_ci;
  ci->next = ci->previous = NULL;
  ci->callstatus = 0;
  ci->func = L1->top;
	/* 为ci入口留出空间 */
  setnilvalue(L1->top++);  /* 'function' entry for this 'ci' */
	/* 移动栈顶,预先留出LUA_MINSTACK的大小 */
  ci->top = L1->top + LUA_MINSTACK;
  L1->ci = ci;
}

/* 释放栈 */
static void freestack (lua_State *L) {
  if (L->stack == NULL)
    return;  /* stack not completely built yet */
  L->ci = &L->base_ci;  /* free the entire 'ci' list */
	/* 释放调用栈 */
  luaE_freeCI(L);
	/* 释放数据栈 */
  luaM_freearray(L, L->stack, L->stacksize);  /* free stack array */
}


/*
** Create registry table and its predefined values
*/
static void init_registry (lua_State *L, global_State *g) {
  TValue mt;
  /* create registry */
  Table *registry = luaH_new(L);
  sethvalue(L, &g->l_registry, registry);
  luaH_resize(L, registry, LUA_RIDX_LAST, 0);
  /* registry[LUA_RIDX_MAINTHREAD] = L */
  setthvalue(L, &mt, L);
  luaH_setint(L, registry, LUA_RIDX_MAINTHREAD, &mt);
  /* registry[LUA_RIDX_GLOBALS] = table of globals */
  sethvalue(L, &mt, luaH_new(L));
  luaH_setint(L, registry, LUA_RIDX_GLOBALS, &mt);
}


/*
** open parts of the state that may cause memory-allocation errors
*/
/* 打开一个虚拟机内存状态
 * L 要打开的虚拟机状态
 * ud 参数 , 未使用
 * 这个函数最重要的就是初始化数据栈空间
 */
static void f_luaopen (lua_State *L, void *ud) {
  global_State *g = G(L);
  UNUSED(ud);
	/* 初始化栈 */
  stack_init(L, L);  /* init stack */
  init_registry(L, g);
  luaS_resize(L, MINSTRTABSIZE);  /* initial size of string table */
  luaT_init(L);
  luaX_init(L);
  /* pre-create memory-error message */
  g->memerrmsg = luaS_newliteral(L, MEMERRMSG);
  luaS_fix(g->memerrmsg);  /* it should never be collected */
  g->gcrunning = 1;  /* allow gc */
  g->version = lua_version(NULL);
  luai_userstateopen(L);
}


/*
** preinitialize a state with consistent values without allocating
** any memory (to avoid errors)
*/
/* 对状态L进行初始化 
 * L 虚拟机状态指针
 * g 虚拟机全局结构指针
 */
static void preinit_state (lua_State *L, global_State *g) {
  G(L) = g;
  L->stack = NULL;
  L->ci = NULL;
  L->stacksize = 0;
  L->errorJmp = NULL;
  L->nCcalls = 0;
  L->hook = NULL;
  L->hookmask = 0;
  L->basehookcount = 0;
  L->allowhook = 1;
  resethookcount(L);
  L->openupval = NULL;
  L->nny = 1;
  L->status = LUA_OK;
  L->errfunc = 0;
}

/* 对虚拟机状态L进行关闭 */
static void close_state (lua_State *L) {
  global_State *g = G(L);
	/* 关闭这条线程的所有upvalues */
  luaF_close(L, L->stack);  /* close all upvalues for this thread */
	/* 释放所有可回收对象内存 */
  luaC_freeallobjects(L);  /* collect all objects */
  if (g->version)  /* closing a fully built state? */
    luai_userstateclose(L);
  luaM_freearray(L, G(L)->strt.hash, G(L)->strt.size);
  luaZ_freebuffer(L, &g->buff);
  freestack(L);
  lua_assert(gettotalbytes(g) == sizeof(LG));
  (*g->frealloc)(g->ud, fromstate(L), sizeof(LG), 0);  /* free main block */
}


LUA_API lua_State *lua_newthread (lua_State *L) {
  lua_State *L1;
  lua_lock(L);
  luaC_checkGC(L);
  L1 = &luaC_newobj(L, LUA_TTHREAD, sizeof(LX), NULL, offsetof(LX, l))->th;
  setthvalue(L, L->top, L1);
  api_incr_top(L);
  preinit_state(L1, G(L));
  L1->hookmask = L->hookmask;
  L1->basehookcount = L->basehookcount;
  L1->hook = L->hook;
  resethookcount(L1);
  luai_userstatethread(L, L1);
  stack_init(L1, L);  /* init stack */
  lua_unlock(L);
  return L1;
}


void luaE_freethread (lua_State *L, lua_State *L1) {
  LX *l = fromstate(L1);
  luaF_close(L1, L1->stack);  /* close all upvalues for this thread */
  lua_assert(L1->openupval == NULL);
  luai_userstatefree(L, L1);
  freestack(L1);
  luaM_free(L, l);
}

/* 分配一个虚拟机状态 
 * f 内存分配函数指针
 * ud 内存句柄
 */
LUA_API lua_State *lua_newstate (lua_Alloc f, void *ud) {
  int i;
  lua_State *L;
  global_State *g;
	/* 一条线程 */
  LG *l = cast(LG *, (*f)(ud, NULL, LUA_TTHREAD, sizeof(LG)));    /* 分配全局状态 */
  if (l == NULL) return NULL;
  L = &l->l.l;       /* 这个是返回当前线程的状态 */
  g = &l->g;         /* 全局状态 */
  L->next = NULL;
  L->tt = LUA_TTHREAD;
  g->currentwhite = bit2mask(WHITE0BIT, FIXEDBIT);
  L->marked = luaC_white(g);
  g->gckind = KGC_NORMAL;         /* 垃圾回收类型为正常 */
	/* 对线程状态初始化 */
  preinit_state(L, g);
  g->frealloc = f;
  g->ud = ud;
  g->mainthread = L;
  g->seed = makeseed(L);
  g->uvhead.u.l.prev = &g->uvhead;
  g->uvhead.u.l.next = &g->uvhead;
  g->gcrunning = 0;  /* no GC while building state */
  g->GCestimate = 0;
  g->strt.size = 0;
  g->strt.nuse = 0;
  g->strt.hash = NULL;
  setnilvalue(&g->l_registry);
  luaZ_initbuffer(L, &g->buff);
  g->panic = NULL;
  g->version = NULL;
  g->gcstate = GCSpause;     /* 垃圾回收首先设置为 停止 */
  g->allgc = NULL;
  g->finobj = NULL;
  g->tobefnz = NULL;
  g->sweepgc = g->sweepfin = NULL;
  g->gray = g->grayagain = NULL;
  g->weak = g->ephemeron = g->allweak = NULL;
  g->totalbytes = sizeof(LG);
	/* 关于垃圾回收初始化 --- */
  g->GCdebt = 0;
  g->gcpause = LUAI_GCPAUSE;
  g->gcmajorinc = LUAI_GCMAJOR;
  g->gcstepmul = LUAI_GCMUL;
	/* --- */
	
	/* 定义基础类型的哈希表初始化 */
  for (i=0; i < LUA_NUMTAGS; i++) g->mt[i] = NULL;
	/* 安全的调用f_luaopen函数打开L状态handle */
  if (luaD_rawrunprotected(L, f_luaopen, NULL) != LUA_OK) {
    /* memory allocation error: free partial state */
    close_state(L);
    L = NULL;
  }
  return L;
}

/* 关闭虚拟机 */
LUA_API void lua_close (lua_State *L) {
  L = G(L)->mainthread;  /* only the main thread can be closed */
  lua_lock(L);
  close_state(L);
}


