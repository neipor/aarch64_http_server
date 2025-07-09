cmake_minimum_required(VERSION 3.10)

# 项目信息
project(anx-http-server
    VERSION 0.8.0
    DESCRIPTION "ANX HTTP Server - High-performance HTTP server with assembly optimizations"
    LANGUAGES C
)

# C标准
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# 默认构建类型
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# 编译选项
set(CMAKE_C_FLAGS_DEBUG "-g -O0 -DDEBUG -Wall -Wextra")
set(CMAKE_C_FLAGS_RELEASE "-O3 -DNDEBUG -march=native")
set(CMAKE_C_FLAGS_RELWITHDEBINFO "-O2 -g -DNDEBUG")
set(CMAKE_C_FLAGS_MINSIZEREL "-Os -DNDEBUG")

# 平台特定设置
if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
    add_definitions(-DANX_ARCH_ARM64=1)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mcpu=native")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64")
    add_definitions(-DANX_ARCH_X86_64=1)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=native")
endif()

# 功能特性定义
add_definitions(
    -DANX_FEATURE_SSL=1
    -DANX_FEATURE_COMPRESSION=1
    -DANX_FEATURE_CACHE=1
    -DANX_FEATURE_LOAD_BALANCER=1
    -DANX_FEATURE_HEALTH_CHECK=1
    -DANX_FEATURE_STREAM=1
    -DANX_FEATURE_PUSH=1
    -DANX_FEATURE_ASM_OPT=1
)

# 查找依赖包
find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)

# OpenSSL
find_package(OpenSSL REQUIRED)
if(OPENSSL_FOUND)
    message(STATUS "Found OpenSSL ${OPENSSL_VERSION}")
endif()

# Zlib
find_package(ZLIB REQUIRED)
if(ZLIB_FOUND)
    message(STATUS "Found Zlib ${ZLIB_VERSION_STRING}")
endif()

# 包含目录
include_directories(
    ${CMAKE_SOURCE_DIR}/src/include
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/http
    ${CMAKE_SOURCE_DIR}/src/proxy
    ${CMAKE_SOURCE_DIR}/src/stream
    ${CMAKE_SOURCE_DIR}/src/utils
    ${CMAKE_SOURCE_DIR}/src/utils/asm
)

# 源文件组织
set(CORE_SOURCES
    src/core/main.c
    src/core/core.c
    src/core/config.c
    src/core/net.c
    src/core/log.c
)

set(HTTP_SOURCES
    src/http/http.c
    src/http/https.c
    src/http/headers.c
    src/http/chunked.c
)

set(PROXY_SOURCES
    src/proxy/proxy.c
    src/proxy/lb_proxy.c
    src/proxy/load_balancer.c
    src/proxy/health_check.c
    src/proxy/health_api.c
)

set(STREAM_SOURCES
    src/stream/stream.c
    src/stream/push.c
)

set(UTILS_SOURCES
    src/utils/util.c
    src/utils/cache.c
    src/utils/compress.c
    src/utils/bandwidth.c
)

set(ASM_SOURCES
    src/utils/asm/asm_opt.c
    src/utils/asm/asm_mempool.c
    src/utils/asm/asm_integration.c
)

# 版本信息源文件
configure_file(
    "${CMAKE_SOURCE_DIR}/src/include/version.h.in"
    "${CMAKE_BINARY_DIR}/include/version.h"
)
include_directories("${CMAKE_BINARY_DIR}/include")

# 创建可执行文件
add_executable(anx
    ${CORE_SOURCES}
    ${HTTP_SOURCES}
    ${PROXY_SOURCES}
    ${STREAM_SOURCES}
    ${UTILS_SOURCES}
    ${ASM_SOURCES}
)

# 链接库
target_link_libraries(anx
    OpenSSL::SSL
    OpenSSL::Crypto
    ZLIB::ZLIB
    Threads::Threads
    m  # 数学库
)

# 安装规则
install(TARGETS anx
    RUNTIME DESTINATION bin
)

install(FILES configs/anx.conf.example
    DESTINATION etc/anx
    RENAME anx.conf.example
)

# 文档安装
install(FILES 
    README.md
    CHANGELOG.md
    ROADMAP.md
    DESTINATION share/doc/anx
)

# 测试支持
enable_testing()

# 单元测试
add_subdirectory(tests EXCLUDE_FROM_ALL)

# 打包支持
set(CPACK_PACKAGE_NAME "anx-http-server")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY ${PROJECT_DESCRIPTION})
set(CPACK_PACKAGE_VENDOR "neipor")
set(CPACK_PACKAGE_CONTACT "neitherportal@proton.me")

# 不同平台的打包格式
if(WIN32)
    set(CPACK_GENERATOR "ZIP;NSIS")
elseif(APPLE)
    set(CPACK_GENERATOR "TGZ;DMG")
else()
    set(CPACK_GENERATOR "TGZ;DEB;RPM")
endif()

include(CPack)

# 自定义目标
add_custom_target(format
    COMMAND find ${CMAKE_SOURCE_DIR}/src -name "*.c" -o -name "*.h" | xargs clang-format -i
    COMMENT "Formatting source code"
)

add_custom_target(analyze
    COMMAND cppcheck --enable=all --std=c99 
            -I${CMAKE_SOURCE_DIR}/src/include
            -I${CMAKE_SOURCE_DIR}/src/core
            -I${CMAKE_SOURCE_DIR}/src/http
            -I${CMAKE_SOURCE_DIR}/src/proxy
            -I${CMAKE_SOURCE_DIR}/src/stream
            -I${CMAKE_SOURCE_DIR}/src/utils
            -I${CMAKE_SOURCE_DIR}/src/utils/asm
            ${CMAKE_SOURCE_DIR}/src
    COMMENT "Running static analysis"
)

# 显示配置信息
message(STATUS "=== ANX HTTP Server Build Configuration ===")
message(STATUS "Version: ${PROJECT_VERSION}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C Compiler: ${CMAKE_C_COMPILER}")
message(STATUS "C Flags: ${CMAKE_C_FLAGS}")
message(STATUS "System: ${CMAKE_SYSTEM_NAME}")
message(STATUS "Architecture: ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "OpenSSL: ${OPENSSL_VERSION}")
message(STATUS "Zlib: ${ZLIB_VERSION_STRING}")
message(STATUS "=============================================") 