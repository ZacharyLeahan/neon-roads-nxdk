XBE_TITLE = Neon\ Roads
NXDK_DIR ?= $(CURDIR)/../../../nxdk
NXDK_NET = y
NXDK_SDL = y
QR_DIR := $(CURDIR)/../../third_party/qr-code-generator/c
TLS_DIR := $(CURDIR)/../../build-xbox/mbedtls-install
SRCS = $(CURDIR)/server.c $(CURDIR)/audio.c $(CURDIR)/game.c $(CURDIR)/race.c $(CURDIR)/view.c $(CURDIR)/../src/picoquic_nxdk_platform.c $(QR_DIR)/qrcodegen.c
include $(NXDK_DIR)/Makefile
$(OBJS): CFLAGS += -I$(CURDIR)/../compat -I$(QR_DIR) -I$(TLS_DIR)/include -I$(CURDIR)/../src -I$(CURDIR)/../../third_party/mbedtls/library -DMBEDTLS_USER_CONFIG_FILE=\"mbedtls_nxdk_config.h\"
main.exe: $(TLS_DIR)/lib/libmbedtls.lib $(TLS_DIR)/lib/libmbedx509.lib $(TLS_DIR)/lib/libmbedcrypto.lib

$(CURDIR)/race.obj: $(CURDIR)/../generated/roads.h
$(CURDIR)/../generated/themes.h $(CURDIR)/../generated/roads.h &: $(CURDIR)/export-roads.mjs $(wildcard $(CURDIR)/../../src/levels/roads/*.ts) $(CURDIR)/../../src/levels/worlds.ts $(CURDIR)/../../src/levels/format.ts $(CURDIR)/../../src/sim/types.ts $(CURDIR)/../../src/audio/audio.ts
	cd $(CURDIR)/../.. && node xbox/https/export-roads.mjs
$(CURDIR)/audio.obj: $(CURDIR)/../generated/themes.h
$(CURDIR)/audio.obj $(CURDIR)/view.obj $(CURDIR)/race.obj: CFLAGS += -O2


$(CURDIR)/view.obj $(CURDIR)/game.obj $(CURDIR)/audio.obj $(CURDIR)/race.obj: $(CURDIR)/game.h
$(CURDIR)/game.obj $(CURDIR)/server.obj $(CURDIR)/audio.obj: $(CURDIR)/audio.h
