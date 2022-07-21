/**
 * @file zbus_backend.c
 * @author Matheus T. dos Santos (matheus.santos@edge.ufal.br)
 * @brief Backend of RART for ZBUS library
 * @version 0.1
 * @date 20/07/2022
 *
 * @copyright Copyright (c) 2022
 *
 */
#include <stdint.h>
#include <zbus.h>

#include "zbus-backend-defines.h"

/**
 * @brief Invalid Zbus Channel ID
 */
#define INVALID_ID ((uint32_t) -1)

/**
 * @brief Invalid index
 */
#define INVALID_INDEX ((zbus_backend_index_t) -1)

/**
 * @brief Type of user read callback called inside default zbus read callback.
 *
 * @param state State of the user read callback. This state is required by RART-rs.
 * @param data Data got by ZBUS read operation.
 * @param data_len Data length of ZBUS data read.
 */
typedef void (*zbus_backend_callback_t)(void *state, void *data, uint32_t data_len);

/**
 * @brief Type of the index used in zbus entry list
 */
typedef uint8_t zbus_backend_index_t;

/**
 * @brief TODO
 */
typedef struct {
    void *state; /**< TODO */
    zbus_backend_callback_t callback; /**< TODO */
    uint32_t id; /**< TODO */
    bool is_free; /**< TODO */
} zbus_backend_entry_t;

/**
 * @brief TODO
 */
static zbus_backend_entry_t entry_list[NUM_OF_OBSERVERS] = {[0 ... (NUM_OF_OBSERVERS - 1)] = {
        .state = NULL,
        .callback = NULL,
        .id = INVALID_ID,
        .is_free = true,
}};

/**
 * @brief TODO
 *
 * @return zbus_backend_index_t
 */
static zbus_backend_index_t search_free_entry();

/**
 * @brief Print a formatted string with background red
 *
 * @param format Formatted string
 * @param ... Variable arguments
 */
static void print_error(const char *format, ...);

/**
 * @brief TODO
 *
 * @param id
 * @param state
 * @param callback
 */
void rtos_zbus_register_observer(uint32_t id, const void *state, zbus_backend_callback_t callback) {
    zbus_backend_index_t idx = search_free_entry();

    if (idx == INVALID_INDEX) {
        print_error("Invalid index\n");
        while (1);
    }

    entry_list[idx].id = id;
    entry_list[idx].callback = callback;
    entry_list[idx].state = (void *) state;
    entry_list[idx].is_free = false;
}

/**
 * @brief TODO
 *
 * @param id
 * @param data
 * @param size
 * @return
 */
int32_t rtos_zbus_publish(uint32_t id, const void *data, uint32_t size) {
    return zbus_chan_pub(zbus_chan_get_by_index(id), (void *) data, size, K_NO_WAIT, false);
}

/**
 * @brief TODO
 *
 * @param idx
 */
void rtos_zbus_default_listener_callback(uint32_t idx) {
    zbus_message_variant_t msg_data;
    struct zbus_channel *channel = zbus_chan_get_by_index(idx);

    zbus_chan_read(channel, (uint8_t *) &msg_data, channel->message_size, K_NO_WAIT);

    for (int i = 0; i < NUM_OF_OBSERVERS; ++i) {
        if (entry_list[i].is_free) {
            continue;
        }

        if (entry_list[i].id == idx) {
            entry_list[i].callback(entry_list[i].state, &msg_data, channel->message_size);
            entry_list[i].is_free = true;
        }
    }
}

static zbus_backend_index_t search_free_entry() {
    for (int i = 0; i < NUM_OF_OBSERVERS; ++i) {
        if (entry_list[i].is_free) {
            return i;
        }
    }

    return INVALID_INDEX;
}

static void print_error(const char *format, ...) {
    printk("\x1b[41m");
    va_list va;
    va_start(va, format);
    vprintk(format, va);
    va_end(va);
    printk("\x1b[0m");
}
