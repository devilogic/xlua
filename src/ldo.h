/*
** $Id: ldo.h,v 2.20.1.1 2013/04/12 18:48:47 roberto Exp $
** Stack and Call structure of Lua
** See Copyright Notice in lua.h
*/

#ifndef ldo_h
#define ldo_h


#include "lobject.h"
#include "lstate.h"
#include "lzio.h"


#define luaD_checkstack(L,n)	if (L->stack_last - L->top <= (n)) \
				    luaD_growstack(L, n); else condmovestack(L);


#define incr_top(L) {L->top++; luaD_checkstack(L,0);}

/* 保存栈顶指针 
 * L->stack是栈基
 * p 是当前栈指针
 * 返回栈指针的地址
 */
#define savestack(L,p)		((char *)(p) - (char *)L->stack)
/* 返回栈地址n的指针 */
#define restorestack(L,n)	((TValue *)((char *)L->stack + (n)))


/* type of protected functions, to be ran by `runprotected' */
/* 提供给 runprotected函数的 调用函数原型 */
typedef void (*Pfunc) (lua_State *L, void *ud);

LUAI_FUNC int luaD_protectedparser (lua_State *L, ZIO *z, const char *name,
                                                  const char *mode);
LUAI_FUNC void luaD_hook (lua_State *L, int event, int line);
LUAI_FUNC int luaD_precall (lua_State *L, StkId func, int nresults);
LUAI_FUNC void luaD_call (lua_State *L, StkId func, int nResults,
                                        int allowyield);
LUAI_FUNC int luaD_pcall (lua_State *L, Pfunc func, void *u,
                                        ptrdiff_t oldtop, ptrdiff_t ef);
LUAI_FUNC int luaD_poscall (lua_State *L, StkId firstResult);
LUAI_FUNC void luaD_reallocstack (lua_State *L, int newsize);
LUAI_FUNC void luaD_growstack (lua_State *L, int n);
LUAI_FUNC void luaD_shrinkstack (lua_State *L);

LUAI_FUNC l_noret luaD_throw (lua_State *L, int errcode);
LUAI_FUNC int luaD_rawrunprotected (lua_State *L, Pfunc f, void *ud);

#endif

