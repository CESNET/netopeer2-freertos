/**
 * @file netopeer2_port.c
 * @author Michal Vasko <mvasko@cesnet.cz>
 * @brief implementation of netopeer2 lib test custom functions
 *
 * @copyright
 * Copyright (c) 2026 CESNET, z.s.p.o.
 * Copyright (c) 2026, B&R Industrial Automation GmbH
 *
 * This source code is licensed under BSD 3-Clause License (the "License").
 * You may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://opensource.org/licenses/BSD-3-Clause
 */

#define _GNU_SOURCE

#include "netopeer2.h"

#include "FreeRTOS.h"
#include "task.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/**< maximum length of a single server string argument */
#define ARG_STR_LEN 255

/**
 * @brief Server task parameter with all the arguments.
 */
struct np_server_arg {
    char *pidfile_path;
    char *sock_path;
    char *server_dir;
};

/**< path to the server PID file */
char pidfile_path[ARG_STR_LEN + 1];

/**
 * @brief netopeer2-server task.
 *
 * @param[in] pvParameters Argument values to use.
 */
static void
np2_server_task(void *pvParameters)
{
    struct np_server_arg *arg = pvParameters;
    char sock_path[ARG_STR_LEN + 1], server_dir[ARG_STR_LEN + 1];
    char *argv[] = {"/netopeer2-server", "-d", "-v3", "-t120", "-p", pidfile_path, "-U", sock_path, "-m 600",
            "-f", server_dir};

    /* move the parameters to the stack of this task */
    strcpy(pidfile_path, arg->pidfile_path);
    strcpy(sock_path, arg->sock_path);
    strcpy(server_dir, arg->server_dir);

    /* free arg */
    free(arg->pidfile_path);
    free(arg->sock_path);
    free(arg->server_dir);
    free(arg);

    /* start server */
    if (np2_server(sizeof argv / sizeof *argv, argv)) {
        printf("%s: np2_server() failed.\n", __func__);
    }

    vTaskDelete(NULL);
}

int
np2_server_test_start(const char *pidfile_path, const char *sock_path, const char *server_dir, const char *extdata_path)
{
    struct np_server_arg *arg = NULL;

    /* task stack string limits */
    if ((strlen(pidfile_path) > ARG_STR_LEN) || (strlen(sock_path) > ARG_STR_LEN) || (strlen(server_dir) > ARG_STR_LEN)) {
        printf("%s: too long arguments.\n", __func__);
        goto error;
    }

    /* ignore extdata because handling additional context is very expensive */

    /* prepare the parameter */
    arg = malloc(sizeof *arg);
    if (!arg) {
        printf("%s: memory allocation failed.\n", __func__);
        goto error;
    }
    arg->pidfile_path = strdup(pidfile_path);
    arg->sock_path = strdup(sock_path);
    arg->server_dir = strdup(server_dir);
    if (!arg->pidfile_path || !arg->sock_path || !arg->server_dir) {
        printf("%s: memory allocation failed.\n", __func__);
        goto error;
    }

    /* start the server task */
    if (xTaskCreate(np2_server_task, "np2_server_task", 16384 * sizeof( StackType_t ), arg, configMAX_PRIORITIES - 1, NULL) != pdPASS) {
        printf("%s: creating a task failed.\n", __func__);
        goto error;
    }

    /* arg pased to the task */
    return 0;

error:
    if (arg) {
        free(arg->pidfile_path);
        free(arg->sock_path);
        free(arg->server_dir);
    }
    free(arg);

    return 1;
}

int
np2_server_test_stop(void)
{
    /* signal to the server it should terminate */
    np2_server_terminate();

    /* wait until it does */
    while (!access(pidfile_path, F_OK)) {
        usleep(10000);
    }

    return 0;
}
