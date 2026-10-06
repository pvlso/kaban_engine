#if !defined(WIN32_OS_H)
/* ========================================================================
   $File: $
   $Date: 2026 $
   $Revision: $
   $Creator: pvlso $
   $Notice: $
   ======================================================================== */

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define OS_LIBRARY_EXTENSION ".dll"

#define WIN32_OS_PATH_COUNT 4096

struct os_file
{
    HANDLE Handle;
};

struct os_directory
{
    HANDLE FindHandle;
    WIN32_FIND_DATAW FindData;
    b32 HasPendingEntry;
    char FileName[WIN32_OS_PATH_COUNT];
};

typedef HANDLE os_semaphore;

#define WIN32_OS_H
#endif
