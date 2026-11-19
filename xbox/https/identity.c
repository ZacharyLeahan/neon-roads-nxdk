#include "identity.h"
#include <mbedtls/platform_util.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>

#define AUTO_KEY "D:\\auto-key.pem"
#define AUTO_CERT "D:\\auto-cert.der"
#define IDENTITY_TEMP "D:\\identity.tmp"
#define DAY_TICKS 864000000000ULL

/* Flush the complete replacement before changing the installed file. The
 * certificate can always be rebuilt after an interrupted replacement. */
static int save_file(const char *path, const unsigned char *data, size_t length, int replace)
{
    FILE *f = fopen(IDENTITY_TEMP, "wb");
    if (!f)
        return -1;
    int ok = fwrite(data, 1, length, f) == length;
    if (fclose(f) || !ok)
        return -1;
    if (replace && GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES && !DeleteFileA(path))
        return -1;
    return MoveFileA(IDENTITY_TEMP, path) ? 0 : -1;
}

static int get_key(mbedtls_pk_context *key, mbedtls_ctr_drbg_context *rng)
{
    unsigned char pem[1024] = {0};
    int rc = -1;
    FILE *f = fopen(AUTO_KEY, "rb");
    if (f) {
        size_t n = fread(pem, 1, sizeof(pem), f);
        if (n && n < sizeof(pem) && !ferror(f))
            rc = mbedtls_pk_parse_key(key, pem, n + 1, NULL, 0, mbedtls_ctr_drbg_random, rng);
        fclose(f);
    } else {
        rc = mbedtls_pk_setup(key, mbedtls_pk_info_from_type(MBEDTLS_PK_ECKEY));
        if (!rc)
            rc = mbedtls_ecp_gen_key(MBEDTLS_ECP_DP_SECP256R1, mbedtls_pk_ec(*key),
                                     mbedtls_ctr_drbg_random, rng);
        if (!rc)
            rc = mbedtls_pk_write_key_pem(key, pem, sizeof(pem));
        if (!rc)
            rc = save_file(AUTO_KEY, pem, strlen((char *)pem), 0);
    }
    mbedtls_platform_zeroize(pem, sizeof(pem));
    return rc;
}

static uint64_t identity_time(void)
{
    FILETIME now;
    GetSystemTimeAsFileTime(&now);
    ULARGE_INTEGER n = {.LowPart = now.dwLowDateTime, .HighPart = now.dwHighDateTime};
    /* An Xbox that has lost power may have a reset RTC. Use the build date
     * as a lower bound so a newly generated certificate can still be used. */
    const char *months = "JanFebMarAprMayJunJulAugSepOctNovDec";
    char month[4] = {0};
    int day, year;
    if (sscanf(__DATE__, "%3s %d %d", month, &day, &year) == 3) {
        const char *m = strstr(months, month);
        SYSTEMTIME build = {0};
        build.wYear = year;
        build.wMonth = m ? (WORD)((m - months) / 3 + 1) : 1;
        build.wDay = day;
        FILETIME ft;
        if (SystemTimeToFileTime(&build, &ft)) {
            ULARGE_INTEGER floor = {.LowPart = ft.dwLowDateTime, .HighPart = ft.dwHighDateTime};
            if (n.QuadPart < floor.QuadPart)
                return floor.QuadPart;
        }
    }
    return n.QuadPart;
}

static int date_string(uint64_t ticks, char out[16])
{
    FILETIME ft = {(DWORD)ticks, (DWORD)(ticks >> 32)};
    SYSTEMTIME t;
    return FileTimeToSystemTime(&ft, &t) &&
           snprintf(out, 16, "%04u%02u%02u%02u%02u%02u", t.wYear, t.wMonth, t.wDay, t.wHour,
                    t.wMinute, t.wSecond) == 14;
}

static int compare_date(const mbedtls_x509_time *t, const char *date)
{
    char stamp[16];
    snprintf(stamp, sizeof(stamp), "%04d%02d%02d%02d%02d%02d", t->year, t->mon, t->day, t->hour,
             t->min, t->sec);
    return strcmp(stamp, date);
}

int xbox_auto_identity(mbedtls_pk_context *key, mbedtls_x509_crt *cert,
                       mbedtls_ctr_drbg_context *rng, const char *ip)
{
    unsigned char addr[16], der[1536], serial[16];
    if (mbedtls_x509_crt_parse_cn_inet_pton(ip, addr) != 4)
        return -1;
    int rc = get_key(key, rng);
    if (rc)
        return rc;
    uint64_t now = identity_time();
    char today[16], from[16], to[16];
    if (!date_string(now, today) || !date_string(now - DAY_TICKS, from) ||
        !date_string(now + 365 * DAY_TICKS, to))
        return -1;
    FILE *f = fopen(AUTO_CERT, "rb");
    if (f) {
        size_t n = fread(der, 1, sizeof(der), f);
        fclose(f);
        if (n && n < sizeof(der) && !mbedtls_x509_crt_parse_der(cert, der, n) &&
            compare_date(&cert->valid_from, today) <= 0 &&
            compare_date(&cert->valid_to, today) > 0 &&
            !mbedtls_pk_check_pair(&cert->pk, key, mbedtls_ctr_drbg_random, rng)) {
            const mbedtls_x509_buf *san = &cert->subject_alt_names.buf;
            if (san->tag == (MBEDTLS_ASN1_CONTEXT_SPECIFIC | MBEDTLS_X509_SAN_IP_ADDRESS) &&
                san->len == 4 && !memcmp(san->p, addr, 4))
                return 0;
        }
        mbedtls_x509_crt_free(cert);
        mbedtls_x509_crt_init(cert);
    }
    mbedtls_x509write_cert w;
    mbedtls_x509write_crt_init(&w);
    mbedtls_x509write_crt_set_version(&w, MBEDTLS_X509_CRT_VERSION_3);
    mbedtls_x509write_crt_set_md_alg(&w, MBEDTLS_MD_SHA256);
    mbedtls_x509write_crt_set_subject_key(&w, key);
    mbedtls_x509write_crt_set_issuer_key(&w, key);
    const char *name = "CN=Neon Roads Xbox";
    rc = mbedtls_x509write_crt_set_subject_name(&w, name);
    if (!rc)
        rc = mbedtls_x509write_crt_set_issuer_name(&w, name);
    if (!rc)
        rc = mbedtls_ctr_drbg_random(rng, serial, sizeof(serial));
    if (!rc) {
        serial[0] = (serial[0] & 0x3f) | 0x40;
        rc = mbedtls_x509write_crt_set_serial_raw(&w, serial, sizeof(serial));
    }
    if (!rc)
        rc = mbedtls_x509write_crt_set_validity(&w, from, to);
    if (!rc)
        rc = mbedtls_x509write_crt_set_basic_constraints(&w, 0, -1);
    if (!rc)
        rc = mbedtls_x509write_crt_set_key_usage(&w, MBEDTLS_X509_KU_DIGITAL_SIGNATURE);
    mbedtls_x509_san_list san = {0};
    san.node.type = MBEDTLS_X509_SAN_IP_ADDRESS;
    san.node.san.unstructured_name.p = addr;
    san.node.san.unstructured_name.len = 4;
    if (!rc)
        rc = mbedtls_x509write_crt_set_subject_alternative_name(&w, &san);
    if (!rc) {
        rc = mbedtls_x509write_crt_der(&w, der, sizeof(der), mbedtls_ctr_drbg_random, rng);
        if (rc > 0) {
            size_t n = (size_t)rc;
            rc = mbedtls_x509_crt_parse_der(cert, der + sizeof(der) - n, n);
            if (!rc)
                rc = save_file(AUTO_CERT, der + sizeof(der) - n, n, 1);
        }
    }
    mbedtls_x509write_crt_free(&w);
    return rc;
}
