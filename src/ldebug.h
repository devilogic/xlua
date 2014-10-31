/*
** $Id: ldebug.h,v 2.7.1.1 2013/04/12 18:48:47 roberto Exp $
** Auxiliary functions from Debug Interface module
** See Copyright Notice in lua.h
*/

#ifndef ldebug_h
#define ldebug_h


#include "lstate.h"

/* pc寄存器重定位 */
#define pcRel(pc, p)	(cast(int, (pc) - (p)->code) - 1)

/* 获取函数的行信息 */
#define getfuncline(f,pc)	(((f)->lineinfo) ? (f)->lineinfo[pc] : 0)

/* 重新设置可HOOK的计数 */
#define resethookcount(L)	(L->hookcount = L->basehookcount)

/* Active Lua function (given call info) */
/* 活动的Lua函数 */
#define ci_func(ci)		(clLvalue((ci)->func))


LUAI_FUNC l_noret luaG_typeerror (lua_State *L, const TValue *o,
                                                const char *opname);
LUAI_FUNC l_noret luaG_concaterror (lua_State *L, StkId p1, StkId p2);
LUAI_FUNC l_noret luaG_aritherror (lua_State *L, const TValue *p1,
                                                 const TValue *p2);
LUAI_FUNC l_noret luaG_ordererror (lua_State *L, const TValue *p1,
                                                 const TValue *p2);
LUAI_FUNC l_noret luaG_runerror (lua_State *L, const char *fmt, ...);
LUAI_FUNC l_noret luaG_errormsg (lua_State *L);

#endif
