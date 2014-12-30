/*
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

#ifdef GDB_STUB

#include "util.h"
#include "arm11.h"
#include "screen.h"
#include "gpu.h"

#include "handles.h"

#include "init.h"

#include <signal.h>
#include <SDL.h>

#include "armemu.h"
#include "gdb/gdbstub.h"
#include "armdefs.h"

#include "mem.h"

#ifdef _WIN32
static volatile bool arm_stall = false;
#else
static bool arm_stall = false;
static pthread_mutex_t arm_stall_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  arm_stall_cond  = PTHREAD_COND_INITIALIZER;
#endif

extern ARMul_State s;


extern u32 global_gdb_port;
extern gdbstub_handle_t gdb_stub;
extern struct armcpu_memory_iface *gdb_memio;
extern struct armcpu_memory_iface gdb_base_memory_iface;
extern struct armcpu_ctrl_iface gdb_ctrl_iface;
extern thread threads[MAX_THREADS];


thread_handle_t
createThread_gdb( thread_func_t thread_function,
                  void *thread_data)
{
#ifdef _WIN32
    u32 ThreadId;
    HANDLE *new_thread = CreateThread(
                             NULL,                   // default security attributes
                             0,                      // use default stack size
                             (LPTHREAD_START_ROUTINE)thread_function,       // thread function name
                             thread_data,          // argument to thread function
                             0,                      // use default creation flags
                             &ThreadId);   // returns the thread identifier

    return new_thread;
#else
    pthread_t ThreadId;
    int rc = pthread_create(&ThreadId, NULL, thread_function, thread_data);
    if(rc != 0)
        printf("pthread_create: %s\n", strerror(rc));
    return ThreadId;
#endif
}

void
joinThread_gdb( thread_handle_t thread_handle)
{
#ifdef _WIN32
    return;//todo
#else
    int rc = pthread_join(thread_handle, NULL);
    if(rc != 0)
        printf("pthread_create: %s\n", strerror(rc));
#endif
}

void wait_while_stall(void)
{
#ifdef _WIN32
    while(arm_stall)
        Sleep(1);
#else
    pthread_mutex_lock(&arm_stall_mutex);
    while(arm_stall)
        pthread_cond_wait(&arm_stall_cond, &arm_stall_mutex);
    pthread_mutex_unlock(&arm_stall_mutex);
#endif
}

void stall_cpu(void *instance)
{
#ifdef _WIN32
    arm_stall = true;
#else
    pthread_mutex_lock(&arm_stall_mutex);
    arm_stall = true;
    pthread_cond_signal(&arm_stall_cond);
    pthread_mutex_unlock(&arm_stall_mutex);
#endif
    s.NumInstrsToExecute = 0;
}

void unstall_cpu(void *instance)
{
#ifdef _WIN32
    arm_stall = false;
#else
    pthread_mutex_lock(&arm_stall_mutex);
    arm_stall = false;
    pthread_cond_signal(&arm_stall_cond);
    pthread_mutex_unlock(&arm_stall_mutex);
#endif
}

u32 read_cpu_reg(void *instance, u32 reg_num,u32 handle)
{
    if (handle == threads_GetCurrentThreadHandle()){

        if (reg_num == 0x10)return s.Cpsr;
#ifdef impropergdb
        if (reg_num == 0xF) {
            if (s.NextInstr == PRIMEPIPE)return arm11_R(reg_num);
            else return arm11_R(reg_num) - 4;
        }
#else
        if (reg_num == 0xF) {
            if (s.NextInstr >= PRIMEPIPE)
                return arm11_R(reg_num);
            return arm11_R(reg_num) - 4;
        }
#endif
        return arm11_R(reg_num);
    }
    else
    {
        if (reg_num >= 17) {
            DEBUG("Invalid reg_num.\n");
            return 0;
        }
        if (reg_num <= 13)
            return threads[threads_FindIdByHandle(handle)].r[reg_num];
        else if (reg_num == 14)
            return threads[threads_FindIdByHandle(handle)].lr;
        else if (reg_num == 15)
        {
            return threads[threads_FindIdByHandle(handle)].pc;
        }
        else if (reg_num == 16)
        {
            return threads[threads_FindIdByHandle(handle)].cpsr;
        }
    }
    return 0;
}
void set_cpu_reg(void *instance, u32 reg_num, u32 value, u32 handle)
{
    if (handle == threads_GetCurrentThreadHandle())
        arm11_SetR(reg_num, value);
    else
    {
        if (reg_num >= 17) {
            DEBUG("Invalid reg_num.\n");
            return;
        }
        if (reg_num <= 13)
            threads[threads_FindIdByHandle(handle)].r[reg_num] = value;
        else if (reg_num == 14)
            threads[threads_FindIdByHandle(handle)].lr = value;
        else if (reg_num == 15)
        {
            threads[threads_FindIdByHandle(handle)].pc = value;
        }
        else if (reg_num == 16)
        {
            threads[threads_FindIdByHandle(handle)].cpsr = value;
        }
    }

}
void install_post_exec_fn(void *instance, void(*ex_fn)(void *, u32 adr, int thumb), void *fn_data)
{
    //armcpu_t *armcpu = (armcpu_t *)instance;
    s.post_ex_fn = ex_fn;
    s.post_ex_fn_data = fn_data;
}
void remove_post_exec_fn(void *instance)
{
    s.post_ex_fn = NULL;
}
u16 gdb_prefetch16(void *data, u32 adr)
{
    //return mem_Read16(adr);
}

u32 gdb_prefetch32(void *data, u32 adr)
{
    //return mem_Read32(adr);
}

u8 gdb_read8(void *data, u32 adr)
{
    //return mem_Read8(adr);
}

u16 gdb_read16(void *data, u32 adr)
{
    //return mem_Read16(adr);
}

u32 gdb_read32(void *data, u32 adr)
{
    //return mem_Read32(adr);
}

void gdb_write8(void *data, u32 adr, u8 val)
{
    //mem_Write8(adr, val);
}

void gdb_write16(void *data, u32 adr, u16 val)
{
    // mem_Write16(adr, val);
}

void gdb_write32(void *data, u32 adr, u32 val)
{
    //mem_Write32(adr, val);
}
#endif
