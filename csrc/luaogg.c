#define LUAOGG_VERSION_MAJOR 1
#define LUAOGG_VERSION_MINOR 2
#define LUAOGG_VERSION_PATCH 0
#define STR(x) #x
#define XSTR(x) STR(x)
#define LUAOGG_VERSION XSTR(LUAOGG_VERSION_MAJOR) "." XSTR(LUAOGG_VERSION_MINOR) "." XSTR(LUAOGG_VERSION_PATCH)

#include <lua.h>
#include <lauxlib.h>
#include <ogg/ogg.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#if defined(_WIN32) || defined(_WIN64) || defined(WIN32) || defined(_MSC_VER)
#define LUAOGG_PUBLIC __declspec(dllexport)
#else
#define LUAOGG_PUBLIC
#endif

#if !defined(luaL_newlibtable) \
  && (!defined LUA_VERSION_NUM || LUA_VERSION_NUM==501)
static void luaL_setfuncs (lua_State *L, const luaL_Reg *l, int nup) {
    luaL_checkstack(L, nup+1, "too many upvalues");
    for (; l->name != NULL; l++) {  /* fill the table with given functions */
        int i;
        lua_pushlstring(L, l->name,strlen(l->name));
        for (i = 0; i < nup; i++)  /* copy upvalues to the top */
            lua_pushvalue(L, -(nup+1));
        lua_pushcclosure(L, l->func, nup);  /* closure with those upvalues */
        lua_settable(L, -(nup + 3));
    }
    lua_pop(L, nup);  /* remove upvalues */
}

static
void luaL_setmetatable(lua_State *L, const char *str) {
    luaL_checkstack(L, 1, "not enough stack slots");
    luaL_getmetatable(L, str);
    lua_setmetatable(L, -2);
}

static
void *luaL_testudata (lua_State *L, int i, const char *tname) {
    void *p = lua_touserdata(L, i);
    luaL_checkstack(L, 2, "not enough stack slots");
    if (p == NULL || !lua_getmetatable(L, i)) {
        return NULL;
    }
    else {
        int res = 0;
        luaL_getmetatable(L, tname);
        res = lua_rawequal(L, -1, -2);
        lua_pop(L, 2);
        if (!res) {
            p = NULL;
        }
    }
    return p;
}
#endif

static const char * const digits                 = "0123456789";
static const char * const luaogg_int64_mt        = "ogg_int64_t";
static const char * const luaogg_uint64_mt       = "ogg_uint64_t";
static const char * const luaogg_sync_state_mt   = "ogg_sync_state";
static const char * const luaogg_stream_state_mt = "ogg_stream_state";

typedef struct luaogg_metamethods_s {
    const char *name;
    const char *metaname;
} luaogg_metamethods;

static char *
luaogg_uint64_to_str(ogg_uint64_t value, char buffer[21], size_t *len) {
    char *p = buffer + 20;
    *p = '\0';
    do {
        *--p = digits[value % 10];
    } while (value /= 10);

    *len = 20 - (p - buffer);
    return p;
}

static char *
luaogg_int64_to_str(ogg_int64_t value, char buffer[22], size_t *len) {
    int sign;
    ogg_uint64_t tmp;

    if(value < 0) {
        sign = 1;
        tmp = -value;
    } else {
        sign = 0;
        tmp = value;
    }
    char *p = luaogg_uint64_to_str(tmp,buffer+1,len);
    if(sign) {
        *--p = '-';
        *len += 1;
    }
    return p;
}

static inline ogg_int64_t
luaogg_toint64(lua_State *L, int idx) {
    ogg_int64_t *t = NULL;
    ogg_uint64_t *r = NULL;
    ogg_int64_t tmp = 0;
    const char *str = NULL;

    switch(lua_type(L,idx)) {
        case LUA_TNONE: {
            return 0;
        }
        case LUA_TNIL: {
            return 0;
        }
        case LUA_TBOOLEAN: {
            return (ogg_int64_t)lua_toboolean(L,idx);
        }
        case LUA_TNUMBER: {
            return (ogg_int64_t)lua_tointeger(L,idx);
        }
        case LUA_TUSERDATA: {
            t = luaL_testudata(L,idx,luaogg_int64_mt);
            if(t != NULL) {
                return *t;
            }
            r = luaL_testudata(L,idx,luaogg_uint64_mt);
            if(r != NULL) {
                if(*r > 0x7FFFFFFFFFFFFFFF) {
                    luaL_error(L,"out of range");
                    return 0;
                }
                return (ogg_int64_t) *r;
            }
            break;
        }
        /* we'll try converting to a string */
        default: break;
    }

    str = lua_tostring(L,idx);
    if(str == NULL) {
        luaL_error(L,"invalid value");
        return 0;
    }
    errno = 0;
    tmp = strtoll(str,NULL,10);
    if(errno) {
        luaL_error(L,"invalid integer string");
        return 0;
    }
    return tmp;
}

static void
luaogg_page_to_table(lua_State *L, ogg_page *page) {
    ogg_int64_t *t = NULL;

    lua_newtable(L);
    lua_pushlstring(L,(const char *)page->header,page->header_len);
    lua_setfield(L,-2,"header");
    lua_pushinteger(L,page->header_len);
    lua_setfield(L,-2,"header_len");
    lua_pushlstring(L,(const char *)page->body,page->body_len);
    lua_setfield(L,-2,"body");
    lua_pushinteger(L,page->body_len);
    lua_setfield(L,-2,"body_len");
    lua_pushinteger(L,ogg_page_version(page));
    lua_setfield(L,-2,"version");
    lua_pushboolean(L,ogg_page_continued(page));
    lua_setfield(L,-2,"continued");
    lua_pushinteger(L,ogg_page_packets(page));
    lua_setfield(L,-2,"packets");
    lua_pushboolean(L,ogg_page_bos(page));
    lua_setfield(L,-2,"bos");
    lua_pushboolean(L,ogg_page_eos(page));
    lua_setfield(L,-2,"eos");
    lua_pushinteger(L,ogg_page_serialno(page));
    lua_setfield(L,-2,"serialno");
    lua_pushinteger(L,ogg_page_pageno(page));
    lua_setfield(L,-2,"pageno");

    t = lua_newuserdata(L,sizeof(ogg_int64_t));
    *t = ogg_page_granulepos(page);
    luaL_setmetatable(L,luaogg_int64_mt);
    lua_setfield(L,-2,"granulepos");
}

static void
luaogg_table_to_page(lua_State *L, int idx, ogg_page *page) {
    lua_getfield(L,idx,"header");
    page->header = (unsigned char *)lua_tolstring(L,-1,(size_t *)&(page->header_len));
    lua_getfield(L,idx,"body");
    page->body = (unsigned char *)lua_tolstring(L,-1,(size_t *)&(page->body_len));
    lua_pop(L,2);
}

static void
luaogg_packet_to_table(lua_State *L, ogg_packet *packet) {
    ogg_int64_t *t = NULL;

    lua_newtable(L);
    lua_pushlstring(L,(const char *)packet->packet,packet->bytes);
    lua_setfield(L,-2,"packet");
    lua_pushinteger(L,packet->bytes);
    lua_setfield(L,-2,"bytes");
    lua_pushboolean(L,packet->b_o_s);
    lua_setfield(L,-2,"b_o_s");
    lua_pushboolean(L,packet->e_o_s);
    lua_setfield(L,-2,"e_o_s");

    t = lua_newuserdata(L,sizeof(ogg_int64_t));
    *t = packet->granulepos;

    luaL_setmetatable(L,luaogg_int64_mt);
    lua_setfield(L,-2,"granulepos");

    t = lua_newuserdata(L,sizeof(ogg_int64_t));
    *t = packet->packetno;

    luaL_setmetatable(L,luaogg_int64_mt);
    lua_setfield(L,-2,"packetno");

}

static void
luaogg_table_to_packet(lua_State *L, int idx, ogg_packet *packet) {
    lua_getfield(L,idx,"packet");
    packet->packet = (unsigned char *)lua_tolstring(L,-1,(size_t *)&packet->bytes);
    lua_getfield(L,idx,"b_o_s");
    packet->b_o_s = lua_toboolean(L,-1);
    lua_getfield(L,idx,"e_o_s");
    packet->e_o_s = lua_toboolean(L,-1);

    lua_getfield(L,idx,"granulepos");
    packet->granulepos = luaogg_toint64(L,-1);
    lua_getfield(L,idx,"packetno");
    packet->packetno = luaogg_toint64(L,-1);

    lua_pop(L,5);
}

static int
luaogg_int64(lua_State *L) {
    /* create a new int64 object from a number or string */
    ogg_int64_t *t = 0;

    t = (ogg_int64_t *)lua_newuserdata(L,sizeof(ogg_int64_t));
    if(t == NULL) {
        return luaL_error(L,"out of memory");
    }

    if(lua_gettop(L) > 1) {
        *t = luaogg_toint64(L,1);
    }
    else {
        *t = 0;
    }

    luaL_setmetatable(L,luaogg_int64_mt);

    return 1;
}

static int
luaogg_int64__unm(lua_State *L) {
    ogg_int64_t o = 0;
    ogg_int64_t *r = NULL;

    o = luaogg_toint64(L,1);

    r = (ogg_int64_t *)lua_newuserdata(L,sizeof(ogg_int64_t));

    *r = -o;

    luaL_setmetatable(L,luaogg_int64_mt);

    return 1;
}

static int
luaogg_int64__add(lua_State *L) {
    ogg_int64_t a = 0;
    ogg_int64_t b = 0;
    ogg_int64_t *res = NULL;

    a = luaogg_toint64(L,1);
    b = luaogg_toint64(L,2);

    res = (ogg_int64_t *)lua_newuserdata(L,sizeof(ogg_int64_t));
    *res = a + b;

    luaL_setmetatable(L,luaogg_int64_mt);

    return 1;
}

static int
luaogg_int64__sub(lua_State *L) {
    ogg_int64_t a = 0;
    ogg_int64_t b = 0;
    ogg_int64_t *res = NULL;

    a = luaogg_toint64(L,1);
    b = luaogg_toint64(L,2);

    res = (ogg_int64_t *)lua_newuserdata(L,sizeof(ogg_int64_t));
    *res = a - b;

    luaL_setmetatable(L,luaogg_int64_mt);

    return 1;
}

static int
luaogg_int64__mul(lua_State *L) {
    ogg_int64_t a = 0;
    ogg_int64_t b = 0;
    ogg_int64_t *res = NULL;

    a = luaogg_toint64(L,1);
    b = luaogg_toint64(L,2);

    res = (ogg_int64_t *)lua_newuserdata(L,sizeof(ogg_int64_t));
    *res = a * b;

    luaL_setmetatable(L,luaogg_int64_mt);

    return 1;
}

static int
luaogg_int64__div(lua_State *L) {
    ogg_int64_t a = 0;
    ogg_int64_t b = 0;
    ogg_int64_t *res = NULL;

    a = luaogg_toint64(L,1);
    b = luaogg_toint64(L,2);

    res = (ogg_int64_t *)lua_newuserdata(L,sizeof(ogg_int64_t));
    *res = a / b;

    luaL_setmetatable(L,luaogg_int64_mt);

    return 1;
}

static int
luaogg_int64__mod(lua_State *L) {
    ogg_int64_t a = 0;
    ogg_int64_t b = 0;
    ogg_int64_t *res = NULL;

    a = luaogg_toint64(L,1);
    b = luaogg_toint64(L,2);

    res = (ogg_int64_t *)lua_newuserdata(L,sizeof(ogg_int64_t));
    *res = a % b;

    luaL_setmetatable(L,luaogg_int64_mt);

    return 1;
}

static int
luaogg_int64__pow(lua_State *L) {
    ogg_int64_t base = 0;
    ogg_int64_t exp = 0;
    ogg_int64_t *res = NULL;
    ogg_int64_t result = 1;

    base = luaogg_toint64(L,1);
    exp = luaogg_toint64(L,2);

    if(exp < 0) {
        return luaL_error(L,"exp must be positive");
    }

    for (;;) {
        if(exp & 1) {
            result *= base;
        }
        exp >>= 1;
        if(!exp) {
            break;
        }
        base *= base;
    }

    res = (ogg_int64_t *)lua_newuserdata(L,sizeof(ogg_int64_t));
    *res = result;

    luaL_setmetatable(L,luaogg_int64_mt);

    return 1;
}

static int
luaogg_int64__eq(lua_State *L) {
    ogg_int64_t a = 0;
    ogg_int64_t b = 0;

    a = luaogg_toint64(L,1);
    b = luaogg_toint64(L,2);

    lua_pushboolean(L,a==b);
    return 1;
}

static int
luaogg_int64__lt(lua_State *L) {
    ogg_int64_t a = 0;
    ogg_int64_t b = 0;

    a = luaogg_toint64(L,1);
    b = luaogg_toint64(L,2);

    lua_pushboolean(L,a<b);
    return 1;
}

static int
luaogg_int64__le(lua_State *L) {
    ogg_int64_t a = 0;
    ogg_int64_t b = 0;

    a = luaogg_toint64(L,1);
    b = luaogg_toint64(L,2);

    lua_pushboolean(L,a<=b);
    return 1;
}

static int
luaogg_int64__band(lua_State *L) {
    ogg_int64_t a = 0;
    ogg_int64_t b = 0;
    ogg_int64_t *res = NULL;

    a = luaogg_toint64(L,1);
    b = luaogg_toint64(L,2);

    res = (ogg_int64_t *)lua_newuserdata(L,sizeof(ogg_int64_t));
    *res = a & b;

    luaL_setmetatable(L,luaogg_int64_mt);

    return 1;
}

static int
luaogg_int64__bor(lua_State *L) {
    ogg_int64_t a = 0;
    ogg_int64_t b = 0;
    ogg_int64_t *res = NULL;

    a = luaogg_toint64(L,1);
    b = luaogg_toint64(L,2);

    res = (ogg_int64_t *)lua_newuserdata(L,sizeof(ogg_int64_t));
    *res = a | b;

    luaL_setmetatable(L,luaogg_int64_mt);

    return 1;
}

static int
luaogg_int64__bxor(lua_State *L) {
    ogg_int64_t a = 0;
    ogg_int64_t b = 0;
    ogg_int64_t *res = NULL;

    a = luaogg_toint64(L,1);
    b = luaogg_toint64(L,2);

    res = (ogg_int64_t *)lua_newuserdata(L,sizeof(ogg_int64_t));
    *res = a ^ b;

    luaL_setmetatable(L,luaogg_int64_mt);

    return 1;
}

static int
luaogg_int64__bnot(lua_State *L) {
    ogg_int64_t a = 0;
    ogg_int64_t *r = NULL;

    a = luaogg_toint64(L,1);

    r = (ogg_int64_t *)lua_newuserdata(L,sizeof(ogg_int64_t));

    *r = ~a;

    luaL_setmetatable(L,luaogg_int64_mt);

    return 1;
}

static int
luaogg_int64__shl(lua_State *L) {
    ogg_int64_t a = 0;
    ogg_int64_t b = 0;
    ogg_int64_t *res = NULL;

    a = luaogg_toint64(L,1);
    b = luaogg_toint64(L,2);

    res = (ogg_int64_t *)lua_newuserdata(L,sizeof(ogg_int64_t));
    *res = (ogg_int64_t)(a << b);

    luaL_setmetatable(L,luaogg_int64_mt);

    return 1;
}

static int
luaogg_int64__shr(lua_State *L) {
    ogg_int64_t a = 0;
    ogg_int64_t b = 0;
    ogg_int64_t *res = NULL;

    a = luaogg_toint64(L,1);
    b = luaogg_toint64(L,2);

    res = (ogg_int64_t *)lua_newuserdata(L,sizeof(ogg_int64_t));
    *res = (ogg_int64_t)(a >> b);

    luaL_setmetatable(L,luaogg_int64_mt);

    return 1;
}

static int
luaogg_int64__tostring(lua_State *L) {
    ogg_int64_t o = 0;
    char *p;
    char t[22];
    size_t l;

    o = luaogg_toint64(L,1);
    p = luaogg_int64_to_str(o,t,&l);
    lua_pushlstring(L,p,l);
    return 1;
}

static int
luaogg_int64__concat(lua_State *L) {
    lua_getglobal(L,"tostring");
    lua_pushvalue(L,1);
    lua_call(L,1,1);

    lua_getglobal(L,"tostring");
    lua_pushvalue(L,2);
    lua_call(L,1,1);

    lua_concat(L,2);
    return 1;
}

static int
luaogg_ogg_sync_state(lua_State *L) {
    ogg_sync_state *state = lua_newuserdata(L,sizeof(ogg_sync_state));
    if(state == NULL) {
        return luaL_error(L,"out of memory");
    }
    ogg_sync_init(state);

    luaL_setmetatable(L,luaogg_sync_state_mt);

    return 1;
}

static int
luaogg_ogg_stream_state(lua_State *L) {
    ogg_stream_state *state = lua_newuserdata(L,sizeof(ogg_stream_state));
    if(state == NULL) {
        return luaL_error(L,"out of memory");
    }
    if(ogg_stream_reset(state) != 0) {
        return luaL_error(L,"error in ogg_stream_reset");
    }

    luaL_setmetatable(L,luaogg_stream_state_mt);

    return 1;
}

static int
luaogg_ogg_sync_init(lua_State *L) {
    ogg_sync_state *state = luaL_checkudata(L,1,luaogg_sync_state_mt);
    ogg_sync_init(state);
    return 0;
}

static int
luaogg_ogg_sync_check(lua_State *L) {
    ogg_sync_state *state = luaL_checkudata(L,1,luaogg_sync_state_mt);
    lua_pushboolean(L,ogg_sync_check(state) == 0);
    return 1;
}

static int
luaogg_ogg_sync_clear(lua_State *L) {
    ogg_sync_state *state = luaL_checkudata(L,1,luaogg_sync_state_mt);
    ogg_sync_clear(state);
    return 0;
}

static int
luaogg_ogg_sync_reset(lua_State *L) {
    ogg_sync_state *state = luaL_checkudata(L,1,luaogg_sync_state_mt);
    lua_pushboolean(L,ogg_sync_reset(state) == 0);
    return 1;
}

static int
luaogg_ogg_sync_buffer(lua_State *L) {
    ogg_sync_state *state = luaL_checkudata(L,1,luaogg_sync_state_mt);
    const char *data = NULL;
    char *buffer = NULL;
    size_t datalen = 0;

    data = lua_tolstring(L,2,&datalen);

    buffer = ogg_sync_buffer(state,datalen);
    if(buffer == NULL) {
        return luaL_error(L,"ogg_sync_buffer error");
    }

    memcpy(buffer,data,datalen);

    lua_pushboolean(L,ogg_sync_wrote(state,datalen) == 0);
    return 1;
}

static int
luaogg_ogg_sync_pageseek(lua_State *L) {
    ogg_page page;
    ogg_sync_state *state = luaL_checkudata(L,1,luaogg_sync_state_mt);
    if(ogg_sync_pageseek(state,&page) > 0) {
        luaogg_page_to_table(L,&page);
    }
    else {
        lua_pushnil(L);
    }
    return 1;
}

static int
luaogg_ogg_sync_pageout(lua_State *L) {
    ogg_page page;
    ogg_sync_state *state = luaL_checkudata(L,1,luaogg_sync_state_mt);
    if(ogg_sync_pageout(state,&page) > 0) {
        luaogg_page_to_table(L,&page);
    }
    else {
        lua_pushnil(L);
    }
    return 1;
}

static int
luaogg_ogg_stream_init(lua_State *L) {
    ogg_stream_state *state = luaL_checkudata(L,1,luaogg_stream_state_mt);
    lua_pushboolean(L,ogg_stream_init(state,luaL_checkinteger(L,2)) == 0);
    return 1;
}

static int
luaogg_ogg_stream_check(lua_State *L) {
    ogg_stream_state *state = luaL_checkudata(L,1,luaogg_stream_state_mt);
    lua_pushboolean(L,ogg_stream_check(state) == 0);
    return 1;
}

static int
luaogg_ogg_stream_clear(lua_State *L) {
    ogg_stream_state *state = luaL_checkudata(L,1,luaogg_stream_state_mt);
    ogg_stream_clear(state);
    return 0;
}

static int
luaogg_ogg_stream_reset(lua_State *L) {
    ogg_stream_state *state = luaL_checkudata(L,1,luaogg_stream_state_mt);
    lua_pushboolean(L,ogg_stream_reset(state) == 0);
    return 1;
}

static int
luaogg_ogg_stream_reset_serialno(lua_State *L) {
    ogg_stream_state *state = luaL_checkudata(L,1,luaogg_stream_state_mt);
    lua_pushboolean(L,ogg_stream_reset_serialno(state,lua_tointeger(L,2)) == 0);
    return 1;
}

static int
luaogg_ogg_stream_pagein(lua_State *L) {
    ogg_page page;
    ogg_stream_state *state = luaL_checkudata(L,1,luaogg_stream_state_mt);

    luaogg_table_to_page(L,2,&page);

    lua_pushboolean(L,ogg_stream_pagein(state,&page) == 0);
    return 1;
}

static int
luaogg_ogg_stream_pageout(lua_State *L) {
    ogg_page page;
    ogg_stream_state *state = luaL_checkudata(L,1,luaogg_stream_state_mt);

    if(ogg_stream_pageout(state,&page) != 0) {
        luaogg_page_to_table(L,&page);
    }
    else {
        lua_pushnil(L);
    }
    return 1;
}

static int
luaogg_ogg_stream_pageout_fill(lua_State *L) {
    ogg_page page;
    ogg_stream_state *state = luaL_checkudata(L,1,luaogg_stream_state_mt);

    if(ogg_stream_pageout_fill(state,&page,luaL_checkinteger(L,2)) != 0) {
        luaogg_page_to_table(L,&page);
    }
    else {
        lua_pushnil(L);
    }
    return 1;
}

static int
luaogg_ogg_stream_flush(lua_State *L) {
    ogg_page page;
    ogg_stream_state *state = luaL_checkudata(L,1,luaogg_stream_state_mt);

    if(ogg_stream_flush(state,&page) != 0) {
        luaogg_page_to_table(L,&page);
    }
    else {
        lua_pushnil(L);
    }
    return 1;
}

static int
luaogg_ogg_stream_flush_fill(lua_State *L) {
    ogg_page page;
    ogg_stream_state *state = luaL_checkudata(L,1,luaogg_stream_state_mt);

    if(ogg_stream_flush_fill(state,&page,luaL_checkinteger(L,2)) != 0) {
        luaogg_page_to_table(L,&page);
    }
    else {
        lua_pushnil(L);
    }
    return 1;
}

static int
luaogg_ogg_stream_packetin(lua_State *L) {
    ogg_packet packet;
    ogg_stream_state *state = luaL_checkudata(L,1,luaogg_stream_state_mt);

    luaogg_table_to_packet(L, 2, &packet);

    lua_pushboolean(L,ogg_stream_packetin(state,&packet) == 0);
    return 1;
}

static int
luaogg_ogg_stream_packetout(lua_State *L) {
    ogg_packet packet;
    ogg_stream_state *state = luaL_checkudata(L,1,luaogg_stream_state_mt);
    if(ogg_stream_packetout(state,&packet) == 1) {
        luaogg_packet_to_table(L,&packet);
    }
    else {
        lua_pushnil(L);
    }
    return 1;
}

static int
luaogg_ogg_stream_packetpeek(lua_State *L) {
    ogg_packet packet;
    ogg_stream_state *state = luaL_checkudata(L,1,luaogg_stream_state_mt);
    if(ogg_stream_packetpeek(state,&packet) == 1) {
        luaogg_packet_to_table(L,&packet);
    }
    else {
        lua_pushnil(L);
    }
    return 1;
}

static const struct luaL_Reg luaogg_int64_metamethods[] = {
    { "__add",      luaogg_int64__add      },
    { "__sub",      luaogg_int64__sub      },
    { "__mul",      luaogg_int64__mul      },
    { "__div",      luaogg_int64__div      },
    { "__idiv",     luaogg_int64__div      },
    { "__mod",      luaogg_int64__mod      },
    { "__pow",      luaogg_int64__pow      },
    { "__unm",      luaogg_int64__unm      },
    { "__band",     luaogg_int64__band     },
    { "__bor",      luaogg_int64__bor      },
    { "__bxor",     luaogg_int64__bxor     },
    { "__bnot",     luaogg_int64__bnot     },
    { "__shl",      luaogg_int64__shl      },
    { "__shr",      luaogg_int64__shr      },
    { "__eq",       luaogg_int64__eq       },
    { "__lt",       luaogg_int64__lt       },
    { "__le",       luaogg_int64__le       },
    { "__tostring", luaogg_int64__tostring },
    { "__concat",   luaogg_int64__concat   },
    { NULL,         NULL                   },
};

static const luaogg_metamethods luaogg_sync_state_metamethods[] = {
    { "ogg_sync_init", "init"         },
    { "ogg_sync_check", "check"       },
    { "ogg_sync_clear", "clear"       },
    { "ogg_sync_reset", "reset"       },
    { "ogg_sync_buffer", "buffer"     },
    { "ogg_sync_pageseek", "pageseek" },
    { "ogg_sync_pageout", "pageout"   },
    { NULL, NULL },
};

static const luaogg_metamethods luaogg_stream_state_metamethods[] = {
    { "ogg_stream_pagein",          "pagein"         },
    { "ogg_stream_packetout",       "packetout"      },
    { "ogg_stream_packetpeek",      "packetpeek"     },
    { "ogg_stream_packetin",        "packetin"       },
    { "ogg_stream_pageout",         "pageout"        },
    { "ogg_stream_pageout_fill",    "pageout_fill"   },
    { "ogg_stream_flush",           "flush"          },
    { "ogg_stream_flush_fill",      "flush_fill"     },
    { "ogg_stream_init",            "init"           },
    { "ogg_stream_check",           "check"          },
    { "ogg_stream_clear",           "clear"          },
    { "ogg_stream_reset",           "reset"          },
    { "ogg_stream_reset_serialno",  "reset_serialno" },
    { NULL, NULL },
};

static const struct luaL_Reg luaogg_functions[] = {
    { "ogg_sync_state",            luaogg_ogg_sync_state },
    { "ogg_sync_init",             luaogg_ogg_sync_init },
    { "ogg_sync_check",            luaogg_ogg_sync_check },
    { "ogg_sync_clear",            luaogg_ogg_sync_clear },
    { "ogg_sync_reset",            luaogg_ogg_sync_reset },
    { "ogg_sync_buffer",           luaogg_ogg_sync_buffer },
    { "ogg_sync_pageseek",         luaogg_ogg_sync_pageseek },
    { "ogg_sync_pageout",          luaogg_ogg_sync_pageout },
    { "ogg_stream_state",          luaogg_ogg_stream_state },
    { "ogg_stream_pagein",         luaogg_ogg_stream_pagein  },
    { "ogg_stream_packetout",      luaogg_ogg_stream_packetout  },
    { "ogg_stream_packetpeek",     luaogg_ogg_stream_packetpeek  },
    { "ogg_stream_packetin",       luaogg_ogg_stream_packetin  },
    { "ogg_stream_pageout",        luaogg_ogg_stream_pageout  },
    { "ogg_stream_pageout_fill",   luaogg_ogg_stream_pageout_fill  },
    { "ogg_stream_flush",          luaogg_ogg_stream_flush  },
    { "ogg_stream_flush_fill",     luaogg_ogg_stream_flush_fill  },
    { "ogg_stream_init",           luaogg_ogg_stream_init  },
    { "ogg_stream_check",          luaogg_ogg_stream_check  },
    { "ogg_stream_clear",          luaogg_ogg_stream_clear  },
    { "ogg_stream_reset",          luaogg_ogg_stream_reset  },
    { "ogg_stream_reset_serialno", luaogg_ogg_stream_reset_serialno  },
    { "ogg_int64_t",               luaogg_int64 },
    { NULL,                        NULL },
};

LUAOGG_PUBLIC
int luaopen_luaogg(lua_State *L) {
    const luaogg_metamethods *sync_mm   = luaogg_sync_state_metamethods;
    const luaogg_metamethods *stream_mm = luaogg_stream_state_metamethods;

    luaL_newmetatable(L,luaogg_int64_mt);
    luaL_setfuncs(L,luaogg_int64_metamethods,0);
    lua_pop(L,1);

    lua_newtable(L);

    lua_pushinteger(L,LUAOGG_VERSION_MAJOR);
    lua_setfield(L,-2,"_VERSION_MAJOR");
    lua_pushinteger(L,LUAOGG_VERSION_MINOR);
    lua_setfield(L,-2,"_VERSION_MINOR");
    lua_pushinteger(L,LUAOGG_VERSION_PATCH);
    lua_setfield(L,-2,"_VERSION_PATCH");
    lua_pushliteral(L,LUAOGG_VERSION);
    lua_setfield(L,-2,"_VERSION");

    luaL_setfuncs(L,luaogg_functions,0);

    luaL_newmetatable(L,luaogg_sync_state_mt);
    lua_getfield(L,-2,"ogg_sync_clear");
    lua_setfield(L,-2,"__gc");

    lua_newtable(L); /* __index */
    while(sync_mm->name != NULL) {
        lua_getfield(L,-3,sync_mm->name);
        lua_setfield(L,-2,sync_mm->metaname);
        sync_mm++;
    }
    lua_setfield(L,-2,"__index");
    lua_pop(L,1);

    luaL_newmetatable(L,luaogg_stream_state_mt);
    lua_getfield(L,-2,"ogg_stream_clear");
    lua_setfield(L,-2,"__gc");

    lua_newtable(L);
    while(stream_mm->name != NULL) {
        lua_getfield(L,-3,stream_mm->name);
        lua_setfield(L,-2,stream_mm->metaname);
        stream_mm++;
    }

    lua_setfield(L,-2,"__index");
    lua_pop(L,1);

    return 1;
}

