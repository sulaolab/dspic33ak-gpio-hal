/*
 * dspic33ak_gpio.c
 * ----------------
 * Small, readable GPIO HAL for dsPIC33AK devices. See dspic33ak_gpio.h for the
 * public contract, pin addressing, and interrupt-safety policy.
 *
 * Implementation notes:
 *   - One device-neutral driver body drives any port through a table of plain
 *     32-bit register pointers (s_gpio_regs[], indexed by port code). Only the
 *     table is device-specific; a port row is emitted only when the device
 *     header defines that port's SFRs (#if defined(LATx)), so the table tracks
 *     the silicon automatically.
 *   - GPIO SFRs on dsPIC33AK are 32-bit in the DFP headers; the register
 *     pointers are uint32_t to match.
 *   - Accessors are plain read-modify-write and never disable interrupts.
 */

//===========================================================
// INCLUDES
//===========================================================
#include "dspic33ak_gpio.h"

#include <xc.h>

#include "dspic33ak_gpio_reg.h"


//===========================================================
// Definition
//===========================================================

/* Per-port register pointers (uniform 32-bit SFRs). */
typedef struct
{
    volatile uint32_t *port;    /* read input level   */
    volatile uint32_t *lat;     /* output latch       */
    volatile uint32_t *tris;    /* 1 = input, 0 = output */
    volatile uint32_t *ansel;   /* 1 = analog, 0 = digital */
    volatile uint32_t *odc;     /* 1 = open-drain     */
    volatile uint32_t *cnpu;    /* 1 = pull-up        */
    volatile uint32_t *cnpd;    /* 1 = pull-down      */
} dspic33ak_gpio_regs_t;

#define DSPIC33AK_GPIO_ARRAY_LEN(a)   (sizeof(a) / sizeof((a)[0]))

#define DSPIC33AK_GPIO_PORT_ROW(L) \
    { &PORT##L, &LAT##L, &TRIS##L, &ANSEL##L, &ODC##L, &CNPU##L, &CNPD##L }
#define DSPIC33AK_GPIO_PORT_NONE \
    { 0, 0, 0, 0, 0, 0, 0 }


//===========================================================
// Function Prototype
//===========================================================

static const dspic33ak_gpio_regs_t *dspic33ak_gpio_regs_for(dspic33ak_gpio_pin_t pin);
static uint32_t                      dspic33ak_gpio_mask(dspic33ak_gpio_pin_t pin);


//===========================================================
// Variables
//===========================================================

/*
 * Indexed by dspic33ak_gpio_port_t (A=0 .. H=7). Every slot is present so the
 * index always equals the port code; a port absent on the device expands to a
 * NULL row and is rejected by dspic33ak_gpio_regs_for().
 */
static const dspic33ak_gpio_regs_t s_gpio_regs[] =
{
#if defined(LATA)
    DSPIC33AK_GPIO_PORT_ROW(A),     /* [0] PORTA */
#else
    DSPIC33AK_GPIO_PORT_NONE,
#endif
#if defined(LATB)
    DSPIC33AK_GPIO_PORT_ROW(B),     /* [1] PORTB */
#else
    DSPIC33AK_GPIO_PORT_NONE,
#endif
#if defined(LATC)
    DSPIC33AK_GPIO_PORT_ROW(C),     /* [2] PORTC */
#else
    DSPIC33AK_GPIO_PORT_NONE,
#endif
#if defined(LATD)
    DSPIC33AK_GPIO_PORT_ROW(D),     /* [3] PORTD */
#else
    DSPIC33AK_GPIO_PORT_NONE,
#endif
#if defined(LATE)
    DSPIC33AK_GPIO_PORT_ROW(E),     /* [4] PORTE */
#else
    DSPIC33AK_GPIO_PORT_NONE,
#endif
#if defined(LATF)
    DSPIC33AK_GPIO_PORT_ROW(F),     /* [5] PORTF */
#else
    DSPIC33AK_GPIO_PORT_NONE,
#endif
#if defined(LATG)
    DSPIC33AK_GPIO_PORT_ROW(G),     /* [6] PORTG */
#else
    DSPIC33AK_GPIO_PORT_NONE,
#endif
#if defined(LATH)
    DSPIC33AK_GPIO_PORT_ROW(H),     /* [7] PORTH */
#else
    DSPIC33AK_GPIO_PORT_NONE,
#endif
};


//===========================================================
// Global Function
//===========================================================

bool dspic33ak_gpio_set_direction(dspic33ak_gpio_pin_t pin, dspic33ak_gpio_dir_t dir)
{
    const dspic33ak_gpio_regs_t *r = dspic33ak_gpio_regs_for(pin);
    if (r == 0)
    {
        return false;
    }
    uint32_t mask = dspic33ak_gpio_mask(pin);

    if (dir == DSPIC33AK_GPIO_DIR_OUTPUT)
    {
        dspic33ak_gpio_reg_clear(r->tris, mask);   /* TRIS = 0 -> output */
    }
    else
    {
        dspic33ak_gpio_reg_set(r->tris, mask);     /* TRIS = 1 -> input  */
    }
    return true;
}

bool dspic33ak_gpio_set_pull(dspic33ak_gpio_pin_t pin, dspic33ak_gpio_pull_t pull)
{
    const dspic33ak_gpio_regs_t *r = dspic33ak_gpio_regs_for(pin);
    if (r == 0)
    {
        return false;
    }
    uint32_t mask = dspic33ak_gpio_mask(pin);

    switch (pull)
    {
    case DSPIC33AK_GPIO_PULL_UP:
        dspic33ak_gpio_reg_clear(r->cnpd, mask);
        dspic33ak_gpio_reg_set(r->cnpu, mask);
        break;
    case DSPIC33AK_GPIO_PULL_DOWN:
        dspic33ak_gpio_reg_clear(r->cnpu, mask);
        dspic33ak_gpio_reg_set(r->cnpd, mask);
        break;
    case DSPIC33AK_GPIO_PULL_NONE:
    default:
        dspic33ak_gpio_reg_clear(r->cnpu, mask);
        dspic33ak_gpio_reg_clear(r->cnpd, mask);
        break;
    }
    return true;
}

bool dspic33ak_gpio_set_analog(dspic33ak_gpio_pin_t pin, bool analog)
{
    const dspic33ak_gpio_regs_t *r = dspic33ak_gpio_regs_for(pin);
    if (r == 0)
    {
        return false;
    }
    uint32_t mask = dspic33ak_gpio_mask(pin);

    if (analog)
    {
        dspic33ak_gpio_reg_set(r->ansel, mask);    /* ANSEL = 1 -> analog  */
    }
    else
    {
        dspic33ak_gpio_reg_clear(r->ansel, mask);  /* ANSEL = 0 -> digital */
    }
    return true;
}

bool dspic33ak_gpio_set_open_drain(dspic33ak_gpio_pin_t pin, bool enable)
{
    const dspic33ak_gpio_regs_t *r = dspic33ak_gpio_regs_for(pin);
    if (r == 0)
    {
        return false;
    }
    uint32_t mask = dspic33ak_gpio_mask(pin);

    if (enable)
    {
        dspic33ak_gpio_reg_set(r->odc, mask);
    }
    else
    {
        dspic33ak_gpio_reg_clear(r->odc, mask);
    }
    return true;
}

bool dspic33ak_gpio_config(dspic33ak_gpio_pin_t pin, const dspic33ak_gpio_config_t *config)
{
    const dspic33ak_gpio_regs_t *r = dspic33ak_gpio_regs_for(pin);
    if ((r == 0) || (config == 0))
    {
        return false;
    }
    uint32_t mask = dspic33ak_gpio_mask(pin);

    /* analog / digital */
    if (config->analog)
    {
        dspic33ak_gpio_reg_set(r->ansel, mask);
    }
    else
    {
        dspic33ak_gpio_reg_clear(r->ansel, mask);
    }

    /* pull */
    (void)dspic33ak_gpio_set_pull(pin, config->pull);

    /* open-drain */
    if (config->open_drain)
    {
        dspic33ak_gpio_reg_set(r->odc, mask);
    }
    else
    {
        dspic33ak_gpio_reg_clear(r->odc, mask);
    }

    /* For an output, set the initial level before enabling the driver so the
     * pin does not glitch to a stale latch value. */
    if (config->dir == DSPIC33AK_GPIO_DIR_OUTPUT)
    {
        if (config->initial_high)
        {
            dspic33ak_gpio_reg_set(r->lat, mask);
        }
        else
        {
            dspic33ak_gpio_reg_clear(r->lat, mask);
        }
        dspic33ak_gpio_reg_clear(r->tris, mask);   /* output */
    }
    else
    {
        dspic33ak_gpio_reg_set(r->tris, mask);     /* input  */
    }
    return true;
}

bool dspic33ak_gpio_write(dspic33ak_gpio_pin_t pin, bool high)
{
    const dspic33ak_gpio_regs_t *r = dspic33ak_gpio_regs_for(pin);
    if (r == 0)
    {
        return false;
    }
    uint32_t mask = dspic33ak_gpio_mask(pin);

    if (high)
    {
        dspic33ak_gpio_reg_set(r->lat, mask);
    }
    else
    {
        dspic33ak_gpio_reg_clear(r->lat, mask);
    }
    return true;
}

bool dspic33ak_gpio_set(dspic33ak_gpio_pin_t pin)
{
    const dspic33ak_gpio_regs_t *r = dspic33ak_gpio_regs_for(pin);
    if (r == 0)
    {
        return false;
    }
    dspic33ak_gpio_reg_set(r->lat, dspic33ak_gpio_mask(pin));
    return true;
}

bool dspic33ak_gpio_clear(dspic33ak_gpio_pin_t pin)
{
    const dspic33ak_gpio_regs_t *r = dspic33ak_gpio_regs_for(pin);
    if (r == 0)
    {
        return false;
    }
    dspic33ak_gpio_reg_clear(r->lat, dspic33ak_gpio_mask(pin));
    return true;
}

bool dspic33ak_gpio_toggle(dspic33ak_gpio_pin_t pin)
{
    const dspic33ak_gpio_regs_t *r = dspic33ak_gpio_regs_for(pin);
    if (r == 0)
    {
        return false;
    }
    dspic33ak_gpio_reg_toggle(r->lat, dspic33ak_gpio_mask(pin));
    return true;
}

bool dspic33ak_gpio_read(dspic33ak_gpio_pin_t pin)
{
    const dspic33ak_gpio_regs_t *r = dspic33ak_gpio_regs_for(pin);
    if (r == 0)
    {
        return false;
    }
    return dspic33ak_gpio_reg_is_set(r->port, dspic33ak_gpio_mask(pin));
}

bool dspic33ak_gpio_read_output(dspic33ak_gpio_pin_t pin)
{
    const dspic33ak_gpio_regs_t *r = dspic33ak_gpio_regs_for(pin);
    if (r == 0)
    {
        return false;
    }
    return dspic33ak_gpio_reg_is_set(r->lat, dspic33ak_gpio_mask(pin));
}


//===========================================================
// Local Function
//===========================================================

/* Resolve a packed pin to its port register set, or NULL if the port is not
 * present on this device (no register row). */
static const dspic33ak_gpio_regs_t *dspic33ak_gpio_regs_for(dspic33ak_gpio_pin_t pin)
{
    unsigned port = (unsigned)(pin >> 4);

    if (port >= DSPIC33AK_GPIO_ARRAY_LEN(s_gpio_regs))
    {
        return 0;
    }
    if (s_gpio_regs[port].lat == 0)
    {
        return 0;
    }
    return &s_gpio_regs[port];
}

/* Single-bit mask for the pin's bit position (0..15). */
static uint32_t dspic33ak_gpio_mask(dspic33ak_gpio_pin_t pin)
{
    return (uint32_t)1u << (pin & 0x0Fu);
}
