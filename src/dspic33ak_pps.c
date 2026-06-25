/*
 * dspic33ak_pps.c
 * ---------------
 * PPS routing implementation. See dspic33ak_pps.h for the layering and contract.
 *
 * Device adaptation is by #ifdef on the XC macros themselves:
 *   - output function code : _RPOUT_<sig>   (e.g. _RPOUT_SCK2)
 *   - output pin register  : _RP<nn>R       (RPORx bit-field alias)
 *   - input-select register: _<sig>R        (RPINRx bit-field alias)
 * A peripheral / RP the selected device header does not define is left out of the
 * relevant switch and the call returns false -- no __dsPIC33AK___ conditionals.
 */

#include "dspic33ak_pps.h"

#include <xc.h>


//===========================================================
// PPS lock gate (RPCON.IOLOCK)
//===========================================================
// dsPIC33A guards RPCON via the PAC, which leaves RPCON writable out of reset
// (RPCONLK=0, RPCONWR=1), so IOLOCK is set/cleared by a direct write
// (__builtin_write_RPCON() is unsupported on this XC-DSC target).
void dspic33ak_pps_unlock(void)
{
    RPCONbits.IOLOCK = 0u;   /* PPS writable  */
}

void dspic33ak_pps_lock(void)
{
    RPCONbits.IOLOCK = 1u;   /* PPS protected */
}


//===========================================================
// Local: peripheral output -> RPORx function code (_RPOUT_*)
//===========================================================
static bool dspic33ak_pps_get_output_code(dspic33ak_pps_output_t output, uint8_t *code)
{
    if (code == NULL)
    {
        return false;
    }

    switch (output)
    {
#ifdef _RPOUT_U1TX
    case DSPIC33AK_PPS_OUTPUT_U1TX: *code = (uint8_t)_RPOUT_U1TX; return true;
#endif
#ifdef _RPOUT_SS1
    case DSPIC33AK_PPS_OUTPUT_SS1:  *code = (uint8_t)_RPOUT_SS1;  return true;
#endif
#ifdef _RPOUT_SCK1
    case DSPIC33AK_PPS_OUTPUT_SCK1: *code = (uint8_t)_RPOUT_SCK1; return true;
#endif
#ifdef _RPOUT_SDO1
    case DSPIC33AK_PPS_OUTPUT_SDO1: *code = (uint8_t)_RPOUT_SDO1; return true;
#endif
#ifdef _RPOUT_SS2
    case DSPIC33AK_PPS_OUTPUT_SS2:  *code = (uint8_t)_RPOUT_SS2;  return true;
#endif
#ifdef _RPOUT_SCK2
    case DSPIC33AK_PPS_OUTPUT_SCK2: *code = (uint8_t)_RPOUT_SCK2; return true;
#endif
#ifdef _RPOUT_SDO2
    case DSPIC33AK_PPS_OUTPUT_SDO2: *code = (uint8_t)_RPOUT_SDO2; return true;
#endif
#ifdef _RPOUT_SCK4
    case DSPIC33AK_PPS_OUTPUT_SCK4: *code = (uint8_t)_RPOUT_SCK4; return true;
#endif
#ifdef _RPOUT_SDO4
    case DSPIC33AK_PPS_OUTPUT_SDO4: *code = (uint8_t)_RPOUT_SDO4; return true;
#endif
#ifdef _RPOUT_CLC1OUT
    case DSPIC33AK_PPS_OUTPUT_CLC1: *code = (uint8_t)_RPOUT_CLC1OUT; return true;
#endif
#ifdef _RPOUT_CLC2OUT
    case DSPIC33AK_PPS_OUTPUT_CLC2: *code = (uint8_t)_RPOUT_CLC2OUT; return true;
#endif
#ifdef _RPOUT_CLC3OUT
    case DSPIC33AK_PPS_OUTPUT_CLC3: *code = (uint8_t)_RPOUT_CLC3OUT; return true;
#endif
#ifdef _RPOUT_CAN1TX
    case DSPIC33AK_PPS_OUTPUT_CAN1TX: *code = (uint8_t)_RPOUT_CAN1TX; return true;
#endif
#ifdef _RPOUT_PWM1H
    case DSPIC33AK_PPS_OUTPUT_PWM1H: *code = (uint8_t)_RPOUT_PWM1H; return true;
#endif
#ifdef _RPOUT_PWM2H
    case DSPIC33AK_PPS_OUTPUT_PWM2H: *code = (uint8_t)_RPOUT_PWM2H; return true;
#endif
#ifdef _RPOUT_PWM3H
    case DSPIC33AK_PPS_OUTPUT_PWM3H: *code = (uint8_t)_RPOUT_PWM3H; return true;
#endif
    default:
        break;
    }
    return false;   /* peripheral output not available on this device */
}


//===========================================================
// Local: write the output function code onto an RP pin's _RPnnR
//===========================================================
// _RPnnR is a bit-field alias (assignment only; no address / no formula). Every
// RP for which the selected device defines an output register (_RPnnR) is
// listed (physical RP1..RP128 only, each #ifdef-guarded so only the device's
// real registers compile). The RPV virtual outputs RP129..RP144 are excluded:
// this API is typed for physical GPIO RPs (see the header). An RP with no
// output PPS register on this device returns false.
// Flat register table by design -- RPORx numbering has gaps and is
// device-specific, so a switch (not a formula) is the safe canonical form.
static bool dspic33ak_pps_write_output_rp(dspic33ak_gpio_rp_t rp, uint8_t code)
{
    switch (rp)
    {
#ifdef _RP1R
    case 1u: _RP1R = code; return true;
#endif
#ifdef _RP2R
    case 2u: _RP2R = code; return true;
#endif
#ifdef _RP3R
    case 3u: _RP3R = code; return true;
#endif
#ifdef _RP4R
    case 4u: _RP4R = code; return true;
#endif
#ifdef _RP5R
    case 5u: _RP5R = code; return true;
#endif
#ifdef _RP6R
    case 6u: _RP6R = code; return true;
#endif
#ifdef _RP7R
    case 7u: _RP7R = code; return true;
#endif
#ifdef _RP8R
    case 8u: _RP8R = code; return true;
#endif
#ifdef _RP9R
    case 9u: _RP9R = code; return true;
#endif
#ifdef _RP10R
    case 10u: _RP10R = code; return true;
#endif
#ifdef _RP11R
    case 11u: _RP11R = code; return true;
#endif
#ifdef _RP12R
    case 12u: _RP12R = code; return true;
#endif
#ifdef _RP13R
    case 13u: _RP13R = code; return true;
#endif
#ifdef _RP14R
    case 14u: _RP14R = code; return true;
#endif
#ifdef _RP15R
    case 15u: _RP15R = code; return true;
#endif
#ifdef _RP16R
    case 16u: _RP16R = code; return true;
#endif
#ifdef _RP17R
    case 17u: _RP17R = code; return true;
#endif
#ifdef _RP18R
    case 18u: _RP18R = code; return true;
#endif
#ifdef _RP19R
    case 19u: _RP19R = code; return true;
#endif
#ifdef _RP20R
    case 20u: _RP20R = code; return true;
#endif
#ifdef _RP21R
    case 21u: _RP21R = code; return true;
#endif
#ifdef _RP22R
    case 22u: _RP22R = code; return true;
#endif
#ifdef _RP23R
    case 23u: _RP23R = code; return true;
#endif
#ifdef _RP24R
    case 24u: _RP24R = code; return true;
#endif
#ifdef _RP25R
    case 25u: _RP25R = code; return true;
#endif
#ifdef _RP26R
    case 26u: _RP26R = code; return true;
#endif
#ifdef _RP27R
    case 27u: _RP27R = code; return true;
#endif
#ifdef _RP28R
    case 28u: _RP28R = code; return true;
#endif
#ifdef _RP29R
    case 29u: _RP29R = code; return true;
#endif
#ifdef _RP30R
    case 30u: _RP30R = code; return true;
#endif
#ifdef _RP31R
    case 31u: _RP31R = code; return true;
#endif
#ifdef _RP32R
    case 32u: _RP32R = code; return true;
#endif
#ifdef _RP33R
    case 33u: _RP33R = code; return true;
#endif
#ifdef _RP34R
    case 34u: _RP34R = code; return true;
#endif
#ifdef _RP35R
    case 35u: _RP35R = code; return true;
#endif
#ifdef _RP36R
    case 36u: _RP36R = code; return true;
#endif
#ifdef _RP37R
    case 37u: _RP37R = code; return true;
#endif
#ifdef _RP38R
    case 38u: _RP38R = code; return true;
#endif
#ifdef _RP39R
    case 39u: _RP39R = code; return true;
#endif
#ifdef _RP40R
    case 40u: _RP40R = code; return true;
#endif
#ifdef _RP41R
    case 41u: _RP41R = code; return true;
#endif
#ifdef _RP42R
    case 42u: _RP42R = code; return true;
#endif
#ifdef _RP43R
    case 43u: _RP43R = code; return true;
#endif
#ifdef _RP44R
    case 44u: _RP44R = code; return true;
#endif
#ifdef _RP45R
    case 45u: _RP45R = code; return true;
#endif
#ifdef _RP46R
    case 46u: _RP46R = code; return true;
#endif
#ifdef _RP47R
    case 47u: _RP47R = code; return true;
#endif
#ifdef _RP48R
    case 48u: _RP48R = code; return true;
#endif
#ifdef _RP49R
    case 49u: _RP49R = code; return true;
#endif
#ifdef _RP50R
    case 50u: _RP50R = code; return true;
#endif
#ifdef _RP51R
    case 51u: _RP51R = code; return true;
#endif
#ifdef _RP52R
    case 52u: _RP52R = code; return true;
#endif
#ifdef _RP53R
    case 53u: _RP53R = code; return true;
#endif
#ifdef _RP54R
    case 54u: _RP54R = code; return true;
#endif
#ifdef _RP55R
    case 55u: _RP55R = code; return true;
#endif
#ifdef _RP56R
    case 56u: _RP56R = code; return true;
#endif
#ifdef _RP57R
    case 57u: _RP57R = code; return true;
#endif
#ifdef _RP58R
    case 58u: _RP58R = code; return true;
#endif
#ifdef _RP59R
    case 59u: _RP59R = code; return true;
#endif
#ifdef _RP60R
    case 60u: _RP60R = code; return true;
#endif
#ifdef _RP61R
    case 61u: _RP61R = code; return true;
#endif
#ifdef _RP62R
    case 62u: _RP62R = code; return true;
#endif
#ifdef _RP63R
    case 63u: _RP63R = code; return true;
#endif
#ifdef _RP64R
    case 64u: _RP64R = code; return true;
#endif
#ifdef _RP65R
    case 65u: _RP65R = code; return true;
#endif
#ifdef _RP66R
    case 66u: _RP66R = code; return true;
#endif
#ifdef _RP67R
    case 67u: _RP67R = code; return true;
#endif
#ifdef _RP68R
    case 68u: _RP68R = code; return true;
#endif
#ifdef _RP69R
    case 69u: _RP69R = code; return true;
#endif
#ifdef _RP70R
    case 70u: _RP70R = code; return true;
#endif
#ifdef _RP71R
    case 71u: _RP71R = code; return true;
#endif
#ifdef _RP72R
    case 72u: _RP72R = code; return true;
#endif
#ifdef _RP73R
    case 73u: _RP73R = code; return true;
#endif
#ifdef _RP74R
    case 74u: _RP74R = code; return true;
#endif
#ifdef _RP75R
    case 75u: _RP75R = code; return true;
#endif
#ifdef _RP76R
    case 76u: _RP76R = code; return true;
#endif
#ifdef _RP77R
    case 77u: _RP77R = code; return true;
#endif
#ifdef _RP78R
    case 78u: _RP78R = code; return true;
#endif
#ifdef _RP79R
    case 79u: _RP79R = code; return true;
#endif
#ifdef _RP80R
    case 80u: _RP80R = code; return true;
#endif
#ifdef _RP81R
    case 81u: _RP81R = code; return true;
#endif
#ifdef _RP82R
    case 82u: _RP82R = code; return true;
#endif
#ifdef _RP83R
    case 83u: _RP83R = code; return true;
#endif
#ifdef _RP84R
    case 84u: _RP84R = code; return true;
#endif
#ifdef _RP85R
    case 85u: _RP85R = code; return true;
#endif
#ifdef _RP86R
    case 86u: _RP86R = code; return true;
#endif
#ifdef _RP87R
    case 87u: _RP87R = code; return true;
#endif
#ifdef _RP88R
    case 88u: _RP88R = code; return true;
#endif
#ifdef _RP89R
    case 89u: _RP89R = code; return true;
#endif
#ifdef _RP90R
    case 90u: _RP90R = code; return true;
#endif
#ifdef _RP91R
    case 91u: _RP91R = code; return true;
#endif
#ifdef _RP92R
    case 92u: _RP92R = code; return true;
#endif
#ifdef _RP93R
    case 93u: _RP93R = code; return true;
#endif
#ifdef _RP94R
    case 94u: _RP94R = code; return true;
#endif
#ifdef _RP95R
    case 95u: _RP95R = code; return true;
#endif
#ifdef _RP96R
    case 96u: _RP96R = code; return true;
#endif
#ifdef _RP97R
    case 97u: _RP97R = code; return true;
#endif
#ifdef _RP98R
    case 98u: _RP98R = code; return true;
#endif
#ifdef _RP99R
    case 99u: _RP99R = code; return true;
#endif
#ifdef _RP100R
    case 100u: _RP100R = code; return true;
#endif
#ifdef _RP101R
    case 101u: _RP101R = code; return true;
#endif
#ifdef _RP102R
    case 102u: _RP102R = code; return true;
#endif
#ifdef _RP103R
    case 103u: _RP103R = code; return true;
#endif
#ifdef _RP104R
    case 104u: _RP104R = code; return true;
#endif
#ifdef _RP105R
    case 105u: _RP105R = code; return true;
#endif
#ifdef _RP106R
    case 106u: _RP106R = code; return true;
#endif
#ifdef _RP107R
    case 107u: _RP107R = code; return true;
#endif
#ifdef _RP108R
    case 108u: _RP108R = code; return true;
#endif
#ifdef _RP109R
    case 109u: _RP109R = code; return true;
#endif
#ifdef _RP110R
    case 110u: _RP110R = code; return true;
#endif
#ifdef _RP111R
    case 111u: _RP111R = code; return true;
#endif
#ifdef _RP112R
    case 112u: _RP112R = code; return true;
#endif
#ifdef _RP113R
    case 113u: _RP113R = code; return true;
#endif
#ifdef _RP114R
    case 114u: _RP114R = code; return true;
#endif
#ifdef _RP115R
    case 115u: _RP115R = code; return true;
#endif
#ifdef _RP116R
    case 116u: _RP116R = code; return true;
#endif
#ifdef _RP117R
    case 117u: _RP117R = code; return true;
#endif
#ifdef _RP118R
    case 118u: _RP118R = code; return true;
#endif
#ifdef _RP119R
    case 119u: _RP119R = code; return true;
#endif
#ifdef _RP120R
    case 120u: _RP120R = code; return true;
#endif
#ifdef _RP121R
    case 121u: _RP121R = code; return true;
#endif
#ifdef _RP122R
    case 122u: _RP122R = code; return true;
#endif
#ifdef _RP123R
    case 123u: _RP123R = code; return true;
#endif
#ifdef _RP124R
    case 124u: _RP124R = code; return true;
#endif
#ifdef _RP125R
    case 125u: _RP125R = code; return true;
#endif
#ifdef _RP126R
    case 126u: _RP126R = code; return true;
#endif
#ifdef _RP127R
    case 127u: _RP127R = code; return true;
#endif
#ifdef _RP128R
    case 128u: _RP128R = code; return true;
#endif
    default:
        break;
    }
    return false;   /* RP has no output PPS register on this device */
}


//===========================================================
// Local: is rp a physical remappable pin on this device?
//===========================================================
// PPS input sources and output pins are the same physical RP set, so an RP that
// the device defines an output register (_RPnnR) for is a valid physical RP.
// Used to validate the RP handed to route_input(); route_output()'s write switch
// validates inherently. RPV virtual pins (RP129..) have no _RPnnR and are rejected.
static bool dspic33ak_pps_rp_is_defined(dspic33ak_gpio_rp_t rp)
{
    switch (rp)
    {
#ifdef _RP1R
    case 1u:
#endif
#ifdef _RP2R
    case 2u:
#endif
#ifdef _RP3R
    case 3u:
#endif
#ifdef _RP4R
    case 4u:
#endif
#ifdef _RP5R
    case 5u:
#endif
#ifdef _RP6R
    case 6u:
#endif
#ifdef _RP7R
    case 7u:
#endif
#ifdef _RP8R
    case 8u:
#endif
#ifdef _RP9R
    case 9u:
#endif
#ifdef _RP10R
    case 10u:
#endif
#ifdef _RP11R
    case 11u:
#endif
#ifdef _RP12R
    case 12u:
#endif
#ifdef _RP13R
    case 13u:
#endif
#ifdef _RP14R
    case 14u:
#endif
#ifdef _RP15R
    case 15u:
#endif
#ifdef _RP16R
    case 16u:
#endif
#ifdef _RP17R
    case 17u:
#endif
#ifdef _RP18R
    case 18u:
#endif
#ifdef _RP19R
    case 19u:
#endif
#ifdef _RP20R
    case 20u:
#endif
#ifdef _RP21R
    case 21u:
#endif
#ifdef _RP22R
    case 22u:
#endif
#ifdef _RP23R
    case 23u:
#endif
#ifdef _RP24R
    case 24u:
#endif
#ifdef _RP25R
    case 25u:
#endif
#ifdef _RP26R
    case 26u:
#endif
#ifdef _RP27R
    case 27u:
#endif
#ifdef _RP28R
    case 28u:
#endif
#ifdef _RP29R
    case 29u:
#endif
#ifdef _RP30R
    case 30u:
#endif
#ifdef _RP31R
    case 31u:
#endif
#ifdef _RP32R
    case 32u:
#endif
#ifdef _RP33R
    case 33u:
#endif
#ifdef _RP34R
    case 34u:
#endif
#ifdef _RP35R
    case 35u:
#endif
#ifdef _RP36R
    case 36u:
#endif
#ifdef _RP37R
    case 37u:
#endif
#ifdef _RP38R
    case 38u:
#endif
#ifdef _RP39R
    case 39u:
#endif
#ifdef _RP40R
    case 40u:
#endif
#ifdef _RP41R
    case 41u:
#endif
#ifdef _RP42R
    case 42u:
#endif
#ifdef _RP43R
    case 43u:
#endif
#ifdef _RP44R
    case 44u:
#endif
#ifdef _RP45R
    case 45u:
#endif
#ifdef _RP46R
    case 46u:
#endif
#ifdef _RP47R
    case 47u:
#endif
#ifdef _RP48R
    case 48u:
#endif
#ifdef _RP49R
    case 49u:
#endif
#ifdef _RP50R
    case 50u:
#endif
#ifdef _RP51R
    case 51u:
#endif
#ifdef _RP52R
    case 52u:
#endif
#ifdef _RP53R
    case 53u:
#endif
#ifdef _RP54R
    case 54u:
#endif
#ifdef _RP55R
    case 55u:
#endif
#ifdef _RP56R
    case 56u:
#endif
#ifdef _RP57R
    case 57u:
#endif
#ifdef _RP58R
    case 58u:
#endif
#ifdef _RP59R
    case 59u:
#endif
#ifdef _RP60R
    case 60u:
#endif
#ifdef _RP61R
    case 61u:
#endif
#ifdef _RP62R
    case 62u:
#endif
#ifdef _RP63R
    case 63u:
#endif
#ifdef _RP64R
    case 64u:
#endif
#ifdef _RP65R
    case 65u:
#endif
#ifdef _RP66R
    case 66u:
#endif
#ifdef _RP67R
    case 67u:
#endif
#ifdef _RP68R
    case 68u:
#endif
#ifdef _RP69R
    case 69u:
#endif
#ifdef _RP70R
    case 70u:
#endif
#ifdef _RP71R
    case 71u:
#endif
#ifdef _RP72R
    case 72u:
#endif
#ifdef _RP73R
    case 73u:
#endif
#ifdef _RP74R
    case 74u:
#endif
#ifdef _RP75R
    case 75u:
#endif
#ifdef _RP76R
    case 76u:
#endif
#ifdef _RP77R
    case 77u:
#endif
#ifdef _RP78R
    case 78u:
#endif
#ifdef _RP79R
    case 79u:
#endif
#ifdef _RP80R
    case 80u:
#endif
#ifdef _RP81R
    case 81u:
#endif
#ifdef _RP82R
    case 82u:
#endif
#ifdef _RP83R
    case 83u:
#endif
#ifdef _RP84R
    case 84u:
#endif
#ifdef _RP85R
    case 85u:
#endif
#ifdef _RP86R
    case 86u:
#endif
#ifdef _RP87R
    case 87u:
#endif
#ifdef _RP88R
    case 88u:
#endif
#ifdef _RP89R
    case 89u:
#endif
#ifdef _RP90R
    case 90u:
#endif
#ifdef _RP91R
    case 91u:
#endif
#ifdef _RP92R
    case 92u:
#endif
#ifdef _RP93R
    case 93u:
#endif
#ifdef _RP94R
    case 94u:
#endif
#ifdef _RP95R
    case 95u:
#endif
#ifdef _RP96R
    case 96u:
#endif
#ifdef _RP97R
    case 97u:
#endif
#ifdef _RP98R
    case 98u:
#endif
#ifdef _RP99R
    case 99u:
#endif
#ifdef _RP100R
    case 100u:
#endif
#ifdef _RP101R
    case 101u:
#endif
#ifdef _RP102R
    case 102u:
#endif
#ifdef _RP103R
    case 103u:
#endif
#ifdef _RP104R
    case 104u:
#endif
#ifdef _RP105R
    case 105u:
#endif
#ifdef _RP106R
    case 106u:
#endif
#ifdef _RP107R
    case 107u:
#endif
#ifdef _RP108R
    case 108u:
#endif
#ifdef _RP109R
    case 109u:
#endif
#ifdef _RP110R
    case 110u:
#endif
#ifdef _RP111R
    case 111u:
#endif
#ifdef _RP112R
    case 112u:
#endif
#ifdef _RP113R
    case 113u:
#endif
#ifdef _RP114R
    case 114u:
#endif
#ifdef _RP115R
    case 115u:
#endif
#ifdef _RP116R
    case 116u:
#endif
#ifdef _RP117R
    case 117u:
#endif
#ifdef _RP118R
    case 118u:
#endif
#ifdef _RP119R
    case 119u:
#endif
#ifdef _RP120R
    case 120u:
#endif
#ifdef _RP121R
    case 121u:
#endif
#ifdef _RP122R
    case 122u:
#endif
#ifdef _RP123R
    case 123u:
#endif
#ifdef _RP124R
    case 124u:
#endif
#ifdef _RP125R
    case 125u:
#endif
#ifdef _RP126R
    case 126u:
#endif
#ifdef _RP127R
    case 127u:
#endif
#ifdef _RP128R
    case 128u:
#endif
        return true;
    default:
        return false;
    }
}


//===========================================================
// Global
//===========================================================
bool dspic33ak_pps_route_output(dspic33ak_pps_output_t output, dspic33ak_gpio_rp_t rp)
{
    uint8_t code;
    if (!dspic33ak_pps_get_output_code(output, &code))
    {
        return false;
    }

    dspic33ak_pps_unlock();
    bool ok = dspic33ak_pps_write_output_rp(rp, code);
    dspic33ak_pps_lock();
    return ok;
}

bool dspic33ak_pps_route_input(dspic33ak_pps_input_t input, dspic33ak_gpio_rp_t rp)
{
    bool ok = true;

    if (!dspic33ak_pps_rp_is_defined(rp))
    {
        return false;   /* rp is not a physical remappable pin on this device */
    }

    dspic33ak_pps_unlock();
    switch (input)
    {
#ifdef _U1RXR
    case DSPIC33AK_PPS_INPUT_U1RX: _U1RXR = rp; break;
#endif
#ifdef _SS1R
    case DSPIC33AK_PPS_INPUT_SS1:  _SS1R  = rp; break;
#endif
#ifdef _SCK1R
    case DSPIC33AK_PPS_INPUT_SCK1: _SCK1R = rp; break;
#endif
#ifdef _SDI1R
    case DSPIC33AK_PPS_INPUT_SDI1: _SDI1R = rp; break;
#endif
#ifdef _SS2R
    case DSPIC33AK_PPS_INPUT_SS2:  _SS2R  = rp; break;
#endif
#ifdef _SCK2R
    case DSPIC33AK_PPS_INPUT_SCK2: _SCK2R = rp; break;
#endif
#ifdef _SDI2R
    case DSPIC33AK_PPS_INPUT_SDI2: _SDI2R = rp; break;
#endif
#ifdef _SDI4R
    case DSPIC33AK_PPS_INPUT_SDI4: _SDI4R = rp; break;
#endif
#ifdef _CLCINAR
    case DSPIC33AK_PPS_INPUT_CLCINA: _CLCINAR = rp; break;
#endif
#ifdef _CLCINBR
    case DSPIC33AK_PPS_INPUT_CLCINB: _CLCINBR = rp; break;
#endif
#ifdef _CLCINCR
    case DSPIC33AK_PPS_INPUT_CLCINC: _CLCINCR = rp; break;
#endif
#ifdef _CAN1RXR
    case DSPIC33AK_PPS_INPUT_CAN1RX: _CAN1RXR = rp; break;
#endif
    default:
        ok = false;   /* peripheral input not available on this device */
        break;
    }
    dspic33ak_pps_lock();
    return ok;
}
