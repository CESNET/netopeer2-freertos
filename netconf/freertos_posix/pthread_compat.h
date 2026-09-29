/**
 * @file pthread_compat.h
 * @author Michal Vasko <mvasko@cesnet.cz>
 * @brief FreeRTOS-POSIX additional pthread compatibility function header
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

#ifndef _PTHREAD_COMPAT_H_
#define _PTHREAD_COMPAT_H_

#include <stdatomic.h>
#include <sys/_pthreadtypes.h>
#include <task.h>

#include "FreeRTOS_POSIX/time.h"

/*
 * pthread_t
 */

int pthread_cancel(pthread_t thread);
int pthread_atfork(void (*prepare)(void), void (*parent)(void), void (*child)(void));

/*
 * pthread_mutex_t
 */

/* has no effect */
#define PTHREAD_PROCESS_PRIVATE 0x0
#define PTHREAD_PROCESS_SHARED 0x1

int pthread_mutexattr_getpshared(const pthread_mutexattr_t *attr, int *pshared);
int pthread_mutexattr_setpshared(pthread_mutexattr_t *attr, int pshared);

#define PTHREAD_MUTEX_STALLED 0x0
#define PTHREAD_MUTEX_ROBUST 0x1

int pthread_mutexattr_getrobust(const pthread_mutexattr_t *attr, int *robustness);
int pthread_mutexattr_setrobust(pthread_mutexattr_t *attr, int robustness);

int pthread_mutex_consistent(pthread_mutex_t *mutex);

/*
 * pthread_cond_t
 */
int pthread_condattr_destroy(pthread_condattr_t *attr);
int pthread_condattr_init(pthread_condattr_t *attr);

int pthread_condattr_getpshared(const pthread_condattr_t *attr, int *pshared);
int pthread_condattr_setpshared(pthread_condattr_t *attr, int pshared);

int pthread_condattr_getclock(const pthread_condattr_t *attr, clockid_t *clock_id);
int pthread_condattr_setclock(pthread_condattr_t *attr, clockid_t clock_id);

/*
 * pthread_rwlock_t TODO
 */
typedef pthread_mutex_t pthread_rwlock_t;
typedef pthread_mutexattr_t pthread_rwlockattr_t;

#define PTHREAD_RWLOCK_INITIALIZER PTHREAD_MUTEX_INITIALIZER

int pthread_rwlockattr_destroy(pthread_rwlockattr_t *attr);
int pthread_rwlockattr_init(pthread_rwlockattr_t *attr);

int pthread_rwlock_destroy(pthread_rwlock_t *rwlock);
int pthread_rwlock_init(pthread_rwlock_t *rwlock, const pthread_rwlockattr_t *attr);

int pthread_rwlock_rdlock(pthread_rwlock_t *rwlock);
int pthread_rwlock_tryrdlock(pthread_rwlock_t *rwlock);
int pthread_rwlock_wrlock(pthread_rwlock_t *rwlock);
int pthread_rwlock_trywrlock(pthread_rwlock_t *rwlock);
int pthread_rwlock_unlock(pthread_rwlock_t *rwlock);

int pthread_rwlock_timedrdlock(pthread_rwlock_t *rwlock, const struct timespec *abstime);
int pthread_rwlock_timedwrlock(pthread_rwlock_t *rwlock, const struct timespec *abstime);

/*
 * pthread_once_t
 */
typedef atomic_char pthread_once_t;

#define PTHREAD_ONCE_INIT 0

int pthread_once(pthread_once_t *once_control, void (*init_routine)(void));

/*
 * pthread_key_t
 */
typedef BaseType_t pthread_key_t;

#define PTHREAD_KEYS_MAX 4 /**< cannot be higher than configNUM_THREAD_LOCAL_STORAGE_POINTERS in FreeRTOSConfig.h */

int pthread_key_create(pthread_key_t *key, void (*destr_function)(void *));
int pthread_key_delete(pthread_key_t key);
int pthread_setspecific(pthread_key_t key, const void *pointer);
void *pthread_getspecific(pthread_key_t key);

#endif /* _PTHREAD_COMPAT_H_ */
