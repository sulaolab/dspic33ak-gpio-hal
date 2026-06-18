# dspic33ak-gpio-hal

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
* Input read (PORT) and output-latch read-back (LAT)
* Optional GPIO CN event dispatch layer, validated in `dspic33ak-hal-starter`

## Design policy

This HAL is intentionally small.

* The API is plain and synchronous; one packed pin handle identifies a pin.
* No XC-DSC / DFP bitfield structures (`LATxbits` / `TRISxbits` / ...) are
  exposed in the public API.
* Device-specific register symbols are isolated in a small per-port pointer
  table. The table is built from `#if defined(LATx)` presence tests, so it
  adapts to the device without device-name conditionals.
* This HAL owns only the GPIO attribute/data registers
  (`ANSEL` / `TRIS` / `LAT` / `PORT` / `CNPU` / `CNPD` / `ODC`). PPS (peripheral
  pin select) routing belongs to the board layer.
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
* Optional Change Notification (CN) event attach/detach/dispatch
* Any port present on the device (A..H as defined by the device header)

Out of scope (not handled here):

* PPS (peripheral pin select) routing — board layer
* HAL-owned interrupt vectors
* Debounce or new event semantics beyond the validated CN event layer
* Atomic set/clear via dedicated SFRs (accessors are read-modify-write)
* RTOS locking / cross-context mutual exclusion

## Files

```text
src/
  dspic33ak_gpio.c
  dspic33ak_gpio.h
  dspic33ak_gpio_event.c
  dspic33ak_gpio_event.h
  dspic33ak_gpio_reg.h
docs/
  gpio_event_design.md
```

`dspic33ak_gpio_reg.h` is the only place that touches the raw GPIO SFRs as
32-bit pointers and bit masks; the driver body drives any port through a
per-port pointer table.

Consumer projects should compile `dspic33ak_gpio_event.c` only when CN event
support is needed.

## Pin addressing

A pin is a single packed number, `(port << 4) | bit`. Do not write the raw
number; always build it with `DSPIC33AK_GPIO_PIN(port, bit)` and give it a
board-level name:

```c
#include "dspic33ak_gpio.h"

#define BOARD_LED0      DSPIC33AK_GPIO_PIN(DSPIC33AK_GPIO_PORT_C, 8)
#define BOARD_FLASH_CS  DSPIC33AK_GPIO_PIN(DSPIC33AK_GPIO_PORT_D, 15)
```

Port codes are `DSPIC33AK_GPIO_PORT_A` .. `DSPIC33AK_GPIO_PORT_H`; `bit` is
`0..15`. `bit` must be `0..15`; values outside this range are masked by the
macro and should not be used.

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

bool pressed = (dspic33ak_gpio_read(BOARD_BUTTON) == false);  /* active-low */
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

Configuration:

* `dspic33ak_gpio_config()`        — one-shot, glitch-aware apply order
* `dspic33ak_gpio_set_direction()`
* `dspic33ak_gpio_set_pull()`
* `dspic33ak_gpio_set_analog()`
* `dspic33ak_gpio_set_open_drain()`

Output:

* `dspic33ak_gpio_write()` — drive a given level
* `dspic33ak_gpio_set()`   — drive high
* `dspic33ak_gpio_clear()` — drive low
* `dspic33ak_gpio_toggle()`

Input / read-back:

* `dspic33ak_gpio_read()`        — pin level from `PORT`
* `dspic33ak_gpio_read_output()` — driven latch from `LAT`

Optional event layer:

* `dspic33ak_gpio_event_attach()`      — register one packed-pin event callback
* `dspic33ak_gpio_event_detach()`      — unregister one packed-pin event
* `dspic33ak_gpio_event_process_isr()` — app-called CN event dispatcher

Every function takes a packed pin from `DSPIC33AK_GPIO_PIN()`. The setters
return `false` if the pin's port is not present on the device (no register row),
otherwise `true`. `dspic33ak_gpio_read()` / `read_output()` return the level, or
`false` for a pin whose port is not present.

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
* This HAL is the GPIO layer only; PPS routing belongs to the board/application
  layer.
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
