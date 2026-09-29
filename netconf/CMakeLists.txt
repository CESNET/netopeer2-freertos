#
# netconf build system
#

cmake_minimum_required(VERSION 3.13.0)

project(netconf C)

#
# dependencies
#
include(FetchContent)
include(GNUInstallDirs)

# standard options used by many projects
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
set(CMAKE_POLICY_DEFAULT_CMP0075 NEW)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
set(CMAKE_C_BYTE_ORDER LITTLE_ENDIAN)
set(CMAKE_DL_LIBS "") # no dl library
set(CMAKE_INSTALL_PREFIX /opt)
set(CMAKE_INSTALL_LIBDIR lib)
set(CMAKE_INSTALL_DATADIR share)
set(BUILD_SHARED_LIBS OFF)
set(CMAKE_DISABLE_FIND_PACKAGE_PkgConfig ON)
set(ENABLE_COMMON_TARGETS OFF) # prevents conflicts of targets in the projects

# add additional macros
add_compile_definitions(_POSIX_C_SOURCE=200809L _GNU_SOURCE _POSIX_THREADS _POSIX_TIMERS _POSIX_READER_WRITER_LOCKS _POSIX_MONOTONIC_CLOCK)
add_compile_options(-Wno-char-subscripts)

# add netconf root to find path prefixes
set(CMAKE_FIND_ROOT_PATH ${CMAKE_FIND_ROOT_PATH} ${NETCONF_FIND_ROOT_PATH})

# change include directories and definitions from the target only to global (for subdirectories)
include_directories(${NETCONF_INCLUDE_DIRS})
include_directories(BEFORE SYSTEM ${NETCONF_SYSTEM_INCLUDE_DIRS})
add_compile_definitions(${NETCONF_DEFINITIONS})

# transform definitions into compile flags
list(TRANSFORM NETCONF_DEFINITIONS PREPEND "-D")

# also add to CMAKE_REQUIRED_INCLUDES (for check_include_file) and set CMAKE_REQUIRED_DEFINITIONS
list(APPEND CMAKE_REQUIRED_INCLUDES ${NETCONF_INCLUDE_DIRS})
list(APPEND CMAKE_REQUIRED_DEFINITIONS ${NETCONF_DEFINITIONS})

##################################
# FreeRTOS-POSIX
message("CONFIG: # freertos-posix")
FetchContent_Declare(freertos-posix
        GIT_REPOSITORY https://github.com/FreeRTOS/Lab-Project-FreeRTOS-POSIX
        GIT_TAG main
        PATCH_COMMAND git apply
            ${PROJECT_SOURCE_DIR}/freertos_posix/missing-errno-fix.patch
            ${PROJECT_SOURCE_DIR}/freertos_posix/mutex-init-fix.patch
            ${PROJECT_SOURCE_DIR}/freertos_posix/clock-id-compat-fix.patch
            ${PROJECT_SOURCE_DIR}/freertos_posix/stack-type-fix.patch
            ${PROJECT_SOURCE_DIR}/freertos_posix/pthread-self-fix.patch
            ${PROJECT_SOURCE_DIR}/freertos_posix/malloc-use-fix.patch
            ${PROJECT_SOURCE_DIR}/freertos_posix/pthread-static-task-fix.patch
        UPDATE_DISCONNECTED 1
)
FetchContent_MakeAvailable(freertos-posix)

add_library(freertos_posix STATIC
    ${freertos-posix_SOURCE_DIR}/FreeRTOS-Plus-POSIX/source/FreeRTOS_POSIX_clock.c
    ${freertos-posix_SOURCE_DIR}/FreeRTOS-Plus-POSIX/source/FreeRTOS_POSIX_pthread.c
    ${freertos-posix_SOURCE_DIR}/FreeRTOS-Plus-POSIX/source/FreeRTOS_POSIX_pthread_mutex.c
    ${freertos-posix_SOURCE_DIR}/FreeRTOS-Plus-POSIX/source/FreeRTOS_POSIX_pthread_cond.c
    ${freertos-posix_SOURCE_DIR}/FreeRTOS-Plus-POSIX/source/FreeRTOS_POSIX_pthread_barrier.c
    ${freertos-posix_SOURCE_DIR}/FreeRTOS-Plus-POSIX/source/FreeRTOS_POSIX_timer.c
    ${freertos-posix_SOURCE_DIR}/FreeRTOS-Plus-POSIX/source/FreeRTOS_POSIX_utils.c
    ${PROJECT_SOURCE_DIR}/freertos_posix/pthread_compat.c
)

target_include_directories(freertos_posix BEFORE PRIVATE ${PROJECT_SOURCE_DIR}/freertos_posix/header_freertos_posix)
target_include_directories(freertos_posix
    PRIVATE
    ${freertos-posix_SOURCE_DIR}/FreeRTOS-Plus-POSIX/include/portable
    PUBLIC
    ${freertos-posix_SOURCE_DIR}/include
    ${freertos-posix_SOURCE_DIR}/include/private
    ${freertos-posix_SOURCE_DIR}/FreeRTOS-Plus-POSIX/include
    ${PROJECT_SOURCE_DIR}/freertos_posix/header_project
    ${PROJECT_SOURCE_DIR}/freertos_posix)

# sys/types.h; pthread.h and the rest
include_directories(BEFORE SYSTEM
    ${PROJECT_SOURCE_DIR}/freertos_posix
    ${PROJECT_SOURCE_DIR}/freertos_posix/include
    ${PROJECT_SOURCE_DIR}/freertos_posix/header_project
    ${freertos-posix_SOURCE_DIR}/include
    ${freertos-posix_SOURCE_DIR}/include/private
    ${freertos-posix_SOURCE_DIR}/FreeRTOS-Plus-POSIX/include)

# reset FindPackage Threads search
unset(CMAKE_HAVE_LIBC_PTHREAD CACHE)

##################################
# emb-POSIX

# file system backend, may be provided by the project, defaults to the built-in littleFS one
IF(NOT DEFINED EMB_POSIX_FS_SOURCE)
    set(EMB_POSIX_FS_SOURCE ${PROJECT_SOURCE_DIR}/emb_posix/emb_posix_fs_lfs.c)
    set(EMB_POSIX_FS_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/emb_posix/backend/lfs)
ENDIF()

# network backend, may be provided by the project, defaults to the built-in lwIP one
IF(NOT DEFINED EMB_POSIX_NET_SOURCE)
    set(EMB_POSIX_NET_SOURCE ${PROJECT_SOURCE_DIR}/emb_posix/emb_posix_net_lwip.c)
    set(EMB_POSIX_NET_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/emb_posix/backend/lwip)
ENDIF()

message(STATUS "emb_posix file system backend: ${EMB_POSIX_FS_SOURCE}")
message(STATUS "emb_posix network backend: ${EMB_POSIX_NET_SOURCE}")

# dirent.h and others; the POSIX socket headers take their types from the network backend
include_directories(BEFORE SYSTEM
    ${PROJECT_SOURCE_DIR}/emb_posix/include
    ${EMB_POSIX_NET_INCLUDE_DIR}/include)

add_library(emb_posix STATIC
    ${PROJECT_SOURCE_DIR}/emb_posix/emb_posix_fdp.c
    ${EMB_POSIX_FS_SOURCE}
    ${EMB_POSIX_NET_SOURCE}
    ${PROJECT_SOURCE_DIR}/emb_posix/emb_posix_shm.c
    ${PROJECT_SOURCE_DIR}/emb_posix/emb_posix_fifo.c
    ${PROJECT_SOURCE_DIR}/emb_posix/emb_posix_fd.c
    ${PROJECT_SOURCE_DIR}/emb_posix/emb_posix_path.c
    ${PROJECT_SOURCE_DIR}/emb_posix/emb_posix_dirent.c
    ${PROJECT_SOURCE_DIR}/emb_posix/emb_posix_stdio.c
    ${PROJECT_SOURCE_DIR}/emb_posix/emb_posix_sys.c
)
target_compile_definitions(emb_posix PRIVATE _POSIX_TIMERS)
target_include_directories(emb_posix
    PUBLIC
        ${EMB_POSIX_FS_INCLUDE_DIR}     # emb_posix_fs_backend.h of the selected backend
        ${EMB_POSIX_NET_INCLUDE_DIR}    # emb_posix_net_backend.h of the selected backend
    PRIVATE
        ${PROJECT_SOURCE_DIR}/emb_posix # emb_posix_fs.h, emb_posix_net.h for an out-of-tree backend
)
target_link_libraries(emb_posix PUBLIC ${NETCONF_BASE_LIBS})

##################################
# crypt
add_library(crypt STATIC
    ${PROJECT_SOURCE_DIR}/crypt/crypt.c
)
include_directories(SYSTEM ${PROJECT_SOURCE_DIR}/crypt/include)
target_link_libraries(crypt mbedtls)

# update CMAKE_REQUIRED_INCLUDES for check_include_file
get_property(INCLUDE_DIRS_PROP DIRECTORY PROPERTY INCLUDE_DIRECTORIES)
list(APPEND CMAKE_REQUIRED_INCLUDES ${INCLUDE_DIRS_PROP})
list(REMOVE_DUPLICATES CMAKE_REQUIRED_INCLUDES)

##################################
# libssh
message("CONFIG: # libssh")
FetchContent_Declare(libssh
        GIT_REPOSITORY https://gitlab.com/libssh/libssh-mirror
        GIT_TAG libssh-0.11.1
        PATCH_COMMAND git apply
            ${PROJECT_SOURCE_DIR}/libssh/out-of-source-build-fix.patch
            ${PROJECT_SOURCE_DIR}/libssh/dist-target-fix.patch
            ${PROJECT_SOURCE_DIR}/libssh/weird-version-define-fix.patch
            ${PROJECT_SOURCE_DIR}/libssh/array-size-redefine-fix.patch
            ${PROJECT_SOURCE_DIR}/libssh/no-termios-fix.patch
            ${PROJECT_SOURCE_DIR}/libssh/strerror-r-available-fix.patch
            ${PROJECT_SOURCE_DIR}/libssh/cmake-install-fix.patch
            ${PROJECT_SOURCE_DIR}/libssh/no-unix-socket-fix.patch
            ${PROJECT_SOURCE_DIR}/libssh/no-mbedtls-thread-init-fix.patch
        UPDATE_DISCONNECTED 1 # do not re-apply patches
)

set(WITH_SFTP OFF)
set(WITH_ZLIB OFF)
set(WITH_EXAMPLES OFF)
set(HAVE_GLOB_H OFF)
set(HAVE_ARPA_INET_H YES)
set(HAVE_CONSTRUCTOR_ATTRIBUTE OFF) # can call ssh_init() only after CRYPTO_InitHardware()
set(HAVE_DESTRUCTOR_ATTRIBUTE OFF)
set(WITH_MBEDTLS ON)
set(MBEDTLS_INCLUDE_DIR ${MBEDTLS_PATH}/include) # no mbedtls/config.h to find
set(MBEDTLS_SSL_LIBRARY mbedtls) # dependent libraries are build after configure steps
set(MBEDTLS_CRYPTO_LIBRARY mbedcrypto)
set(MBEDTLS_X509_LIBRARY mbedx509)

set(PREV_CMAKE_C_FLAGS ${CMAKE_C_FLAGS}) # temporary
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-format -Wno-deprecated-declarations")
FetchContent_MakeAvailable(libssh) # provides ssh
set(CMAKE_C_FLAGS "${PREV_CMAKE_C_FLAGS}")
get_directory_property(DIR_COMPILE_DEFS COMPILE_DEFINITIONS)
list(FILTER DIR_COMPILE_DEFS EXCLUDE REGEX [[^_GNU_SOURCE$]])
set_directory_properties(PROPERTIES COMPILE_DEFINITIONS "${DIR_COMPILE_DEFS}")

include_directories(SYSTEM ${libssh_SOURCE_DIR}/include)
include_directories(SYSTEM ${libssh_BINARY_DIR}/include)

##################################
# PCRE2
message("CONFIG: # pcre2")
FetchContent_Declare(pcre2
        GIT_REPOSITORY https://github.com/PCRE2Project/pcre2
        GIT_TAG pcre2-10.44
        PATCH_COMMAND git apply
            ${PROJECT_SOURCE_DIR}/pcre2/mismatched-type-fix.patch
        UPDATE_DISCONNECTED 1
)

set(PCRE2_BUILD_PCRE2GREP OFF)
set(PCRE2_BUILD_TESTS OFF)
set(PCRE2_SHOW_REPORT OFF)
FetchContent_MakeAvailable(pcre2) # provides pcre2-8-static

include_directories(SYSTEM ${pcre2_BINARY_DIR})

##################################
# libcurl
message("CONFIG: # libcurl")
FetchContent_Declare(libcurl
        GIT_REPOSITORY https://github.com/curl/curl
        GIT_TAG curl-8_7_1
        PATCH_COMMAND git apply
            ${PROJECT_SOURCE_DIR}/libcurl/cmake-check-vars-fix.patch
            ${PROJECT_SOURCE_DIR}/libcurl/type-range-fix.patch
        UPDATE_DISCONNECTED 1
)

set(BUILD_LIBCURL_DOCS OFF)
set(BUILD_CURL_EXE OFF)
set(ENABLE_CURL_MANUAL OFF)
set(HAVE_STRDUP ON)
set(HAVE_FTRUNCATE ON)
set(HAVE_INET_NTOP ON)
set(HAVE_INET_PTON ON)
set(HAVE_CLOCK_GETTIME_MONOTONIC ON)
set(HAVE_SETSOCKOPT_SO_NONBLOCK ON)
set(HAVE_IN_ADDR_T ON)
set(HAVE_MEMRCHR OFF)
set(HAVE_GETHOSTBYNAME_R OFF)
set(HAVE_LIBSOCKET OFF)
set(HAVE_BASENAME OFF)
set(HAVE_SIGACTION OFF)
set(HAVE_FNMATCH OFF)
set(USE_LIBIDN2 OFF)
set(MBEDTLS_INCLUDE_DIRS ${MBEDTLS_PATH}/include)
set(MBEDTLS_LIBRARY mbedtls)
set(MBEDCRYPTO_LIBRARY mbedcrypto)
set(MBEDX509_LIBRARY mbedx509)

set(CURL_ENABLE_EXPORT_TARGET OFF)
set(CURL_DISABLE_DICT ON)
set(CURL_DISABLE_GOPHER ON)
set(CURL_USE_MBEDTLS ON)
set(CURL_DISABLE_IMAP ON)
set(CURL_DISABLE_MQTT ON)
set(CURL_DISABLE_POP3 ON)
set(CURL_DISABLE_SMB ON)
set(CURL_DISABLE_SMTP ON)
set(CURL_DISABLE_TELNET ON)
set(CURL_DISABLE_TESTS ON)
add_compile_definitions(_BSD_SOURCE) # arc4random declaration
FetchContent_MakeAvailable(libcurl) # provides libcurl_static

##################################
# libyang
message("CONFIG: # libyang")
FetchContent_Declare(libyang
        GIT_REPOSITORY https://github.com/CESNET/libyang
        GIT_TAG v5.8.6
)

set(HAVE_TIMEGM OFF)
set(HAVE_MMAP OFF)
set(ENABLE_TOOLS OFF)
set(PCRE2_INCLUDE_DIR ${pcre2_BINARY_DIR})
set(PCRE2_LIBRARY ${LIBRARY_OUTPUT_PATH}/pcre2-8)
add_compile_definitions(INET6_ADDRSTRLEN=16) # IPv6 unsupported
FetchContent_MakeAvailable(libyang) # provides yang

include_directories(SYSTEM ${libyang_BINARY_DIR})

##################################
# sysrepo
message("CONFIG: # sysrepo")
FetchContent_Declare(sysrepo
        GIT_REPOSITORY https://github.com/sysrepo/sysrepo
        GIT_TAG v5.1.0
        PATCH_COMMAND git apply
            ${PROJECT_SOURCE_DIR}/sysrepo/timeout-fix.patch
            ${PROJECT_SOURCE_DIR}/sysrepo/shm-resize-check-fix.patch
            ${PROJECT_SOURCE_DIR}/sysrepo/yang-dir-fix.patch
            ${PROJECT_SOURCE_DIR}/sysrepo/printed-context-fix.patch
            ${PROJECT_SOURCE_DIR}/sysrepo/hardcoded-context-file-fix.patch
            ${PROJECT_SOURCE_DIR}/sysrepo/printed-context-linker-addr-fix.patch
        UPDATE_DISCONNECTED 1
)

set(ENABLE_EXAMPLES OFF)
set(ENABLE_SYSREPOCTL OFF)
set(ENABLE_SYSREPOCFG OFF)
set(ENABLE_SYSREPO_NOTIFD OFF)
set(NOTIFD_SETUP OFF)
set(HAVE_STRDUPA 1)
set(REPO_PATH "/tmp/sr") # all files will be temporary except for the following
set(STARTUP_DATA_PATH "/etc/sysrepo/data") # startup datastore files persistent
set(FACTORY_DEFAULT_DATA_PATH "/etc/sysrepo/data") # factory-default datastore files persistent
set(NOTIFICATION_PATH "/etc/sysrepo/data/notif") # notifications for replay persistent
set(YANG_MODULE_PATH "/etc/sysrepo/yang") # YANG modules persistent
set(LIBYANG_INCLUDE_DIRS ${libyang_BINARY_DIR})
set(LIBYANG_LIBRARIES ${LIBRARY_OUTPUT_PATH}/yang)
list(APPEND CMAKE_REQUIRED_DEFINITIONS -D_POSIX_TIMERS)
FetchContent_MakeAvailable(sysrepo) # provides sysrepo
list(REMOVE_ITEM CMAKE_REQUIRED_DEFINITIONS -D_POSIX_TIMERS)

include_directories(SYSTEM ${sysrepo_BINARY_DIR}/include)

##################################
# libnetconf2
message("CONFIG: # libnetconf2")
FetchContent_Declare(libnetconf2
        GIT_REPOSITORY https://github.com/CESNET/libnetconf2
        GIT_TAG v4.4.10
        PATCH_COMMAND git apply
            ${PROJECT_SOURCE_DIR}/libnetconf2/timeout-fix.patch
            ${PROJECT_SOURCE_DIR}/libnetconf2/no-unix-socket-fix.patch
            ${PROJECT_SOURCE_DIR}/libnetconf2/no-ipv6-fix.patch
            ${PROJECT_SOURCE_DIR}/libnetconf2/no-tls-io-fix.patch
            ${PROJECT_SOURCE_DIR}/libnetconf2/blocking-socket-fix.patch
        UPDATE_DISCONNECTED 1
)

set(TRANSPORT_HANDSHAKE_TIMEOUT 60)
set(HAVE_PTHREAD_RWLOCKATTR_SETKIND_NP OFF)
set(MBEDTLS_INCLUDE_DIR ${MBEDTLS_PATH}/include)
set(MBEDTLS_LIBRARIES mbedtls mbedcrypto mbedx509)
set(LIBSSH_INCLUDE_DIRS ${libssh_SOURCE_DIR}/include ${libssh_BINARY_DIR}/include)
set(LIBSSH_LIBRARIES ${LIBRARY_OUTPUT_PATH}/ssh)
set(CURL_INCLUDE_DIR ${libcurl_SOURCE_DIR}/include)
set(CURL_LIBRARY ${LIBRARY_OUTPUT_PATH}/libcurl)
FetchContent_MakeAvailable(libnetconf2) # provides netconf2

include_directories(SYSTEM ${libnetconf2_BINARY_DIR}/include)

##################################
# netopeer2
message("CONFIG: # netopeer2")
FetchContent_Declare(netopeer2
        GIT_REPOSITORY https://github.com/CESNET/netopeer2
        GIT_TAG v2.8.7
        PATCH_COMMAND git apply
            ${PROJECT_SOURCE_DIR}/netopeer2/timeout-fix.patch
            ${PROJECT_SOURCE_DIR}/netopeer2/no-nacm-stats-thread-fix.patch
        UPDATE_DISCONNECTED 1
)

set(BUILD_SERVER OFF)
set(BUILD_CLI OFF)
set(BUILD_NETOPEER2_LIB ON)
set(SYSREPO_SETUP OFF)
set(PIDFILE_PREFIX "/")
set(THREAD_COUNT 1)
set(PS_BACKOFF_SLEEP 200)
set(LN2_YANG_MODULE_DIR ${libnetconf2_SOURCE_DIR}/modules)
set(SYSREPO_INCLUDE_DIRS ${sysrepo_BINARY_DIR}/include)
set(SYSREPO_LIBRARIES ${LIBRARY_OUTPUT_PATH}/sysrepo)
set(LIBNETCONF2_INCLUDE_DIRS ${libnetconf2_BINARY_DIR}/include)
set(LIBNETCONF2_LIBRARIES ${LIBRARY_OUTPUT_PATH}/netconf2)
FetchContent_MakeAvailable(netopeer2) # provides netopeer2

add_library(netopeer2_port STATIC
    ${PROJECT_SOURCE_DIR}/netopeer2/netopeer2_port.c
)
target_include_directories(netopeer2_port PRIVATE
    ${netopeer2_BINARY_DIR}/lib/include
)

#
# create the final netconf library
#
add_library(netconf INTERFACE)
target_include_directories(netconf INTERFACE
    ${PROJECT_SOURCE_DIR}/emb_posix/include
    ${EMB_POSIX_NET_INCLUDE_DIR}/include
    ${libssh_BINARY_DIR}/include
    ${libyang_BINARY_DIR}
    ${sysrepo_BINARY_DIR}/include
    ${libnetconf2_BINARY_DIR}/include
    ${netopeer2_BINARY_DIR}/lib/include
)
target_link_libraries(netconf INTERFACE
    netopeer2 netopeer2_port
    netconf2
    sysrepo
    yang
    libcurl_static
    pcre2-8-static
    ssh
    crypt
    mbedtls
    emb_posix
    freertos_posix
)
