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
#include "svc.h"

#include "mem.h"

#define MAX_NUM_HANDLES 0x1000

//#define EXIT_ON_ERROR 1

static handleinfo handles[MAX_NUM_HANDLES];
static u32 handles_num;


#define NUM_HANDLE_TYPES ARRAY_SIZE(handle_types)

void handle_Init()
{
    for (int i = 0; i < MAX_NUM_HANDLES; i++)
    {
        handles[i].taken = false;
        handles[i].handle = HANDLES_BASE + i;
    }
}

u32 handle_New(u32 type, uintptr_t subtype)
{
    int nexthandle = handles_num;
    if(handles_num == MAX_NUM_HANDLES) {
        for (int i = 0; i < MAX_NUM_HANDLES; i++)
            if (!handles[i].taken)
                nexthandle = i;
        if (handles_num == MAX_NUM_HANDLES)
        {
            ERROR("not enough handles..\n");
            arm11_Dump();
            exit(1);
        }
    }
    DEBUG("handle=%x\n", HANDLES_BASE + nexthandle);
    handles[nexthandle].taken = true;
    handles[nexthandle].type = type;
    handles[nexthandle].subtype = subtype;
    handles[nexthandle].locked = false;
    handles[nexthandle].locktype = LOCK_TYPE_STICKY;

    if (handles_num != MAX_NUM_HANDLES)
        handles_num++;

    return HANDLES_BASE + nexthandle;
}

handleinfo* handle_Get(u32 handle)
{
    u32 idx = handle - HANDLES_BASE;

    if (idx < handles_num) {
        if (!handles[idx].taken)
        {
            ERROR("handle_Get on free handle %08x", handle);
            return NULL;
        }
        if (handles[idx].type == HANDLE_TYPE_REDIR)
            return handle_Get(handles[idx].subtype);

        else return &handles[idx];
    }
    return NULL;
}


/* Generic SVC implementations. */
u32 svcSendSyncRequest()
{
    u32 handle = arm11_R(0);
    handleinfo* hi = handle_Get(handle);

    if(hi == NULL) {
        ERROR("handle %08x not found.\n", handle);
        IPC_debugprint(arm11_ServiceBufferAddress() + 0x80);
        arm11_Dump();
        PAUSE();
#ifdef EXIT_ON_ERROR
        exit(1);
#else
        return 0;
#endif
    }

    if(hi->type >= NUM_HANDLE_TYPES) {
        // This should never happen.
        ERROR("handle %08x has non-defined type.\n", handle);
        PAUSE();
        exit(1);
    }

    // Lookup actual callback in table.
    if (handle_types[hi->type].fnSyncRequest != NULL) {
        u32 ret;
        bool locked = false;

        ret = handle_types[hi->type].fnSyncRequest(hi, &locked);

        // Handle is locked so we put thread into WAITING state.
        if (locked) {
            u32* wait_list = malloc(4);
            wait_list[0] = handle;

            threads_SetCurrentThreadWaitList(wait_list, true, 1);
        }

        return ret;
    } else {
        ERROR("svcSyncRequest undefined for handle-type \"%s\".\n",
              handle_types[hi->type].name);
        PAUSE();
        exit(1);
    }
}

extern ARMul_State s;

u32 svcCloseHandle()
{
    u32 handle = arm11_R(0);

    if(handle == HANDLE_CURRENT_PROCESS) {
        printf("Program exited successfully.\n");
        PAUSE();
        exit(0);
    }

    handleinfo* hi = handle_Get(handle);

    if(hi == NULL) {
        ERROR("handle %08x not found.\n", handle);
        PAUSE();
#ifdef EXIT_ON_ERROR
        exit(1);
#else
        return 0;
#endif
    }

    if(hi->type >= NUM_HANDLE_TYPES) {
        // This should never happen.
        ERROR("handle %08x has non-defined type.\n", handle);
        PAUSE();
        exit(1);
    }

    // Lookup actual callback in table.
    if(handle_types[hi->type].fnCloseHandle != NULL)
        return handle_types[hi->type].fnCloseHandle(&s, handle);

    ERROR("svcCloseHandle undefined for handle-type \"%s\".\n",
          handle_types[hi->type].name);
    PAUSE();
    return 0;
}

u32 svcWaitSynchronization1() //todo timeout
{
    u32 handle = arm11_R(0);
    handleinfo* hi = handle_Get(handle);

    if(hi == NULL) {
        ERROR("handle %08x not found.\n", handle);
        PAUSE();
#ifdef EXIT_ON_ERROR
        exit(1);
#else
        return 0;
#endif
    }

    if(hi->type >= NUM_HANDLE_TYPES) {
        // This should never happen.
        ERROR("handle %08x has non-defined type.\n", handle);
        PAUSE();
        exit(1);
    }

    // Lookup actual callback in table.
    if (handle_types[hi->type].fnWaitSynchronization != NULL) {
        u32 ret;
        bool locked = false;

        ret = handle_types[hi->type].fnWaitSynchronization(hi, &locked);

        if (locked) {
            // If handle is locked we put thread in WAITING state.
            u32* wait_list = (u32*) malloc(4);
            wait_list[0] = handle;

            threads_SetCurrentThreadWaitList(wait_list, true, 1);
        }

        return ret;
    } else {
        ERROR("WaitSynchronization undefined for handle-type \"%s\".\n",
              handle_types[hi->type].name);
        PAUSE();
        return 0;
    }

}


u32 wrapWaitSynchronizationN(u32 nanoseconds1,u32 handles_ptr,u32 handles_count,u32 wait_all,u32 nanoseconds2,u32 out) // TODO: timeouts
{
    bool all_unlocked = true;

    for (u32 i = 0; i < handles_count; i++) {
        u32 handle = mem_Read32(handles_ptr + i * 4);
        handleinfo* hi = handle_Get(handle);

        if (hi == NULL) {
            arm11_SetR(1, i);
            ERROR("handle %08x not found.\n", handle);
            PAUSE();
#ifdef EXIT_ON_ERROR
            exit(1);
#endif
            return -1;
        }

        if (hi->type >= NUM_HANDLE_TYPES) {
            // This should never happen.
            ERROR("handle %08x has non-defined type.\n", handle);
            PAUSE();
            exit(1);
        }

        // Lookup actual callback in table.
        if (handle_types[hi->type].fnWaitSynchronization != NULL) {
            bool locked = false;

            handle_types[hi->type].fnWaitSynchronization(hi, &locked);

            if (!locked && !wait_all) {
                arm11_SetR(1, i);
                return 0;
            } else
                all_unlocked = false;

        } else {
            ERROR("WaitSynchronization undefined for handle-type \"%s\".\n",
                  handle_types[hi->type].name);
            PAUSE();
            arm11_SetR(1, i); //we just say this one is open
            return 0;
        }
    }

    if(wait_all && all_unlocked) {
        arm11_SetR(1, handles_count);
        return 0;
    }

    // Put thread in WAITING state if not all handles were unlocked.

    u32* wait_list = malloc(handles_count*4);
    mem_Read((u8 *) wait_list, handles_ptr, handles_count * 4);

    threads_SetCurrentThreadWaitList(wait_list, wait_all, handles_count);
    return 0;

}

u32 svcWaitSynchronizationN() // TODO: timeouts
{
    u32 nanoseconds1  = arm11_R(0);
    u32 handles_ptr   = arm11_R(1);
    u32 handles_count = arm11_R(2);
    u32 wait_all      = arm11_R(3);
    u32 nanoseconds2  = arm11_R(4);
    u32 out =           arm11_R(5);

    wrapWaitSynchronizationN(nanoseconds1, handles_ptr, handles_count, wait_all, nanoseconds2, out);
    return 0;
}

u32 nop_SyncRequest(handleinfo* h, bool *locked)
{
    return 0;
};
