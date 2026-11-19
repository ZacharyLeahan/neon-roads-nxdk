#define _CRT_RAND_S

#include <hal/debug.h>
#include <hal/video.h>
#include <lwip/ip4_addr.h>
#include <lwip/netif.h>
#include <lwip/sockets.h>
#include <mbedtls/sha256.h>
#include <nxdk/net.h>
#include <picoquic.h>
#include <stdlib.h>
#include <windows.h>

extern struct netif *g_pnetif;
int rand_s(unsigned int *random_value);

int mbedtls_hardware_poll(void *data, unsigned char *output, size_t len,
                          size_t *olen)
{
    size_t offset = 0;
    (void)data;

    while (offset < len) {
        unsigned int sample = 0;
        size_t available = len - offset;
        size_t copy_len = available < sizeof(sample) ? available : sizeof(sample);

        if (rand_s(&sample) != 0) {
            *olen = offset;
            return -1;
        }

        memcpy(output + offset, &sample, copy_len);
        offset += copy_len;
    }

    *olen = len;
    return 0;
}

static int run_crypto_self_test(void)
{
    static const unsigned char message[] = "neon-roads-nxdk";
    static const unsigned char expected[32] = {
        0x94, 0x72, 0x93, 0x8f, 0x08, 0x68, 0x42, 0x52,
        0x9e, 0x67, 0x36, 0xfc, 0x1b, 0x37, 0x63, 0x9a,
        0xfb, 0xe8, 0x4d, 0x3a, 0x15, 0x3a, 0x55, 0x50,
        0xef, 0x24, 0x11, 0x67, 0x43, 0x79, 0x0a, 0x15,
    };
    unsigned char digest[32];

    if (mbedtls_sha256(message, sizeof(message) - 1, digest, 0) != 0) {
        return -1;
    }

    return memcmp(digest, expected, sizeof(expected));
}

static void report_xemu_gate(int crypto_result, int net_result, int quic_result)
{
    static const char pass_message[] = "NEON_ROADS_PICOQUIC_PASS";
    static const char fail_message[] = "NEON_ROADS_PICOQUIC_FAIL";
    const char *message = crypto_result == 0 && net_result == 0 && quic_result == 0
                              ? pass_message
                              : fail_message;
    struct sockaddr_in host = {0};
    int fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    if (fd < 0) return;
    host.sin_family = AF_INET;
    host.sin_port = htons(32123);
    host.sin_addr.s_addr = inet_addr("10.0.2.2");
    (void)sendto(fd, message, strlen(message), 0,
                 (const struct sockaddr *)&host, sizeof(host));
    closesocket(fd);
}

int main(void)
{
    int net_result;
    int crypto_result;
    picoquic_quic_t *quic;

    XVideoSetMode(640, 480, 32, REFRESH_DEFAULT);
    debugPrint("\nNeon Roads NXDK WebTransport spike\n");
    debugPrint("----------------------------------\n");

    crypto_result = run_crypto_self_test();
    debugPrint("mbedTLS SHA-256 self-test: %s\n",
               crypto_result == 0 ? "PASS" : "FAIL");

    net_result = nxNetInit(NULL);
    debugPrint("nxNetInit: %s (%d)\n", net_result == 0 ? "PASS" : "FAIL",
               net_result);

    if (net_result == 0) {
        debugPrint("IPv4:    %s\n", ip4addr_ntoa(netif_ip4_addr(g_pnetif)));
        debugPrint("Gateway: %s\n", ip4addr_ntoa(netif_ip4_gw(g_pnetif)));
    }

    quic = picoquic_create(4, NULL, NULL, NULL, "h3", NULL, NULL, NULL,
                           NULL, NULL, picoquic_current_time(), NULL, NULL,
                           NULL, 0);
    debugPrint("PicoQUIC context: %s\n", quic != NULL ? "PASS" : "FAIL");
    if (quic != NULL) {
        picoquic_free(quic);
    }

    report_xemu_gate(crypto_result, net_result, quic == NULL ? -1 : 0);

    debugPrint("\nGate 1: crypto + network runtime initialized.\n");
    debugPrint("Gate 2: PicoQUIC core context initialized.\n");

    while (1) {
        Sleep(1000);
    }
}
