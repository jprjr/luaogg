#define LUAOGG_VERSION_MAJOR 1
#define LUAOGG_VERSION_MINOR 0
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
#endif

static const char * const digits = "0123456789";

static char *
luaogg_int64_to_str(ogg_int64_t value, char buffer[24], size_t *len) {
    char *p = buffer + 23;
    int negative = value < 0;
    *p = '\0';
    do {
        *--p = digits[labs(value % 10)];
    } while (value /= 10);

    if(negative) {
        *--p = '-';
    }

    *len = 23 - (p - buffer);
    return p;
}

static inline ogg_int64_t
luaogg_toint64(lua_State *L, int idx) {
    ogg_int64_t *t = NULL;
    ogg_int64_t tmp = 0;
    const char *str = NULL;

    switch(lua_type(L,idx)) {
        case LUA_TUSERDATA: {
            t = lua_touserdata(L,idx);
            return *t;
        }
        case LUA_TNUMBER: {
            return (ogg_int64_t)lua_tointeger(L,idx);
        }
        default: break;
    }

    str = lua_tostring(L,idx);
    errno = 0;
    tmp = strtoll(str,NULL,10);
    if(errno) {
        luaL_error(L,"invalid integer string");
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
    lua_pushvalue(L,lua_upvalueindex(1));
    lua_setmetatable(L,-2);
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

    lua_pushvalue(L,lua_upvalueindex(1));
    lua_setmetatable(L,-2);
    lua_setfield(L,-2,"granulepos");

    t = lua_newuserdata(L,sizeof(ogg_int64_t));
    *t = packet->packetno;

    lua_pushvalue(L,lua_upvalueindex(1));
    lua_setmetatable(L,-2);
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
    const char *str = NULL;
    ogg_int64_t *t = 0;

    t = (ogg_int64_t *)lua_newuserdata(L,sizeof(ogg_int64_t));
    if(t == NULL) {
        return luaL_error(L,"out of memory");
    }

    if(lua_isnumber(L,1)) {
        *t = (ogg_int64_t)lua_tointeger(L,1);
    }
    else if(!lua_isnoneornil(L,1)) {
        str = lua_tostring(L,1);
        errno = 0;
        *t = strtoll(str,NULL,10);
        if(errno) {
            luaL_error(L,"invalid integer string");
        }
    } else {
      *t = 0;
    }

    lua_pushvalue(L,lua_upvalueindex(1));
    lua_setmetatable(L,-2);
    return 1;
}

static int
luaogg_int64__unm(lua_State *L) {
    ogg_int64_t *o = NULL;
    ogg_int64_t *r = NULL;

    o = lua_touserdata(L,1);

    r = (ogg_int64_t *)lua_newuserdata(L,sizeof(ogg_int64_t));

    *r = *o * -1;

    lua_pushvalue(L,lua_upvalueindex(1));
    lua_setmetatable(L,-2);

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

    lua_pushvalue(L,lua_upvalueindex(1));
    lua_setmetatable(L,-2);

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

    lua_pushvalue(L,lua_upvalueindex(1));
    lua_setmetatable(L,-2);

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

    lua_pushvalue(L,lua_upvalueindex(1));
    lua_setmetatable(L,-2);

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

    lua_pushvalue(L,lua_upvalueindex(1));
    lua_setmetatable(L,-2);

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

    lua_pushvalue(L,lua_upvalueindex(1));
    lua_setmetatable(L,-2);

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

    lua_pushvalue(L,lua_upvalueindex(1));
    lua_setmetatable(L,-2);

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

    lua_pushvalue(L,lua_upvalueindex(1));
    lua_setmetatable(L,-2);

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

    lua_pushvalue(L,lua_upvalueindex(1));
    lua_setmetatable(L,-2);

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

    lua_pushvalue(L,lua_upvalueindex(1));
    lua_setmetatable(L,-2);

    return 1;
}

static int
luaogg_int64__bnot(lua_State *L) {
    ogg_int64_t *o = NULL;
    ogg_int64_t *r = NULL;

    o = lua_touserdata(L,1);

    r = (ogg_int64_t *)lua_newuserdata(L,sizeof(ogg_int64_t));

    *r = ~*o;

    lua_pushvalue(L,lua_upvalueindex(1));
    lua_setmetatable(L,-2);

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

    lua_pushvalue(L,lua_upvalueindex(1));
    lua_setmetatable(L,-2);

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

    lua_pushvalue(L,lua_upvalueindex(1));
    lua_setmetatable(L,-2);

    return 1;
}

static int
luaogg_int64__tostring(lua_State *L) {
    char *p;
    char t[24];
    size_t l;

    ogg_int64_t *o = (ogg_int64_t *)lua_touserdata(L,1);
    p = luaogg_int64_to_str(*o,t,&l);
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

    lua_pushvalue(L,lua_upvalueindex(1));
    lua_setmetatable(L,-2);

    return 1;
}

static int
luaogg_ogg_stream_state(lua_State *L) {
    ogg_stream_state *state = lua_newuserdata(L,sizeof(ogg_stream_state));
    if(state == NULL) {
        return luaL_error(L,"out of memory");
    }

    lua_pushvalue(L,lua_upvalueindex(1));
    lua_setmetatable(L,-2);

    return 1;
}

static int
luaogg_ogg_sync_init(lua_State *L) {
    ogg_sync_state *state = lua_touserdata(L,1);
    ogg_sync_init(state);
    return 0;
}

static int
luaogg_ogg_sync_check(lua_State *L) {
    ogg_sync_state *state = lua_touserdata(L,1);
    lua_pushboolean(L,ogg_sync_check(state) == 0);
    return 1;
}

static int
luaogg_ogg_sync_clear(lua_State *L) {
    ogg_sync_state *state = lua_touserdata(L,1);
    ogg_sync_clear(state);
    return 0;
}

static int
luaogg_ogg_sync_reset(lua_State *L) {
    ogg_sync_state *state = lua_touserdata(L,1);
    lua_pushboolean(L,ogg_sync_reset(state) == 0);
    return 1;
}

static int
luaogg_ogg_sync_buffer(lua_State *L) {
    ogg_sync_state *state = lua_touserdata(L,1);
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
    ogg_sync_state *state = lua_touserdata(L,1);
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
    ogg_sync_state *state = lua_touserdata(L,1);
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
    ogg_stream_state *state = lua_touserdata(L,1);
    lua_pushboolean(L,ogg_stream_init(state,luaL_checkinteger(L,2)) == 0);
    return 1;
}

static int
luaogg_ogg_stream_check(lua_State *L) {
    ogg_stream_state *state = lua_touserdata(L,1);
    lua_pushboolean(L,ogg_stream_check(state) == 0);
    return 1;
}

static int
luaogg_ogg_stream_clear(lua_State *L) {
    ogg_stream_state *state = lua_touserdata(L,1);
    ogg_stream_clear(state);
    return 0;
}

static int
luaogg_ogg_stream_reset(lua_State *L) {
    ogg_stream_state *state = lua_touserdata(L,1);
    lua_pushboolean(L,ogg_stream_reset(state) == 0);
    return 1;
}

static int
luaogg_ogg_stream_reset_serialno(lua_State *L) {
    ogg_stream_state *state = lua_touserdata(L,1);
    lua_pushboolean(L,ogg_stream_reset_serialno(state,lua_tointeger(L,2)) == 0);
    return 1;
}

static int
luaogg_ogg_stream_pagein(lua_State *L) {
    ogg_page page;
    ogg_stream_state *state = lua_touserdata(L,1);

    luaogg_table_to_page(L,2,&page);

    lua_pushboolean(L,ogg_stream_pagein(state,&page) == 0);
    return 1;
}

static int
luaogg_ogg_stream_pageout(lua_State *L) {
    ogg_page page;
    ogg_stream_state *state = lua_touserdata(L,1);

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
    ogg_stream_state *state = lua_touserdata(L,1);

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
    ogg_stream_state *state = lua_touserdata(L,1);

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
    ogg_stream_state *state = lua_touserdata(L,1);

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
    ogg_stream_state *state = lua_touserdata(L,1);

    luaogg_table_to_packet(L, 2, &packet);

    lua_pushboolean(L,ogg_stream_packetin(state,&packet) == 0);
    return 1;
}

static int
luaogg_ogg_stream_packetout(lua_State *L) {
    ogg_packet packet;
    ogg_stream_state *state = lua_touserdata(L,1);
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
    ogg_stream_state *state = lua_touserdata(L,1);
    if(ogg_stream_packetpeek(state,&packet) == 1) {
        luaogg_packet_to_table(L,&packet);
    }
    else {
        lua_pushnil(L);
    }
    return 1;
}

static const struct luaL_Reg luaogg_functions[] = {
    { "ogg_sync_init", luaogg_ogg_sync_init },
    { "ogg_sync_check", luaogg_ogg_sync_check },
    { "ogg_sync_clear", luaogg_ogg_sync_clear },
    { "ogg_sync_reset", luaogg_ogg_sync_reset },
    { "ogg_sync_buffer", luaogg_ogg_sync_buffer },
    { "ogg_sync_pageseek", luaogg_ogg_sync_pageseek },
    { "ogg_sync_pageout", luaogg_ogg_sync_pageout },
    { "ogg_stream_pagein",       luaogg_ogg_stream_pagein  },
    { "ogg_stream_packetout",    luaogg_ogg_stream_packetout  },
    { "ogg_stream_packetpeek",   luaogg_ogg_stream_packetpeek  },
    { "ogg_stream_packetin",     luaogg_ogg_stream_packetin  },
    { "ogg_stream_pageout",      luaogg_ogg_stream_pageout  },
    { "ogg_stream_pageout_fill", luaogg_ogg_stream_pageout_fill  },
    { "ogg_stream_flush",        luaogg_ogg_stream_flush  },
    { "ogg_stream_flush_fill",   luaogg_ogg_stream_flush_fill  },
    { "ogg_stream_init",         luaogg_ogg_stream_init  },
    { "ogg_stream_check",        luaogg_ogg_stream_check  },
    { "ogg_stream_clear",        luaogg_ogg_stream_clear  },
    { "ogg_stream_reset",        luaogg_ogg_stream_reset  },
    { "ogg_stream_reset_serialno",   luaogg_ogg_stream_reset_serialno  },
    { NULL, NULL },
};

LUAOGG_PUBLIC
int luaopen_luaogg(lua_State *L) {
    lua_newtable(L);

    lua_pushinteger(L,LUAOGG_VERSION_MAJOR);
    lua_setfield(L,-2,"_VERSION_MAJOR");
    lua_pushinteger(L,LUAOGG_VERSION_MINOR);
    lua_setfield(L,-2,"_VERSION_MINOR");
    lua_pushinteger(L,LUAOGG_VERSION_PATCH);
    lua_setfield(L,-2,"_VERSION_PATCH");
    lua_pushliteral(L,LUAOGG_VERSION);
    lua_setfield(L,-2,"_VERSION");

    lua_newtable(L); /* int64 metatable */

    lua_pushvalue(L,-1);
    lua_pushcclosure(L,luaogg_int64,1);
    lua_setfield(L,-3,"ogg_int64_t");

    lua_pushvalue(L,-1);
    lua_pushcclosure(L,luaogg_int64__add,1);
    lua_setfield(L,-2,"__add");

    lua_pushvalue(L,-1);
    lua_pushcclosure(L,luaogg_int64__sub,1);
    lua_setfield(L,-2,"__sub");

    lua_pushvalue(L,-1);
    lua_pushcclosure(L,luaogg_int64__mul,1);
    lua_setfield(L,-2,"__mul");

    lua_pushvalue(L,-1);
    lua_pushcclosure(L,luaogg_int64__div,1);
    lua_setfield(L,-2,"__div");

    lua_pushvalue(L,-1);
    lua_pushcclosure(L,luaogg_int64__mod,1);
    lua_setfield(L,-2,"__mod");

    lua_pushvalue(L,-1);
    lua_pushcclosure(L,luaogg_int64__pow,1);
    lua_setfield(L,-2,"__pow");

    lua_pushvalue(L,-1);
    lua_pushcclosure(L,luaogg_int64__unm,1);
    lua_setfield(L,-2,"__unm");

    lua_pushvalue(L,-1);
    lua_pushcclosure(L,luaogg_int64__band,1);
    lua_setfield(L,-2,"__band");

    lua_pushvalue(L,-1);
    lua_pushcclosure(L,luaogg_int64__bor,1);
    lua_setfield(L,-2,"__bor");

    lua_pushvalue(L,-1);
    lua_pushcclosure(L,luaogg_int64__bxor,1);
    lua_setfield(L,-2,"__bxor");

    lua_pushvalue(L,-1);
    lua_pushcclosure(L,luaogg_int64__bnot,1);
    lua_setfield(L,-2,"__bnot");

    lua_pushvalue(L,-1);
    lua_pushcclosure(L,luaogg_int64__shl,1);
    lua_setfield(L,-2,"__shl");

    lua_pushvalue(L,-1);
    lua_pushcclosure(L,luaogg_int64__shr,1);
    lua_setfield(L,-2,"__shr");

    lua_pushcclosure(L,luaogg_int64__eq,0);
    lua_setfield(L,-2,"__eq");

    lua_pushcclosure(L,luaogg_int64__lt,0);
    lua_setfield(L,-2,"__lt");

    lua_pushcclosure(L,luaogg_int64__le,0);
    lua_setfield(L,-2,"__le");

    lua_pushcclosure(L,luaogg_int64__tostring,0);
    lua_setfield(L,-2,"__tostring");

    lua_pushcclosure(L,luaogg_int64__concat,0);
    lua_setfield(L,-2,"__concat");

    lua_getfield(L,-1,"__div");
    lua_setfield(L,-2,"__idiv");

    luaL_setfuncs(L,luaogg_functions,1);

    lua_newtable(L); /* ogg sync metatable */
    lua_newtable(L); /* __index */
    lua_getfield(L,-3,"ogg_sync_init");
    lua_setfield(L,-2,"init");
    lua_getfield(L,-3,"ogg_sync_check");
    lua_setfield(L,-2,"check");
    lua_getfield(L,-3,"ogg_sync_clear");
    lua_setfield(L,-2,"clear");
    lua_getfield(L,-3,"ogg_sync_reset");
    lua_setfield(L,-2,"reset");
    lua_getfield(L,-3,"ogg_sync_buffer");
    lua_setfield(L,-2,"buffer");
    lua_getfield(L,-3,"ogg_sync_pageseek");
    lua_setfield(L,-2,"pageseek");
    lua_getfield(L,-3,"ogg_sync_pageout");
    lua_setfield(L,-2,"pageout");
    lua_setfield(L,-2,"__index");

    lua_getfield(L,-2,"ogg_sync_clear");
    lua_setfield(L,-2,"__gc");

    lua_pushcclosure(L,luaogg_ogg_sync_state,1);
    lua_setfield(L,-2,"ogg_sync_state");

    lua_newtable(L); /* ogg stream metatable */
    lua_newtable(L); /* __index */

    lua_getfield(L,-3,"ogg_stream_pagein");
    lua_setfield(L,-2,"pagein");
    lua_getfield(L,-3,"ogg_stream_packetout");
    lua_setfield(L,-2,"packetout");
    lua_getfield(L,-3,"ogg_stream_packetpeek");
    lua_setfield(L,-2,"packetpeek");
    lua_getfield(L,-3,"ogg_stream_packetin");
    lua_setfield(L,-2,"packetin");
    lua_getfield(L,-3,"ogg_stream_pageout");
    lua_setfield(L,-2,"pageout");
    lua_getfield(L,-3,"ogg_stream_pageout_fill");
    lua_setfield(L,-2,"pageout_fill");
    lua_getfield(L,-3,"ogg_stream_flush");
    lua_setfield(L,-2,"flush");
    lua_getfield(L,-3,"ogg_stream_flush_fill");
    lua_setfield(L,-2,"flush_fill");
    lua_getfield(L,-3,"ogg_stream_init");
    lua_setfield(L,-2,"init");
    lua_getfield(L,-3,"ogg_stream_check");
    lua_setfield(L,-2,"check");
    lua_getfield(L,-3,"ogg_stream_clear");
    lua_setfield(L,-2,"clear");
    lua_getfield(L,-3,"ogg_stream_reset");
    lua_setfield(L,-2,"reset");
    lua_getfield(L,-3,"ogg_stream_reset_serialno");
    lua_setfield(L,-2,"reset_serialno");

    lua_setfield(L,-2,"__index");

    lua_getfield(L,-2,"ogg_stream_clear");
    lua_setfield(L,-2,"__gc");

    lua_pushcclosure(L,luaogg_ogg_stream_state,1);
    lua_setfield(L,-2,"ogg_stream_state");

    return 1;
}

