/*
* Copyright (C) 2014 - plutoo
* Copyright (C) 2014 - ichfly
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "util.h"
#include "arm11.h"
#include "handles.h"
#include "mem.h"
#include "svc.h"


// -- Mutexes --

u32 mutex_SyncRequest(handleinfo* h, bool *locked)
{
    ERROR("ERROR: mutex should not be SyncRequest'd\n");
    return -1;
}

u32 mutex_WaitSynchronization(handleinfo* h, bool *locked)
{
    PAUSE();

    if(h->locked) { // If it's locked, then we say it's locked
        *locked = 1;
    } else { // Otherwise we lock it
        h->locked = 1;
        *locked = 0;
    }

    return 0;
}

u32 svcCreateMutex()
{
    u32 locked = arm11_R(1);
    u32 handle = handle_New(HANDLE_TYPE_MUTEX, 0);

    handleinfo* h = handle_Get(handle);
    if(h == NULL) {
        DEBUG("failed to get newly created mutex\n");
        PAUSE();
        return -1;
    }

    h->locked = !!locked;

    arm11_SetR(1, handle); // handle_out
    return 0;
}

u32 svcReleaseMutex()
{
    u32 handle = arm11_R(0);
    handleinfo* h = handle_Get(handle);

    if (0x1234 == handle) {
        ERROR("stub handle\n");
        return 0;
    }

    if(h == NULL) {
        ERROR("svcReleaseMutex on an invalid handle\n");
        PAUSE();
        return -1;
    }

    if (h->type != HANDLE_TYPE_MUTEX) {
        ERROR("svcReleaseMutex on a handle that is not a MUTEX\n");
        PAUSE();
        return -1;
    }

    h->locked = false;
    return 0;
}

// -- This does not belong here --

u32 svcDuplicateHandle()
{
    u32 to_clone = arm11_R(1);
    u32 handle;

    if (to_clone == HANDLE_CURRENT_THREAD)
        to_clone = threads_GetCurrentThreadHandle();

    if (to_clone == HANDLE_CURRENT_PROCESS)
        to_clone = g_process_handle;

    handle = handle_New(HANDLE_TYPE_REDIR, to_clone);

    handleinfo* h = handle_Get(handle);
    if (h == NULL) {
        DEBUG("Failed to get newly created copy\n");
        PAUSE();
        return -1;
    }
    arm11_SetR(1, handle); // handle_out
    return 0;
}

// -- Semaphores --

// Result CreateSemaphore(Handle* semaphore, s32 initialCount, s32 maxCount)
u32 svcCreateSemaphore()
{
    u32 initialCount = arm11_R(1);
    u32 maxCount = arm11_R(2);
    u32 handle = handle_New(HANDLE_TYPE_SEMAPHORE, 0);

    handleinfo* h = handle_Get(handle);
    if (h == NULL) {
        DEBUG("failed to get newly created semaphore\n");
        PAUSE();
        return -1;
    }

    h->misc[0] = initialCount;
    h->misc[1] = maxCount;

    arm11_SetR(1, handle); // handle_out
    return 0;
}

// Result ReleaseSemaphore(s32* count, Handle semaphore, s32 releaseCount)
u32 svcReleaseSemaphore()
{
    u32 count = arm11_R(0);
    u32 handle = arm11_R(1);
    u32 releaseCount = arm11_R(2);
    handleinfo* h = handle_Get(handle);

    if (h == NULL) {
        ERROR("svcReleaseSemaphore on an invalid handle\n");
        PAUSE();
        return -1;
    }

    if (h->type != HANDLE_TYPE_SEMAPHORE) {
        ERROR("svcReleaseSemaphore on a handle that is not a SEMAPHORE\n");
        PAUSE();
        return -1;
    }

    if(releaseCount > h->misc[0]) {
        ERROR("Releasing too much on a semaphore!\n!");
        h->misc[0] = 0;
    } else
        h->misc[0] -= releaseCount;

    arm11_SetR(1, h->misc[0]); // count_out
    return 0;
}

u32 semaphore_SyncRequest(handleinfo* h, bool *locked)
{
    ERROR("ERROR: semaphore should not be SyncRequest'd\n");
    return -1;
}

u32 semaphore_WaitSynchronization(handleinfo* h, bool *locked)
{
    if(h->misc[0] < h->misc[1]) {
        h->misc[0]++;
        *locked = 0;
    } else {
        *locked = 1;
    }

    return 0;
}
