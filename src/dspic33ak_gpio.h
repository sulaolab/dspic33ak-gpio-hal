#ifndef DSPIC33AK_GPIO_H
#define DSPIC33AK_GPIO_H

/*
 * dspic33ak_gpio.h
 * ----------------
 * Small, readable GPIO HAL for dsPIC33AK devices.
 *
 * API orientation (two layers, both declared in this header):
 *   1. RP-first API (PREFERRED for normal dsPIC33AK board / application code) --
 *      address a pin by its Remappable-Pin number, the same RPn used by the PPS
 *      map (see the RP-first section near the bottom), e.g.:
 *          dspic33ak_gpio_rp_config_digital_output(101u, false); // RP101
 *          dspic33ak_gpio_rp_set(101u);
 *   2. Packed-pin core API -- the core HAL and the advanced/fixed-function
 *      interface. The RP API is a thin adapter over it. Use the packed-pin API
 *      for non-RP pins, special attribute combinations, or HAL internals.
 *
 * Scope (intentionally small):
 *   - per-pin direction (input / output)
 *   - per-pin pull-up / pull-down / none
 *   - per-pin analog / digital select (ANSEL)
 *   - per-pin open-drain enable
 *   - simple one-call digital input / output configuration
 *   - output write / set / clear / toggle; input read (3-state level result)
 *
 * Out of scope (each a separate, companion concern in the hal_gpio family):
 *   - change-notification (CN) / edge interrupts   (dspic33ak_gpio_event.*)
 *   - PPS (peripheral pin select) routing          (dspic33ak_pps.*)
 *
 * Packed pin addressing:
 *   A pin is a single packed number: (port << 4) | bit. Do NOT write the raw
 *   number; build it with DSPIC33AK_GPIO_PIN(port, bit). Prefer an RP number
 *   (RP-first API) when the pin has one; use a packed pin for non-RP pins.
 *
 * Interrupt safety:
 *   The accessors are plain read-modify-write and do NOT disable interrupts. If
 *   the same LAT port is updated from both main-line code and an ISR, the caller
 *   must provide the mutual exclusion.
 *
 * The register table adapts to the device automatically: a port row is emitted
 * only when the device header defines that port's SFRs.
 *
 * Limitations (all layers): this HAL configures the GPIO electrical attributes
 * only. It does NOT validate (a) that the device implements the pin, (b) that the
 * selected package bonds it out, or (c) that the board exposes it -- those are
 * device / package / board concerns.
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

/*
 * Extract the port code / bit from a packed pin. Inverse of DSPIC33AK_GPIO_PIN().
 *   DSPIC33AK_GPIO_PIN_PORT(pin) -> DSPIC33AK_GPIO_PORT_* value (0..7)
 *   DSPIC33AK_GPIO_PIN_BIT(pin)  -> 0..15
 */
#define DSPIC33AK_GPIO_PIN_PORT(pin) ((uint8_t)(((uint16_t)(pin) >> 4) & 0x07u))
#define DSPIC33AK_GPIO_PIN_BIT(pin)  ((uint8_t)((uint16_t)(pin) & 0x0Fu))

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
 * Pin level result for the read functions. A small SIGNED type so a read can
 * report an error distinctly from a valid Low/High -- the result is NOT a plain
 * bool. Callers must check for the error value before treating it as a level.
 */
typedef int8_t dspic33ak_gpio_level_t;

#define DSPIC33AK_GPIO_LEVEL_ERROR ((dspic33ak_gpio_level_t)-1)   /* invalid / unavailable GPIO */
#define DSPIC33AK_GPIO_LEVEL_LOW   ((dspic33ak_gpio_level_t)0)    /* pin / latch is Low         */
#define DSPIC33AK_GPIO_LEVEL_HIGH  ((dspic33ak_gpio_level_t)1)    /* pin / latch is High        */

/*
 * All functions take a packed pin from DSPIC33AK_GPIO_PIN(). Setter / config
 * functions return bool (false if the pin's port is not present; otherwise true).
 * The read functions return a 3-state dspic33ak_gpio_level_t (see below), NOT a
 * bool -- DSPIC33AK_GPIO_LEVEL_ERROR for a pin whose port is not present.
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

/*
 * Simple one-call configuration for the common cases, so callers do not build a
 * config struct. Both go through dspic33ak_gpio_config() (no duplicated register
 * logic).
 *   digital_input : direction=input,  analog=off, pull=none, open-drain=off.
 *   digital_output: direction=output, analog=off, pull=none, open-drain=off,
 *                   LAT seeded to initial_high BEFORE the pin is driven (the
 *                   glitch-aware order: attributes -> LAT -> TRIS=output).
 * Return false only when the pin's port is not present on the device.
 */
bool dspic33ak_gpio_config_digital_input(dspic33ak_gpio_pin_t pin);
bool dspic33ak_gpio_config_digital_output(dspic33ak_gpio_pin_t pin, bool initial_high);

bool dspic33ak_gpio_write(dspic33ak_gpio_pin_t pin, bool high);
bool dspic33ak_gpio_set(dspic33ak_gpio_pin_t pin);
bool dspic33ak_gpio_clear(dspic33ak_gpio_pin_t pin);
bool dspic33ak_gpio_toggle(dspic33ak_gpio_pin_t pin);

/*
 * dspic33ak_gpio_read       : read the pin's actual level   (PORT register)
 * dspic33ak_gpio_read_output: read the driven output latch  (LAT register)
 * read() reflects the physical pin; read_output() reflects the last value
 * written, regardless of the pin's electrical state.
 *
 * Return a 3-state dspic33ak_gpio_level_t -- NOT a bool:
 *   DSPIC33AK_GPIO_LEVEL_ERROR (-1) : port not present / invalid pin
 *   DSPIC33AK_GPIO_LEVEL_LOW   ( 0) : Low
 *   DSPIC33AK_GPIO_LEVEL_HIGH  ( 1) : High
 * Do NOT use the result directly as a boolean (ERROR is non-zero, i.e. truthy);
 * compare against the named constants and handle ERROR first.
 */
dspic33ak_gpio_level_t dspic33ak_gpio_read(dspic33ak_gpio_pin_t pin);
dspic33ak_gpio_level_t dspic33ak_gpio_read_output(dspic33ak_gpio_pin_t pin);


/*===========================================================================
 * RP-first API -- PREFERRED for normal board / application code.
 *
 * Address a pin by its Remappable-Pin number (the same RPn the PPS map uses).
 * Encoding is a flat rule, NO table: RPn = packed_pin + 1 = port*16 + bit + 1,
 * 1-based (0 = "no RP"). The functions below are thin adapters over the
 * packed-pin API above (convert RPn -> packed, then call it) and share its
 * return contract. This is the ENCODING/range layer only -- it does NOT check
 * package bonding or board availability (a per-device bonded-pin table can be
 * generated from the ATDF; see tools/atdf_rp_table).
 *===========================================================================*/

/* Remappable-pin number. RPn is 1-based; 0 means "no RP". */
typedef uint8_t dspic33ak_gpio_rp_t;

/* 8 ports x 16 = 128 encodable RP numbers. */
#define DSPIC33AK_GPIO_RP_MAX  (128u)

/*
 * Validated RPn <-> packed-pin conversion. Return false (leaving *out untouched)
 * for a NULL pointer or an out-of-range argument: rp == 0 / rp > RP_MAX for
 * pin_from_rp; pin >= RP_MAX for rp_from_pin. Range only (see note above).
 */
bool dspic33ak_gpio_pin_from_rp(uint8_t rp, dspic33ak_gpio_pin_t *pin);
bool dspic33ak_gpio_rp_from_pin(dspic33ak_gpio_pin_t pin, uint8_t *rp);

/*
 * Same semantics as the like-named packed-pin functions above. config_* set
 * analog off / no pull / no open-drain (output seeds LAT to initial_high first);
 * read / read_output return the 3-state dspic33ak_gpio_level_t -- handle
 * DSPIC33AK_GPIO_LEVEL_ERROR, do NOT use as a bool. All return false on an RP
 * conversion failure or the underlying GPIO failure.
 */
bool dspic33ak_gpio_rp_config_digital_input(dspic33ak_gpio_rp_t rp);
bool dspic33ak_gpio_rp_config_digital_output(dspic33ak_gpio_rp_t rp, bool initial_high);

bool dspic33ak_gpio_rp_set(dspic33ak_gpio_rp_t rp);
bool dspic33ak_gpio_rp_clear(dspic33ak_gpio_rp_t rp);
bool dspic33ak_gpio_rp_toggle(dspic33ak_gpio_rp_t rp);
bool dspic33ak_gpio_rp_write(dspic33ak_gpio_rp_t rp, bool high);

dspic33ak_gpio_level_t dspic33ak_gpio_rp_read(dspic33ak_gpio_rp_t rp);
dspic33ak_gpio_level_t dspic33ak_gpio_rp_read_output(dspic33ak_gpio_rp_t rp);

#ifdef __cplusplus
}
#endif

#endif /* DSPIC33AK_GPIO_H */
