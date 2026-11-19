set(CMAKE_C_FLAGS
    "-U_WIN32 -U_MSC_VER -I${CMAKE_CURRENT_LIST_DIR}/../src -DMBEDTLS_USER_CONFIG_FILE=\\\"mbedtls_nxdk_config.h\\\""
    CACHE STRING "NXDK compatibility flags for mbed TLS" FORCE)
