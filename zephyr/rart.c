/**
 * @file rart.c
 * @author Matheus T. dos Santos (tenoriomatheus0@gmail.com)
 * @brief
 * @version 0.1
 * @date 09/07/2022
 *
 * @copyright Copyright (c) 2022
 *
 */
#include <stdbool.h>
#include <stdint.h>
#include <zephyr.h>

#include "../generated/rart-defines.h"

#define NUM_OF_MUTEXES (7 * NUM_OF_TASKS)
#define NUM_OF_MSGQ (4 * NUM_OF_TASKS)
#define NUM_OF_MSG_ITENS (4 * NUM_OF_TASKS)

#define MSG_ITEM_SIZE 8

#define HEAP_TOTAL 1024

#define INVALID_INDEX ((rart_index_t) -1)

/**
 * @brief Heap definition used by Rust
 */
K_HEAP_DEFINE(rtos_allocator, HEAP_TOTAL);

/**
 * @brief Type of the timer callback called after the Zephyr timer
 */
typedef void (*rart_timer_callback_t)(const void *);

/**
 * @brief Type of the index used in this lib.
 */
typedef uint8_t rart_index_t;

/**
 * @brief Struct with global variables of the RART-c
 */
static struct rart_fields {
    struct {
        struct k_mutex mutex;  /**< Zephyr OS mutex. */
        bool is_free;          /**< Flag to check if the mutex is free */
        bool is_init;          /**< Flag to check the mutex initialization. */
    } mutexes[NUM_OF_MUTEXES]; /**< List of mutexes */
    struct {
        struct {
            uint8_t
                buffer[NUM_OF_MSG_ITENS * MSG_ITEM_SIZE]; /**< Message queue storage */
            struct k_msgq msgq;                           /**< Zephyr OS message queue. */
        } instance[NUM_OF_MSGQ];                          /**< List of Message Queues */
        rart_index_t index; /**< Index of the next available message queue */
        bool is_init;       /**< Flag to check all message queues initialization */
    } msgq;                 /**< Message queue sub-struct */
    struct {
        const void
            *state; /**< Reference to state that will be sent in the timer callback. */
        rart_timer_callback_t callback; /**< Timer callback */
        struct k_timer timer;           /**< Zephyr OS timer */
        bool is_free;                   /**< Flag to check if the mutex is free */
    } timers[NUM_OF_TASKS];             /**< List of timers */
} self = {
    .mutexes = {[0 ...(NUM_OF_MUTEXES - 1)] =
                    {
                        .mutex   = {0},
                        .is_free = true,
                        .is_init = false,
                    }},
    .msgq =
        {
            .index    = 0,
            .is_init  = false,
            .instance = {[0 ...(NUM_OF_MSGQ - 1)] =
                             {
                                 .msgq   = {0},
                                 .buffer = {0},
                             }},
        },
    .timers = {[0 ...(NUM_OF_TASKS - 1)] =
                   {
                       .state    = NULL,
                       .callback = NULL,
                       .timer    = {0},
                       .is_free  = true,
                   }},
};

/**
 * @brief Search the next timer free
 *
 * @return rart_index_t Index of the next free timer
 */
static rart_index_t search_free_timer();

/**
 * @brief Search the next free mutex
 *
 * @return rart_index_t Index of the next free mutex
 */
static rart_index_t search_free_mutex();

/**
 * @brief Search the mutex by its address
 *
 * @param mutex[in] Mutex address
 * @return rart_index_t Index of the mutex.
 */
static rart_index_t search_mutex(struct k_mutex *mutex);

/**
 * @brief Callback called when Zephyr timer expire
 *
 * @param timer_id[in] Reference of the expired timer
 */
static void default_callback(struct k_timer *timer_id);

/**
 * @brief Print a formatted string with background red
 *
 * @param format Formatted string
 * @param ... Variable arguments
 */
void print_error(const char *format, ...)
{
    printk("\x1b[41m");
    va_list va;
    va_start(va, format);
    vprintk(format, va);
    va_end(va);
    printk("\x1b[0m");
}

/**
 * @brief Print a formatted string in purple
 *
 * @param format Formatted string
 * @param ... Variable arguments
 */
void log_fn(const char *format, ...)
{
    printk("\x1b[35m");
    va_list va;
    va_start(va, format);
    vprintk(format, va);
    va_end(va);
    printk("\x1b[0m");
}

/**
 * @brief Print the filename and line in cyan
 *
 * @param file[in] The filename
 * @param line The line
 */
void trace_fn(const char *file, uint32_t line)
{
    printk("\x1b[36m%s:%d\x1b[0m\n", file, line);
}

/**
 * @brief Get the current timestamp
 *
 * @return uint32_t Current timestamp
 */
uint32_t timestamp()
{
    return k_uptime_get() / 1000;
}

/**
 * @brief Print a formatted string in red and stay on the infinite loop
 *
 * @param format Formatted string
 * @param ... Variable arguments
 */
void panic(const char *format, ...)
{
    printk("\x1b[31m");
    va_list va;
    va_start(va, format);
    vprintk(format, va);
    va_end(va);
    printk("\x1b[0m");

    while (1)
        ;
}

/**
 * @brief Get a new mutex in the list
 *
 * @return void* Zephyr mutex C reference
 */
void *rtos_mutex_new()
{
    rart_index_t idx = search_free_mutex();

    if (idx == INVALID_INDEX) {
        print_error("No mutex available\n");
        return NULL;
    }

    return &self.mutexes[idx].mutex;
}

/**
 * @brief Free a mutex used
 *
 * @param mutex[in] Zephyr mutex C reference
 */
void rtos_mutex_del(void *mutex)
{
    rart_index_t idx = search_mutex(mutex);

    if (idx == INVALID_INDEX) {
        return;
    }

    self.mutexes[idx].is_free = true;
}

/**
 * @brief Lock a Zephyr mutex
 *
 * @param mutex[in] Zephyr mutex C reference
 * @param timeout Timeout of mutex lock operation
 * @return int32_t 0 if success, errno otherwise.
 */
int32_t rtos_mutex_lock(void *mutex, uint32_t timeout)
{
    return k_mutex_lock(mutex, K_MSEC(timeout));
}

/**
 * @brief Unlock a Zephyr mutex
 *
 * @param mutex Zephyr mutex C reference
 * @return 0 if success, errno otherwise.
 */
int32_t rtos_mutex_unlock(void *mutex)
{
    return k_mutex_unlock(mutex);
}

/**
 * @brief
 *
 * @param data_size Size, in bytes, of the allocated data
 * @return void* Address of the data.
 */
void *rtos_msgq_new(size_t data_size)
{
    if (!self.msgq.is_init) {
        self.msgq.is_init = true;
        for (int i = 0; i < NUM_OF_MSGQ; ++i) {
            k_msgq_init(&self.msgq.instance[i].msgq, self.msgq.instance[i].buffer,
                        data_size, NUM_OF_MSG_ITENS);
        }
    }

    void *msgq      = &self.msgq.instance[self.msgq.index].msgq;
    self.msgq.index = (self.msgq.index >= NUM_OF_MSGQ - 1) ? 0 : (self.msgq.index + 1);

    return msgq;
}

/**
 * @brief
 *
 * @param msgq
 * @param data
 * @param timeout
 * @return
 */
int32_t rtos_msgq_send(void *msgq, const void *data, uint32_t timeout)
{
    return k_msgq_put(msgq, data, K_MSEC(timeout));
}

/**
 * @brief
 *
 * @param msgq
 * @param data_out
 * @param timeout
 * @return
 */
int32_t rtos_msgq_recv(void *msgq, void *data_out, uint32_t timeout)
{
    return k_msgq_get(msgq, data_out, K_MSEC(timeout));
}

/**
 * @brief
 */
void rtos_timer_init()
{
    for (int i = 0; i < NUM_OF_TASKS; ++i) {
        self.timers[i].is_free = true;
        k_timer_init(&self.timers[i].timer, default_callback, NULL);
    }
}

/**
 * @brief
 *
 * @param callback
 * @param state
 * @param timeout
 */
void rtos_timer_reschedule(void (*callback)(const void *), const void *state,
                           uint32_t timeout)
{
    rart_index_t idx = search_free_timer();

    if (idx == INVALID_INDEX) {
        print_error("Invalid index\n");
        while (1)
            ;
    }
    self.timers[idx].callback = callback;
    self.timers[idx].state    = state;

    k_timer_start(&self.timers[idx].timer, K_SECONDS(timeout), K_NO_WAIT);
}

/**
 * @brief
 *
 * @param timer_id
 * @return
 */
rart_index_t search_timer(struct k_timer *timer_id)
{
    for (int i = 0; i < NUM_OF_TASKS; ++i) {
        if (&self.timers[i].timer == timer_id) {
            return i;
        }
    }

    return 0xff;
}

/**
 * @brief
 *
 * @param align
 * @param bytes
 * @return
 */
const void *heap_alloc(size_t align, size_t bytes)
{
    void *ptr = k_heap_aligned_alloc(&rtos_allocator, align, bytes, K_NO_WAIT);
    if (ptr == NULL) {
        printk("Allocation error\n");
        while (1)
            ;
    }
    return ptr;
}

/**
 * @brief
 *
 * @param mem
 */
void heap_free(const void *mem)
{
    k_heap_free(&rtos_allocator, (void *) mem);
}

static rart_index_t search_free_mutex()
{
    for (int i = 0; i < NUM_OF_MUTEXES; ++i) {
        if (self.mutexes[i].is_free || !self.mutexes[i].is_init) {
            self.mutexes[i].is_free = false;
            if (!self.mutexes[i].is_init) {
                self.mutexes[i].is_init = true;
                k_mutex_init(&self.mutexes[i].mutex);
            }

            return i;
        }
    }

    return INVALID_INDEX;
}

static rart_index_t search_mutex(struct k_mutex *mutex)
{
    for (int i = 0; i < NUM_OF_MUTEXES; ++i) {
        if (&self.mutexes[i].mutex == mutex) {
            return i;
        }
    }

    return INVALID_INDEX;
}

static void default_callback(struct k_timer *timer_id)
{
    rart_index_t idx = search_timer(timer_id);

    if (idx == INVALID_INDEX) {
        print_error("Invalid index\n");
        while (1)
            ;
    }

    self.timers[idx].callback(self.timers[idx].state);
    self.timers[idx].is_free = true;
}

static rart_index_t search_free_timer()
{
    for (int i = 0; i < NUM_OF_TASKS; ++i) {
        if (self.timers[i].is_free) {
            self.timers[i].is_free = false;
            return i;
        }
    }

    return INVALID_INDEX;
}
