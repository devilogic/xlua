/*
** $Id: ltable.c,v 2.72.1.1 2013/04/12 18:48:47 roberto Exp $
** Lua tables (hash)
** See Copyright Notice in lua.h
*/


/*
** Implementation of tables (aka arrays, objects, or hash tables).
** Tables keep its elements in two parts: an array part and a hash part.
** Non-negative integer keys are all candidates to be kept in the array
** part. The actual size of the array is the largest `n' such that at
** least half the slots between 0 and n are in use.
** Hash uses a mix of chained scatter table with Brent's variation.
** A main invariant of these tables is that, if an element is not
** in its main position (i.e. the `original' position that its hash gives
** to it), then the colliding element is in its own main position.
** Hence even when the load factor reaches 100%, performance remains good.
*/

#include <string.h>

#define ltable_c
#define LUA_CORE

#include "lua.h"

#include "ldebug.h"
#include "ldo.h"
#include "lgc.h"
#include "lmem.h"
#include "lobject.h"
#include "lstate.h"
#include "lstring.h"
#include "ltable.h"
#include "lvm.h"


/*
** max size of array part is 2^MAXBITS
*/
#if LUAI_BITSINT >= 32
#define MAXBITS		30
#else
#define MAXBITS		(LUAI_BITSINT-2)
#endif

#define MAXASIZE	(1 << MAXBITS)

/* t 哈希表
 * n 节点的数量
 */
#define hashpow2(t,n)		(gnode(t, lmod((n), sizenode(t))))

/* 通过字符串类型的哈希值 */
#define hashstr(t,str)		hashpow2(t, (str)->tsv.hash)
#define hashboolean(t,p)	hashpow2(t, p)


/*
** for some types, it is better to avoid modulus by power of 2, as
** they tend to have many 2 factors.
*/
/* 进行哈希表模个数的运算 
 * ((sizenode(t)-1)|1) 保证操作数不为0
 */
#define hashmod(t,n)	(gnode(t, ((n) % ((sizenode(t)-1)|1))))

/* 哈希指针值 */
#define hashpointer(t,p)	hashmod(t, IntPoint(p))

/* 空哈希节点 */
#define dummynode		(&dummynode_)
/* 阶段n是否为空 */
#define isdummy(n)		((n) == dummynode)
/* 空的哈希节点 */
static const Node dummynode_ = {
  {NILCONSTANT},  /* value */
  {{NILCONSTANT, NULL}}  /* key */
};


/*
** hash for lua_Numbers
*/
/* 使用lua_Numbers进行哈希算法
 * t 哈希表
 * n lua_Number类型
 */
static Node *hashnum (const Table *t, lua_Number n) {
  int i;
  luai_hashnum(i, n);
	/* 如果计算出的i最高位为1 */
  if (i < 0) {
		/* 强制转换成 */
    if (cast(unsigned int, i) == 0u - i)  /* use unsigned to avoid overflows */
      i = 0;  /* handle INT_MIN */
		/* 必须是整值 */
    i = -i;  /* must be a positive value */
  }
	/* 获取哈希树的节点位置 */
  return hashmod(t, i);
}


/*
** returns the `main' position of an element in a table (that is, the index
** of its hash value)
*/
/* 返回在哈希表中的一个元素的主位置
 * t 哈希表指针
 * key 健的值
 * 返回新的节点
 */
static Node *mainposition (const Table *t, const TValue *key) {
	/* 判断新值的类型 */
  switch (ttype(key)) {
		/* 数字型 */
    case LUA_TNUMBER:
      return hashnum(t, nvalue(key));
		/* 长字符串类型 */
    case LUA_TLNGSTR: {
			/* 获取字符串类型指针 */
      TString *s = rawtsvalue(key);
			/* 如果字符串类型没有哈希值 */
      if (s->tsv.extra == 0) {  /* no hash? */
				/* 计算哈希值 */
        s->tsv.hash = luaS_hash(getstr(s), s->tsv.len, s->tsv.hash);
        s->tsv.extra = 1;  /* now it has its hash */
      }
			/* 以哈希值再次进行哈希 */
      return hashstr(t, rawtsvalue(key));
    }
		/* 短字符串直接进行哈希 */
    case LUA_TSHRSTR:
      return hashstr(t, rawtsvalue(key));
		/* 布尔值进行哈希 */
    case LUA_TBOOLEAN:
      return hashboolean(t, bvalue(key));
		/* 轻型用户数据类型 */
    case LUA_TLIGHTUSERDATA:
      return hashpointer(t, pvalue(key));
		/* 函数类型 */
    case LUA_TLCF:
      return hashpointer(t, fvalue(key));
		/* 默认直接哈希可回收类型 */
    default:
      return hashpointer(t, gcvalue(key));
  }
}


/*
** returns the index for `key' if `key' is an appropriate key to live in
** the array part of the table, -1 otherwise.
*/
static int arrayindex (const TValue *key) {
  if (ttisnumber(key)) {
    lua_Number n = nvalue(key);
    int k;
    lua_number2int(k, n);
    if (luai_numeq(cast_num(k), n))
      return k;
  }
  return -1;  /* `key' did not match some condition */
}


/*
** returns the index of a `key' for table traversals. First goes all
** elements in the array part, then elements in the hash part. The
** beginning of a traversal is signaled by -1.
*/
static int findindex (lua_State *L, Table *t, StkId key) {
  int i;
  if (ttisnil(key)) return -1;  /* first iteration */
  i = arrayindex(key);
  if (0 < i && i <= t->sizearray)  /* is `key' inside array part? */
    return i-1;  /* yes; that's the index (corrected to C) */
  else {
    Node *n = mainposition(t, key);
    for (;;) {  /* check whether `key' is somewhere in the chain */
      /* key may be dead already, but it is ok to use it in `next' */
      if (luaV_rawequalobj(gkey(n), key) ||
            (ttisdeadkey(gkey(n)) && iscollectable(key) &&
             deadvalue(gkey(n)) == gcvalue(key))) {
        i = cast_int(n - gnode(t, 0));  /* key index in hash table */
        /* hash elements are numbered after array ones */
        return i + t->sizearray;
      }
      else n = gnext(n);
      if (n == NULL)
        luaG_runerror(L, "invalid key to " LUA_QL("next"));  /* key not found */
    }
  }
}


int luaH_next (lua_State *L, Table *t, StkId key) {
  int i = findindex(L, t, key);  /* find original element */
  for (i++; i < t->sizearray; i++) {  /* try first array part */
    if (!ttisnil(&t->array[i])) {  /* a non-nil value? */
      setnvalue(key, cast_num(i+1));
      setobj2s(L, key+1, &t->array[i]);
      return 1;
    }
  }
  for (i -= t->sizearray; i < sizenode(t); i++) {  /* then hash part */
    if (!ttisnil(gval(gnode(t, i)))) {  /* a non-nil value? */
      setobj2s(L, key, gkey(gnode(t, i)));
      setobj2s(L, key+1, gval(gnode(t, i)));
      return 1;
    }
  }
  return 0;  /* no more elements */
}


/*
** {=============================================================
** Rehash
** ==============================================================
*/


static int computesizes (int nums[], int *narray) {
  int i;
  int twotoi;  /* 2^i */
  int a = 0;  /* number of elements smaller than 2^i */
  int na = 0;  /* number of elements to go to array part */
  int n = 0;  /* optimal size for array part */
  for (i = 0, twotoi = 1; twotoi/2 < *narray; i++, twotoi *= 2) {
    if (nums[i] > 0) {
      a += nums[i];
      if (a > twotoi/2) {  /* more than half elements present? */
        n = twotoi;  /* optimal size (till now) */
        na = a;  /* all elements smaller than n will go to array part */
      }
    }
    if (a == *narray) break;  /* all elements already counted */
  }
  *narray = n;
  lua_assert(*narray/2 <= na && na <= *narray);
  return na;
}


static int countint (const TValue *key, int *nums) {
  int k = arrayindex(key);
  if (0 < k && k <= MAXASIZE) {  /* is `key' an appropriate array index? */
    nums[luaO_ceillog2(k)]++;  /* count as such */
    return 1;
  }
  else
    return 0;
}


static int numusearray (const Table *t, int *nums) {
  int lg;
  int ttlg;  /* 2^lg */
  int ause = 0;  /* summation of `nums' */
  int i = 1;  /* count to traverse all array keys */
  for (lg=0, ttlg=1; lg<=MAXBITS; lg++, ttlg*=2) {  /* for each slice */
    int lc = 0;  /* counter */
    int lim = ttlg;
    if (lim > t->sizearray) {
      lim = t->sizearray;  /* adjust upper limit */
      if (i > lim)
        break;  /* no more elements to count */
    }
    /* count elements in range (2^(lg-1), 2^lg] */
    for (; i <= lim; i++) {
      if (!ttisnil(&t->array[i-1]))
        lc++;
    }
    nums[lg] += lc;
    ause += lc;
  }
  return ause;
}


static int numusehash (const Table *t, int *nums, int *pnasize) {
  int totaluse = 0;  /* total number of elements */
  int ause = 0;  /* summation of `nums' */
  int i = sizenode(t);
  while (i--) {
    Node *n = &t->node[i];
    if (!ttisnil(gval(n))) {
      ause += countint(gkey(n), nums);
      totaluse++;
    }
  }
  *pnasize += ause;
  return totaluse;
}

/* 设置哈希表队列向量
 * L 虚拟机状态
 * t 哈希表
 * size 队列个数
 */
static void setarrayvector (lua_State *L, Table *t, int size) {
  int i;
	/* 重新分配size个TValue对象 */
  luaM_reallocvector(L, t->array, t->sizearray, size, TValue);
  for (i=t->sizearray; i<size; i++)
     setnilvalue(&t->array[i]);
  t->sizearray = size;
}

/* 设置哈希表节点队列
 * L 虚拟机状态指针
 * t 哈希表
 * size 指定有多少个元素在哈希表部分
 */
static void setnodevector (lua_State *L, Table *t, int size) {
  int lsize;
  if (size == 0) {  /* no elements to hash part? */
    t->node = cast(Node *, dummynode);  /* use common `dummynode' */
    lsize = 0;
  }
  else {
    int i;
		/* 计算size以2为底的对数 */
    lsize = luaO_ceillog2(size);
    if (lsize > MAXBITS)
      luaG_runerror(L, "table overflow");
		/* 还原真实的大小 */
    size = twoto(lsize);
		/* 分配节点内存 */
    t->node = luaM_newvector(L, size, Node);
		/* 遍历所有节点，设置空值 */
    for (i=0; i<size; i++) {
      Node *n = gnode(t, i);
      gnext(n) = NULL;
      setnilvalue(gkey(n));
      setnilvalue(gval(n));
    }
  }
	/* 设置节点的数量 */
  t->lsizenode = cast_byte(lsize);
	/* 所有的节点是空闲的 */
  t->lastfree = gnode(t, size);  /* all positions are free */
}

/* 重新设置哈希表大小
 * L 虚拟机状态
 * t 哈希表指针
 * nasize 节点队列个数
 * nhsize 节点向量个数
 */
void luaH_resize (lua_State *L, Table *t, int nasize, int nhsize) {
  int i;
  int oldasize = t->sizearray;     /* 队列长度 */
  int oldhsize = t->lsizenode;     /* 节点个数 */
	
	/* 获取节点队列 */
  Node *nold = t->node;  /* save old hash ... */
	/* 新设定的队列大小大于原来的,则增长队列 */
  if (nasize > oldasize)  /* array part must grow? */
    setarrayvector(L, t, nasize);
  /* create new hash part with appropriate size */
	/* 设置节点向量 */
  setnodevector(L, t, nhsize);
	/* 如果新的队列小于旧的队列 */
  if (nasize < oldasize) {  /* array part must shrink? */
    t->sizearray = nasize;
    /* re-insert elements from vanishing slice */
		/* 将队列中超出的部分设置为空值 */
    for (i=nasize; i<oldasize; i++) {
			/* 如果节点非空对象 */
      if (!ttisnil(&t->array[i]))
        luaH_setint(L, t, i + 1, &t->array[i]);
    }
    /* shrink array */
    luaM_reallocvector(L, t->array, oldasize, nasize, TValue);
  }
  /* re-insert elements from hash part */
  for (i = twoto(oldhsize) - 1; i >= 0; i--) {
    Node *old = nold+i;
    if (!ttisnil(gval(old))) {
      /* doesn't need barrier/invalidate cache, as entry was
         already present in the table */
      setobjt2t(L, luaH_set(L, t, gkey(old)), gval(old));
    }
  }
  if (!isdummy(nold))
    luaM_freearray(L, nold, cast(size_t, twoto(oldhsize))); /* free old array */
}


void luaH_resizearray (lua_State *L, Table *t, int nasize) {
  int nsize = isdummy(t->node) ? 0 : sizenode(t);
  luaH_resize(L, t, nasize, nsize);
}

/* 增长哈希表内存
 * L 虚拟机状态指针
 * t 哈希表指针
 * ek
 */
static void rehash (lua_State *L, Table *t, const TValue *ek) {
  int nasize, na;
  int nums[MAXBITS+1];  /* nums[i] = number of keys with 2^(i-1) < k <= 2^i */
  int i;
  int totaluse;
	/* 遍历所有健 */
  for (i=0; i<=MAXBITS; i++) nums[i] = 0;  /* reset counts */
	/* 计算健在队列部分 */
  nasize = numusearray(t, nums);  /* count keys in array part */
  totaluse = nasize;  /* all those keys are integer keys */
  totaluse += numusehash(t, nums, &nasize);  /* count keys in hash part */
  /* count extra key */
  nasize += countint(ek, nums);
  totaluse++;
  /* compute new size for array part */
  na = computesizes(nums, &nasize);
  /* resize the table to new computed sizes */
  luaH_resize(L, t, nasize, totaluse - na);
}



/*
** }=============================================================
*/

/* 创建新的哈希表 */
Table *luaH_new (lua_State *L) {
	/* 创建哈希表对象 */
  Table *t = &luaC_newobj(L, LUA_TTABLE, sizeof(Table), NULL, 0)->h;
  t->metatable = NULL;
  t->flags = cast_byte(~0);
  t->array = NULL;
  t->sizearray = 0;
	/* 真正的初始化表 */
  setnodevector(L, t, 0);
  return t;
}


void luaH_free (lua_State *L, Table *t) {
  if (!isdummy(t->node))
    luaM_freearray(L, t->node, cast(size_t, sizenode(t)));
  luaM_freearray(L, t->array, t->sizearray);
  luaM_free(L, t);
}

/* 从一个哈希表中获取空闲位置 */
static Node *getfreepos (Table *t) {
	/* 循环遍历直到所有节点遍历完成 */
  while (t->lastfree > t->node) {
    t->lastfree--;      /* 递减 */
		/* 如果为空则直接返回 */
    if (ttisnil(gkey(t->lastfree)))
      return t->lastfree;
  }
  return NULL;  /* could not find a free place */
}



/*
** inserts a new key into a hash table; first, check whether key's main
** position is free. If not, check whether colliding node is in its main
** position or not: if it is not, move colliding node to an empty place and
** put new key in its main position; otherwise (colliding node is in its main
** position), new key goes to an empty position.
*/
/* 插入一个新的健到一个哈希表中；首先，检查健的主位置是否为空。如果不为空，检查
 * 冲突节点是否在他的主位置是否为空或者空闲：如果不是空闲，移动冲突的节点到一个空
 * 的位置并且放置一个新的健在它的主位置；或者新健到一个空的位置。
 */
TValue *luaH_newkey (lua_State *L, Table *t, const TValue *key) {
  Node *mp;
	/* 如果健的值为空则抛出异常 */
  if (ttisnil(key)) luaG_runerror(L, "table index is nil");
	/* 健的值是数字并且数字为nan */
  else if (ttisnumber(key) && luai_numisnan(L, nvalue(key)))
    luaG_runerror(L, "table index is NaN");
	/* 通过计算得到一个位置 */
  mp = mainposition(t, key);
	
	/* 主位置被占用或者空值 */
  if (!ttisnil(gval(mp)) || isdummy(mp)) {  /* main position is taken? */
    Node *othern;
		/* 获取一个空闲位置 */
    Node *n = getfreepos(t);  /* get a free place */
		/* 如果找不到一个空闲的位置，则增长哈希表内存 */
    if (n == NULL) {  /* cannot find a free place? */
			/* 重新增长哈希表 */
      rehash(L, t, key);  /* grow table */
      /* whatever called 'newkey' take care of TM cache and GC barrier */
			/* 塞入一个健到表中 */
      return luaH_set(L, t, key);  /* insert key into grown table */
    }
    lua_assert(!isdummy(n));
    othern = mainposition(t, gkey(mp));
    if (othern != mp) {  /* is colliding node out of its main position? */
      /* yes; move colliding node into free position */
      while (gnext(othern) != mp) othern = gnext(othern);  /* find previous */
      gnext(othern) = n;  /* redo the chain with `n' in place of `mp' */
      *n = *mp;  /* copy colliding node into free pos. (mp->next also goes) */
      gnext(mp) = NULL;  /* now `mp' is free */
      setnilvalue(gval(mp));
    }
    else {  /* colliding node is in its own main position */
      /* new node will go into free position */
      gnext(n) = gnext(mp);  /* chain new position */
      gnext(mp) = n;
      mp = n;
    }
  }
  setobj2t(L, gkey(mp), key);
  luaC_barrierback(L, obj2gco(t), key);
  lua_assert(ttisnil(gval(mp)));
  return gval(mp);
}


/*
** search function for integers
*/
/* 搜索整型健的值
 * t 哈希表指针
 * key 要获取的整型健,这个值总要比要检测的值+1
 */
const TValue *luaH_getint (Table *t, int key) {
  /* (1 <= key && key <= t->sizearray) */
	/* 如果整型健值小于队列长度,直接从队列中取得值 */
  if (cast(unsigned int, key-1) < cast(unsigned int, t->sizearray))
    return &t->array[key-1];
  else {
		/* 转换健成lua数字类型 */
    lua_Number nk = cast_num(key);
		/* 进行哈希算法并返回节点 */
    Node *n = hashnum(t, nk);
    do {  /* check whether `key' is somewhere in the chain */
      if (ttisnumber(gkey(n)) && luai_numeq(nvalue(gkey(n)), nk))
        return gval(n);  /* that's it */
      else n = gnext(n);
    } while (n);
    return luaO_nilobject;
  }
}


/*
** search function for short strings
*/
/* 寻找短字符串作为key的值
 * t 哈希表
 * key 健
 */
const TValue *luaH_getstr (Table *t, TString *key) {
  Node *n = hashstr(t, key);
  lua_assert(key->tsv.tt == LUA_TSHRSTR);
  do {  /* check whether `key' is somewhere in the chain */
    if (ttisshrstring(gkey(n)) && eqshrstr(rawtsvalue(gkey(n)), key))
      return gval(n);  /* that's it */
    else n = gnext(n);
  } while (n);
  return luaO_nilobject;
}


/*
** main search function
*/
/* 获取一个健的值
 * t 哈希表指针
 * key 要获取的健
 */
const TValue *luaH_get (Table *t, const TValue *key) {
	/* 判断健的类型 */
  switch (ttype(key)) {
		/* 字符串类型 */
    case LUA_TSHRSTR: return luaH_getstr(t, rawtsvalue(key));
		/* 空值类型 */
    case LUA_TNIL: return luaO_nilobject;
		/* 数字类型 */
    case LUA_TNUMBER: {
      int k;
      lua_Number n = nvalue(key);          /* 获取哈希表键的值 */
      lua_number2int(k, n);                /* 转换lua_number类型到整型 */
			/* 如果两个值相等 */
      if (luai_numeq(cast_num(k), n)) /* index is int? */
				/* 从整型key中获取值 */
        return luaH_getint(t, k);  /* use specialized version */
      /* else go through */
    }
		/* 默认 */
    default: {
			/* 找到哈希表的键的节点位置 */
      Node *n = mainposition(t, key);
      do {  /* check whether `key' is somewhere in the chain */
				/* 对比两个对象的键是否相等 */
        if (luaV_rawequalobj(gkey(n), key))
          return gval(n);  /* that's it */
        else n = gnext(n);
      } while (n);
			
			/* 找不到返回n */
      return luaO_nilobject;
    }
  }
}


/*
** beware: when using this function you probably need to check a GC
** barrier and invalidate the TM cache.
*/
/* 设置一个健的值
 * L 虚拟机状态指针
 * t 哈希表指针
 * key 要设置的健
 */
TValue *luaH_set (lua_State *L, Table *t, const TValue *key) {
	/* 从健中获取值 */
  const TValue *p = luaH_get(t, key);
  if (p != luaO_nilobject)
    return cast(TValue *, p);
  else return luaH_newkey(L, t, key);
}

/* 设置整型健的值
 * L 虚拟机状态
 * t 哈希表
 * key 哈希健
 * value 哈希值的指针指针
 */
void luaH_setint (lua_State *L, Table *t, int key, TValue *value) {
  const TValue *p = luaH_getint(t, key);
  TValue *cell;
  if (p != luaO_nilobject)
    cell = cast(TValue *, p);
  else {
    TValue k;
    setnvalue(&k, cast_num(key));
    cell = luaH_newkey(L, t, &k);
  }
  setobj2t(L, cell, value);
}


static int unbound_search (Table *t, unsigned int j) {
  unsigned int i = j;  /* i is zero or a present index */
  j++;
  /* find `i' and `j' such that i is present and j is not */
  while (!ttisnil(luaH_getint(t, j))) {
    i = j;
    j *= 2;
    if (j > cast(unsigned int, MAX_INT)) {  /* overflow? */
      /* table was built with bad purposes: resort to linear search */
      i = 1;
      while (!ttisnil(luaH_getint(t, i))) i++;
      return i - 1;
    }
  }
  /* now do a binary search between them */
  while (j - i > 1) {
    unsigned int m = (i+j)/2;
    if (ttisnil(luaH_getint(t, m))) j = m;
    else i = m;
  }
  return i;
}


/*
** Try to find a boundary in table `t'. A `boundary' is an integer index
** such that t[i] is non-nil and t[i+1] is nil (and 0 if t[1] is nil).
*/
int luaH_getn (Table *t) {
  unsigned int j = t->sizearray;
  if (j > 0 && ttisnil(&t->array[j - 1])) {
    /* there is a boundary in the array part: (binary) search for it */
    unsigned int i = 0;
    while (j - i > 1) {
      unsigned int m = (i+j)/2;
      if (ttisnil(&t->array[m - 1])) j = m;
      else i = m;
    }
    return i;
  }
  /* else must find a boundary in hash part */
  else if (isdummy(t->node))  /* hash part is empty? */
    return j;  /* that is easy... */
  else return unbound_search(t, j);
}



#if defined(LUA_DEBUG)

Node *luaH_mainposition (const Table *t, const TValue *key) {
  return mainposition(t, key);
}

int luaH_isdummy (Node *n) { return isdummy(n); }

#endif
