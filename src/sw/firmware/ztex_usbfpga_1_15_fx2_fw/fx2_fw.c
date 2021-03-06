/* Copyright (c) 2012-2013 by the author(s)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * Author(s):
 *   Philipp Wagner <philipp.wagner@tum.de>
 */

/*
 * IO Mapping
 *
 * Name       | FX2 | FPGA | Dir.  | Description
 * -----------------------------------------------------------------------------
 * RESET      | 72  | G20  | OUT   | PC0, soft reset (requires PORTCCFG.0 = 0)
 * IFCLK      | 32  | K20  | OUT   | interface clock
 * FLAGA      | 69  | F20  | OUT   | EP2 empty flag
 * FLAGB      | 70  | F19  | OUT   | FIFOADR selected FIFO full flag (default)
 * FLAGC      | 71  | F18  | OUT   | EP6 full flag
 * FLAGD      | 92  | AB17 | OUT   | EP6 almost full flag (requires PORTACFG.7 = 1)
 * SLRD       | 4   | N22  | IN    | read strobe (active low)
 * SLWR       | 5   | M22  | IN    | write strobe (active low)
 * SLOE       | 84  | U15  | IN    | enable FX2 output on data bus (active low)
 * PKTEND     | 91  | AB5  | IN    | send packet (active low)
 * FIFOADR[0] | 89  | W17  | IN    | select FIFO (lsb)
 * FIFOADR[1] | 90  | Y18  | IN    | select FIFO (msb)
 * FD[0]      | 44  | Y17  | INOUT | data bus (lsb)
 * FD[1]      | 45  | V13  | INOUT | data bus
 * FD[2]      | 46  | W13  | INOUT | data bus
 * FD[3]      | 47  | AA8  | INOUT | data bus
 * FD[4]      | 54  | AB8  | INOUT | data bus
 * FD[5]      | 55  | W6   | INOUT | data bus
 * FD[6]      | 56  | Y6   | INOUT | data bus
 * FD[7]      | 57  | Y9   | INOUT | data bus
 * FD[8]      | 102 | V21  | INOUT | data bus
 * FD[9]      | 103 | V22  | INOUT | data bus
 * FD[10]     | 104 | U20  | INOUT | data bus
 * FD[11]     | 105 | U22  | INOUT | data bus
 * FD[12]     | 121 | R20  | INOUT | data bus
 * FD[13]     | 122 | R22  | INOUT | data bus
 * FD[14]     | 123 | P18  | INOUT | data bus
 * FD[15]     | 124 | P19  | INOUT | data bus (msb)
 *
 * Note: The direction is given from a FX2 point-of-view.
 *
 * References:
 * - ZTEX USB-FPGA Module 1.15 IO mapping documentation:
 *   http://www.ztex.de/downloads/usb-fpga-1.xls
 * - Cypress EZ-USB FX2 CY7C68013A-128AXC datasheet:
 *   http://www.cypress.com/?rID=38801 (p. 17)
 */

#include[ztex-conf.h]
#include[ztex-utils.h]

ENABLE_FLASH;

/*
 * Configure the USB endpoints
 *
 * endpoint 2: OUT, 4x buffered, 512 bytes, interface 0
 * endpoint 6: IN, 4x buffered, 512 bytes, interface 0
 *
 * Note: OUT and IN are seen from the host side (as defined in the USB standard)
 */
EP_CONFIG(2,0,BULK,OUT,512,4);
EP_CONFIG(6,0,BULK,IN,512,4);

/*
 * select ZTEX USB FPGA Module 1.15 as target (required for FPGA configuration)
 */
IDENTITY_UFM_1_15(10.13.0.0,0);

/*
 * Set the product and manufacturer strings.
 *
 * These strings can be set freely, but they are used by liboptimsochost to 
 * identify the device, so make sure that the changes are done in the library
 * as well.
 */
#define[PRODUCT_STRING]["OpTiMSoC - ZTEX USB 1.15"]
#define[MANUFACTURER_STRING]["TUM LIS"]

/*
 * Enables high speed FPGA configuration via EP2
 *
 * With this setting, the bulk endpoint and the CPLD is used for FPGA
 * configuration, reducing the configuration time from approx. 5 seconds to
 * 100 ms.
 */
ENABLE_HS_FPGA_CONF(2);

/*
 * Code to be called after FPGA configuration
 *
 * This configures the FX2 chip to use Slave FIFO mode and resets the system.
 */
#define[POST_FPGA_CONFIG][POST_FPGA_CONFIG
    /*
     * hold system in soft-reset
     */
    IOC0 = 1;
    OEC |= bmBIT0;

    /*
     * clear stall bit on EP6 (taken from ZTEX examples)
     *
     * XXX: Where does this flag come from in the first place? It should be 0 by
     *      default. From the FPGA high-speed configuration?
     */
    EP2CS &= ~bmBIT0;
    EP6CS &= ~bmBIT0;

    /*
     * required for Slave FIFO mode
     */
    REVCTL = 0x3;
    SYNCDELAY;

    /*
     * configure clock and operation mode
     *
     * internal clock (IFCONFIG.7 = 1), 30 MHz (IFCONFIG.6 = 0),
     * drive IFCLK output (IFCONFIG.5 = 1), non-inverted (IFCONFIG.4 = 0)
     *
     * synchronous Slave FIFO mode (IFCONFIG.3 = 0)
     *
     * don't drive GSTATE, it's only for diagnostic purposes (IFCONFIG.2 = 0)
     *
     * Slave FIFO Interface (IFCONFIG[1:0] = 11)
     *
     * References:
     *   TRM, chapter 15.5.2 IFCONFIG and chapter 9.2.3 Interface Clock
     */
    IFCONFIG = bmBIT7 | bmBIT5 | 0x3;
    SYNCDELAY;

    /*
     * NAK all incoming data while we configure the OUT endpoint and reset
     * the internal FIFOs. This is requires to avoid race conditions during
     * configuration.
     */
    FIFORESET = 0x80;
    SYNCDELAY;

    /*
     * configure EP2 (OUT)
     *
     * EP2FIFOCFG.4 = 1 (AUTOOUT)
     * EP2FIFOCFG.3 = 0 (AUTOIN)
     * EP2FIFOCFG.2 = 0 (ZEROLENIN)
     * EP2FIFOCFG.0 = 1 (WORDWIDE, i.e. 16-bit mode)
     *
     * The AUTOOUT=0 to AUTOOUT=1 transition is be needed for the core to see
     * this change under all circumstances ("In order to set this bit it has to
     * transition from a 0 to 1. This is a requirement." [FAQ]).
     *
     * The OUTPKTEND is necessary to get the core to accept any OUT data at all.
     * There is conflicting documentation circulating on if this is necessary
     * or not, but experience shows it is.
     *
     * References:
     * - TRM, chapter 9.3.9 Auto In/Auto Out Initialization
     * - TRM, chapter 15.6.4 EPxFIFOCFG
     * - Setting OUT Endpoint in Auto Mode for FX2LP,
     *   http://www.cypress.com/?id=4&rID=31735 [FAQ]
     * - Cypress AN61345, rev *E, p. 3
     */
    EP2FIFOCFG = 0x00; // switch EP2 to manual mode (AUTOOUT=0)
    SYNCDELAY;
    FIFORESET = 0x02; // reset EP2 FIFO
    SYNCDELAY;
    OUTPKTEND = 0x82; // arm all 4 buffers (EP2 is quad-buffered, see above)
    SYNCDELAY;
    OUTPKTEND = 0x82;
    SYNCDELAY;
    OUTPKTEND = 0x82;
    SYNCDELAY;
    OUTPKTEND = 0x82;
    SYNCDELAY;
    EP2FIFOCFG = bmBIT4 | bmBIT0; // configure EP2: AUTOOUT=1, WORDWIDE=1

    /*
     * Reset EP4, EP6 and EP8 FIFOs
     */
    FIFORESET = 0x04;
    SYNCDELAY;
    FIFORESET = 0x06;
    SYNCDELAY;
    FIFORESET = 0x08;
    SYNCDELAY;

    /*
     * disable NAK-ALL to accept OUT data again.
     */
    FIFORESET = 0x00;
    SYNCDELAY;

    /*
     * configure EP6 (IN)
     *
     * EP6FIFOCFG.4 = 0 (AUTOOUT)
     * EP6FIFOCFG.3 = 1 (AUTOIN)
     * EP6FIFOCFG.2 = 0 (ZEROLENIN)
     * EP6FIFOCFG.0 = 1 (WORDWIDE, i.e. 16-bit mode)
     *
     * References:
     *   TRM, chapter 15.6.4 EPxFIFOCFG
     */
    EP6FIFOCFG = bmBIT3 | bmBIT0;
    SYNCDELAY;

    /*
     * Auto-commit data in 512-byte chunks
     * XXX: should be default!?
     */
    EP6AUTOINLENH = 0x02;
    SYNCDELAY;
    EP6AUTOINLENL = 0x00;
    SYNCDELAY;

    /*
     * Set control signals to be active-low
     */
    FIFOPINPOLAR = 0;
    SYNCDELAY;

    /*
     * Configure EP6 programmable flag (will be FLAGD)
     *
     * It is configured to be an "almost full" flag with one more word to write
     * until the FIFO is full. It otherwise behaves the same as the regular
     * EP6 full flag (EP6FF).
     *
     * DECIS = 0
     * PKTSTAT = 0
     * PKTS = 3
     * PFC = 509 = 0x1FD
     *
     * Reference:
     *   TRM, chapter 15.6.10 and 15.6.11
     */
    EP6FIFOPFH = 0x19;
    SYNCDELAY;
    EP6FIFOPFL = 0xFD;
    SYNCDELAY;

    /*
     * Set meaning of the flags FLAGA through FLAGD
     *
     * FLAGA: EP2 empty flag (EF, "no more data available from host")
     * FLAGB: FIFOADR selected FIFO full flag (FF)
     * FLAGC: EP6 full flag (FF, "sending data to host not possible")
     * FLAGD: EP6 programmable flag (PF, "only one more byte can be sent")
     *
     * To use FLAGD, PORTA must be properly configured (it defaults to PA7).
     *
     * Reference:
     *   TRM, chapter 15.5.3 PINFLAGSxx
     */
    PINFLAGSAB = 0x08;
    SYNCDELAY;
    PINFLAGSCD = 0x6E;
    SYNCDELAY;
    PORTACFG |= bmBIT7;
    SYNCDELAY;

    /*
     * disable soft-reset
     */
    IOC0 = 0;
]

/*
 * Soft reset
 *
 * This defines a vendor command at 0x60 to soft-reset the whole OpTiMSoC
 * system. "Soft-reset" means the FPGA is not reset, only the system's reset
 * functionality is triggered.
 */
ADD_EP0_VENDOR_COMMAND((0x60,,
    if (SETUPDAT[2] == 1) { /* wValueL, see TRM section 2.3 */
        IOC0 = 1;
        uwait(1); // 10 us

        /* reset all FIFOs. See above for an explanation */
        FIFORESET = 0x80;
        SYNCDELAY;

        EP2FIFOCFG = 0x00; // switch EP2 to manual mode (AUTOOUT=0)
        SYNCDELAY;
        FIFORESET = 0x02; // reset EP2 FIFO
        SYNCDELAY;
        OUTPKTEND = 0x82; // arm all 4 buffers (EP2 is quad-buffered, see above)
        SYNCDELAY;
        OUTPKTEND = 0x82;
        SYNCDELAY;
        OUTPKTEND = 0x82;
        SYNCDELAY;
        OUTPKTEND = 0x82;
        SYNCDELAY;
        EP2FIFOCFG = bmBIT4 | bmBIT0; // configure EP2: AUTOOUT=1, WORDWIDE=1

        FIFORESET = 0x04;
        SYNCDELAY;
        FIFORESET = 0x06;
        SYNCDELAY;
        FIFORESET = 0x08;
        SYNCDELAY;

        FIFORESET = 0x00;
        SYNCDELAY;
    } else {
        IOC0 = 0;
    }
,,
    NOP;
));;

#include[ztex.h]

void main(void)
{
    init_USB();

    while (1) {
      /*
       * Nothing is to do for the embedded CPU since we're using the Slave FIFO
       * mode.
       */
    }
}
