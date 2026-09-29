# netopeer2-FreeRTOS

[![BSD license](https://img.shields.io/badge/License-BSD-blue.svg)](https://opensource.org/licenses/BSD-3-Clause)

netopeer2-FreeRTOS is a port of [Netopeer2](https://github.com/CESNET/Netopeer2) NETCONF server on an embedded device
running FreeRTOS. It was extracted from an image for the NXP i.mx RT1180 board but should be easily adjusted for
any FreeRTOS-based image.

## Requirements

### Build Requirements

* target device C cross-compiler
  - ARM: arm-gnu-toolchain-14.3.rel1-x86_64-arm-none-eabi
* cmake >= 3.13.0
* FreeRTOS >= 11.1.0
* littlefs >= 2.9
* lwIP >= 2.2.1
* MbedTLS >= 3.6.0

### Requirements Config

#### FreeRTOS

Additional kernel `FreeRTOSConfig.h` configuration:
```
#define INCLUDE_xSemaphoreGetMutexHolder        1
#define configUSE_APPLICATION_TASK_TAG          1
```

#### littlefs

The littlefs filesystem needs to be configured with the exact same parameters as the generated image
for it to be mounted successfully. By default, there are `1024` blocks, each with size `4096`.

Then, the filesystem needs to be configured in runtime and mounted/unmounted:
```
/* init FS */
struct lfs_config *lfs_cfg = embp_get_lfs_cfg();
lfs_t *lfs = embp_get_lfs();

lfs_get_default_config(lfs_cfg);
lfs_storage_init(lfs_cfg);
lfs_mount(lfs, lfs_cfg);

...

lfs_unmount(embp_get_lfs());
```

#### lwIP

Additional `lwipopts.h` configuration:
```
#ifndef LWIP_TCP_KEEPALIVE
#define LWIP_TCP_KEEPALIVE 1
#endif

#ifndef LWIP_PROVIDE_ERRNO
#define LWIP_PROVIDE_ERRNO 0
#endif

#ifndef LWIP_PROVIDE_ERRNO
#define LWIP_ERRNO_STDINCLUDE 1
#endif

```

#### MbedTLS

Additional `mbedtls_config.h` configuration:
```
#define MBEDTLS_ERROR_C
#define MBEDTLS_THREADING_C // requires threads implementation
```

## Building

The `netconf` directory is meant to be included in the main project as a subproject. More details can be found in the
`CMakeLists-snippet.txt` file that includes an example of code of a parent project including *netopeer2-FreeRTOS*.

## Usage

The main server function `np2_server()` is declared in the header:
```
#include <netopeer2.h>
```
but functions from other headers may be used:
```
#include <emb_posix.h>
#include <libyang/libyang.h>
#include <sysrepo.h>
```

An example of a FreeRTOS task running the NETCONF server:
```
static void *
np2_task(void *arg)
{
    char *argv[] = {"/netopeer2-server", "-v3", "-t120", "-p/tmp/netopeer2-server.pid"};

    /* set emb_posix syslog callback for netopeer2 */
    embp_syslog_set_cb(np2_embp_syslog_example_cb);

    /* init FS */
    struct lfs_config *lfs_cfg = embp_get_lfs_cfg();
    lfs_t *lfs = embp_get_lfs();

    lfs_get_default_config(lfs_cfg);
    lfs_storage_init(lfs_cfg);
    lfs_mount(lfs, lfs_cfg);
    
    /* start the server */
    np2_server(sizeof argv / sizeof *argv, argv);

    lfs_unmount(embp_get_lfs());
    vTaskDelete(NULL);
    return NULL;
}
```

The project must then link the `netconf` static library.

## Configuration

Server uses *sysrepo* to store and manage its full configuration. Look into `flash-fs` dir for more information and
instructions on how to customize it.
