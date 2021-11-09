/* Listpack -- A lists of strings serialization format
 *
 * This file implements the specification you can find at:
 *
 *  https://github.com/antirez/listpack
 *
 * Copyright (c) 2017, Salvatore Sanfilippo <antirez at gmail dot com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Redis nor the names of its contributors may be used
 *     to endorse or promote products derived from this software without
 *     specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
/*
1、ziplist 设计的初衷就是「节省内存」，在存储数据时，把内存利用率发挥到了极致：

- 数字按「整型」编码存储，比直接当字符串存内存占用少
- 数据「长度」字段，会根据内容的大小选择最小的长度编码
- 甚至对于极小的数据，干脆把内容直接放到了「长度」字段中（前几个位表示长度，后几个位存数据）

2、但 ziplist 的劣势也很明显：

- 寻找元素只能挨个遍历，存储过长数据，查询性能很低
- 每个元素中保存了「上一个」元素的长度（为了方便反向遍历），这会导致上一个元素内容发生修改，长度超过了原来的编码长度，下一个元素的内容也要跟着变，重新分配内存，进而就有可能再次引起下一级的变化，一级级更新下去，频繁申请内存

3、想要缓解 ziplist 的问题，比较简单直接的方案就是，多个数据项，不再用一个 ziplist 来存，而是分拆到多个 ziplist 中，每个 ziplist 用指针串起来，这样修改其中一个数据项，即便发生级联更新，也只会影响这一个 ziplist，其它 ziplist 不受影响，这种方案就是 quicklist：

qucklist: ziplist1(也叫quicklistNode) <-> ziplist2 <-> ziplist3 <-> ...

4、List 数据类型底层实现，就是用的 quicklist，因为它是一个链表，所以 LPUSH/LPOP/RPUSH/RPOP 的复杂度是 O(1)

5、List 中每个 ziplist 节点可以存的元素个数/总大小，可以通过 list-max-ziplist-size 配置：

- 正数：ziplist 最多包含几个数据项
- 负数：取值 -1 ~ -5，表示每个 ziplist 存储最大的字节数，默认 -2，每个ziplist 8KB

ziplist 超过上述任一配置，添加新元素就会新建 ziplist 插入到链表中。

6、List 因为更多是两头操作，为了节省内存，还可以把中间的 ziplist「压缩」，具体可看 list-compress-depth 配置项，默认配置不压缩

7、要想彻底解决 ziplist 级联更新问题，本质上要修改 ziplist 的存储结构，也就是不要让每个元素保存「上一个」元素的长度即可，所以才有了 listpack

8、listpack 每个元素项不再保存上一个元素的长度，而是优化元素内字段的顺序，来保证既可以从前也可以向后遍历

9、listpack 是为了替代 ziplist 为设计的，但因为 List/Hash/Set/ZSet 都严重依赖 ziplist，所以这个替换之路很漫长，目前只有 Stream 数据类型用到了 listpack
*/

#ifndef __LISTPACK_H
#define __LISTPACK_H

#include <stdint.h>

#define LP_INTBUF_SIZE 21 /* 20 digits of -2^63 + 1 null term = 21. */

/* lpInsert() where argument possible values: */
#define LP_BEFORE 0
#define LP_AFTER 1
#define LP_REPLACE 2

unsigned char *lpNew(void);
void lpFree(unsigned char *lp);
unsigned char *lpInsert(unsigned char *lp, unsigned char *ele, uint32_t size, unsigned char *p, int where, unsigned char **newp);
unsigned char *lpAppend(unsigned char *lp, unsigned char *ele, uint32_t size);
unsigned char *lpDelete(unsigned char *lp, unsigned char *p, unsigned char **newp);
uint32_t lpLength(unsigned char *lp);
unsigned char *lpGet(unsigned char *p, int64_t *count, unsigned char *intbuf);
unsigned char *lpFirst(unsigned char *lp);
unsigned char *lpLast(unsigned char *lp);
unsigned char *lpNext(unsigned char *lp, unsigned char *p);
unsigned char *lpPrev(unsigned char *lp, unsigned char *p);
uint32_t lpBytes(unsigned char *lp);
unsigned char *lpSeek(unsigned char *lp, long index);

#endif
