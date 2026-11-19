XBE_TITLE = Neon\ Roads\ -\ WebTransport\ Spike
GEN_XISO = neon-roads-picoquic-spike.iso
PICOQUIC_DIR := $(CURDIR)/../third_party/picoquic
PICOTLS_DIR := $(CURDIR)/../third_party/picotls

PICOQUIC_SRCS := $(wildcard $(PICOQUIC_DIR)/picoquic/*.c)
PICOQUIC_SRCS := $(filter-out \
	$(PICOQUIC_DIR)/picoquic/picosocks.c \
	$(PICOQUIC_DIR)/picoquic/sockloop.c \
	$(PICOQUIC_DIR)/picoquic/picoquic_af_xdp.c \
	$(PICOQUIC_DIR)/picoquic/picoquic_l3tx.c \
	$(PICOQUIC_DIR)/picoquic/picoquic_neigh.c \
	$(PICOQUIC_DIR)/picoquic/picoquic_ptls_minicrypto.c \
	$(PICOQUIC_DIR)/picoquic/winsockloop.c,$(PICOQUIC_SRCS))

PICOTLS_SRCS := \
	$(PICOTLS_DIR)/lib/hpke.c \
	$(PICOTLS_DIR)/lib/asn1.c \
	$(PICOTLS_DIR)/lib/picotls.c \
	$(PICOTLS_DIR)/lib/pembase64.c

PICOQUIC_MBEDTLS_SRCS := \
	$(PICOQUIC_DIR)/picoquic_mbedtls/ptls_mbedtls.c \
	$(PICOQUIC_DIR)/picoquic_mbedtls/ptls_mbedtls_sign.c

SRCS = \
	$(CURDIR)/src/main.c \
	$(CURDIR)/src/picoquic_nxdk_platform.c \
	$(PICOQUIC_SRCS) \
	$(PICOTLS_SRCS) \
	$(PICOQUIC_MBEDTLS_SRCS)
NXDK_DIR ?= $(CURDIR)/../../nxdk
NXDK_NET = y

MBEDTLS_DIR := $(CURDIR)/../build-xbox/mbedtls-install

PORT_CFLAGS := \
	-I$(CURDIR)/compat \
	-I$(PICOQUIC_DIR)/picoquic \
	-I$(PICOQUIC_DIR)/picoquic_mbedtls \
	-I$(PICOTLS_DIR)/include \
	-I$(CURDIR)/../third_party/mbedtls/library \
	-I$(CURDIR)/src \
	-I$(MBEDTLS_DIR)/include \
	-include pthread.h \
	-DMBEDTLS_USER_CONFIG_FILE=\"mbedtls_nxdk_config.h\" \
	-DPTLS_HAVE_LOG=0 \
	-DPTLS_WITHOUT_OPENSSL \
	-DPICOQUIC_WITH_MBEDTLS

include $(NXDK_DIR)/Makefile

$(OBJS): CFLAGS += $(PORT_CFLAGS)

main.exe: \
	$(MBEDTLS_DIR)/lib/libmbedtls.lib \
	$(MBEDTLS_DIR)/lib/libmbedx509.lib \
	$(MBEDTLS_DIR)/lib/libmbedcrypto.lib
