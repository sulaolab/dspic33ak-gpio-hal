/*
 * dspic33ak_gpio_event.c
 * ----------------------
 * Thin dsPIC33AK Change Notification (CN) event dispatcher for GPIO pins.
 *
 * The app/example owns the actual CN interrupt vector and forwards it here.
 * This keeps vector ownership out of the GPIO core while allowing small
 * validation apps to attach callbacks to selected pins.
 */

#include "dspic33ak_gpio_event.h"

#include <stddef.h>
#include <xc.h>


//===========================================================
// Definition
//===========================================================

#define DSPIC33AK_GPIO_EVENT_PORT_COUNT    8u
#define DSPIC33AK_GPIO_EVENT_PIN_COUNT     16u
#define DSPIC33AK_GPIO_EVENT_CNCON_CNSTYLE 0x00000800u
#define DSPIC33AK_GPIO_EVENT_CNCON_ON      0x00008000u

typedef struct
{
    volatile uint32_t *port;
    volatile uint32_t *cncon;
    volatile uint32_t *cnen0;
    volatile uint32_t *cnen1;
    volatile uint32_t *cnf;
    volatile uint32_t *ifs;
    uint32_t           ifs_mask;
} dspic33ak_gpio_event_regs_t;

typedef struct
{
    dspic33ak_gpio_pin_t            pin;
    dspic33ak_gpio_event_edge_t     trigger;
    dspic33ak_gpio_event_callback_t callback;
    void                           *user_data;
    bool                            previous_high;
} dspic33ak_gpio_event_slot_t;

#define DSPIC33AK_GPIO_EVENT_PORT_ROW(L, IFS_REG, IFS_MASK) \
    { &PORT##L, &CNCON##L, &CNEN0##L, &CNEN1##L, &CNF##L, &IFS_REG, (uint32_t)(IFS_MASK) }
#define DSPIC33AK_GPIO_EVENT_PORT_NONE \
    { 0, 0, 0, 0, 0, 0, 0u }


//===========================================================
// Function Prototype
//===========================================================

static const dspic33ak_gpio_event_regs_t *dspic33ak_gpio_event_regs_for(dspic33ak_gpio_pin_t pin);
static uint8_t                            dspic33ak_gpio_event_port_index(dspic33ak_gpio_pin_t pin);
static uint8_t                            dspic33ak_gpio_event_bit_index(dspic33ak_gpio_pin_t pin);
static uint32_t                           dspic33ak_gpio_event_mask(dspic33ak_gpio_pin_t pin);
static bool                               dspic33ak_gpio_event_trigger_valid(dspic33ak_gpio_event_edge_t trigger);
static bool                               dspic33ak_gpio_event_trigger_matches(dspic33ak_gpio_event_edge_t trigger,
                                                                              dspic33ak_gpio_event_edge_t actual_edge);


//===========================================================
// Variables
//===========================================================

static const dspic33ak_gpio_event_regs_t s_event_regs[DSPIC33AK_GPIO_EVENT_PORT_COUNT] =
{
#if defined(PORTA) && defined(CNCONA) && defined(CNEN0A) && defined(CNEN1A) && defined(CNFA) && defined(_IFS3_CNAIF_MASK)
    DSPIC33AK_GPIO_EVENT_PORT_ROW(A, IFS3, _IFS3_CNAIF_MASK),     /* [0] PORTA */
#else
    DSPIC33AK_GPIO_EVENT_PORT_NONE,
#endif
#if defined(PORTB) && defined(CNCONB) && defined(CNEN0B) && defined(CNEN1B) && defined(CNFB) && defined(_IFS3_CNBIF_MASK)
    DSPIC33AK_GPIO_EVENT_PORT_ROW(B, IFS3, _IFS3_CNBIF_MASK),     /* [1] PORTB */
#else
    DSPIC33AK_GPIO_EVENT_PORT_NONE,
#endif
#if defined(PORTC) && defined(CNCONC) && defined(CNEN0C) && defined(CNEN1C) && defined(CNFC) && defined(_IFS3_CNCIF_MASK)
    DSPIC33AK_GPIO_EVENT_PORT_ROW(C, IFS3, _IFS3_CNCIF_MASK),     /* [2] PORTC */
#else
    DSPIC33AK_GPIO_EVENT_PORT_NONE,
#endif
#if defined(PORTD) && defined(CNCOND) && defined(CNEN0D) && defined(CNEN1D) && defined(CNFD) && defined(_IFS3_CNDIF_MASK)
    DSPIC33AK_GPIO_EVENT_PORT_ROW(D, IFS3, _IFS3_CNDIF_MASK),     /* [3] PORTD */
#else
    DSPIC33AK_GPIO_EVENT_PORT_NONE,
#endif
#if defined(PORTE) && defined(CNCONE) && defined(CNEN0E) && defined(CNEN1E) && defined(CNFE) && defined(_IFS9_CNEIF_MASK)
    DSPIC33AK_GPIO_EVENT_PORT_ROW(E, IFS9, _IFS9_CNEIF_MASK),     /* [4] PORTE */
#else
    DSPIC33AK_GPIO_EVENT_PORT_NONE,
#endif
#if defined(PORTF) && defined(CNCONF) && defined(CNEN0F) && defined(CNEN1F) && defined(CNFF) && defined(_IFS9_CNFIF_MASK)
    DSPIC33AK_GPIO_EVENT_PORT_ROW(F, IFS9, _IFS9_CNFIF_MASK),     /* [5] PORTF */
#else
    DSPIC33AK_GPIO_EVENT_PORT_NONE,
#endif
#if defined(PORTG) && defined(CNCONG) && defined(CNEN0G) && defined(CNEN1G) && defined(CNFG) && defined(_IFS9_CNGIF_MASK)
    DSPIC33AK_GPIO_EVENT_PORT_ROW(G, IFS9, _IFS9_CNGIF_MASK),     /* [6] PORTG */
#else
    DSPIC33AK_GPIO_EVENT_PORT_NONE,
#endif
#if defined(PORTH) && defined(CNCONH) && defined(CNEN0H) && defined(CNEN1H) && defined(CNFH) && defined(_IFS9_CNHIF_MASK)
    DSPIC33AK_GPIO_EVENT_PORT_ROW(H, IFS9, _IFS9_CNHIF_MASK),     /* [7] PORTH */
#else
    DSPIC33AK_GPIO_EVENT_PORT_NONE,
#endif
};

static dspic33ak_gpio_event_slot_t s_event_slots[DSPIC33AK_GPIO_EVENT_PORT_COUNT][DSPIC33AK_GPIO_EVENT_PIN_COUNT];
static volatile uint32_t s_event_owned_masks[DSPIC33AK_GPIO_EVENT_PORT_COUNT];


//===========================================================
// Global Function
//===========================================================

bool dspic33ak_gpio_event_attach(dspic33ak_gpio_pin_t pin,
                                 dspic33ak_gpio_event_edge_t trigger,
                                 dspic33ak_gpio_event_callback_t callback,
                                 void *user_data)
{
    const dspic33ak_gpio_event_regs_t *regs = dspic33ak_gpio_event_regs_for(pin);
    uint8_t port_index = dspic33ak_gpio_event_port_index(pin);
    uint8_t bit_index = dspic33ak_gpio_event_bit_index(pin);
    uint32_t mask = dspic33ak_gpio_event_mask(pin);

    if ((regs == 0) || (callback == 0) || !dspic33ak_gpio_event_trigger_valid(trigger))
    {
        return false;
    }

    *regs->cnen0 &= ~mask;
    *regs->cnen1 &= ~mask;
    *regs->cnf          &= ~mask;
    *regs->ifs          &= ~regs->ifs_mask;

    s_event_slots[port_index][bit_index].pin       = pin;
    s_event_slots[port_index][bit_index].trigger   = trigger;
    s_event_slots[port_index][bit_index].callback  = callback;
    s_event_slots[port_index][bit_index].user_data = user_data;
    s_event_slots[port_index][bit_index].previous_high = ((*regs->port & mask) != 0u);
    s_event_owned_masks[port_index] |= mask;

    *regs->cncon |= (DSPIC33AK_GPIO_EVENT_CNCON_CNSTYLE | DSPIC33AK_GPIO_EVENT_CNCON_ON);

    *regs->cnen0 |= mask;
    *regs->cnen1 |= mask;

    return true;
}

bool dspic33ak_gpio_event_detach(dspic33ak_gpio_pin_t pin)
{
    const dspic33ak_gpio_event_regs_t *regs = dspic33ak_gpio_event_regs_for(pin);
    uint8_t port_index = dspic33ak_gpio_event_port_index(pin);
    uint8_t bit_index = dspic33ak_gpio_event_bit_index(pin);
    uint32_t mask = dspic33ak_gpio_event_mask(pin);

    if (regs == 0)
    {
        return false;
    }

    *regs->cnen0 &= ~mask;
    *regs->cnen1 &= ~mask;
    *regs->cnf          &= ~mask;
    *regs->ifs          &= ~regs->ifs_mask;

    s_event_slots[port_index][bit_index].pin       = 0u;
    s_event_slots[port_index][bit_index].trigger   = DSPIC33AK_GPIO_EVENT_EDGE_NONE;
    s_event_slots[port_index][bit_index].callback  = 0;
    s_event_slots[port_index][bit_index].user_data = 0;
    s_event_slots[port_index][bit_index].previous_high = false;
    s_event_owned_masks[port_index] &= ~mask;

    return true;
}

void dspic33ak_gpio_event_process_isr(void)
{
    uint8_t port_index;

    for (port_index = 0u; port_index < DSPIC33AK_GPIO_EVENT_PORT_COUNT; port_index++)
    {
        const dspic33ak_gpio_event_regs_t *regs = &s_event_regs[port_index];
        uint32_t pending_mask;
        uint32_t port_level;
        uint8_t bit_index;

        if (regs->port == 0)
        {
            continue;
        }

        pending_mask = *regs->cnf & s_event_owned_masks[port_index];
        if (pending_mask == 0u)
        {
            if ((s_event_owned_masks[port_index] != 0u) &&
                ((*regs->ifs & regs->ifs_mask) != 0u))
            {
                *regs->ifs &= ~regs->ifs_mask;
            }
            continue;
        }

        port_level = *regs->port;

        for (bit_index = 0u; bit_index < DSPIC33AK_GPIO_EVENT_PIN_COUNT; bit_index++)
        {
            uint32_t bit_mask = (uint32_t)1u << bit_index;
            dspic33ak_gpio_event_slot_t *slot;
            dspic33ak_gpio_event_edge_t actual_edge;
            bool current_high;

            if ((pending_mask & bit_mask) == 0u)
            {
                continue;
            }

            slot = &s_event_slots[port_index][bit_index];
            current_high = ((port_level & bit_mask) != 0u);
            if (current_high == slot->previous_high)
            {
                continue;
            }

            actual_edge = current_high ?
                          DSPIC33AK_GPIO_EVENT_EDGE_RISING :
                          DSPIC33AK_GPIO_EVENT_EDGE_FALLING;
            slot->previous_high = current_high;

            if ((slot->callback != 0) &&
                dspic33ak_gpio_event_trigger_matches(slot->trigger, actual_edge))
            {
                slot->callback(slot->pin, actual_edge, slot->user_data);
            }
        }

        *regs->cnf &= ~pending_mask;
        *regs->ifs &= ~regs->ifs_mask;
    }
}


//===========================================================
// Local Function
//===========================================================

static const dspic33ak_gpio_event_regs_t *dspic33ak_gpio_event_regs_for(dspic33ak_gpio_pin_t pin)
{
    uint8_t port_index = dspic33ak_gpio_event_port_index(pin);

    if (port_index >= DSPIC33AK_GPIO_EVENT_PORT_COUNT)
    {
        return 0;
    }
    if (s_event_regs[port_index].port == 0)
    {
        return 0;
    }
    return &s_event_regs[port_index];
}

static uint8_t dspic33ak_gpio_event_port_index(dspic33ak_gpio_pin_t pin)
{
    return (uint8_t)(pin >> 4);
}

static uint8_t dspic33ak_gpio_event_bit_index(dspic33ak_gpio_pin_t pin)
{
    return (uint8_t)(pin & 0x0Fu);
}

static uint32_t dspic33ak_gpio_event_mask(dspic33ak_gpio_pin_t pin)
{
    return (uint32_t)1u << dspic33ak_gpio_event_bit_index(pin);
}

static bool dspic33ak_gpio_event_trigger_valid(dspic33ak_gpio_event_edge_t trigger)
{
    return (trigger == DSPIC33AK_GPIO_EVENT_EDGE_EITHER);
}

static bool dspic33ak_gpio_event_trigger_matches(dspic33ak_gpio_event_edge_t trigger,
                                                 dspic33ak_gpio_event_edge_t actual_edge)
{
    return ((trigger == DSPIC33AK_GPIO_EVENT_EDGE_EITHER) ||
            (trigger == actual_edge));
}
