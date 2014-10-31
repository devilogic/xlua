/*
** $Id: lobject.c,v 2.58.1.1 2013/04/12 18:48:47 roberto Exp $
** Some generic functions over Lua objects
** See Copyright Notice in lua.h
*/

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define lobject_c
#define LUA_CORE

#include "lua.h"

#include "lctype.h"
#include "ldebug.h"
#include "ldo.h"
#include "lmem.h"
#include "lobject.h"
#include "lstate.h"
#include "lstring.h"
#include "lvm.h"


/* 定义空值 */
LUAI_DDEF const TValue luaO_nilobject_ = {NILCONSTANT};


/*
** converts an integer to a "floating point byte", represented as
** (eeeeexxx), where the real value is (1xxx) * 2^(eeeee - 1) if
** eeeee != 0 and (xxx) otherwise.
*/
/* 将一个整型转换为一个"浮点字节" */
int luaO_int2fb (unsigned int x) {
	/* e为指数部分 */
  int e = 0;  /* exponent */
  if (x < 8) return x;
  while (x >= 0x10) {
    x = (x+1) >> 1;
    e++;
  }
  return ((e+1) << 3) | (cast_int(x) - 8);
}


/* converts back */
/* lua0_fb2int的逆运算 */
int luaO_fb2int (int x) {
  int e = (x >> 3) & 0x1f;
  if (e == 0) return x;
  else return ((x & 7) + 8) << (e - 1);
}


int luaO_ceillog2 (unsigned int x) {
	/* 1-256 以2为底的指数表*/
  static const lu_byte log_2[256] = {
    0,1,2,2,3,3,3,3,4,4,4,4,4,4,4,4,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,
    6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
    7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
    7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
    8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
    8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
    8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
    8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8
  };
  int l = 0;
  x--;
  while (x >= 256) { l += 8; x >>= 8; }
  return l + log_2[x];
}

/* 进行算数操作 */
lua_Number luaO_arith (int op, lua_Number v1, lua_Number v2) {
  switch (op) {
    case LUA_OPADD: return luai_numadd(NULL, v1, v2);    /* 加 */
    case LUA_OPSUB: return luai_numsub(NULL, v1, v2);    /* 减 */
    case LUA_OPMUL: return luai_nummul(NULL, v1, v2);    /* 乘 */
    case LUA_OPDIV: return luai_numdiv(NULL, v1, v2);    /* 除 */
    case LUA_OPMOD: return luai_nummod(NULL, v1, v2);    /* 模 */
    case LUA_OPPOW: return luai_numpow(NULL, v1, v2);    /* 平方 */
    case LUA_OPUNM: return luai_numunm(NULL, v1);        
    default: lua_assert(0); return 0;
  }
}

/* 将asnii字符的16进制c转换乘对应的整型值 */
int luaO_hexavalue (int c) {
  if (lisdigit(c)) return c - '0';
  else return ltolower(c) - 'a' + 10;
}

/* 这里定义 lua_strx2number函数,将字符串转换成整型 */
#if !defined(lua_strx2number)

#include <math.h>


static int isneg (const char **s) {
  if (**s == '-') { (*s)++; return 1; }
  else if (**s == '+') (*s)++;
  return 0;
}


static lua_Number readhexa (const char **s, lua_Number r, int *count) {
  for (; lisxdigit(cast_uchar(**s)); (*s)++) {  /* read integer part */
    r = (r * cast_num(16.0)) + cast_num(luaO_hexavalue(cast_uchar(**s)));
    (*count)++;
  }
  return r;
}


/*
** convert an hexadecimal numeric string to a number, following
** C99 specification for 'strtod'
*/
/* 字符串转数字 */
static lua_Number lua_strx2number (const char *s, char **endptr) {
  lua_Number r = 0.0;
  int e = 0, i = 0;
  int neg = 0;  /* 1 if number is negative */
  *endptr = cast(char *, s);  /* nothing is valid yet */
  while (lisspace(cast_uchar(*s))) s++;  /* skip initial spaces */
  neg = isneg(&s);  /* check signal */
  if (!(*s == '0' && (*(s + 1) == 'x' || *(s + 1) == 'X')))  /* check '0x' */
    return 0.0;  /* invalid format (no '0x') */
  s += 2;  /* skip '0x' */
  r = readhexa(&s, r, &i);  /* read integer part */
  if (*s == '.') {
    s++;  /* skip dot */
    r = readhexa(&s, r, &e);  /* read fractional part */
  }
  if (i == 0 && e == 0)
    return 0.0;  /* invalid format (no digit) */
  e *= -4;  /* each fractional digit divides value by 2^-4 */
  *endptr = cast(char *, s);  /* valid up to here */
  if (*s == 'p' || *s == 'P') {  /* exponent part? */
    int exp1 = 0;
    int neg1;
    s++;  /* skip 'p' */
    neg1 = isneg(&s);  /* signal */
    if (!lisdigit(cast_uchar(*s)))
      goto ret;  /* must have at least one digit */
    while (lisdigit(cast_uchar(*s)))  /* read exponent */
      exp1 = exp1 * 10 + *(s++) - '0';
    if (neg1) exp1 = -exp1;
    e += exp1;
  }
  *endptr = cast(char *, s);  /* valid up to here */
 ret:
  if (neg) r = -r;
  return l_mathop(ldexp)(r, e);
}

#endif

/* 将字符串转成数字
 * s 是字符串缓冲区
 * len 字符串长度
 * result 是转化成数字
 */
int luaO_str2d (const char *s, size_t len, lua_Number *result) {
  char *endptr;
	/* 判断字符串是否是无限 'nN' */
  if (strpbrk(s, "nN"))  /* reject 'inf' and 'nan' */
    return 0;
	/* 字符串是16进制 */
  else if (strpbrk(s, "xX"))  /* hexa? */
    *result = lua_strx2number(s, &endptr);
  else
    *result = lua_str2number(s, &endptr);
  if (endptr == s) return 0;  /* nothing recognized */
  while (lisspace(cast_uchar(*endptr))) endptr++;
  return (endptr == s + len);  /* OK if no trailing characters */
}


/* 对栈中压入字符串 */
static void pushstr (lua_State *L, const char *str, size_t l) {
  setsvalue2s(L, L->top++, luaS_newlstr(L, str, l));
}


/* this function handles only `%d', `%c', %f, %p, and `%s' formats */
/* 压入一个带有格式化类型字符串 */
const char *luaO_pushvfstring (lua_State *L, const char *fmt, va_list argp) {
  int n = 0;
	/* 遍历整个字符串 */
  for (;;) {
    const char *e = strchr(fmt, '%');
    if (e == NULL) break;
		/* 检查栈中是否可以存放 格式化字符串 + 项目 */
    luaD_checkstack(L, 2);  /* fmt + item */
		/* 压入格式化字符串 */
    pushstr(L, fmt, e - fmt);
		/* 跳过%号并判断类型 */
    switch (*(e+1)) {
      case 's': {
        const char *s = va_arg(argp, char *);
        if (s == NULL) s = "(null)";
        pushstr(L, s, strlen(s));
        break;
      }
      case 'c': {
        char buff;
        buff = cast(char, va_arg(argp, int));
        pushstr(L, &buff, 1);
        break;
      }
      case 'd': {
        setnvalue(L->top++, cast_num(va_arg(argp, int)));
        break;
      }
      case 'f': {
        setnvalue(L->top++, cast_num(va_arg(argp, l_uacNumber)));
        break;
      }
      case 'p': {
        char buff[4*sizeof(void *) + 8]; /* should be enough space for a `%p' */
        int l = sprintf(buff, "%p", va_arg(argp, void *));
        pushstr(L, buff, l);
        break;
      }
      case '%': {
        pushstr(L, "%", 1);
        break;
      }
      default: {
        luaG_runerror(L,
            "invalid option " LUA_QL("%%%c") " to " LUA_QL("lua_pushfstring"),
            *(e + 1));
      }
    }
    n += 2;   /* 每次增加前一个常量字符串 ＋ 变量字符串的个数 */
		/* 跳过%x */
    fmt = e+2;
  }
	/* 增大栈的大小 */
  luaD_checkstack(L, 1);
	/* 压入剩余的字符串 */
  pushstr(L, fmt, strlen(fmt));
	/* 链接栈中的字符串 */
  if (n > 0) luaV_concat(L, n + 1);
  return svalue(L->top - 1);
}

/* 压入格式化字符串外层函数
 * L 虚拟机状态
 * fmt 字符串格式
 * ... 参数
 */
const char *luaO_pushfstring (lua_State *L, const char *fmt, ...) {
  const char *msg;
  va_list argp;
  va_start(argp, fmt);
  msg = luaO_pushvfstring(L, fmt, argp);
  va_end(argp);
  return msg;
}


/* number of chars of a literal string without the ending \0 */
#define LL(x)	(sizeof(x)/sizeof(char) - 1)

#define RETS	"..."
#define PRE	"[string \""
#define POS	"\"]"

/* 复制b到a，l个字节，并且a增加l */
#define addstr(a,b,l)	( memcpy(a,b,(l) * sizeof(char)), a += (l) )
/* 按照不同的类型复制source到out
 * bufflen是out的大小
 */
void luaO_chunkid (char *out, const char *source, size_t bufflen) {
  size_t l = strlen(source);
	/* 文本源 */
  if (*source == '=') {  /* 'literal' source */
		/* 要复制的数据比缓存小则直接复制 */
    if (l <= bufflen)  /* small enough? */
      memcpy(out, source + 1, l * sizeof(char));
    else {  /* truncate it */
			/* 仅复制bufflen-1个,阶段字符串 */
      addstr(out, source + 1, bufflen - 1);
      *out = '\0';
    }
  }
	/* 文件名 */
  else if (*source == '@') {  /* file name */
    if (l <= bufflen)  /* small enough? */
      memcpy(out, source + 1, l * sizeof(char));
    else {  /* add '...' before rest of name */
			/* 如果缓存不够大，则在之前添加 '...'字符 */
      addstr(out, RETS, LL(RETS));
      bufflen -= LL(RETS);
      memcpy(out, source + 1 + l - bufflen, bufflen * sizeof(char));
    }
  }
	/* 字符串,按照\n添加 */
  else {  /* string; format as [string "source"] */
    const char *nl = strchr(source, '\n');  /* find first new line (if any) */
		/* 添加[string \ 前缀 */
    addstr(out, PRE, LL(PRE));  /* add prefix */
    bufflen -= LL(PRE RETS POS) + 1;  /* save space for prefix+suffix+'\0' */
		/* 小于缓存则直接添加 */
    if (l < bufflen && nl == NULL) {  /* small one-line source? */
      addstr(out, source, l);  /* keep it */
    }
    else {
			/* 找到一行字符串的长度 */
      if (nl != NULL) l = nl - source;  /* stop at first newline */
      if (l > bufflen) l = bufflen;
      addstr(out, source, l);
      addstr(out, RETS, LL(RETS));
    }
		/* 最后添加一个 ] 号 */
    memcpy(out, POS, (LL(POS) + 1) * sizeof(char));
  }
}

