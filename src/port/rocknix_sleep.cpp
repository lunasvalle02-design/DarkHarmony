#include "rocknix_sleep.h"

#if defined(__linux__)
#include <dlfcn.h>
#include <unistd.h>
#endif
#include <cstdint>
#include <cstdio>

namespace port {

#if defined(__linux__)
namespace {

struct DBusConnection;
struct DBusMessage;
using dbus_bool_t = std::uint32_t;
struct DBusError {
    const char* name;
    const char* message;
    unsigned int dummy1 : 1, dummy2 : 1, dummy3 : 1, dummy4 : 1, dummy5 : 1;
    void* padding1;
};

constexpr int kBusSystem = 1;
constexpr int kTypeInvalid = 0, kTypeBoolean = 'b', kTypeString = 's', kTypeUnixFd = 'h';

struct Api {
    void (*error_init)(DBusError*);
    void (*error_free)(DBusError*);
    dbus_bool_t (*error_is_set)(const DBusError*);
    DBusConnection* (*bus_get_private)(int, DBusError*);
    void (*set_exit_on_disconnect)(DBusConnection*, dbus_bool_t);
    void (*bus_add_match)(DBusConnection*, const char*, DBusError*);
    DBusMessage* (*new_method_call)(const char*, const char*, const char*, const char*);
    dbus_bool_t (*append_args)(DBusMessage*, int, ...);
    DBusMessage* (*send_with_reply_and_block)(DBusConnection*, DBusMessage*, int, DBusError*);
    dbus_bool_t (*get_args)(DBusMessage*, DBusError*, int, ...);
    void (*message_unref)(DBusMessage*);
    dbus_bool_t (*read_write)(DBusConnection*, int);
    DBusMessage* (*pop_message)(DBusConnection*);
    dbus_bool_t (*is_signal)(DBusMessage*, const char*, const char*);
    void (*connection_close)(DBusConnection*);
    void (*connection_unref)(DBusConnection*);
} d;

void* g_lib = nullptr;
DBusConnection* g_conn = nullptr;
int g_inhibit_fd = -1;

bool load_libdbus() {
    g_lib = dlopen("libdbus-1.so.3", RTLD_NOW | RTLD_LOCAL);
    if (!g_lib) return false;
#define SYM(field, name) \
    if (!(*reinterpret_cast<void**>(&d.field) = dlsym(g_lib, name))) return false
    SYM(error_init, "dbus_error_init");
    SYM(error_free, "dbus_error_free");
    SYM(error_is_set, "dbus_error_is_set");
    SYM(bus_get_private, "dbus_bus_get_private");
    SYM(set_exit_on_disconnect, "dbus_connection_set_exit_on_disconnect");
    SYM(bus_add_match, "dbus_bus_add_match");
    SYM(new_method_call, "dbus_message_new_method_call");
    SYM(append_args, "dbus_message_append_args");
    SYM(send_with_reply_and_block, "dbus_connection_send_with_reply_and_block");
    SYM(get_args, "dbus_message_get_args");
    SYM(message_unref, "dbus_message_unref");
    SYM(read_write, "dbus_connection_read_write");
    SYM(pop_message, "dbus_connection_pop_message");
    SYM(is_signal, "dbus_message_is_signal");
    SYM(connection_close, "dbus_connection_close");
    SYM(connection_unref, "dbus_connection_unref");
#undef SYM
    return true;
}

// logind Inhibit("sleep", who, why, "delay") -> fd; the lock lasts while it is open.
void take_inhibitor() {
    if (!g_conn || g_inhibit_fd >= 0) return;
    DBusMessage* m = d.new_method_call("org.freedesktop.login1", "/org/freedesktop/login1",
                                       "org.freedesktop.login1.Manager", "Inhibit");
    if (!m) return;
    const char* what = "sleep";
    const char* who = "Harmony of Dissonance";
    const char* why = "Write the save before suspending";
    const char* mode = "delay";
    d.append_args(m, kTypeString, &what, kTypeString, &who, kTypeString, &why, kTypeString, &mode,
                  kTypeInvalid);
    DBusError err;
    d.error_init(&err);
    DBusMessage* reply = d.send_with_reply_and_block(g_conn, m, 1000, &err);
    d.message_unref(m);
    if (!reply) {
        std::printf("suspend: logind Inhibit failed: %s\n", d.error_is_set(&err) ? err.message : "?");
        d.error_free(&err);
        return;
    }
    int fd = -1;
    if (d.get_args(reply, &err, kTypeUnixFd, &fd, kTypeInvalid)) g_inhibit_fd = fd;
    else d.error_free(&err);
    d.message_unref(reply);
}

void release_inhibitor() {
    if (g_inhibit_fd >= 0) {
        close(g_inhibit_fd);
        g_inhibit_fd = -1;
    }
}

}  // namespace

bool sleep_init() {
    if (!load_libdbus()) {
        std::printf("suspend: libdbus-1 not available, suspend integration off\n");
        return false;
    }
    DBusError err;
    d.error_init(&err);
    g_conn = d.bus_get_private(kBusSystem, &err);
    if (!g_conn) {
        std::printf("suspend: no system bus (%s), suspend integration off\n",
                    d.error_is_set(&err) ? err.message : "?");
        d.error_free(&err);
        return false;
    }
    d.set_exit_on_disconnect(g_conn, 0);  // a bus hiccup must never kill the game
    d.bus_add_match(g_conn,
                    "type='signal',interface='org.freedesktop.login1.Manager',member='PrepareForSleep'",
                    &err);
    if (d.error_is_set(&err)) {
        std::printf("suspend: AddMatch failed: %s\n", err.message);
        d.error_free(&err);
        sleep_shutdown();
        return false;
    }
    take_inhibitor();
    std::printf("suspend: logind integration %s\n",
                g_inhibit_fd >= 0 ? "on (delay inhibitor held)" : "on (no inhibitor)");
    std::fflush(stdout);
    return true;
}

SleepEvent sleep_poll() {
    if (!g_conn) return SleepEvent::None;
    SleepEvent result = SleepEvent::None;
    d.read_write(g_conn, 0);
    while (DBusMessage* msg = d.pop_message(g_conn)) {
        if (d.is_signal(msg, "org.freedesktop.login1.Manager", "PrepareForSleep")) {
            DBusError err;
            d.error_init(&err);
            dbus_bool_t start = 0;
            if (d.get_args(msg, &err, kTypeBoolean, &start, kTypeInvalid))
                result = start ? SleepEvent::GoingToSleep : SleepEvent::Resumed;
            else
                d.error_free(&err);
        }
        d.message_unref(msg);
    }
    if (result == SleepEvent::Resumed) take_inhibitor();
    return result;
}

void sleep_ready() { release_inhibitor(); }

void sleep_shutdown() {
    release_inhibitor();
    if (g_conn) {
        d.connection_close(g_conn);
        d.connection_unref(g_conn);
        g_conn = nullptr;
    }
}

#else  // !__linux__

bool sleep_init() { return false; }
SleepEvent sleep_poll() { return SleepEvent::None; }
void sleep_ready() {}
void sleep_shutdown() {}

#endif

}  // namespace port
