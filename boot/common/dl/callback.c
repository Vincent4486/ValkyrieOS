// SPDX-License-Identifier: GPL-3.0-only

#include <stddef.h>

#include "callback.h"

DL_CallbackOpsPatch g_DlCallbackOpsPatch = {
    ._signature = {'V', 'L', 'S', 'O'}, // "VLSO" in actual bytes
    .dl_callback_ops = NULL,
};
