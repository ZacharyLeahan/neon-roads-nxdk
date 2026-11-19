#include <mbedtls/platform_util.h>
#include <mbedtls/x509_crt.h>
#include <psa/crypto.h>
#include <psa_crypto_storage.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int mbedtls_hardware_poll(void *data, unsigned char *output, size_t len,
                          size_t *olen);

int mbedtls_platform_entropy_poll(void *data, unsigned char *output, size_t len,
                                  size_t *olen)
{
    return mbedtls_hardware_poll(data, output, len, olen);
}

int mbedtls_x509_crt_parse_file(mbedtls_x509_crt *chain, const char *path)
{
    FILE *file = fopen(path, "rb");
    unsigned char *buffer;
    long file_size;
    size_t read_size;
    int ret;

    if (file == NULL || fseek(file, 0, SEEK_END) != 0 ||
        (file_size = ftell(file)) < 0 || fseek(file, 0, SEEK_SET) != 0) {
        if (file != NULL) fclose(file);
        return MBEDTLS_ERR_X509_FILE_IO_ERROR;
    }

    buffer = malloc((size_t)file_size + 1);
    if (buffer == NULL) {
        fclose(file);
        return MBEDTLS_ERR_X509_ALLOC_FAILED;
    }

    read_size = fread(buffer, 1, (size_t)file_size, file);
    fclose(file);
    if (read_size != (size_t)file_size) {
        mbedtls_platform_zeroize(buffer, (size_t)file_size + 1);
        free(buffer);
        return MBEDTLS_ERR_X509_FILE_IO_ERROR;
    }

    buffer[file_size] = 0;
    ret = mbedtls_x509_crt_parse(chain, buffer, (size_t)file_size + 1);
    mbedtls_platform_zeroize(buffer, (size_t)file_size + 1);
    free(buffer);
    return ret;
}

psa_status_t psa_save_persistent_key(const psa_key_attributes_t *attributes,
                                     const uint8_t *data, size_t data_length)
{
    (void)attributes; (void)data; (void)data_length;
    return PSA_ERROR_NOT_SUPPORTED;
}

psa_status_t psa_load_persistent_key(psa_key_attributes_t *attributes,
                                     uint8_t **data, size_t *data_length)
{
    (void)attributes;
    *data = NULL;
    *data_length = 0;
    return PSA_ERROR_DOES_NOT_EXIST;
}

psa_status_t psa_destroy_persistent_key(mbedtls_svc_key_id_t key)
{
    (void)key;
    return PSA_ERROR_NOT_SUPPORTED;
}

void psa_free_persistent_key_data(uint8_t *data, size_t data_length)
{
    if (data != NULL) {
        mbedtls_platform_zeroize(data, data_length);
        free(data);
    }
}

void picoquic_ptls_minicrypto_load(int unload)
{
    (void)unload;
}
