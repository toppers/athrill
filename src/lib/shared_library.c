#include "shared_library.h"

#include <stdint.h>
#include <stdio.h>

static void set_error_message(
    char *error_message,
    size_t error_message_size,
    const char *message)
{
    if ((error_message == NULL) || (error_message_size == 0U)) {
        return;
    }
    (void)snprintf(error_message, error_message_size, "%s", message);
}

#ifdef _WIN32

#include <windows.h>

static void set_windows_error(
    char *error_message,
    size_t error_message_size,
    DWORD error_code)
{
    DWORD length;

    if ((error_message == NULL) || (error_message_size == 0U)) {
        return;
    }
    length = FormatMessageA(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        error_code,
        0,
        error_message,
        (DWORD)error_message_size,
        NULL);
    if (length == 0U) {
        (void)snprintf(
            error_message,
            error_message_size,
            "Windows error %lu",
            (unsigned long)error_code);
    }
}

AthrillSharedLibraryHandle athrill_shared_library_open(
    const char *path,
    char *error_message,
    size_t error_message_size)
{
    HMODULE handle;

    if (path == NULL) {
        set_error_message(error_message, error_message_size, "library path is null");
        return NULL;
    }
    handle = LoadLibraryA(path);
    if (handle == NULL) {
        set_windows_error(error_message, error_message_size, GetLastError());
    }
    return (AthrillSharedLibraryHandle)handle;
}

void *athrill_shared_library_symbol(
    AthrillSharedLibraryHandle handle,
    const char *symbol_name,
    char *error_message,
    size_t error_message_size)
{
    FARPROC symbol;

    if ((handle == NULL) || (symbol_name == NULL)) {
        set_error_message(error_message, error_message_size, "invalid symbol lookup");
        return NULL;
    }
    symbol = GetProcAddress((HMODULE)handle, symbol_name);
    if (symbol == NULL) {
        set_windows_error(error_message, error_message_size, GetLastError());
        return NULL;
    }
    return (void *)(uintptr_t)symbol;
}

void athrill_shared_library_close(AthrillSharedLibraryHandle handle)
{
    if (handle != NULL) {
        (void)FreeLibrary((HMODULE)handle);
    }
}

#else

#include <dlfcn.h>

AthrillSharedLibraryHandle athrill_shared_library_open(
    const char *path,
    char *error_message,
    size_t error_message_size)
{
    AthrillSharedLibraryHandle handle;
    const char *error;

    if (path == NULL) {
        set_error_message(error_message, error_message_size, "library path is null");
        return NULL;
    }
    (void)dlerror();
    handle = dlopen(path, RTLD_NOW);
    if (handle == NULL) {
        error = dlerror();
        set_error_message(
            error_message,
            error_message_size,
            error != NULL ? error : "dlopen failed");
    }
    return handle;
}

void *athrill_shared_library_symbol(
    AthrillSharedLibraryHandle handle,
    const char *symbol_name,
    char *error_message,
    size_t error_message_size)
{
    void *symbol;
    const char *error;

    if ((handle == NULL) || (symbol_name == NULL)) {
        set_error_message(error_message, error_message_size, "invalid symbol lookup");
        return NULL;
    }
    (void)dlerror();
    symbol = dlsym(handle, symbol_name);
    error = dlerror();
    if (error != NULL) {
        set_error_message(error_message, error_message_size, error);
        return NULL;
    }
    return symbol;
}

void athrill_shared_library_close(AthrillSharedLibraryHandle handle)
{
    if (handle != NULL) {
        (void)dlclose(handle);
    }
}

#endif
