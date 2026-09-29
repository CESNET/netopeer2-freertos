/**
 * @file pthread_compat.c
 * @author Michal Vasko <mvasko@cesnet.cz>
 * @brief FreeRTOS-POSIX additional pthread compatibility functions
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

#include "pthread_compat.h"

#include <errno.h>
#include <stdatomic.h>
//#include <sys/types.h>
#include <task.h>

#include "FreeRTOS_POSIX/pthread.h"

/*
 * pthread_t
 */

int
pthread_cancel(pthread_t thread)
{
    return ENOSYS;
}

int
pthread_atfork(void (*prepare)(void), void (*parent)(void), void (*child)(void))
{
    return ENOSYS;
}

/*
 * pthread_mutex_t
 */

int
pthread_mutexattr_getpshared(const pthread_mutexattr_t *attr, int *pshared)
{
    *pshared = PTHREAD_PROCESS_SHARED;
    return 0;
}

int
pthread_mutexattr_setpshared(pthread_mutexattr_t *attr, int pshared)
{
    return 0;
}

int
pthread_mutexattr_getrobust(const pthread_mutexattr_t *attr, int *robustness)
{
    *robustness = PTHREAD_MUTEX_ROBUST;
    return 0;
}

int
pthread_mutexattr_setrobust(pthread_mutexattr_t *attr, int robustness)
{
    return 0;
}

int
pthread_mutex_consistent(pthread_mutex_t *mutex)
{
    return 0;
}

/*
 * pthread_cond_t
 */

int
pthread_condattr_destroy(pthread_condattr_t *attr)
{
    return 0;
}

int
pthread_condattr_init(pthread_condattr_t *attr)
{
    return 0;
}

int
pthread_condattr_getpshared(const pthread_condattr_t *attr, int *pshared)
{
    *pshared = PTHREAD_PROCESS_SHARED;
    return 0;
}

int
pthread_condattr_setpshared(pthread_condattr_t *attr, int pshared)
{
    return 0;
}

int
pthread_condattr_getclock(const pthread_condattr_t *attr, clockid_t *clock_id)
{
    *clock_id = CLOCK_REALTIME;
    return 0;
}

int
pthread_condattr_setclock(pthread_condattr_t *attr, clockid_t clock_id)
{
    if (clock_id != CLOCK_REALTIME) {
        return EINVAL;
    }

    return 0;
}

/*
 * pthread_rwlock_t
 */

int
pthread_rwlockattr_destroy(pthread_rwlockattr_t *attr)
{
    return pthread_mutexattr_destroy(attr);
}

int
pthread_rwlockattr_init(pthread_rwlockattr_t *attr)
{
    return pthread_mutexattr_init(attr);
}

int
pthread_rwlock_destroy(pthread_rwlock_t *rwlock)
{
    return pthread_mutex_destroy(rwlock);
}

int
pthread_rwlock_init(pthread_rwlock_t *rwlock, const pthread_rwlockattr_t *attr)
{
    return pthread_mutex_init(rwlock, attr);
}

int
pthread_rwlock_rdlock(pthread_rwlock_t *rwlock)
{
    return pthread_mutex_lock(rwlock);
}

int
pthread_rwlock_tryrdlock(pthread_rwlock_t *rwlock)
{
    return pthread_mutex_trylock(rwlock);
}

int
pthread_rwlock_wrlock(pthread_rwlock_t *rwlock)
{
    return pthread_mutex_lock(rwlock);
}

int
pthread_rwlock_trywrlock(pthread_rwlock_t *rwlock)
{
    return pthread_mutex_trylock(rwlock);
}

int
pthread_rwlock_unlock(pthread_rwlock_t *rwlock)
{
    return pthread_mutex_unlock(rwlock);
}

int
pthread_rwlock_timedrdlock(pthread_rwlock_t *rwlock, const struct timespec *abstime)
{
    return pthread_mutex_timedlock(rwlock, abstime);
}

int
pthread_rwlock_timedwrlock(pthread_rwlock_t *rwlock, const struct timespec *abstime)
{
    return pthread_mutex_timedlock(rwlock, abstime);
}

/*
 * pthread_once_t
 */

int
pthread_once(pthread_once_t *once_control, void (*init_routine)(void))
{
    char expected = 0;

    if (atomic_compare_exchange_strong(once_control, &expected, 1)) {
        /* while init is executing, once_control is already set to initialized (which may not be okay) */
        init_routine();
    } /* else already performed */

    return 0;
}

/*
 * pthread_key_t
 */

static BaseType_t pthread_keys[PTHREAD_KEYS_MAX];

int
pthread_key_create(pthread_key_t *key, void (*destr_function)(void *))
{
    BaseType_t i;

    /* find an unallocated key */
    for (i = 0; i < PTHREAD_KEYS_MAX; ++i) {
        if (!pthread_keys[i]) {
            break;
        }
    }
    if (i == PTHREAD_KEYS_MAX) {
        return EAGAIN;
    }

    /* allocate the key */
    pthread_keys[i] = 1;
    *key = i;

    /* no way to call the destructor */
    (void)destr_function;

    return 0;
}

int
pthread_key_delete(pthread_key_t key)
{
    if ((key >= PTHREAD_KEYS_MAX) || !pthread_keys[key]) {
        /* invalid */
        return EINVAL;
    }

    /* "free" the key */
    pthread_keys[key] = 0;

    return 0;
}

int
pthread_setspecific(pthread_key_t key, const void *pointer)
{
    if ((key >= PTHREAD_KEYS_MAX) || !pthread_keys[key]) {
        /* invalid */
        return EINVAL;
    }

    /* store pointer for the current task */
    vTaskSetThreadLocalStoragePointer(NULL, key, (void *)pointer);

    return 0;
}

void *
pthread_getspecific(pthread_key_t key)
{
    if ((key >= PTHREAD_KEYS_MAX) || !pthread_keys[key]) {
        /* invalid */
        return NULL;
    }

    /* get TLS for the current task */
    return pvTaskGetThreadLocalStoragePointer(NULL, key);
}
