#ifndef ATHRILL_SHARED_LIBRARY_H
#define ATHRILL_SHARED_LIBRARY_H

#include <stddef.h>

typedef void *AthrillSharedLibraryHandle;

AthrillSharedLibraryHandle athrill_shared_library_open(
    const char *path,
    char *error_message,
    size_t error_message_size);
void *athrill_shared_library_symbol(
    AthrillSharedLibraryHandle handle,
    const char *symbol_name,
    char *error_message,
    size_t error_message_size);
void athrill_shared_library_close(AthrillSharedLibraryHandle handle);

#endif /* ATHRILL_SHARED_LIBRARY_H */
