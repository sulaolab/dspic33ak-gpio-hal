#ifndef DSPIC33AK_GPIO_EVENT_H
#define DSPIC33AK_GPIO_EVENT_H

/*
 * dspic33ak_gpio_event.h
 * ----------------------
 * Small Change Notification (CN) event layer for the GPIO HAL.
 *
 * This is intentionally separate from the core src/dspic33ak_gpio.* files:
 *   - event_attach() only configures CN edge detection state
 *   - it does not change PPS
 *   - it does not change ANSEL, TRIS, CNPU, or CNPD
 *   - the application owns interrupt vectors; this layer can arm the matching
 *     CN port interrupt priority / flag / enable bits when requested
 *   - dspic33ak_gpio_event_process_isr() clears the handled per-pin CNF bits
 *     and the matching port interrupt flag
 *
 * A typical app vector forwards to dspic33ak_gpio_event_process_isr().
 */

#include <stdbool.h>
#include <stdint.h>

#include "dspic33ak_gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    DSPIC33AK_GPIO_EVENT_EDGE_NONE = 0,
    DSPIC33AK_GPIO_EVENT_EDGE_FALLING,
    DSPIC33AK_GPIO_EVENT_EDGE_RISING,
    DSPIC33AK_GPIO_EVENT_EDGE_EITHER
} dspic33ak_gpio_event_edge_t;

/* Phase 1 attach supports EDGE_EITHER. attach() captures the current PORT
 * level, and the callback receives FALLING/RISING after the ISR compares the
 * captured level with the new PORT level. */
typedef void (*dspic33ak_gpio_event_callback_t)(dspic33ak_gpio_pin_t pin,
                                                dspic33ak_gpio_event_edge_t edge,
                                                void *user_data);

bool dspic33ak_gpio_event_attach(dspic33ak_gpio_pin_t pin,
                                 dspic33ak_gpio_event_edge_t trigger,
                                 dspic33ak_gpio_event_callback_t callback,
                                 void *user_data);
bool dspic33ak_gpio_event_detach(dspic33ak_gpio_pin_t pin);

/* Optional CPU interrupt helpers. The actual vector remains application-owned,
 * but these helpers hide the scattered CNxIP/CNxIF/CNxIE symbols. The selected
 * pin identifies the CN port interrupt line to arm.
 *
 * irq_enable()/irq_disable() are setup/teardown helpers and clear the CN port
 * flag. irq_is_enabled()/irq_set_enabled() only read/write the IEC bit, for a
 * brief application critical section that must not clear a pending CN event. */
bool dspic33ak_gpio_event_irq_enable(dspic33ak_gpio_pin_t pin, uint8_t priority);
bool dspic33ak_gpio_event_irq_disable(dspic33ak_gpio_pin_t pin);
bool dspic33ak_gpio_event_irq_is_enabled(dspic33ak_gpio_pin_t pin, bool *enabled);
bool dspic33ak_gpio_event_irq_set_enabled(dspic33ak_gpio_pin_t pin, bool enable);

/* RP-first wrappers for event users whose board pins are named by RP number. */
bool dspic33ak_gpio_event_rp_attach(dspic33ak_gpio_rp_t rp,
                                    dspic33ak_gpio_event_edge_t trigger,
                                    dspic33ak_gpio_event_callback_t callback,
                                    void *user_data);
bool dspic33ak_gpio_event_rp_detach(dspic33ak_gpio_rp_t rp);
bool dspic33ak_gpio_event_rp_irq_enable(dspic33ak_gpio_rp_t rp, uint8_t priority);
bool dspic33ak_gpio_event_rp_irq_disable(dspic33ak_gpio_rp_t rp);
bool dspic33ak_gpio_event_rp_irq_is_enabled(dspic33ak_gpio_rp_t rp, bool *enabled);
bool dspic33ak_gpio_event_rp_irq_set_enabled(dspic33ak_gpio_rp_t rp, bool enable);

void dspic33ak_gpio_event_process_isr(void);

#ifdef __cplusplus
}
#endif

#endif /* DSPIC33AK_GPIO_EVENT_H */
