/*
** $Id: ltable.h,v 2.16.1.2 2013/08/30 15:49:41 roberto Exp $
** Lua tables (hash)
** See Copyright Notice in lua.h
*/

/* 此源文件为语言内哈希表数据类型的定义 */

#ifndef ltable_h
#define ltable_h

#include "lobject.h"

/* 从表t中获取第i个节点 */
#define gnode(t,i)	(&(t)->node[i])
/* 获取节点n的健 */
#define gkey(n)		(&(n)->i_key.tvk)
/* 获取节点n的值 */
#define gval(n)		(&(n)->i_val)
/* 获取下一个节点 */
#define gnext(n)	((n)->i_key.nk.next)
/* 清空元操作 */
#define invalidateTMcache(t)	((t)->flags = 0)

/* returns the key, given the value of a table entry */
#define keyfromval(v) \
  (gkey(cast(Node *, cast(char *, (v)) - offsetof(Node, i_val))))

/* 获取整型key的值 */
LUAI_FUNC const TValue *luaH_getint (Table *t, int key);
/* 设置整型key的值 */
LUAI_FUNC void luaH_setint (lua_State *L, Table *t, int key, TValue *value);
/* 获取字符串型key的值 */
LUAI_FUNC const TValue *luaH_getstr (Table *t, TString *key);
/* 获取任意值类型key的值 */
LUAI_FUNC const TValue *luaH_get (Table *t, const TValue *key);
/* 创建一个新的key 
 * L 虚拟机状态指针
 * t 哈希表指针
 * key 新健健的值
 */
LUAI_FUNC TValue *luaH_newkey (lua_State *L, Table *t, const TValue *key);
/* 对一个key进行值设置 */
LUAI_FUNC TValue *luaH_set (lua_State *L, Table *t, const TValue *key);
/* 创建一个新的哈希表 */
LUAI_FUNC Table *luaH_new (lua_State *L);
/* 重新设定表的长度 */
LUAI_FUNC void luaH_resize (lua_State *L, Table *t, int nasize, int nhsize);
/* 重新设定表队列的长度 */
LUAI_FUNC void luaH_resizearray (lua_State *L, Table *t, int nasize);
/* 释放表空间 */
LUAI_FUNC void luaH_free (lua_State *L, Table *t);
/* 获取下一个表中的健 */
LUAI_FUNC int luaH_next (lua_State *L, Table *t, StkId key);
/*  */
LUAI_FUNC int luaH_getn (Table *t);


#if defined(LUA_DEBUG)
LUAI_FUNC Node *luaH_mainposition (const Table *t, const TValue *key);
/* 是否是空表 */
LUAI_FUNC int luaH_isdummy (Node *n);
#endif


#endif
