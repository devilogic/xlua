/*
** $Id: lzio.c,v 1.35.1.1 2013/04/12 18:48:47 roberto Exp $
** Buffered streams
** See Copyright Notice in lua.h
*/


#include <string.h>

#define lzio_c
#define LUA_CORE

#include "lua.h"

#include "llimits.h"
#include "lmem.h"
#include "lstate.h"
#include "lzio.h"

/* 从IO中读取一个字节 */
int luaZ_fill (ZIO *z) {
  size_t size;
  lua_State *L = z->L;         /* 获取lua虚拟机状态 */
  const char *buff;
  lua_unlock(L);
  buff = z->reader(L, z->data, &size);
  lua_lock(L);
  if (buff == NULL || size == 0)
    return EOZ;
  z->n = size - 1;  /* discount char being returned */
  z->p = buff;
  return cast_uchar(*(z->p++));
}

/* IO初始化
 * L 虚拟机状态
 * z IO结构指针
 * reader 读取函数指针
 * data 要关联的缓存指针
 */
void luaZ_init (lua_State *L, ZIO *z, lua_Reader reader, void *data) {
  z->L = L;
  z->reader = reader;
  z->data = data;
  z->n = 0;
  z->p = NULL;
}


/* --------------------------------------------------------------- read --- */
/* 从z中读取n个字节到b中 */
size_t luaZ_read (ZIO *z, void *b, size_t n) {
  while (n) {
    size_t m;
    if (z->n == 0) {  /* no bytes in buffer? */
      if (luaZ_fill(z) == EOZ)  /* try to read more */
        return n;  /* no more input; return number of missing bytes */
      else {
				/* 调用luaZ_fill中会返回缓冲中第一个字节，这里做逆运算将其返回 */
        z->n++;  /* luaZ_fill consumed first byte; put it back */
        z->p--;
      }
    }
		/* 小于等于则返回要读取的长度否则直接返回缓存大小 */
    m = (n <= z->n) ? n : z->n;  /* min. between n and z->n */
    memcpy(b, z->p, m);
    z->n -= m;
    z->p += m;
    b = (char *)b + m;
    n -= m;
  }
  return 0;
}

/* ------------------------------------------------------------------------ */
/* 打开一个内存空间,根据n的大小，动态扩展或者直接打开当前缓存 */
char *luaZ_openspace (lua_State *L, Mbuffer *buff, size_t n) {
  if (n > buff->buffsize) {
    if (n < LUA_MINBUFFER) n = LUA_MINBUFFER;
		/* 重新设定内存大小 */
    luaZ_resizebuffer(L, buff, n);
  }
  return buff->buffer;
}


