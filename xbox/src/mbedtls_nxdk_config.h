#pragma once

/* NXDK targets a Pentium III-compatible original Xbox CPU. Upstream mbed TLS
 * sees the Win32 target triple and otherwise enables desktop-only intrinsics
 * and operating-system services. Keep the maintained 3.6 LTS defaults, then
 * select portable implementations for the console port. */
#include "mbedtls/mbedtls_config.h"

#undef MBEDTLS_AESNI_C
#undef MBEDTLS_PADLOCK_C
#undef MBEDTLS_FS_IO
#undef MBEDTLS_NET_C
#undef MBEDTLS_PSA_ITS_FILE_C
#undef MBEDTLS_PSA_CRYPTO_STORAGE_C
#undef MBEDTLS_HAVE_TIME
#undef MBEDTLS_HAVE_TIME_DATE
#undef MBEDTLS_SSL_SESSION_TICKETS
#undef MBEDTLS_SSL_TICKET_C
#undef MBEDTLS_TIMING_C

#define MBEDTLS_NO_PLATFORM_ENTROPY
#define MBEDTLS_ENTROPY_HARDWARE_ALT
