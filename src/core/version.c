#include "../include/version.h"
#include <stdio.h>

const char* anx_get_version(void) {
    return ANX_VERSION_STRING;
}

const char* anx_get_build_info(void) {
    static char build_info[512];
    snprintf(build_info, sizeof(build_info),
        "%s %s\n"
        "Built on %s %s\n"
        "Architecture: %s\n"
        "Features: SSL=%d, Compression=%d, Cache=%d, LoadBalancer=%d, Stream=%d, ASM=%d",
        ANX_NAME, ANX_VERSION_STRING,
        ANX_BUILD_DATE, ANX_BUILD_TIME,
        ANX_ARCH_STRING,
        ANX_FEATURE_SSL, ANX_FEATURE_COMPRESSION, ANX_FEATURE_CACHE,
        ANX_FEATURE_LOAD_BALANCER, ANX_FEATURE_STREAM, ANX_FEATURE_ASM_OPT
    );
    return build_info;
}

void anx_print_version(void) {
    printf("%s\n", anx_get_build_info());
} 