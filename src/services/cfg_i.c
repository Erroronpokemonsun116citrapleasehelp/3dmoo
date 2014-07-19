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
#include "handles.h"
#include "mem.h"
#include "arm11.h"

#include "service_macros.h"

 
SERVICE_START(cfg_i);

SERVICE_CMD(0x00010082) { // GetConfigInfoBlk8
    u32 size = CMD(1);
    u32 id = CMD(2);
    u32 pointer = CMD(4);

    DEBUG("GetConfigInfoBlk8 %08x %08x %08x\n", size, id, pointer);

    switch (id) {
    case 0x00070001:// Sound Mode?
        mem_Write8(pointer, 0);
        break;
    case 0x000A0002: // Language
        mem_Write8(pointer, 1); // 1=English
        break;
    default:
        ERROR("Unknown id %08x\n", id);
        break;
    }

    RESP(1, 0); // Result
    return 0;
}
SERVICE_CMD(0x00040000) {

    DEBUG("unk4\n");

    RESP(2, 0); // return byte

    RESP(1, 0); // Result
    return 0;
}
SERVICE_END();