#ifndef DSPIC33AK_PPS_H
#define DSPIC33AK_PPS_H

/*
 * dspic33ak_pps.h
 * ---------------
 * Peripheral Pin Select (PPS) routing for dsPIC33AK -- the companion to the
 * GPIO HAL (same hal_gpio family). hal_gpio owns the pin's ELECTRICAL attributes
 * (TRIS/LAT/ANSEL/pull/OD); this module owns the SIGNAL ROUTING (which RP pin a
 * peripheral input reads from / a peripheral output drives), i.e. the RPINRx
 * (input-select) and RPORx (output _RPnnR) registers.
 *
 * Why a thin PPS layer:
 *   Board code otherwise writes device SFRs directly -- e.g.
 *       _RP90R = 29;        // SCK2 out on RP90 (what is 29?)
 *       _SS2R  = 29;        // SS2 in from RP29
 *   which leaks the RPORx/RPINRx map and the raw function codes into the board.
 *   These two calls hide that:
 *       dspic33ak_pps_route_output(DSPIC33AK_PPS_OUTPUT_SCK2, 90u);
 *       dspic33ak_pps_route_input (DSPIC33AK_PPS_INPUT_SS2,   29u);
 *   leaving only "which signal" + "which RP" at the call site.
 *
 * Device adaptation: the implementation keys off the XC SFR/constant macros
 * (_RPOUT_xxx, _RPnnR, the input-select registers) with #ifdef, so a peripheral
 * or RP that the selected device does not define is simply unroutable (the call
 * returns false) -- no per-device part-number conditionals here.
 *
 * Coverage: both route_* accept any PHYSICAL remappable pin the selected device
 * defines (RP1..RP128, #ifdef-guarded on _RPnnR). RP is a dspic33ak_gpio_rp_t
 * (physical GPIO RP, max DSPIC33AK_GPIO_RP_MAX=128); the RPV virtual outputs
 * (RP129..RP144) are intentionally NOT routable here -- if they are ever needed,
 * add a separate virtual-output API rather than overloading this GPIO-typed one.
 * route_input() rejects an rp that is not a physical pin on this device (returns
 * false before writing). The peripheral-SIGNAL enums contain the signals currently
 * required by this codebase (U1TX/RX, SPI1/2/4, CLC1-3, PWM1H/2H/3H,
 * PWM5H/5L-8H/8L, REFI1, CAN1TX/RX); they do NOT represent every PPS-capable
 * peripheral the device supports. Add a new signal by extending the enum and the
 * matching case in dspic33ak_pps.c. Routing a signal absent on the selected
 * device returns false.
 *
 * Pairing with hal_gpio: PPS routing does NOT configure the pin's direction or
 * analog/digital select. Configure the pin first (dspic33ak_gpio_rp_config_*),
 * then route the signal. Order is glitch-aware for outputs: seed the GPIO output
 * (LAT/TRIS) before the PPS output starts driving it.
 *
 * IOLOCK: each route_* call brackets its own write with unlock()/lock() (IOLOCK
 * resets to 0 = unlocked, so this only adds protection). unlock()/lock() are also
 * exposed for code that writes PPS SFRs directly (e.g. the PWM driver) or wants a
 * single window around a batch of direct writes.
 */

#include <stdbool.h>

#include "dspic33ak_gpio.h"   /* dspic33ak_gpio_rp_t */

#ifdef __cplusplus
extern "C" {
#endif

/*
 * PPS OUTPUT functions (a peripheral output driven onto an RP pin). Names mirror
 * the XC _RPOUT_* output-code set; the implementation maps each to its _RPOUT_x
 * value, so AK128/AK512 code differences are handled by the device header.
 */
typedef enum
{
    DSPIC33AK_PPS_OUTPUT_U1TX,

    DSPIC33AK_PPS_OUTPUT_SS1,
    DSPIC33AK_PPS_OUTPUT_SCK1,
    DSPIC33AK_PPS_OUTPUT_SDO1,

    DSPIC33AK_PPS_OUTPUT_SS2,
    DSPIC33AK_PPS_OUTPUT_SCK2,
    DSPIC33AK_PPS_OUTPUT_SDO2,

    DSPIC33AK_PPS_OUTPUT_SCK4,
    DSPIC33AK_PPS_OUTPUT_SDO4,

    DSPIC33AK_PPS_OUTPUT_CLC1,
    DSPIC33AK_PPS_OUTPUT_CLC2,
    DSPIC33AK_PPS_OUTPUT_CLC3,

    DSPIC33AK_PPS_OUTPUT_PWM1H,
    DSPIC33AK_PPS_OUTPUT_PWM2H,
    DSPIC33AK_PPS_OUTPUT_PWM3H,
    DSPIC33AK_PPS_OUTPUT_PWM5H,
    DSPIC33AK_PPS_OUTPUT_PWM5L,
    DSPIC33AK_PPS_OUTPUT_PWM6H,
    DSPIC33AK_PPS_OUTPUT_PWM6L,
    DSPIC33AK_PPS_OUTPUT_PWM7H,
    DSPIC33AK_PPS_OUTPUT_PWM7L,
    DSPIC33AK_PPS_OUTPUT_PWM8H,
    DSPIC33AK_PPS_OUTPUT_PWM8L,

    DSPIC33AK_PPS_OUTPUT_CAN1TX
} dspic33ak_pps_output_t;

/*
 * PPS INPUT functions (a peripheral input fed from an RP pin). Each maps to its
 * RPINRx input-select register (a bit-field alias; assignment takes the RP
 * number directly).
 */
typedef enum
{
    DSPIC33AK_PPS_INPUT_U1RX,

    DSPIC33AK_PPS_INPUT_SS1,
    DSPIC33AK_PPS_INPUT_SCK1,
    DSPIC33AK_PPS_INPUT_SDI1,

    DSPIC33AK_PPS_INPUT_SS2,
    DSPIC33AK_PPS_INPUT_SCK2,
    DSPIC33AK_PPS_INPUT_SDI2,

    DSPIC33AK_PPS_INPUT_SDI4,

    DSPIC33AK_PPS_INPUT_CLCINA,
    DSPIC33AK_PPS_INPUT_CLCINB,
    DSPIC33AK_PPS_INPUT_CLCINC,

    DSPIC33AK_PPS_INPUT_REFI1,

    DSPIC33AK_PPS_INPUT_CAN1RX
} dspic33ak_pps_input_t;

/*
 * PPS lock gate (RPCON.IOLOCK). unlock() makes the PPS map writable, lock()
 * protects it. route_*() below bracket their own writes; expose these only for
 * direct-SFR PPS writers or a single window around a batch.
 */
void dspic33ak_pps_unlock(void);   /* IOLOCK = 0 : PPS registers writable  */
void dspic33ak_pps_lock(void);     /* IOLOCK = 1 : PPS registers protected */

/*
 * Route a peripheral OUTPUT onto an RP pin (writes the RP's _RPnnR with the
 * peripheral's output function code). Self-brackets IOLOCK. Returns false if the
 * peripheral output is not available on this device, or the RP pin has no output
 * PPS register here (range/encoding contract only -- not a board-bonding check;
 * configure the GPIO output first via dspic33ak_gpio_rp_config_digital_output()).
 */
bool dspic33ak_pps_route_output(dspic33ak_pps_output_t output, dspic33ak_gpio_rp_t rp);

/*
 * Route a peripheral INPUT to read from an RP pin (writes the peripheral's RPINRx
 * input-select with the RP number). Self-brackets IOLOCK. Returns false if the
 * peripheral input is not available on this device, OR if rp is not a physical
 * remappable pin on this device (rp == 0, rp > 128, or an undefined RP is
 * rejected before any register write). Configure the GPIO input first via
 * dspic33ak_gpio_rp_config_digital_input().
 */
bool dspic33ak_pps_route_input(dspic33ak_pps_input_t input, dspic33ak_gpio_rp_t rp);

#ifdef __cplusplus
}
#endif

#endif /* DSPIC33AK_PPS_H */
