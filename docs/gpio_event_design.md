# GPIO CN Event Design

This note describes the small GPIO Change Notification (CN) event layer ported
from the hardware-validated `dspic33ak-hal-starter` GPIO validation branch. It
is an experiment for readable FAE/evaluation code, not a production-grade GPIO
driver and not a CMSIS Driver_GPIO implementation.

## File Split

- `src/dspic33ak_gpio.h` and `src/dspic33ak_gpio.c` remain the GPIO core. They
  handle ANSEL, TRIS, LAT, PORT, ODC, CNPU, and CNPD.
- `src/dspic33ak_gpio_event.h` and `src/dspic33ak_gpio_event.c` provide the CN
  event layer above the existing GPIO pin representation.
- A consumer application owns the board pin definitions, CN interrupt vector,
  interrupt priority, and IEC enable bit.
- The event layer is a library component; it does not bring an MPLAB X project
  file or any board-specific vector into this repository.

## API

The current event API is intentionally small:

```c
bool dspic33ak_gpio_event_attach(dspic33ak_gpio_pin_t pin,
                                 dspic33ak_gpio_event_edge_t trigger,
                                 dspic33ak_gpio_event_callback_t callback,
                                 void *user_data);

bool dspic33ak_gpio_event_detach(dspic33ak_gpio_pin_t pin);

void dspic33ak_gpio_event_process_isr(void);
```

Phase 1 accepts `DSPIC33AK_GPIO_EVENT_EDGE_EITHER` for `attach()`. The enum also
contains `FALLING` and `RISING` because callbacks report the detected edge, but
single-edge attach policy is intentionally deferred until the dsPIC33AK
`CNEN0x`/`CNEN1x` polarity mapping is finalized for the wrapper API.

## ISR Ownership

The HAL event layer does not define interrupt vectors. The application or
example owns the vector and forwards to the event dispatcher:

```c
void __attribute__((__interrupt__, __no_auto_psv__)) _CNBInterrupt(void)
{
    dspic33ak_gpio_event_process_isr();
}
```

This keeps the HAL reusable for different applications that may choose different
CN ports, priorities, nesting rules, or interrupt routing.

## CN Flag Ownership

`dspic33ak_gpio_event_process_isr()` owns cleanup for pins registered through
the event layer:

- It reads the per-port `CNFx` pending bits.
- It handles only bits registered by `dspic33ak_gpio_event_attach()`.
- It clears the handled `CNFx` bits.
- It clears the matching port interrupt flag, such as `CNBIF` through the port's
  `IFSx` register mask.

The application still owns interrupt setup. In the original SW3 validation path,
the app cleared `_CNBIF`, set `_CNBIP`, and enabled `_CNBIE` after SW3 was
configured and attached.

## Edge Detection

`attach()` captures the current `PORTx` level as `previous_high` before enabling
CN for that pin. During `dspic33ak_gpio_event_process_isr()`:

1. The dispatcher reads the current `PORTx` level.
2. Each pending registered pin compares current level with `previous_high`.
3. A high-to-low transition reports `DSPIC33AK_GPIO_EVENT_EDGE_FALLING`.
4. A low-to-high transition reports `DSPIC33AK_GPIO_EVENT_EDGE_RISING`.
5. The saved `previous_high` level is updated before the callback runs.

For the board's active-low SW3 on RB2:

- falling edge = released high to pressed low = press = LED5 ON
- rising edge = pressed low to released high = release = LED5 OFF

The SW3 callback only updates volatile state. The main loop writes LED5, so the
callback stays small and does not directly touch LED outputs.

## What Attach Does Not Do

`dspic33ak_gpio_event_attach()` does not modify:

- ANSEL
- TRIS
- CNPU/CNPD
- PPS
- interrupt priority
- interrupt enable bits

The application must configure the pin as a digital input before attaching an
event. Use `dspic33ak_gpio_config()` with an explicit config struct for a
single-call, glitch-aware setup:

## Consumer Snippet

A native consumer application should keep the interrupt vector outside the HAL:

```c
void __attribute__((__interrupt__, __no_auto_psv__)) _CNBInterrupt(void)
{
    dspic33ak_gpio_event_process_isr();
}
```

The app configures the pin first, then attaches the event:

```c
static volatile bool sw3_changed;
static volatile dspic33ak_gpio_event_edge_t sw3_edge;

static void sw3_event_cb(dspic33ak_gpio_pin_t pin,
                         dspic33ak_gpio_event_edge_t edge,
                         void *user_data)
{
    (void)pin;
    (void)user_data;
    sw3_edge = edge;
    sw3_changed = true;
}

void app_gpio_event_init(void)
{
    static const dspic33ak_gpio_config_t sw_cfg = {
        .dir          = DSPIC33AK_GPIO_DIR_INPUT,
        .pull         = DSPIC33AK_GPIO_PULL_UP,
        .analog       = false,
        .open_drain   = false,
        .initial_high = false,
    };
    (void)dspic33ak_gpio_config(BOARD_SW3, &sw_cfg);
    (void)dspic33ak_gpio_event_attach(BOARD_SW3,
                                      DSPIC33AK_GPIO_EVENT_EDGE_EITHER,
                                      sw3_event_cb,
                                      0);
}
```

## Current Validation Behavior

- SW1 remains a polling input and drives LED7.
- SW2 remains a polling input and drives LED6.
- SW3 uses the CN event layer and drives LED5.
- SW3 is active-low.
- `_CNBInterrupt()` was defined in the validation app, not in the HAL.

Expected serial banner line:

```text
 LEDs: SW1/SW2 poll LED7/LED6; SW3 CN event drives LED5.
```

## Limitations

- Phase 1 supports `EDGE_EITHER` attach only.
- There is no debounce filtering.
- There is one callback slot per port bit.
- The event layer uses a single dispatcher that scans all supported CN ports.
- It does not arbitrate shared CN port ownership with unrelated code that also
  directly modifies `CNENx`, `CNFx`, or the same port interrupt flag.
- It does not check package-level pin availability.
- It does not configure analog protection or PPS ownership.

## Future CMSIS Driver_GPIO Mapping

This layer is a useful stepping stone for a future CMSIS-Driver GPIO-like
wrapper:

- `dspic33ak_gpio_pin_t` can remain the low-level pin handle.
- `dspic33ak_gpio_config()` maps naturally to pin initialization and direction.
- `dspic33ak_gpio_write()`, `toggle()`, `read()`, and `read_output()` map to
  output and input operations.
- `dspic33ak_gpio_set_pull()` and `set_open_drain()` map to optional pin
  attributes.
- `dspic33ak_gpio_event_attach()` can become the basis for event callbacks after
  edge policy, debounce, and ownership rules are defined.

## Resolved decisions

### PPS ownership

`dspic33ak_pps.*` is a generic companion module in the GPIO HAL family. It
handles PPS register routing without board-specific knowledge. Actual
signal-to-RP assignments remain in the board layer (`board.c` /
`board_pins.h`).

### Pin representation

PPS-capable pins use RP-first addressing (`dspic33ak_gpio_rp_*`) in normal
board and application code. Non-PPS GPIO-only pins and low-level GPIO core
calls may use packed `(port, bit)` handles.

### ANSEL ownership

The caller explicitly selects analog or digital operation. The one-shot
`dspic33ak_gpio_config()` helper applies the caller-specified `analog` field;
the `config_digital_input/output()` shortcuts set `analog=false`. GPIO init
does not automatically force ANSEL — this is intentional so analog-input pins
can be configured independently.

## Open CN-event questions

The following remain open and are intentionally deferred:

- **Single-edge attach** (`FALLING` / `RISING`): deferred until the dsPIC33AK
  `CNEN0x`/`CNEN1x` polarity mapping is finalized for the wrapper API.
- **Debounce filtering**: not implemented.
- **Multiple callback slots per pin**: currently one slot per port bit.
- **Shared CN-port ownership**: the dispatcher does not arbitrate with unrelated
  code that directly modifies `CNENx`, `CNFx`, or the same port interrupt flag.
