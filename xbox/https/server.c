#define _CRT_RAND_S
#include <hal/debug.h>
#include <hal/video.h>
#include <hal/xbox.h>
#include <lwip/netif.h>
#include <lwip/sockets.h>
#include <nxdk/net.h>
#include <windows.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/sha256.h>
#include <mbedtls/ssl.h>
#include <mbedtls/platform_util.h>
#include <psa/crypto.h>
#include <qrcodegen.h>
#include "game.h"
#include "audio.h"
#include "identity.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

extern struct netif *g_pnetif;
int rand_s(unsigned int *);
static mbedtls_ssl_config config;
static mbedtls_ctr_drbg_context rng;
static mbedtls_entropy_context entropy;
static mbedtls_pk_context key;
static mbedtls_x509_crt cert;
static unsigned char seed[32];
static char token[33], page[24576];
static size_t page_size;
static char xbox_ip[24];


/* Mix a fresh timing sample with a secret from this installation for every block. */
int mbedtls_hardware_poll(void *unused, unsigned char *out, size_t len, size_t *written)
{
    (void)unused;
    *written = 0;
    while (*written < len) {
        unsigned char block[36];
        unsigned int sample;
        if (rand_s(&sample)) return -1;
        memcpy(block, seed, 32);
        memcpy(block + 32, &sample, 4);
        if (mbedtls_sha256(block, sizeof(block), seed, 0)) return -1;
        mbedtls_platform_zeroize(block, sizeof(block));
        size_t n = len - *written;
        if (n > 32) n = 32;
        memcpy(out + *written, seed, n);
        *written += n;
    }
    return 0;
}

static size_t load(const char *name, void *data, size_t capacity)
{
    FILE *file = fopen(name, "rb");
    if (!file) return 0;
    size_t n = fread(data, 1, capacity - 1, file);
    int ok = !ferror(file) && fgetc(file) == EOF;
    fclose(file);
    ((char *)data)[n] = 0;
    return ok ? n : 0;
}

static int setup_tls(void)
{
    unsigned char pem[4096], pairing[16];
    size_t n = load("D:\\key.pem", pem, sizeof(pem));
    int supplied = GetFileAttributesA("D:\\key.pem") != INVALID_FILE_ATTRIBUTES ||
                   GetFileAttributesA("D:\\server.pem") != INVALID_FILE_ATTRIBUTES;
    if (supplied) {
        if (!n || mbedtls_sha256(pem, n, seed, 0))
            return -1;
    } else {
        debugPrint("Preparing local HTTPS identity...\n");
        if (xbox_entropy_seed(seed))
            return -7;
    }
    if (psa_crypto_init() != PSA_SUCCESS)
        return -5;
    mbedtls_ctr_drbg_init(&rng);
    mbedtls_entropy_init(&entropy);
    mbedtls_pk_init(&key);
    mbedtls_x509_crt_init(&cert);
    mbedtls_ssl_config_init(&config);
    int rc = mbedtls_ctr_drbg_seed(&rng, mbedtls_entropy_func, &entropy, NULL, 0);
    if (!rc && supplied)
        rc = mbedtls_pk_parse_key(&key, pem, n + 1, NULL, 0, mbedtls_ctr_drbg_random, &rng);
    mbedtls_platform_zeroize(pem, sizeof(pem));
    if (rc)
        return rc;
    if (!supplied) {
        if ((rc = xbox_auto_identity(&key, &cert, &rng, xbox_ip)))
            return rc;
    } else {
        n = load("D:\\server.pem", pem, sizeof(pem));
        if (!n)
            return -2;
        if ((rc = mbedtls_x509_crt_parse(&cert, pem, n + 1)))
            return rc;
    }
    if ((rc =
             mbedtls_ssl_config_defaults(&config, MBEDTLS_SSL_IS_SERVER,
                                         MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT)))
        return rc;
    /* TLS 1.2 ECDHE/ECDSA is supported by the iPhone and the ported library. */
    mbedtls_ssl_conf_max_tls_version(&config, MBEDTLS_SSL_VERSION_TLS1_2);
    mbedtls_ssl_conf_rng(&config, mbedtls_ctr_drbg_random, &rng);
    if ((rc = mbedtls_ssl_conf_own_cert(&config, &cert, &key))) return rc;
    if ((rc = mbedtls_ctr_drbg_random(&rng, pairing, sizeof(pairing)))) return rc;
    for (size_t i = 0; i < sizeof(pairing); ++i) sprintf(token + 2*i, "%02x", pairing[i]);
    return 0;
}

static void show_pairing(int reconnect)
{
    char url[96];
    uint8_t temp[qrcodegen_BUFFER_LEN_FOR_VERSION(6)], qr[sizeof(temp)];
    snprintf(url, sizeof(url), "https://%s/#%s", xbox_ip, token);
    if (!qrcodegen_encodeText(url, temp, qr, qrcodegen_Ecc_MEDIUM, 1, 6,
                              qrcodegen_Mask_AUTO, true)) return;
    if(!reconnect) debugClearScreen();
    if(!reconnect) debugPrint("Neon Roads   |   Scan to steer\nhttps://%s/", xbox_ip);
    int size = qrcodegen_getSize(qr);
    int scale = reconnect ? 2 : 264 / (size + 8), width = (size + 8) * scale;
    int top=reconnect?350:76, left=reconnect?540:(640-width)/2;
    uint32_t *fb = (uint32_t *)XVideoGetFB();
    for (int y = 0; y < width; ++y)
        for (int x = 0; x < width; ++x)
            fb[(top+y)*640+left+x] =
                qrcodegen_getModule(qr, x/scale-4, y/scale-4) ? BLACK : WHITE;
    if(reconnect) { debugMoveCursor(40,365); debugPrint("Waiting for phone connection"); }
    else { debugMoveCursor(80,365); debugPrint("Scan to connect your phone"); }
}

static DWORD __stdcall play_thread(void *unused) { game_run(show_pairing); return 0; }

static int readable(int fd, unsigned ms)
{
    fd_set set;
    FD_ZERO(&set);
    FD_SET(fd, &set);
    struct timeval timeout = { ms/1000, (ms%1000)*1000 };
    return select(fd+1, &set, NULL, NULL, &timeout);
}

static int receive_tls(void *ctx, unsigned char *buf, size_t len)
{
    int fd = *(int *)ctx;
    if (readable(fd, 1200) <= 0) return MBEDTLS_ERR_SSL_TIMEOUT;
    return recv(fd, buf, len, 0);
}

static int send_tls(void *ctx, const unsigned char *buf, size_t len)
{
    int fd = *(int *)ctx;
    return send(fd, buf, len, 0);
}

static int write_all(mbedtls_ssl_context *ssl, const char *buf, size_t n)
{
    while (n) {
        int sent = mbedtls_ssl_write(ssl, (const unsigned char *)buf, n);
        if (sent <= 0) return -1;
        buf += sent;
        n -= sent;
    }
    return 0;
}

typedef enum { REQUEST_DONE, REQUEST_MORE, REQUEST_RESET } RequestResult;

static RequestResult handle(mbedtls_ssl_context *ssl, int listener)
{
    char request[4096], header[320], position_header[48]="", stats[256];
    static unsigned packets;
    static int latest_lift, peak_lift, sensor_live;
    int lift=0, peak=0, live=0;
    size_t used = 0;
    do {
        int n = mbedtls_ssl_read(ssl, (unsigned char *)request + used, sizeof(request)-1-used);
        if (n <= 0) return 0;
        used += n;
        request[used] = 0;
    } while (!strstr(request, "\r\n\r\n") && used < sizeof(request)-1);
    char *line = strstr(request, "\r\n");
    if (!line || !strstr(request, "\r\n\r\n")) return 0;
    *line = 0;
    int paired = 0, brake = 0, setup = 0;
    BoardAngles angles={0};
    int strength=100, height=0;
    char action = 0; unsigned action_id = 0;
    for (char *p = line+2; *p && *p != '\r';) {
        char *end = strstr(p, "\r\n");
        if (!end) return 0;
        *end = 0;
        if (!_strnicmp(p, "X-Pair:", 7)) {
            char *value = p+7;
            while (*value == ' ' || *value == '\t') ++value;
            paired = strcmp(token, value) == 0;
        }
        if (!_strnicmp(p,"X-Angles: ",10)) {
            char *value=p+10;
            int fields[5]={0};
            for (int i=0;i<5;++i) {
                char *end;
                long n=strtol(value,&end,10);
                if(end==value || n < -1800 || n > 1800) break;
                fields[i]=(int)n;
                if (*end!=',') break;
                value=end+1;
            }
            angles=(BoardAngles){fields[0],fields[1],fields[2],fields[3],fields[4]};
        }
        if (!_strnicmp(p,"X-Setup: ",9)) { long step=strtol(p+9,NULL,10); if(step>=1 && step<=10) setup=(int)step; }
        if (!_strnicmp(p,"X-Settle: ",10)){
            long n=strtol(p+10,NULL,10);
            if(n>=0 && n<=100)angles.settle=(int)n;
        }
        if (!_strnicmp(p,"X-Board-Pose: ",14)) {
            int r,t;
            if(sscanf(p+14,"%d,%d",&r,&t)==2 && abs(r)<=18000 && abs(t)<=18000){
                angles.pose_valid=1;angles.roll100=r;angles.pitch100=t;
            }
        }
        if (!_strnicmp(p,"X-Accel-Y: ",11)) {
            long y=strtol(p+11,NULL,10);
            if(y>=-10000 && y<=10000){angles.y_valid=1;angles.accel_y100=(int)y;}
        }
        if (!_strnicmp(p,"X-Lift: ",8)) {
            if(sscanf(p+8,"%d,%d",&lift,&peak)==2 && lift>=-10000 && lift<=10000 && peak>=0 && peak<=10000) live=1;
        }
        if (!_strnicmp(p,"X-Brake: 1",10)) brake = 1;
        if (!_strnicmp(p,"X-Board-Height: ",16)) {
            long n=strtol(p+16,NULL,10);
            if(n>=0 && n<=100)height=(int)n;
        }
        if (!_strnicmp(p,"X-Jump-Strength: ",17)) {
            long n=strtol(p+17,NULL,10);
            if(n>=1 && n<=100) strength=(int)n;
        }
        if (!_strnicmp(p,"X-Action: ",10)) { action=p[10]; if(action) action_id=strtoul(p+11,NULL,10); }
        if (!_strnicmp(p, "Transfer-Encoding:", 18) ||
            (!_strnicmp(p, "Content-Length:", 15) && strcmp(p+15, " 0"))) return 0;
        p = end+2;
    }
    const char *status = "404 Not Found", *body = "";
    size_t len = 0;
    int keep = 0, reset = 0;
    if (!strcmp(request, "GET / HTTP/1.1")) {
        status = "200 OK"; body = page; len = page_size;
    } else if (!strcmp(request,"GET /jump-stats HTTP/1.1")) {
        if(!paired) status="403 Forbidden";
        else {
            Phase phase;
            unsigned requests,takeoffs;
            int actual_strength,y100;
            game_jump_stats(&phase,&requests,&takeoffs,&actual_strength,&y100);
            snprintf(stats,sizeof(stats),"{\"packets\":%u,\"live\":%d,\"lift100\":%d,\"peak100\":%d,\"phase\":%d,\"commands\":%u,\"takeoffs\":%u,\"heightPercent\":%d,\"shipY100\":%d}",packets,sensor_live,latest_lift,peak_lift,(int)phase,requests,takeoffs,actual_strength,y100);
            peak_lift=0;
            body=stats; len=strlen(body); status="200 OK";
        }
    } else if (!strcmp(request, "POST /dashboard HTTP/1.1")) {
        if (paired) {
            status = "202 Accepted";
            reset = 1;
        } else status = "403 Forbidden";
    } else if (!strncmp(request, "POST /tilt/", 11)) {
        char *end;
        long angle = strtol(request+11, &end, 10);
        if (!paired) status = "403 Forbidden";
        else if (end == request+11 || strcmp(end, " HTTP/1.1") || angle < -90 || angle > 90)
            status = "400 Bad Request";
        else {
            static const char *phases[]={"ready","racing","lost","finished"};
            angles.lift_valid=live;angles.lift100=lift;angles.height=height;
            ++packets; sensor_live=live; latest_lift=lift;
            if(live && peak>peak_lift)peak_lift=peak;
            body=phases[game_input((int)angle,brake,action_id,action,strength,height,setup,angles)];
            Phase observed_phase; unsigned requests,takeoffs; int strength_at_takeoff,y100;
            game_jump_stats(&observed_phase,&requests,&takeoffs,&strength_at_takeoff,&y100);
            snprintf(position_header,sizeof(position_header),"X-Ship-Y100: %d\r\n",y100);
            status = "200 OK"; len = strlen(body); keep = readable(listener, 0) == 0;
        }
    }
    snprintf(header, sizeof(header), "HTTP/1.1 %s\r\nContent-Type: %s\r\nContent-Length: %u\r\nCache-Control: no-store\r\nConnection: %s\r\n%s\r\n",
             status, body == page ? "text/html; charset=utf-8" : "text/plain",
             (unsigned)len, keep ? "keep-alive" : "close",position_header);
    size_t header_len=strlen(header);
    if(len<=sizeof(header)-header_len) {
        memcpy(header+header_len,body,len);
        if(write_all(ssl,header,header_len+len)) return REQUEST_DONE;
    } else if(write_all(ssl,header,header_len) || write_all(ssl,body,len)) return REQUEST_DONE;
    if (reset) return REQUEST_RESET;
    return keep ? REQUEST_MORE : REQUEST_DONE;
}

static void serve(int listener)
{
    for (;;) {

        if (readable(listener, 100) <= 0) continue;
        int fd = accept(listener, NULL, NULL);
        if (fd < 0) continue;
        int no_delay=1;
        setsockopt(fd,IPPROTO_TCP,TCP_NODELAY,&no_delay,sizeof(no_delay));
        RequestResult result = REQUEST_DONE;
        mbedtls_ssl_context ssl;
        mbedtls_ssl_init(&ssl);
        if (!mbedtls_ssl_setup(&ssl, &config)) {
            mbedtls_ssl_set_bio(&ssl, &fd, send_tls, receive_tls, NULL);
            if (!mbedtls_ssl_handshake(&ssl)) {
                do { result = handle(&ssl, listener); } while (result == REQUEST_MORE);
                mbedtls_ssl_close_notify(&ssl);
            }
        }
        mbedtls_ssl_free(&ssl);
        closesocket(fd);
        if (result == REQUEST_RESET) { closesocket(listener); XReboot(); return; }
    }
}

int main(void)
{
    XVideoSetMode(640,480,32,REFRESH_DEFAULT);
    debugPrint("Starting Neon Roads HTTPS\n");
    int rc = nxNetInit(NULL);
    if (rc) goto error;
    page_size = load("D:\\controller.html", page, sizeof(page));
    if (!page_size) { rc = -3; goto error; }
    snprintf(xbox_ip,sizeof(xbox_ip),"%s",ip4addr_ntoa(netif_ip4_addr(g_pnetif)));
    rc = setup_tls();
    if (rc) goto error;
    int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(443);
    if (fd < 0 || bind(fd,(struct sockaddr *)&addr,sizeof(addr)) || listen(fd,4)) {
        rc = -4; goto error;
    }

    if (!audio_open()) debugPrint("Audio initialization failed\n");
    game_init();
    if (CreateThread(NULL,65536,play_thread,NULL,0,NULL)) serve(fd);
    rc = -6;
error:
    debugPrint("Startup error %d\n", rc);
    for (;;) Sleep(1000);
}
