#ifndef DSPIC33AK_GPIO_H
#define DSPIC33AK_GPIO_H

/*
 * dspic33ak_gpio.h
 * ----------------
 * Small, readable GPIO HAL for dsPIC33AK devices.
 *
 * Scope (intentionally small):
 *   - per-pin direction (input / output)
 *   - per-pin pull-up / pull-down / none
 *   - per-pin analog / digital select (ANSEL)
 *   - per-pin open-drain enable
 *   - output write / set / clear / toggle, input read
 *
 * Out of scope:
 *   - change-notification (CN) / edge interrupts
 *   - PPS (peripheral pin select) routing          (separate concern)
 *
 * Pin addressing:
 *   A pin is a single packed number: (port << 4) | bit. Do NOT write the raw
 *   number; always build it with DSPIC33AK_GPIO_PIN(port, bit) and give it a
 *   board-level name, e.g.
 *
 *       #define BOARD_LED0   DSPIC33AK_GPIO_PIN(DSPIC33AK_GPIO_PORT_C, 8)
 *       #define BOARD_FLASH_CS DSPIC33AK_GPIO_PIN(DSPIC33AK_GPIO_PORT_D, 15)
 *
 *       dspic33ak_gpio_set_direction(BOARD_LED0, DSPIC33AK_GPIO_DIR_OUTPUT);
 *       dspic33ak_gpio_write(BOARD_LED0, true);
 *
 *   The (port, bit) pair is only used through the macro; there is no public
 *   two-argument API.
 *
 * Interrupt safety:
 *   The accessors are plain read-modify-write and do NOT disable interrupts by
 *   default (same model as the previous GPIO_* macros). If the same LAT port is
 *   updated from both main-line code and an ISR, the caller must provide the
 *   mutual exclusion.
 *
 * The register table adapts to the device automatically: a port row is emitted
 * only when the device header defines that port's SFRs.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Port codes. Values are the high nibble of a packed pin number. */
typedef enum
{
    DSPIC33AK_GPIO_PORT_A = 0,
    DSPIC33AK_GPIO_PORT_B = 1,
    DSPIC33AK_GPIO_PORT_C = 2,
    DSPIC33AK_GPIO_PORT_D = 3,
    DSPIC33AK_GPIO_PORT_E = 4,
    DSPIC33AK_GPIO_PORT_F = 5,
    DSPIC33AK_GPIO_PORT_G = 6,
    DSPIC33AK_GPIO_PORT_H = 7
} dspic33ak_gpio_port_t;

/* Packed pin handle: (port << 4) | bit. Build only via DSPIC33AK_GPIO_PIN(). */
typedef uint16_t dspic33ak_gpio_pin_t;

/*
 * DSPIC33AK_GPIO_PIN(port, bit)
 * - port : a DSPIC33AK_GPIO_PORT_* code
 * - bit  : 0..15
 * bit must be 0..15. Values outside this range are masked by the macro and
 * should not be used.
 * Use this (and a board-level name) instead of a raw pin number.
 */
#define DSPIC33AK_GPIO_PIN(port, bit) \
    ((dspic33ak_gpio_pin_t)((((uint16_t)(port)) << 4) | ((uint16_t)(bit) & 0x0Fu)))

typedef enum
{
    DSPIC33AK_GPIO_DIR_INPUT  = 0,
    DSPIC33AK_GPIO_DIR_OUTPUT = 1
} dspic33ak_gpio_dir_t;

typedef enum
{
    DSPIC33AK_GPIO_PULL_NONE = 0,
    DSPIC33AK_GPIO_PULL_UP,
    DSPIC33AK_GPIO_PULL_DOWN
} dspic33ak_gpio_pull_t;

/*
 * Optional one-shot configuration. Applied in a glitch-aware order:
 *   analog/digital -> pull -> open-drain -> initial output level -> direction.
 */
typedef struct
{
    dspic33ak_gpio_dir_t  dir;          /* input or output                    */
    dspic33ak_gpio_pull_t pull;         /* none / up / down                   */
    bool                  analog;       /* true = analog (ANSEL=1)            */
    bool                  open_drain;   /* true = open-drain output           */
    bool                  initial_high; /* output only: level set before dir  */
} dspic33ak_gpio_config_t;

/*
 * All functions take a packed pin from DSPIC33AK_GPIO_PIN(). Setters return
 * false if the pin's port is not present on the device (no register row);
 * otherwise true. dspic33ak_gpio_read() returns the pin level, or false for a
 * pin whose port is not present.
 *
 * Return-value contract:
 *   - The setter / config functions return false only when the port is not
 *     present in the selected device header, or (for dspic33ak_gpio_config())
 *     when config is NULL.
 *   - false does NOT indicate an electrical pin fault; it means "this device
 *     header has no register for that port".
 *   - This HAL does not check whether a given package actually exposes the pin.
 *     Package-level and board-level pin validity are the board layer's
 *     responsibility.
 */
bool dspic33ak_gpio_set_direction(dspic33ak_gpio_pin_t pin, dspic33ak_gpio_dir_t dir);
bool dspic33ak_gpio_set_pull(dspic33ak_gpio_pin_t pin, dspic33ak_gpio_pull_t pull);
bool dspic33ak_gpio_set_analog(dspic33ak_gpio_pin_t pin, bool analog);
bool dspic33ak_gpio_set_open_drain(dspic33ak_gpio_pin_t pin, bool enable);
bool dspic33ak_gpio_config(dspic33ak_gpio_pin_t pin, const dspic33ak_gpio_config_t *config);

bool dspic33ak_gpio_write(dspic33ak_gpio_pin_t pin, bool high);
bool dspic33ak_gpio_set(dspic33ak_gpio_pin_t pin);
bool dspic33ak_gpio_clear(dspic33ak_gpio_pin_t pin);
bool dspic33ak_gpio_toggle(dspic33ak_gpio_pin_t pin);

/*
 * dspic33ak_gpio_read       : read the pin's actual level   (PORT register)
 * dspic33ak_gpio_read_output: read the driven output latch  (LAT register)
 * read() reflects the physical pin; read_output() reflects the last value
 * written, regardless of the pin's electrical state. Both return false for a
 * pin whose port is not present.
 */
bool dspic33ak_gpio_read(dspic33ak_gpio_pin_t pin);
bool dspic33ak_gpio_read_output(dspic33ak_gpio_pin_t pin);

#ifdef __cplusplus
}
#endif

#endif /* DSPIC33AK_GPIO_H */
