#ifndef XIAOJI_PLUGIN_API_H
#define XIAOJI_PLUGIN_API_H

#include <stdint.h>
#include <stddef.h>

#if defined(_WIN32)
#define XJ_CALL __cdecl
#define XJ_EXPORT __declspec(dllexport)
#else
#define XJ_CALL
#define XJ_EXPORT __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define XJ_PLUGIN_ABI_VERSION 0x00010000u
#define XJ_PLUGIN_ENTRY "XiaojiPluginGetApi"

typedef int32_t XjResult;
enum { XJ_OK = 0, XJ_ERROR = -1, XJ_UNSUPPORTED = -2, XJ_INVALID_ARGUMENT = -3, XJ_INCOMPATIBLE = -4 };
enum { XJ_KEYBOARD = 0, XJ_MOUSE = 1 };
enum { XJ_SOURCE_PHYSICAL = 0, XJ_SOURCE_INJECTED = 1, XJ_SOURCE_REPEATER = 2, XJ_SOURCE_MACRO = 3, XJ_SOURCE_CAPTURE = 4 };
enum { XJ_TAP = 0, XJ_DOWN = 1, XJ_UP = 2, XJ_HELD_DOWN = 3, XJ_RESTORE_PHYSICAL = 4, XJ_MOVE = 5, XJ_WHEEL = 6 };
enum { XJ_CONSUME_HOOK = 0, XJ_CONSUME_RAW = 1, XJ_CONSUME_WHEEL = 2 };
enum { XJ_TEXT_ERROR = 0, XJ_TEXT_KEY_SUPPORT = 1, XJ_TEXT_PORT = 2, XJ_TEXT_FIRMWARE = 3, XJ_TEXT_DIAGNOSTIC = 4 };
enum { XJ_CONTROL_HEARTBEAT = 1, XJ_CONTROL_SUSPEND = 2, XJ_CONTROL_RESUME = 3, XJ_CONTROL_DEVICE_CHANGE = 4 };

#define XJ_CAP_KEYBOARD UINT64_C(1)
#define XJ_CAP_MOUSE (UINT64_C(1) << 1)
#define XJ_CAP_MOVE (UINT64_C(1) << 2)
#define XJ_CAP_WHEEL (UINT64_C(1) << 3)
#define XJ_CAP_MODIFIER_ISOLATION (UINT64_C(1) << 4)
#define XJ_CAP_PAUSE_MODIFIER_RELAY (UINT64_C(1) << 5)
#define XJ_CAP_HARDWARE_OUTPUT (UINT64_C(1) << 6)

typedef struct XjKey {
    uint32_t device;
    uint32_t code;
    uint32_t modifiers;
} XjKey;

typedef struct XjInput {
    uint32_t size;
    uint32_t action;
    uint32_t source;
    XjKey key;
    int32_t x;
    int32_t y;
    int32_t wheel_delta;
} XjInput;

/* Strings are UTF-8. Buffers and structures remain owned by their caller.
   Callbacks must not throw. A host context remains valid until shutdown returns. */
typedef struct XjHostApi {
    uint32_t size;
    uint32_t abi_version;
    void* context;
    void (XJ_CALL *log)(void* context, uint32_t level, const char* message);
    /* Optional 1.x extension. JSON is a UTF-8 object, at most 65536 bytes.
       These callbacks copy their inputs and may be called from plugin threads. */
    void (XJ_CALL *reply)(void* context, const char* request_id, XjResult result, const char* json, uint32_t length);
    void (XJ_CALL *publish_event)(void* context, const char* topic, const char* json, uint32_t length);
} XjHostApi;

typedef struct XjPluginPaths {
    uint32_t size;
    const char* plugin_directory;
    const char* data_directory;
    const char* host_directory;
} XjPluginPaths;

typedef struct XjDriverApi {
    uint32_t size;
    uint32_t abi_version;
    void* instance;
    void (XJ_CALL *destroy)(void* instance);
    XjResult (XJ_CALL *connect)(void* instance, const char* argument);
    void (XJ_CALL *disconnect)(void* instance);
    uint32_t (XJ_CALL *is_ready)(void* instance);
    uint64_t (XJ_CALL *capabilities)(void* instance);
    XjResult (XJ_CALL *send)(void* instance, const XjInput* input);
    void (XJ_CALL *release_all)(void* instance);
    void (XJ_CALL *clear_states)(void* instance);
    uint32_t (XJ_CALL *consume)(void* instance, const XjInput* input, uint32_t channel);
    /* Return required UTF-8 bytes INCLUDING the NUL; a null buffer queries size. */
    uint32_t (XJ_CALL *get_text)(void* instance, uint32_t kind, const XjKey* key, char* buffer, uint32_t capacity);
    XjResult (XJ_CALL *control)(void* instance, uint32_t operation, const char* argument);
    void (XJ_CALL *set_source_keys)(void* instance, uint32_t source, const XjKey* keys, uint32_t count);
    void (XJ_CALL *release_source)(void* instance, uint32_t source);
    /* Optional hook-path query: must not wait, perform I/O or acquire engine locks. */
    uint32_t (XJ_CALL *supports_physical_key)(void* instance, const XjKey* key);
} XjDriverApi;

typedef struct XjPluginApi {
    uint32_t size;
    uint32_t abi_version;
    const char* plugin_id;
    const char* plugin_version;
    XjResult (XJ_CALL *initialize)(const XjHostApi* host, const XjPluginPaths* paths, void** instance);
    XjResult (XJ_CALL *create_driver)(void* instance, const char* provider, XjDriverApi* driver);
    void (XJ_CALL *shutdown)(void* instance);
    /* Optional 1.x extension; worker-thread calls, serialized per plugin runtime.
       stop_background must finish/join background work before it returns.
       handle_request must return promptly; copy inputs for asynchronous work,
       then call host->reply exactly once. No UI or synchronous host waits. */
    XjResult (XJ_CALL *start_background)(void* instance);
    void (XJ_CALL *stop_background)(void* instance);
    XjResult (XJ_CALL *handle_request)(void* instance, const char* request_id, const char* command, const char* json, uint32_t length);
} XjPluginApi;

/* Check size before accessing optional members; ABI 1.0 prefixes remain valid. */
#define XJ_HAS_FIELD(object, type, field) ((object) && (object)->size >= offsetof(type, field) + sizeof((object)->field))

typedef const XjPluginApi* (XJ_CALL *XjGetPluginApi)(void);

#ifdef __cplusplus
}
#endif
#endif
