#pragma once

#include_next <mbedtls/x509_crt.h>

/* NXDK supplies the file adapter even though the core mbedTLS build keeps
 * MBEDTLS_FS_IO disabled. PicoQUIC's mbedTLS provider expects this helper. */
int mbedtls_x509_crt_parse_file(mbedtls_x509_crt *chain, const char *path);
