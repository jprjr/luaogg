# luaogg

Lua bindings to [libogg](https://xiph.org/ogg/doc/libogg/reference.html)

MIT licensed (see file `LICENSE`)

# Installation

Available on [luarocks](https://luarocks.org/modules/jprjr/luaogg):

```bash
luarocks install luaogg
```

Available on [AUR](https://aur.archlinux.org/packages/lua-luaogg/)

## Building from source

You can build with luarocks or cmake.

# Table of Contents

* [Synopsis](#synopsis)
* [Implementation Notes](#implementation-notes)
  * [Pages](#pages)
  * [Packets](#packets)
  * [granulepos and packetno userdata](#granulepos-and-packetno-userdata)
* [Functions](#functions)

# Synopsis

```lua
local ogg = require'luaogg'

local sync = ogg.ogg_sync_state()
local stream = ogg.ogg_stream_state()

local f = io.open(arg[1],'rb')
if not f then
  os.exit(1)
end

sync:init()

-- function to read file f until we get a page
local function get_page()
  local page = sync:pageout()
  while not page do
    local chunk = f:read(4096)
    if not chunk then
      f:close()
      return nil
    end
    sync:buffer(chunk)
    page = sync:pageout()
  end
  return page
end

-- function to read pages until we get a packet
local function get_packet()
  local packet = stream:packetout()
  while not packet do
    local page = get_page()
    if not page then
      return nil
    end
    if page.bos then
      stream:init(page.serialno)
    end
    stream:pagein(page)
    packet = stream:packetout()
  end
  return packet
end

local packet = get_packet()
print('packet info:')
print('  bytes: ', packet.bytes)
print('  b_o_s: ', packet.b_o_s)
print('  e_o_s: ', packet.e_o_s)
print('  granulepos: ', packet.granulepos)
print('  packetno: ', packet.packetno)
-- packet data is at packet.packet

f:close()
```

This only implements the ogg bitstream functions, it does not implement
the bitpacking functions and structures.

# Implementation Notes

Ogg Packets and Pages are implemented as regular Lua tables rather than
userdata.

## Pages

When a Page is returned, libogg's page metadata functions are automatically
called and set as keys on the table, you can expect the following fields:

| table key | lua type |
|-----------|-----------|
| `header`  | `string` |
| `header_len` | `number` |
| `body`    | `string` |
| `body_len` | `number` |
| `version` | `number` |
| `continued` | `boolean` |
| `packets` | `number` |
| `bos` | `boolean` |
| `eos` | `boolean` |
| `granulepos` | `userdata` (see below) |
| `serialno` | `number` |
| `pageno` | `number` |

When a function has a Page as input, it needs the following keys set:

| table key | lua type |
|-----------|----------|
| `header`  | `string` |
| `body`    | `string` |

## Packets

When a Packet is returned, you can expect the value keys:

| table key | lua type |
|-----------|----------|
| `packet`  | `string` |
| `bytes`   | `number` |
| `b_o_s`   | `boolean` |
| `e_o_s`   | `boolean` |
| `granulepos`   | `userdata` (see below) |
| `packetno`   | `userdata` (see below) |

When a function has a Packet as input, it needs the following keys set:

| table key | lua type |
|-----------|----------|
| `packet`  | `string` |
| `b_o_s`   | `boolean` |
| `e_o_s`   | `boolean` |
| `granulepos`   | `userdata` or `number` or `string` |
| `packetno`   | `userdata` or `number` or `string` |

## granulepos and packetno userdata

Ogg uses 64-bit integers for the `granulepos` and `packetno` fields.

Lua 5.3 (and above) *may* have 64-bit integers (it's a compile-time option),
whereas Lua 5.2 (and below) do not.

Returned Pages and Packets have the `granulepos` and `packetno` fields set
as a userdata. The userdata has a metadata table allowing for arithmetic
operations, comparison operations, and supports `tostring`.

Functions that read Pages and Packets can accept the userdata, or a regular
Lua number, or a Lua string (provided it can be parsed with `strtoll`).

You can create your own userdata instance as well:

```lua
local i = ogg.ogg_int64_t('9223372036854775806') -- creates a new userdata
i = i + 1
print(i) -- prints "9223372036854775807", the max 64-bit signed int
```

If you're building packets and want to keep a running `granulepos`, this
is a good way to do it.

# Functions

* [ogg\_int64\_t](#ogg_int64_t)
* [ogg\_sync\_state](#ogg_sync_state)
* [ogg\_stream\_state](#ogg_stream_state)
* [ogg\_sync\_init](#ogg_sync_init)
* [ogg\_sync\_check](#ogg_sync_check)
* [ogg\_sync\_clear](#ogg_sync_clear)
* [ogg\_sync\_reset](#ogg_sync_reset)
* [ogg\_sync\_buffer](#ogg_sync_buffer)
* [ogg\_sync\_pageseek](#ogg_sync_pageseek)
* [ogg\_sync\_pageout](#ogg_sync_pageout)
* [ogg\_stream\_init](#ogg_stream_init)
* [ogg\_stream\_check](#ogg_stream_check)
* [ogg\_stream\_clear](#ogg_stream_clear)
* [ogg\_stream\_reset](#ogg_stream_reset)
* [ogg\_stream\_reset\_serialno](#ogg_stream_reset_serialno)
* [ogg\_stream\_pagein](#ogg_stream_pagein)
* [ogg\_stream\_packetout](#ogg_stream_packetout)
* [ogg\_stream\_packetpeek](#ogg_stream_packetpeek)
* [ogg\_stream\_packetin](#ogg_stream_packetin)
* [ogg\_stream\_pageout](#ogg_stream_pageout)
* [ogg\_stream\_pageout\_fill](#ogg_stream_pageout_fill)
* [ogg\_stream\_flush](#ogg_stream_flush)
* [ogg\_stream\_flush\_fill](#ogg_stream_flush_fill)

## ogg_int64_t

**syntax:** `userdata state = ogg.ogg_int64_t(nil | number | string)`

Returns a new 64-bit integer.

## ogg_sync_state

**syntax:** `userdata state = ogg.ogg_sync_state()`

Returns a new `ogg_sync_state` data structure for the various
`ogg_sync_*` functions.

Also has a metatable for object-oriented usage. For example,
`ogg.ogg_sync_init(state)` could be called as `state:init()`.


## ogg_stream_state

**syntax:** `userdata state = ogg.ogg_stream_state()`

Returns a new `ogg_stream_state` data structure for the various
`ogg_stream_*` functions.

Also has a metatable for object-oriented usage. For example,
`ogg.ogg_stream_init(state,1234)` could be called as `state:init(1234)`.

## ogg_sync_init

**syntax**: `ogg.ogg_sync_init(userdata state)`

Initializes a `ogg_sync_state`, no return value.

## ogg_sync_check

**syntax**: `boolean ready = ogg.ogg_sync_check(userdata state)`

Returns `true` if the given `ogg_sync_state` is initialized and ready.

## ogg_sync_clear

**syntax:** `ogg.ogg_sync_clear(userdata state)`

Frees internal storage of an `ogg_sync_state` object.

This is automatically called when the object is garbage-collected.

No return value.

## ogg_sync_reset

**syntax:** `ogg.ogg_sync_reset(userdata state)`

Resets internal counters on an `ogg_sync_state` object.

No return value.

## `ogg_sync_buffer`

**syntax:** `boolean success = ogg.ogg_sync_buffer(userdata state, string data)`

Feeds `data` to the given `ogg_sync_state`.

This internally calls libogg's `ogg_sync_buffer` and `ogg_sync_wrote`.

Returns `true` on success.

## `ogg_sync_pageseek`

**syntax:** `table page = ogg.ogg_sync_pageseek(userdata state)`

Synchronizes the `ogg_sync_state` to the next page.

Returns page on success, or nil.

## `ogg_sync_pageout`

**syntax:** `table page = ogg.ogg_sync_pageout(userdata state)`

Attempts to return an ogg page from an `ogg_sync_state`.

Returns a page on success, or `nil` otherwise (more data needed, internal error, etc).

## ogg_stream_init

**syntax:** `boolean success = ogg.ogg_stream_init(userdata state, number serialno)`

Initializes an `ogg_stream_state` for encoding/decoding.

Returns `true` on success.

## ogg_stream_check

**syntax**: `boolean ready = ogg.ogg_stream_check(userdata state)`

Returns `true` if the given `ogg_stream_state` is initialized and ready.

## ogg_stream_clear

**syntax:** `ogg.ogg_stream_clear(userdata state)`

Frees internal storage of an `ogg_stream_state` object.

This is automatically called when the object is garbage-collected.

No return value.

## ogg_stream_reset

**syntax:** `boolean success = ogg.ogg_stream_reset(userdata state)`

Resets internal values on an `ogg_stream_state` object.

Returns `true` on success.

## ogg_stream_reset_serialno

**syntax:** `boolean success = ogg.ogg_stream_reset_serialno(userdata state, number serialno)`

Resets internal values on an `ogg_stream_state` object, and resets the serial number.

Returns `true` on success.

## ogg_stream_pagein

**syntax:** `boolean success = ogg.ogg_stream_pagein(userdata state, table page)`

Add a page to a given `ogg_stream_state` object.

Returns `true` on success.


## ogg_stream_packetout

**syntax:** `table packet = ogg.ogg_stream_packetout(userdata state)`

Attempts to read and return a packet from an `ogg_stream_state` object.

Returns a `table` on success, `nil` otherwise (not enough data read, internal error, etc).


## ogg_stream_packetpeek

**syntax:** `table packet = ogg.ogg_stream_packetpeek(userdata state)`

Attempts to read and return a packet from an `ogg_stream_state` object,
without advancing any internal data pointers.

Returns a `table` on success, `nil` otherwise (not enough data read, internal error, etc).

## ogg_stream_packetin

**syntax:** `boolean success = ogg.ogg_stream_packetin(userdata state, table packet)`

Add a packet to a given `ogg_stream_state` object.

Returns `true` on success.

## ogg_stream_pageout

**syntax:** `table page = ogg.ogg_stream_pageout(userdata state)`

Attempts to read and return a page from an `ogg_stream_state` object.

Returns a `table` on success, `nil` otherwise (not enough packets to make a page, internal error, etc).

## ogg_stream_pageout_fill

**syntax:** `table page = ogg.ogg_stream_packetout(userdata state, number fillbytes)`

Attempts to read and return a page from an `ogg_stream_state` object with
an explicit page spill size.

Returns a `table` on success, `nil` otherwise (not enough packets to make a page, internal error, etc).

## ogg_stream_flush

**syntax:** `table page = ogg.ogg_stream_flush(userdata state)`

Attempts to read and return a page from an `ogg_stream_state` object, even
if the page is undersized.

Returns a `table` on success, `nil` otherwise (no packets to make a page, internal error, etc).

## ogg_stream_flush_fill

**syntax:** `table page = ogg.ogg_stream_packetout(userdata state, number fillbytes)`

Attempts to read and return a page from an `ogg_stream_state` object, even
if the page is undersized, with an explicit page spill size.

Returns a `table` on success, `nil` otherwise.
