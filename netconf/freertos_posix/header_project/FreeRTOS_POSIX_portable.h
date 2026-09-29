
#ifndef _FREERTOS_POSIX_PORTABLE_H_
#define _FREERTOS_POSIX_PORTABLE_H_

#define configUSE_POSIX_ERRNO           0

/**
 * @name Enable typedefs of POSIX types.
 *
 * Set these values to 1 or 0 to enable or disable the typedefs, respectively.
 * These typedefs should only be disabled if they conflict with system typedefs.
 */
/**@{ */
#define posixconfigENABLE_CLOCK_T                1 /**< clock_t in sys/types.h */
#define posixconfigENABLE_CLOCKID_T              0 /**< clockid_t in sys/types.h */
#define posixconfigENABLE_MODE_T                 0 /**< mode_t in sys/types.h */
#define posixconfigENABLE_PID_T                  1 /**< pid_t in sys/types.h */
#define posixconfigENABLE_PTHREAD_ATTR_T         1 /**< pthread_attr_t in sys/types.h */
#define posixconfigENABLE_PTHREAD_COND_T         1 /**< pthread_cond_t in sys/types.h */
#define posixconfigENABLE_PTHREAD_CONDATTR_T     1 /**< pthread_condattr_t in sys/types.h */
#define posixconfigPTHREAD_COND_MAX_WAITERS      4
#define posixconfigENABLE_PTHREAD_MUTEX_T        1 /**< pthread_mutex_t in sys/types.h */
#define posixconfigENABLE_PTHREAD_MUTEXATTR_T    1 /**< pthread_mutexattr_t in sys/types.h */
#define posixconfigENABLE_PTHREAD_T              1 /**< pthread_t in sys/types.h */
#define posixconfigENABLE_SSIZE_T                1 /**< ssize_t in sys/types.h */
#define posixconfigENABLE_TIME_T                 1 /**< time_t in sys/types.h */
#define posixconfigENABLE_TIMER_T                0 /**< timer_t in sys/types.h */
#define posixconfigENABLE_USECONDS_T             1 /**< useconds_t in sys/types.h */
#define posixconfigENABLE_TIMESPEC               1 /**< struct timespec in time.h */
#define posixconfigENABLE_ITIMERSPEC             1 /**< struct itimerspec in time.h */
#define posixconfigENABLE_SEM_T                  1 /**< struct sem_t in semaphore.h */
#define posixconfigENABLE_PTHREAD_BARRIER_T      1 /**< pthread_barrier_t in sys/types.h */
#define posixconfigENABLE_SCHED_PARAM            1 /**< sched_param in sched.h */
/**@} */

#endif /* ifndef _FREERTOS_POSIX_PORTABLE_H_ */
