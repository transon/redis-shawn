#String -> String Map data structure optimized for size.

## Introduction ##

```
/* 
 * String -> String Map data structure optimized for size.
 * This file implements a data structure mapping strings to other strings
 * implementing an O(n) lookup data structure designed to be very memory
 * efficient.
 *
 * The Redis Hash type uses this data structure for hashes composed of a small
 * number of elements, to switch to an hash table once a given number of
 * elements is reached.
 *
 * Given that many times Redis Hashes are used to represent objects composed
 * of few fields, this is a very big win in terms of used memory.
 * Memory layout of a zipmap, for the map "foo" => "bar", "hello" => "world":
 *
 * <zmlen><len>"foo"<len><free>"bar"<len>"hello"<len><free>"world"
 *
 * <zmlen> is 1 byte length that holds the current size of the zipmap.
 * When the zipmap length is greater than or equal to 254, this value
 * is not used and the zipmap needs to be traversed to find out the length.
 *
 * <len> is the length of the following string (key or value).
 * <len> lengths are encoded in a single value or in a 5 bytes value.
 * If the first byte value (as an unsigned 8 bit value) is between 0 and
 * 252, it's a single-byte length. If it is 253 then a four bytes unsigned
 * integer follows (in the host byte ordering). A value fo 255 is used to
 * signal the end of the hash. The special value 254 is used to mark
 * empty space that can be used to add new key/value pairs.
 *
 * <free> is the number of free unused bytes
 * after the string, resulting from modification of values associated to a
 * key (for instance if "foo" is set to "bar', and later "foo" will be se to
 * "hi", I'll have a free byte to use if the value will enlarge again later,
 * or even in order to add a key/value pair if it fits.
 *
 * <free> is always an unsigned 8 bit number, because if after an
 * update operation there are more than a few free bytes, the zipmap will be
 * reallocated to make sure it is as small as possible.
 *
 * The most compact representation of the above two elements hash is actually:
 *
 * "\x02\x03foo\x03\x00bar\x05hello\x05\x00world\xff"
 *
 * Note that because keys and values are prefixed length "objects",
 * the lookup will take O(N) where N is the number of elements
 * in the zipmap and *not* the number of bytes needed to represent the zipmap.
 * This lowers the constant times considerably.
 */
```

e.g. Memory layout of a zipmap, for the map "foo" => "bar", "hello" => "world":
```

\x02  \x03  foo \x03  \x00  bar \x05  hello \x05  \x00  world  \xff
  \     \    \    \     \    \    \      \    \     \     \
   \     \    \    \     \    \    \      \    \     \     \
    \     \    \    \     \    \    \      \    \     \     \
  <zmlen><len>"foo"<len><free>"bar"<len>"hello"<len><free>"world"

Note: zmlen的值是2，因为这个例子包含两个map； 而\xoff是结束符。

这样就将下面的map结构体可以映射为zipmap，查找时间为O(N)，N为zmlen

typedef struct zipmap {
    char *foo;
    char *hello;
}zipmap;

zipmap.foo = "bar";
zipmap.hello = "world";


```

## Dependency files ##
```
  zmalloc.h
  zmalloc.c
```

## Implement files ##
```
  zipmap.h
  zipmap.c
```

## Details ##

Parse the source code at here!