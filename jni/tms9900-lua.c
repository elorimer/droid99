// Testing LuaJIT integration

#include <lua.h>
#include <lauxlib.h>
#include <android/log.h>
#include <jni.h>
#include <string.h>
#include "tms9900-core.h"

extern JavaVM *gJavaVM;
struct TMS9900 *CPU;

static lua_State *lua_engine = NULL;
static jobject outStreamObj;
static int tracing_vdp = 0, tracing_cpu = 0, branch_tracing = 0;


static void output(const char *s)
{
    JNIEnv *env;
    (*gJavaVM)->GetEnv(gJavaVM, (void **)&env, JNI_VERSION_1_4);
    if (env == NULL) {
        __android_log_print(ANDROID_LOG_ERROR, "And99", "GetEnv failed!");
        return;
    }
    jclass klass = (*env)->GetObjectClass(env, outStreamObj);
    jmethodID method = (*env)->GetMethodID(env, klass, "write", "([C)V");
    if (method == NULL) {
        __android_log_print(ANDROID_LOG_ERROR, "And99", "JNI failed to lookup write method");
        return;
    }
    int len = strlen(s);
    jcharArray jStr = (*env)->NewCharArray(env, len);
    jchar *jcharBuffer = (jchar *)malloc(sizeof(jchar) * len);
    int i;
    for (i=0; i < len; i++)
        jcharBuffer[i] = (jchar)s[i];
    (*env)->SetCharArrayRegion(env, jStr, 0, len, jcharBuffer);
    (*env)->CallVoidMethod(env, outStreamObj, method, jStr);

    (*env)->DeleteLocalRef(env, klass);
    (*env)->DeleteLocalRef(env, jStr);

    free(jcharBuffer);
}


static int l_output(lua_State *L)
{
    const char *s = luaL_checkstring(L, 1);
    output(s);
    return 0;
}


static int l_get_pc(lua_State *L)
{
    lua_pushinteger(L, CPU->PC << 1);
    return 1;
}

static int l_break_cpu(lua_State *L)
{
    CPU->exception = 1;
    return 0;
}


static int lua_memory_index(lua_State *L)
{
    lua_Integer i = lua_tointeger(L, -1);
    lua_pop(L, 1);
    // top of stack has the table.  lookup the '__private_user_data' field
    lua_getfield(L, -1, "__private_user_data");
    uint8_t *memory = lua_touserdata(L, -1);
    lua_pop(L, 2);

    // XXX: much better error checking needed
    lua_pushinteger(L, memory[i]);
    return 1;
}


static int lua_memory_newindex(lua_State *L)
{
    lua_Integer value = lua_tointeger(L, -1);
    lua_pop(L, 1);
    lua_Integer key = lua_tointeger(L, -1);
    lua_pop(L, 1);
    // top of stack has the table.  lookup the '__private_user_data' field
    lua_getfield(L, -1, "__private_user_data");
    uint8_t *memory = lua_touserdata(L, -1);
    lua_pop(L, 2);

    // XXX: much better error checking needed
    memory[key] = value;

    return 0;
}


void
Java_com_emllabs_droid99_SocketDebugBridge_StartupLua(JNIEnv *env, jobject thiz,
                                                   jobject logStream)
{
    outStreamObj = (*env)->NewGlobalRef(env, logStream);

    if (!lua_engine) {
        lua_engine = luaL_newstate();
        luaL_openlibs(lua_engine);
    }

    // register the output function for writing data back
    lua_pushcfunction(lua_engine, l_output);
    lua_setglobal(lua_engine, "output");

    // register the callback table
    lua_newtable(lua_engine);
    lua_setglobal(lua_engine, "callbacks");
}


void
Java_com_emllabs_droid99_SocketDebugBridge_ExecuteLua(JNIEnv *env, jobject thiz,
                                                   jstring code)
{
    const char *nativeString = (*env)->GetStringUTFChars(env, code, 0);

    int status = luaL_loadstring(lua_engine, nativeString);
    if (status) {
        __android_log_print(ANDROID_LOG_ERROR, "And99", "luaL_loadstring failed.");
        return;
    }

    int result = lua_pcall(lua_engine, 0, 1, 0);
    if (result) {
        const char *msg = lua_tostring(lua_engine, -1);
        output("Lua ERROR: ");
        output(msg);
        lua_pop(lua_engine, 1);
        return;
    }

    // clean up the stack
    lua_pop(lua_engine, 1);

    (*env)->ReleaseStringUTFChars(env, code, nativeString);

    // See if any of the callbacks have changed and update flags
    lua_getglobal(lua_engine, "callbacks");
    lua_getfield(lua_engine, -1, "vdp");
    tracing_vdp = 1;
    if (lua_isnil(lua_engine, -1))
        tracing_vdp = 0;
    lua_pop(lua_engine, 1);
    lua_getfield(lua_engine, -1, "cpu");
    tracing_cpu = 1;
    if (lua_isnil(lua_engine, -1))
        tracing_cpu = 0;
    lua_pop(lua_engine, 1);
    lua_getfield(lua_engine, -1, "branch");
    branch_tracing = 1;
    if (lua_isnil(lua_engine, -1))
        branch_tracing = 0;

    lua_pop(lua_engine, 2);
}


void LuaBindMemory(uint8_t *memory, const char *symbol)
{
    if (!lua_engine) {
        lua_engine = luaL_newstate();
        luaL_openlibs(lua_engine);
    }

    lua_newtable(lua_engine);
    lua_pushlightuserdata(lua_engine, memory);
    lua_setfield(lua_engine, -2, "__private_user_data");
    lua_newtable(lua_engine);       // metatable
    lua_pushcfunction(lua_engine, lua_memory_index);
    lua_setfield(lua_engine, -2, "__index");
    lua_pushcfunction(lua_engine, lua_memory_newindex);
    lua_setfield(lua_engine, -2, "__newindex");
    lua_setmetatable(lua_engine, -2);
    lua_setglobal(lua_engine, symbol);
}


void LuaBindCPU(struct TMS9900 *cpu)
{
    CPU = cpu;
    lua_pushcfunction(lua_engine, l_get_pc);
    lua_setglobal(lua_engine, "_get_pc");
    lua_pushcfunction(lua_engine, l_break_cpu);
    lua_setglobal(lua_engine, "_break_cpu");
}


void LuaTraceVDP(uint16_t address, uint8_t source)
{
    if (!tracing_vdp)
        return;

    // call callbacks.vdp
    lua_getglobal(lua_engine, "callbacks");
    lua_getfield(lua_engine, -1, "vdp");
    lua_pushinteger(lua_engine, address);
    lua_pushinteger(lua_engine, source);
    lua_pcall(lua_engine, 2, 0, 0);
    // handle errors
    lua_pop(lua_engine, 1);
}


void LuaTraceCPU(uint16_t id)
{
    if (!tracing_cpu)
        return;

    lua_getglobal(lua_engine, "callbacks");
    lua_getfield(lua_engine, -1, "cpu");
    lua_pushinteger(lua_engine, id);
    lua_pcall(lua_engine, 1, 0, 0);

    lua_pop(lua_engine, 1);
}


void LuaBranchTrace(uint16_t new_pc)
{
    if (!branch_tracing)
        return;

    lua_getglobal(lua_engine, "callbacks");
    lua_getfield(lua_engine, -1, "branch");
    lua_pushinteger(lua_engine, new_pc);
    lua_pcall(lua_engine, 1, 0, 0);

    lua_pop(lua_engine, 1);
}
