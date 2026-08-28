// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#if defined(RELEASE)
#define BUILD_TYPE "release"
#else
#define BUILD_TYPE "debug"
#endif

#define THEBOOTLOADER_SO_PATH                                                     \
   "/boot/libTheBootloader-" OS_VERSION "_" BUILD_TYPE ".so"

#define THEBOOTLOADER_CONF_PATH "/boot/TheBootloader.conf"