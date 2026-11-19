#pragma once
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/x509_crt.h>

int xbox_entropy_seed(unsigned char seed[32]);
int xbox_auto_identity(mbedtls_pk_context *key, mbedtls_x509_crt *cert,
                       mbedtls_ctr_drbg_context *rng, const char *ip);
