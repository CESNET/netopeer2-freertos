
#ifndef _FREERTOS_POSIX_PORTABLE_H_
#define _FREERTOS_POSIX_PORTABLE_H_

#define posixconfigPTHREAD_TASK_NAME    "pthread"   /**< Task name. */
#define posixconfigTIMER_NAME           "timer"     /**< Timer name. */
#define posixconfigMQ_MAX_MESSAGES      10          /**< Maximum number of messages in an mq at one time. */
#define posixconfigMQ_MAX_SIZE          128         /**< Maximum size (in bytes) of each message. */
#define PTHREAD_STACK_MIN               32768 * sizeof( StackType_t ) /**< Minimum size in bytes of thread stack storage. */
#define SEM_VALUE_MAX                   0x7FFFU     /**< Maximum value of a sem_t. */

#define configUSE_POSIX_ERRNO           0

#endif /* ifndef _FREERTOS_POSIX_PORTABLE_H_ */
