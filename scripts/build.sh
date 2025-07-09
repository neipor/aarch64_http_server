#!/bin/bash

# ANX HTTP Server - 构建脚本
# 提供统一的构建接口，支持多种构建模式

set -e  # 遇到错误立即退出

# 脚本配置
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
PROJECT_NAME="anx"

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 日志函数
log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

log_debug() {
    if [[ "$VERBOSE" == "1" ]]; then
        echo -e "${BLUE}[DEBUG]${NC} $1"
    fi
}

# 显示帮助信息
show_help() {
    cat << EOF
ANX HTTP Server 构建脚本

用法: $0 [选项] [模式]

模式:
  debug         构建调试版本 (默认)
  release       构建发布版本
  profile       构建性能分析版本
  clean         清理构建文件
  test          运行测试
  package       打包发布
  install       安装到系统
  uninstall     从系统卸载

选项:
  -h, --help    显示此帮助信息
  -v, --verbose 详细输出
  -j, --jobs N  并行编译任务数 (默认: 自动检测)
  -c, --cmake   使用CMake构建 (默认: Makefile)
  -f, --format  构建前格式化代码
  -a, --analyze 运行静态分析
  --clean-first 构建前先清理

示例:
  $0 release              # 构建发布版本
  $0 -v debug             # 构建调试版本（详细输出）
  $0 -j4 -f release       # 4线程构建发布版本，构建前格式化代码
  $0 --cmake release      # 使用CMake构建发布版本
  $0 test                 # 运行测试

EOF
}

# 检查依赖
check_dependencies() {
    log_info "检查构建依赖..."
    
    # 检查编译器
    if ! command -v gcc &> /dev/null; then
        log_error "gcc 未安装或不在PATH中"
        exit 1
    fi
    
    # 检查必要的库
    if ! pkg-config --exists openssl; then
        log_error "OpenSSL 开发库未安装"
        exit 1
    fi
    
    if ! pkg-config --exists zlib; then
        log_error "zlib 开发库未安装"
        exit 1
    fi
    
    log_info "依赖检查通过"
}

# 获取CPU核心数
get_cpu_cores() {
    if command -v nproc &> /dev/null; then
        nproc
    elif [[ -f /proc/cpuinfo ]]; then
        grep -c ^processor /proc/cpuinfo
    else
        echo "4"  # 默认值
    fi
}

# 格式化代码
format_code() {
    log_info "格式化源代码..."
    if command -v clang-format &> /dev/null; then
        find "$PROJECT_ROOT/src" -name "*.c" -o -name "*.h" | xargs clang-format -i
        log_info "代码格式化完成"
    else
        log_warn "clang-format 未安装，跳过代码格式化"
    fi
}

# 静态分析
analyze_code() {
    log_info "运行静态分析..."
    if command -v cppcheck &> /dev/null; then
        cppcheck --enable=all --std=c99 \
            -I"$PROJECT_ROOT/src/include" \
            -I"$PROJECT_ROOT/src/core" \
            -I"$PROJECT_ROOT/src/http" \
            -I"$PROJECT_ROOT/src/proxy" \
            -I"$PROJECT_ROOT/src/stream" \
            -I"$PROJECT_ROOT/src/utils" \
            -I"$PROJECT_ROOT/src/utils/asm" \
            "$PROJECT_ROOT/src"
        log_info "静态分析完成"
    else
        log_warn "cppcheck 未安装，跳过静态分析"
    fi
}

# Makefile构建
build_with_make() {
    local mode="$1"
    local jobs="$2"
    
    log_info "使用 Makefile 构建 ($mode 模式, $jobs 并行任务)..."
    
    cd "$PROJECT_ROOT"
    
    if [[ "$CLEAN_FIRST" == "1" ]]; then
        make clean
    fi
    
    if [[ "$mode" == "clean" ]]; then
        make clean-all
        return
    fi
    
    # 使用新的 Makefile
    if [[ -f "Makefile.new" ]]; then
        make -f Makefile.new -j"$jobs" "$mode"
    else
        make -j"$jobs" MODE="$mode"
    fi
    
    log_info "构建完成"
}

# CMake构建
build_with_cmake() {
    local mode="$1"
    local jobs="$2"
    
    log_info "使用 CMake 构建 ($mode 模式, $jobs 并行任务)..."
    
    local build_dir="$PROJECT_ROOT/build-cmake"
    local cmake_build_type
    
    case "$mode" in
        debug)   cmake_build_type="Debug" ;;
        release) cmake_build_type="Release" ;;
        profile) cmake_build_type="RelWithDebInfo" ;;
        clean)
            rm -rf "$build_dir"
            log_info "CMake 构建目录已清理"
            return
            ;;
        *)
            log_error "不支持的 CMake 构建模式: $mode"
            exit 1
            ;;
    esac
    
    mkdir -p "$build_dir"
    cd "$build_dir"
    
    cmake -DCMAKE_BUILD_TYPE="$cmake_build_type" "$PROJECT_ROOT"
    cmake --build . --parallel "$jobs"
    
    log_info "构建完成"
}

# 运行测试
run_tests() {
    log_info "运行测试..."
    cd "$PROJECT_ROOT"
    
    if [[ "$USE_CMAKE" == "1" ]]; then
        cd build-cmake
        ctest
    else
        if [[ -f "Makefile.new" ]]; then
            make -f Makefile.new test
        else
            make test
        fi
    fi
    
    log_info "测试完成"
}

# 打包
create_package() {
    log_info "创建发布包..."
    
    cd "$PROJECT_ROOT"
    
    if [[ "$USE_CMAKE" == "1" ]]; then
        cd build-cmake
        cpack
    else
        if [[ -f "Makefile.new" ]]; then
            make -f Makefile.new package
        else
            make package
        fi
    fi
    
    log_info "打包完成"
}

# 安装
install_package() {
    log_info "安装到系统..."
    
    cd "$PROJECT_ROOT"
    
    if [[ "$USE_CMAKE" == "1" ]]; then
        cd build-cmake
        sudo cmake --install .
    else
        if [[ -f "Makefile.new" ]]; then
            sudo make -f Makefile.new install
        else
            sudo make install
        fi
    fi
    
    log_info "安装完成"
}

# 卸载
uninstall_package() {
    log_info "从系统卸载..."
    
    cd "$PROJECT_ROOT"
    
    if [[ -f "Makefile.new" ]]; then
        sudo make -f Makefile.new uninstall
    else
        sudo make uninstall
    fi
    
    log_info "卸载完成"
}

# 主函数
main() {
    # 默认值
    local mode="debug"
    local jobs=$(get_cpu_cores)
    local USE_CMAKE=0
    local FORMAT_CODE=0
    local ANALYZE_CODE=0
    local CLEAN_FIRST=0
    local VERBOSE=0
    
    # 解析命令行参数
    while [[ $# -gt 0 ]]; do
        case $1 in
            -h|--help)
                show_help
                exit 0
                ;;
            -v|--verbose)
                VERBOSE=1
                shift
                ;;
            -j|--jobs)
                jobs="$2"
                shift 2
                ;;
            -c|--cmake)
                USE_CMAKE=1
                shift
                ;;
            -f|--format)
                FORMAT_CODE=1
                shift
                ;;
            -a|--analyze)
                ANALYZE_CODE=1
                shift
                ;;
            --clean-first)
                CLEAN_FIRST=1
                shift
                ;;
            debug|release|profile|clean|test|package|install|uninstall)
                mode="$1"
                shift
                ;;
            *)
                log_error "未知选项: $1"
                show_help
                exit 1
                ;;
        esac
    done
    
    # 导出变量供子函数使用
    export VERBOSE USE_CMAKE CLEAN_FIRST
    
    log_info "ANX HTTP Server 构建脚本"
    log_info "项目根目录: $PROJECT_ROOT"
    log_info "构建模式: $mode"
    log_info "并行任务数: $jobs"
    log_info "构建工具: $([ "$USE_CMAKE" == "1" ] && echo "CMake" || echo "Makefile")"
    
    # 检查依赖
    check_dependencies
    
    # 格式化代码（如果需要）
    if [[ "$FORMAT_CODE" == "1" ]]; then
        format_code
    fi
    
    # 静态分析（如果需要）
    if [[ "$ANALYZE_CODE" == "1" ]]; then
        analyze_code
    fi
    
    # 执行构建
    case "$mode" in
        debug|release|profile|clean)
            if [[ "$USE_CMAKE" == "1" ]]; then
                build_with_cmake "$mode" "$jobs"
            else
                build_with_make "$mode" "$jobs"
            fi
            ;;
        test)
            run_tests
            ;;
        package)
            create_package
            ;;
        install)
            install_package
            ;;
        uninstall)
            uninstall_package
            ;;
        *)
            log_error "不支持的模式: $mode"
            exit 1
            ;;
    esac
    
    log_info "所有操作完成"
}

# 运行主函数
main "$@" 