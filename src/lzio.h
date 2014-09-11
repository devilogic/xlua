/*
** $Id: lzio.h,v 1.26.1.1 2013/04/12 18:48:47 roberto Exp $
** Buffered streams
** See Copyright Notice in lua.h
*/

/* IO控制的作用是作为缓存与读取中的环节, 用户 <-> IO <-> 缓存
 */
#ifndef lzio_h
#define lzio_h

#include "lua.h"

#include "lmem.h"


/* 数据流末尾 */
#define EOZ	(-1)			/* end of stream */

/* IO结构 */
typedef struct Zio ZIO;

/* 从IO中读取数据，当io中还存在数据则读取，否则调用luaZ_fill从缓存中读取 */
#define zgetc(z)  (((z)->n--)>0 ?  cast_uchar(*(z)->p++) : luaZ_fill(z))

/* 内存空间结构 */
typedef struct Mbuffer {
  char *buffer;           /* 内存指针 */
  size_t n;               /* 当前内存的索引 */
  size_t buffsize;        /* 内存总大小 */
} Mbuffer;

#define luaZ_initbuffer(L, buff) ((buff)->buffer = NULL, (buff)->buffsize = 0)

#define luaZ_buffer(buff)	((buff)->buffer)
#define luaZ_sizebuffer(buff)	((buff)->buffsize)
#define luaZ_bufflen(buff)	((buff)->n)

#define luaZ_resetbuffer(buff) ((buff)->n = 0)

/* 对缓存buff进行重新分配大小 */
#define luaZ_resizebuffer(L, buff, size) \
	(luaM_reallocvector(L, (buff)->buffer, (buff)->buffsize, size, char), \
	(buff)->buffsize = size)

/* 对缓存buff进行空间释放 */
#define luaZ_freebuffer(L, buff)	luaZ_resizebuffer(L, buff, 0)


LUAI_FUNC char *luaZ_openspace (lua_State *L, Mbuffer *buff, size_t n);
LUAI_FUNC void luaZ_init (lua_State *L, ZIO *z, lua_Reader reader,
                                        void *data);
LUAI_FUNC size_t luaZ_read (ZIO* z, void* b, size_t n);	/* read next n bytes */



/* --------- Private Part ------------------ */
/* 私有IO结构 */
struct Zio {
	/* io中还未读取的长度 */
  size_t n;			/* bytes still unread */
	/* 在缓冲中的当前位置 */
  const char *p;		/* current position in buffer */
	/* 读取函数 */
  lua_Reader reader;		/* reader function */
	/* 附加数据 */
  void* data;			/* additional data */
	/* lua虚拟机状态 */
  lua_State *L;			/* Lua state (for reader) */
};


LUAI_FUNC int luaZ_fill (ZIO *z);

#endif
