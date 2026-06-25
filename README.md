# dspic33ak-gpio-hal

> Want to run it on hardware first?
> Start with [dspic33ak-hal-starter](https://github.com/sulaolab/dspic33ak-hal-starter),
> which vendors validated snapshots of the dsPIC33AK HAL repositories and
> provides a ready-to-build MPLAB X project for the dsPIC33AK Curiosity board.

Small, readable GPIO HAL for Microchip dsPIC33AK devices.

This project is intended as a compact alternative to large generated driver code.
The goal is not to hide everything behind a framework, but to provide a simple
GPIO layer that is easy to read, test, modify, and adapt.

## Status

Current validation target:

* Device: dsPIC33AK512MPS512
* Compiler: XC-DSC v3.31.01
* DFP: Microchip dsPIC33AK-MP DFP 1.3.185 (or compatible)

The port register table is built from `#if defined(LATx)` presence tests, so it
adapts to whichever ports the selected device header defines, without
device-name conditionals.

This HAL is used in a larger board project; on the dsPIC33AK512MPS512 target it
has been validated for:

* LED output
* button input
* external SPI-flash control pins (chip-select / reset / write-protect)
* UART1 pin pre-configuration
* ADC analog input pre-configuration
* TDM / I2S peripheral pin pre-configuration

Confirmed operations on the validation target:

* Per-pin direction (input / output)
* Per-pin pull-up / pull-down / none
* Per-pin analog / digital (ANSEL) select
* Per-pin open-drain enable
* One-shot `dspic33ak_gpio_config()` with a glitch-aware apply order
* Output write / set / clear / toggle
* Input read (PORT) and output-latch read-back (LAT), 3-state level result
* RP-first addressing (RPn) over the packed-pin core
* Optional PPS peripheral-pin routing (companion `dspic33ak_pps.*`), exercised by
  the SPI / UART / TDM pin routing in the perseus board project
* Optional GPIO CN event dispatch layer, validated in `dspic33ak-hal-starter`

## Design policy

This HAL is intentionally small.

* A pin is addressed either by its Remappable-Pin number (RPn) through the
  **RP-first API** (preferred for normal board and application code) or by a
  packed `(port, bit)` handle through the **core packed-pin API** (for non-RP
  pins, low-level GPIO, or HAL internals). Both APIs reach the same GPIO
  register implementation; the RP-first functions are thin wrappers that convert
  RPn to a packed pin and delegate to the core.
* No XC-DSC / DFP bitfield structures (`LATxbits` / `TRISxbits` / ...) are
  exposed in the public API.
* Device-specific register symbols are isolated in a small per-port pointer
  table. The table is built from `#if defined(LATx)` presence tests, so it
  adapts to the device without device-name conditionals.
* The core GPIO layer (`dspic33ak_gpio.*`) owns only the GPIO attribute/data
  registers (`ANSEL` / `TRIS` / `LAT` / `PORT` / `CNPU` / `CNPD` / `ODC`). PPS
  signal routing is a separate, optional companion module (`dspic33ak_pps.*`) in
  this same family; the board layer owns the policy — which signal maps to which
  RP pin.
* The core GPIO layer does not own interrupt vectors. The optional CN event
  layer only dispatches registered GPIO events when the application calls it
  from an app-owned vector.
* The accessors are plain read-modify-write and do NOT disable interrupts; if a
  port is updated from both main-line code and an ISR, the caller provides the
  mutual exclusion.

## Scope

In scope:

* Direction, pull, analog/digital, open-drain configuration
* Output write / set / clear / toggle
* Input read and output-latch read-back
* RP-first addressing (RPn) as a thin adapter over the packed-pin core
* Optional PPS (peripheral pin select) routing — companion `dspic33ak_pps.*`
* Optional Change Notification (CN) event attach/detach/dispatch
* Any port present on the device (A..H as defined by the device header)

Out of scope (not handled here):

* HAL-owned interrupt vectors
* Debounce or new event semantics beyond the validated CN event layer
* Atomic set/clear via dedicated SFRs (accessors are read-modify-write)
* RTOS locking / cross-context mutual exclusion

## Files

```text
src/
  dspic33ak_gpio.c        core GPIO (attributes + data) + RP-first adapter
  dspic33ak_gpio.h
  dspic33ak_pps.c         optional PPS (peripheral pin select) routing
  dspic33ak_pps.h
  dspic33ak_gpio_event.c  optional Change Notification (CN) event layer
  dspic33ak_gpio_event.h
  dspic33ak_gpio_reg.h
docs/
  gpio_event_design.md
```

`dspic33ak_gpio_reg.h` is the only place that touches the raw GPIO SFRs as
32-bit pointers and bit masks; the driver body drives any port through a
per-port pointer table.

The two optional companions are compiled only when their feature is used:
compile `dspic33ak_gpio_event.c` only when CN event support is needed, and
`dspic33ak_pps.c` only when the board routes peripherals through PPS.

## Pin addressing

Two addressing styles are supported. Use RP-first for PPS-capable board and
application pins; use packed-pin for non-RP pins or low-level core usage.

**RP-first (preferred for board/application code with PPS):** a pin is
identified by its Remappable-Pin number `RPn` — the same number the PPS map
uses — so the GPIO call and the PPS route refer to the pin identically:

```c
#define BOARD_UART1_TX_RP  (114u)   /* U1TX -> RH1 */

dspic33ak_gpio_rp_config_digital_output(BOARD_UART1_TX_RP, true);
dspic33ak_pps_route_output(DSPIC33AK_PPS_OUTPUT_U1TX, BOARD_UART1_TX_RP);
```

**Packed-pin (core API — for non-RP pins or HAL internals):** a pin is a
packed number `(port << 4) | bit`. Always build it with
`DSPIC33AK_GPIO_PIN(port, bit)` and give it a board-level name:

```c
#include "dspic33ak_gpio.h"

#define BOARD_LED0      DSPIC33AK_GPIO_PIN(DSPIC33AK_GPIO_PORT_C, 8)
#define BOARD_FLASH_CS  DSPIC33AK_GPIO_PIN(DSPIC33AK_GPIO_PORT_D, 15)
```

Port codes are `DSPIC33AK_GPIO_PORT_A` .. `DSPIC33AK_GPIO_PORT_H`; `bit` is
`0..15`. Values outside this range are masked by the macro and should not be
used.

## Basic usage

One-shot configuration (recommended) applies attributes in a glitch-aware order
(analog/digital → pull → open-drain → initial output level → direction):

```c
const dspic33ak_gpio_config_t led_cfg = {
    .dir          = DSPIC33AK_GPIO_DIR_OUTPUT,
    .pull         = DSPIC33AK_GPIO_PULL_NONE,
    .analog       = false,
    .open_drain   = false,
    .initial_high = false,   /* LAT seeded before the pin becomes an output */
};
(void)dspic33ak_gpio_config(BOARD_LED0, &led_cfg);

dspic33ak_gpio_set(BOARD_LED0);      /* drive high            */
dspic33ak_gpio_clear(BOARD_LED0);    /* drive low             */
dspic33ak_gpio_toggle(BOARD_LED0);   /* flip the output latch */
```

Reading an input:

```c
const dspic33ak_gpio_config_t btn_cfg = {
    .dir = DSPIC33AK_GPIO_DIR_INPUT, .pull = DSPIC33AK_GPIO_PULL_UP,
    .analog = false, .open_drain = false, .initial_high = false,
};
(void)dspic33ak_gpio_config(BOARD_BUTTON, &btn_cfg);

/* read() returns a 3-state level (ERROR / LOW / HIGH) -- not a bool. Handle
 * DSPIC33AK_GPIO_LEVEL_ERROR first, then compare against LOW / HIGH. */
bool pressed = (dspic33ak_gpio_read(BOARD_BUTTON) == DSPIC33AK_GPIO_LEVEL_LOW);  /* active-low */
```

Individual attribute setters are also available when one-shot config is not
wanted:

```c
(void)dspic33ak_gpio_set_analog(BOARD_LED0, false);
(void)dspic33ak_gpio_set_pull(BOARD_LED0, DSPIC33AK_GPIO_PULL_NONE);
(void)dspic33ak_gpio_set_open_drain(BOARD_LED0, false);
(void)dspic33ak_gpio_set_direction(BOARD_LED0, DSPIC33AK_GPIO_DIR_OUTPUT);
```

GPIO CN event usage keeps the vector in the application:

```c
static volatile bool sw3_changed;

static void sw3_event_cb(dspic33ak_gpio_pin_t pin,
                         dspic33ak_gpio_event_edge_t edge,
                         void *user_data)
{
    (void)pin;
    (void)edge;
    (void)user_data;
    sw3_changed = true;
}

void __attribute__((__interrupt__, __no_auto_psv__)) _CNBInterrupt(void)
{
    dspic33ak_gpio_event_process_isr();
}
```

## API summary

### Packed-pin core API

Configuration (glitch-aware apply order: analog → pull → open-drain → LAT → direction):

* `dspic33ak_gpio_config()` — one-shot via config struct
* `dspic33ak_gpio_config_digital_input()` — shortcut: digital input, no pull
* `dspic33ak_gpio_config_digital_output()` — shortcut: digital output, seeds LAT first
* `dspic33ak_gpio_set_direction()`
* `dspic33ak_gpio_set_pull()`
* `dspic33ak_gpio_set_analog()`
* `dspic33ak_gpio_set_open_drain()`

Output:

* `dspic33ak_gpio_write()` — drive a given level
* `dspic33ak_gpio_set()`   — drive high
* `dspic33ak_gpio_clear()` — drive low
* `dspic33ak_gpio_toggle()`

Input / read-back (return a 3-state `dspic33ak_gpio_level_t`, **not a bool**):

* `dspic33ak_gpio_read()`        — pin level from `PORT`
* `dspic33ak_gpio_read_output()` — driven latch from `LAT`

### RP-first API

Thin wrappers over the packed-pin core (convert RPn → packed pin, then call
the matching core function). Preferred for board and application code that uses
PPS-capable pins. Do not duplicate register-access logic.

Conversion:

* `dspic33ak_gpio_pin_from_rp()`
* `dspic33ak_gpio_rp_from_pin()`

Full configuration (via config struct):

* `dspic33ak_gpio_rp_config()`

Individual attribute setters:

* `dspic33ak_gpio_rp_set_direction()`
* `dspic33ak_gpio_rp_set_pull()`
* `dspic33ak_gpio_rp_set_analog()`
* `dspic33ak_gpio_rp_set_open_drain()`

Digital shortcuts (most common cases):

* `dspic33ak_gpio_rp_config_digital_input()`
* `dspic33ak_gpio_rp_config_digital_output()`

Output operations:

* `dspic33ak_gpio_rp_write()`
* `dspic33ak_gpio_rp_set()`
* `dspic33ak_gpio_rp_clear()`
* `dspic33ak_gpio_rp_toggle()`

Read operations (3-state `dspic33ak_gpio_level_t`, **not a bool**):

* `dspic33ak_gpio_rp_read()`
* `dspic33ak_gpio_rp_read_output()`

Optional PPS routing (`dspic33ak_pps.*`, compiled only when used):

* `dspic33ak_pps_route_output()` — drive a peripheral output onto an RP pin
* `dspic33ak_pps_route_input()`  — feed a peripheral input from an RP pin
* `dspic33ak_pps_unlock()` / `_lock()` — IOLOCK gate for direct PPS writers

Optional event layer:

* `dspic33ak_gpio_event_attach()`      — register one packed-pin event callback
* `dspic33ak_gpio_event_detach()`      — unregister one packed-pin event
* `dspic33ak_gpio_event_process_isr()` — app-called CN event dispatcher

The setters take a packed pin from `DSPIC33AK_GPIO_PIN()` (or use the RP-first
wrappers) and return `false` if the pin's port is not present on the device (no
register row), otherwise `true`. `dspic33ak_gpio_read()` / `read_output()`
return a 3-state `dspic33ak_gpio_level_t` — `DSPIC33AK_GPIO_LEVEL_ERROR` (`-1`)
for a pin whose port is not present, else `..._LOW` (`0`) / `..._HIGH` (`1`).
Do not use the result directly as a bool (ERROR is truthy); compare against the
named constants and handle ERROR first.

## Glitch-aware order

`dspic33ak_gpio_config()` applies an output pin as: select digital, set pull and
open-drain, seed the output latch (`initial_high`), and only then switch the pin
to an output. Seeding `LAT` before flipping `TRIS` avoids driving an undefined
level for one cycle when a pin first becomes an output.

## Device mapping

The per-port register table in `dspic33ak_gpio.c` is the only place that
references `LATA` / `TRISA` / `PORTA` / ... Each row is emitted only when the
device header defines that port's SFRs, so the table tracks the silicon
automatically and the driver body stays device-neutral.

## Notes

* The GPIO SFRs are 32-bit on the dsPIC33A core; the register layer uses 32-bit
  pointers to match the DFP device headers.
* The core `dspic33ak_gpio.*` layer drives GPIO attributes/data only; PPS signal
  routing lives in the optional companion `dspic33ak_pps.*`. The board layer
  still owns the policy — which signal maps to which RP pin.
* The optional event layer does not change ANSEL, TRIS, CNPU/CNPD, PPS,
  interrupt priority, or IEC enable bits in `dspic33ak_gpio_event_attach()`.
* CMSIS-Driver GPIO-style wrappers are intentionally kept in separate
  repositories, such as `dspic33ak-gpio-cmsis-driver`.
* This HAL checks whether a GPIO port exists in the selected device header. It
  does not check whether a specific package exposes that pin. Package-level and
  board-level pin validity must be handled by the board layer.
* This repository does not include Microchip DFP header files.

## License

MIT No Attribution License (MIT-0). See [LICENSE](LICENSE).

Attribution is appreciated but not required.
