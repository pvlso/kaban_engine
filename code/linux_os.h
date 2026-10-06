#if !defined(LINUX_OS_H)
/* ========================================================================
   $File: $
   $Date: 2026 $
   $Revision: $
   $Creator: pvlso $
   $Notice: $
   ======================================================================== */

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <dlfcn.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#define OS_LIBRARY_EXTENSION ".so"

struct os_file
{
    int FileDescriptor;
};

struct os_directory
{
    DIR *Handle;
    char Path[4096];
};

typedef sem_t os_semaphore;

#define LINUX_OS_H
#endif
