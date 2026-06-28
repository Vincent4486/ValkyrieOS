// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <stdint.h>

typedef struct DL_CallbackOperations DL_CallbackOperations;
typedef struct DL_CallbackOpsPatch DL_CallbackOpsPatch;

struct __attribute__((packed)) DL_CallbackOperations
{
   int (*DISK_Read)(uint8_t drive, uint16_t cylinder, uint8_t sector,
                    uint8_t head, uint8_t count, void *buffer);
   int (*DISK_ReadLBA)(uint8_t drive, uint64_t lba, uint16_t count,
                       void *buffer);

};

struct __attribute__((packed)) DL_CallbackOpsPatch
{
   char _signature[4];
   DL_CallbackOperations *dl_callback_ops;
};

#ifndef CORE
extern DL_CallbackOpsPatch g_DlCallbackOpsPatch;

#define g_DlCallbackOps g_DlCallbackOpsPatch.dl_callback_ops
#endif
