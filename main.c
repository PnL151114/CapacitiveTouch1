//******************************************************************************
//  MSP430G2553 - Capacitive Touch, Pin Oscillator Method, 4-sensors
//
//  Description: Basic 4-sensors input using the built-in pin oscillation feature on GPIO input structure.
//  PinOsc signal feed into TA0CLK. Difference in measurements indicate button touch.
//  LEDs flash if input is touched.
//
//  Input 1: LED1 (LED2 off)
//  Input 2: LED2 (LED1 off)
//  Input 3: Toggle LEDs
//
//  ACLK = VLO = 12kHz, MCLK = SMCLK = 1MHz DCO
//
//               MSP430G2xx3
//             -----------------
//         /|\|              XIN|-
//          | |                 |
//          --|RST          XOUT|-
//            |                 |
//            |             P2.2|<--Capacitive Touch Input 1 - Left
//            |                 |
//  LED 2  <--|P1.6         P2.3|<--Capacitive Touch Input 2 - Right
//            |                 |
//  LED 1  <--|P1.0         P2.5|<--Capacitive Touch Input 3 - Top
//            |                 |
//
//   Built with CCS version: 6.0.1.00040
//******************************************************************************

#include  "msp430g2553.h"
#include "inc\msp430_uart.h"

/* Define User Configuration values */
/*----------------------------------*/
/* Defines WDT SMCLK interval for sensor measurements*/
//#define WDT_meas_setting (DIV_SMCLK_512)
#define WDT_meas_setting (DIV_SMCLK_8192)
/* Defines WDT ACLK interval for delay between measurement cycles*/
#define WDT_delay_setting (DIV_ACLK_512)

/* Sensor settings*/
#define NUM_SEN     3                       // Defines number of sensors
//#define KEY_LVL   20                      // Defines threshold for a key press - mica 2mm
//#define KEY_LVL     200                      // Defines threshold for a key press - keo
#define KEY_LVL     500                      // Defines threshold for a key press - keo
//#define PROXIMITY_LVL     10
#define PROXIMITY_LVL     20
/*Set to ~ half the max delta expected*/

/* Definitions for use with the WDT settings*/
#define DIV_ACLK_32768  (WDT_ADLY_1000)     // ACLK/32768
#define DIV_ACLK_8192   (WDT_ADLY_250)      // ACLK/8192
#define DIV_ACLK_512    (WDT_ADLY_16)       // ACLK/512
#define DIV_ACLK_64     (WDT_ADLY_1_9)      // ACLK/64
#define DIV_SMCLK_32768 (WDT_MDLY_32)       // SMCLK/32768
#define DIV_SMCLK_8192  (WDT_MDLY_8)        // SMCLK/8192
#define DIV_SMCLK_512   (WDT_MDLY_0_5)      // SMCLK/512
#define DIV_SMCLK_64    (WDT_MDLY_0_064)    // SMCLK/64

#define LED_1   (0x01)                      // P1.0 LED output
#define LED_2   (0x40)                      // P1.6 LED output

#define SENSOR_PIN1 BIT2
#define SENSOR_PIN2 BIT3
#define SENSOR_PIN3 BIT5
#define SENSOR_PIN4 BIT4

#define SENSOR_DIR  P2DIR
#define SENSOR_SEL  P2SEL
#define SENSOR_SEL2 P2SEL2

#define AVERAGE_TIMES       15
#define FAST_SCAN_TIMEOUT   500

// Global variables for sensing
unsigned int base_cnt[NUM_SEN] = { 0 };
unsigned int meas_cnt[NUM_SEN] = { 0 };
int delta_cnt[NUM_SEN] = { 0 };
unsigned char key_press[NUM_SEN] = { 0 };
unsigned int touch_cnt[NUM_SEN] = { 0  };

int px_base_cnt, px_meas_cnt;
int px_delta_cnt;
int px_touch_cnt;

char key_pressed;
int cycles;

int mode;
const unsigned char electrode_bit[NUM_SEN] =
{ SENSOR_PIN1, SENSOR_PIN2, SENSOR_PIN3 };
/* System Routines*/
void measure_count(void);                   // Measures each capacitive sensor
void px_measure_count(void);
void pulse_LED(void);                       // LED gradient routine

/* Main Function*/
void main(void)
{
    unsigned int i, j;
    WDTCTL = WDTPW | WDTHOLD;                 // Stop watchdog timer

    BCSCTL1 = 0x008D;                         // Set DCO to 8MHz
    DCOCTL = 0x009B;
//  f           CALBC1      CALDCO
//  1MHz        0x0087      0x0043
//  8MHz        0x008D      0x009B
//  12MHz       0x008F      0x0006
//  16MHz       0x008F      0x00A4
    BCSCTL3 |= LFXT1S_2;                      // LFXT1 = VLO

    IE1 |= WDTIE;                             // enable WDT interrupt

    P2SEL = 0x00;                             // No XTAL
    P1DIR = LED_1 | LED_2;                    // P1.0 & P1.6 = LEDs
    P1OUT = 0x00;

    UART_init();

    __bis_SR_register(GIE);                  // Enable interrupts

    measure_count();           // Establish baseline capacitance for each sensor
    for (i = 0; i < NUM_SEN; i++)
        base_cnt[i] = meas_cnt[i];

    for (i = AVERAGE_TIMES; i > 0; i--) // Repeat and avg base measurement for each sensor
    {
        measure_count();
        for (j = 0; j < NUM_SEN; j++)
            base_cnt[j] = (meas_cnt[j] + base_cnt[j]) >> 1;
    }

    mode = 0;

    /* Main loop starts here*/
    while (1)
    {
        //Serialprint("C/T: 1 1 1 2 2 2 3 3 3 4 4 4 \r\n");

//        Serialprint("C/T: ");
//
//        SerialprintInt16(base_cnt[0]); Serialprint(" ");
//        SerialprintInt16(meas_cnt[0]); Serialprint(" ");
//        SerialprintInt16(delta_cnt[0]); Serialprint("   ");
//
//        SerialprintInt16(base_cnt[1]); Serialprint(" ");
//        SerialprintInt16(meas_cnt[1]); Serialprint(" ");
//        SerialprintInt16(delta_cnt[1]); Serialprint("   ");
//
//        SerialprintInt16(base_cnt[2]); Serialprint(" ");
//        SerialprintInt16(meas_cnt[2]); Serialprint(" ");
//        SerialprintInt16(delta_cnt[2]); Serialprint("   ");
//
//        SerialprintInt16(px_base_cnt); Serialprint(" ");
//        SerialprintInt16(px_meas_cnt); Serialprint(" ");
//        SerialprintInt16(px_delta_cnt); Serialprint(" \r\n");

        switch (mode)
        {
            case 1:
                key_pressed = 0;                        // Assume no keys are pressed

                px_measure_count();                        // Measure all sensors

                px_delta_cnt = px_base_cnt - px_meas_cnt;        // Calculate delta: c_change

                /* Handle baseline measurment for a base C decrease*/
                if (px_delta_cnt < 0)                      // If negative: result increased
                {                                       // beyond baseline, i.e. cap dec
                    px_base_cnt = (px_base_cnt + px_meas_cnt) >> 1; // Re-average quickly
                    px_delta_cnt = 0;                    // Zero out for pos determination
                }

                if (px_delta_cnt > KEY_LVL)
                {
                    key_pressed = 1;                    // key pressed
                    px_touch_cnt++;
                }
                else
                {
                    key_pressed = 0;
                    px_touch_cnt = 0;
                }

                if (key_pressed)
                {
                    if (px_touch_cnt > 500)
                    {
                        px_touch_cnt = 0;
                        P1OUT &= ~(LED_1 | LED_2);
                        mode = 0;
                        _delay_cycles(125000);
                    }
                }

                // LED2 indicator proximity threshold
                if (px_delta_cnt > PROXIMITY_LVL)
                {
                    P1OUT |= LED_2;
                    BCSCTL1 = (BCSCTL1 & 0x0CF) + DIVA_0; // ACLK/(0:1,1:2,2:4,3:8)
                    cycles = FAST_SCAN_TIMEOUT;
                }
                else
                {
                    cycles--;
                    if (cycles > 0)
                    {
                        P1OUT |= LED_2;
                        BCSCTL1 = (BCSCTL1 & 0x0CF) + DIVA_0; // ACLK/(0:1,1:2,2:4,3:8)
                    }
                    else
                    {
                        P1OUT &= ~LED_2;
                        BCSCTL1 = (BCSCTL1 & 0x0CF) + DIVA_3; // ACLK/(0:1,1:2,2:4,3:8)
                        cycles = 0;
                    }
                }

                // Fading LED1 depend on proximity level
                for (i = 10; i < 100; i++) // i = 10 to strim out delta_cnt noise
                {

                    if (i < px_delta_cnt)
                        P1OUT |= LED_1;
                    else
                        P1OUT &= ~LED_1;
                }
                break;
            default:
                j = KEY_LVL;

                key_pressed = 0;                        // Assume no keys are pressed

                measure_count();                        // Measure all sensors

                for (i = 0; i < NUM_SEN; i++)
                {
                    delta_cnt[i] = base_cnt[i] - meas_cnt[i]; // Calculate delta: c_change

                    /* Handle baseline measurment for a base C decrease*/
                    if (delta_cnt[i] < 0)               // If negative: result increased
                    {                                   // beyond baseline, i.e. cap dec
                        base_cnt[i] = (base_cnt[i] + meas_cnt[i]) >> 1; // Re-average quickly
                        delta_cnt[i] = 0;              // Zero out for pos determination
                    }
                    if (delta_cnt[i] > j)            // Determine if each key is pressed
                    {                                     // per a preset threshold
                        key_press[i] = 1;                   // Specific key pressed
                        j = delta_cnt[i];
                        key_pressed = i + 1;                  // key pressed
                        touch_cnt[i]++;
                    }
                    else {
                        key_press[i] = 0;
                        touch_cnt[i] = 0;
                    }
                }

                /* Delay to next sample, sample more slowly if no keys are pressed*/
                if (key_pressed)
                {
                    BCSCTL1 = (BCSCTL1 & 0x0CF) + DIVA_0; // ACLK/(0:1,1:2,2:4,3:8)
                    cycles = FAST_SCAN_TIMEOUT;
                }
                else
                {
                    cycles--;
                    if (cycles > 0)
                        BCSCTL1 = (BCSCTL1 & 0x0CF) | DIVA_0; // ACLK/(0:1,1:2,2:4,3:8)
                    else
                    {
                        BCSCTL1 = (BCSCTL1 & 0x0CF) | DIVA_3; // ACLK/(0:1,1:2,2:4,3:8)
                        cycles = 0;
                    }
                }
                WDTCTL = WDT_delay_setting;             // WDT, ACLK, interval timer

                /* Handle baseline measurment for a base C increase*/
                if (!key_pressed)                       // Only adjust baseline down
                {                                       // if no keys are touched
                    for (i = 0; i < NUM_SEN; i++)
                        base_cnt[i] = base_cnt[i] - 1; // Adjust baseline down, should be
                }                                      // slow to accomodate for genuine
                pulse_LED();                           // changes in sensor C

                __bis_SR_register(LPM3_bits);
                break;
        }
    }
}                                           // End Main

/* Measure count result (capacitance) of each sensor*/
/* Routine setup for four sensors, not dependent on NUM_SEN value!*/

void measure_count(void)
{
    char i;

    TA0CTL = TASSEL_3 | MC_2;                   // TACLK, cont mode
    TA0CCTL1 = CM_3 | CCIS_2 | CAP;               // Pos&Neg,GND,Cap

    for (i = 0; i < NUM_SEN; i++)
    {
        P2SEL &= ~SENSOR_PIN4;
        P2SEL2 &= ~SENSOR_PIN4;    // P2.4 -> I/O funtion
        P2DIR |= SENSOR_PIN4;      // P2.4 -> Output
        P2OUT &= ~SENSOR_PIN4;     // P2.4 -> GND

        /*Configure Ports for relaxation oscillator*/
        /*The P2SEL2 register allows Timer_A to receive it's clock from a GPIO*/
        /*See the Application Information section of the device datasheet for info*/
        SENSOR_DIR &= ~electrode_bit[i];
        SENSOR_SEL &= ~electrode_bit[i];
        SENSOR_SEL2 |= electrode_bit[i];                // input oscillation feeds TACLK

        /*Setup Gate Timer*/
        WDTCTL = WDT_meas_setting;              // WDT, SMCLK, interval timer - SCMK/512 = 1MHz/512 = 512us interval
        TA0CTL |= TACLR;                        // Clear Timer_A TAR
        __bis_SR_register(LPM0_bits | GIE);       // Wait for WDT interrupt
        TA0CCTL1 ^= CCIS0; // Create SW capture of CCR1. Because capture mode set at Pos&Neg so that switching input signal between VCC & GND will active capture
        meas_cnt[i] = TACCR1;                   // Save result
        WDTCTL = WDTPW | WDTHOLD;               // Stop watchdog timer
        SENSOR_SEL2 &= ~electrode_bit[i];           // disable input TACLK
    }
    TA0CTL = 0;                             // Stop Timer_A
}

void px_measure_count(void)
{

    TA0CTL = TASSEL_3 + MC_2;                   // TACLK, cont mode
    TA0CCTL1 = CM_3 + CCIS_2 + CAP;             // Pos&Neg,GND,Cap

    SENSOR_SEL2 &= ~(SENSOR_PIN1 | SENSOR_PIN2 | SENSOR_PIN3);    //  -> I/O funtion
    SENSOR_SEL &= ~(SENSOR_PIN1 | SENSOR_PIN2 | SENSOR_PIN3);
    SENSOR_DIR &= ~(SENSOR_PIN1 | SENSOR_PIN2 | SENSOR_PIN3);     //  -> Input


    /*Configure Ports for relaxation oscillator*/
    /*The P2SEL2 register allows Timer_A to receive it's clock from a GPIO*/
    /*See the Application Information section of the device datasheet for info*/
    SENSOR_DIR &= ~ SENSOR_PIN4; // P2.2 is the input used here to get sensor signal
    SENSOR_SEL &= ~ SENSOR_PIN4; // PxSEL.y = 0 & PxSEL2.y = 1 to enable Timer_A clock source form sensor signal
    SENSOR_SEL2 |= SENSOR_PIN4;

    /*Setup Gate Timer*/
    WDTCTL = WDT_meas_setting; // WDT, SMCLK, interval timer - SCMK/512 = 1MHz/512 = 512us interval
    TA0CTL |= TACLR;                            // Clear Timer_A TAR
    __bis_SR_register(LPM0_bits + GIE);         // Wait for WDT interrupt
    TA0CCTL1 ^= CCIS0; // Create SW capture of CCR1. Because capture mode set at Pos&Neg so that switching input signal between VCC & GND will active capture
    px_meas_cnt = TACCR1;                      // Save result
    WDTCTL = WDTPW + WDTHOLD;               // Stop watchdog timer
    SENSOR_SEL2 &= ~SENSOR_PIN4; // Disable sensor signal line to Timer_A clock source

    TA0CTL = 0;                                 // Stop Timer_A
}

void pulse_LED(void)
{
    if (key_press[2])
    {
        if (touch_cnt[2] > 25)
        {
            touch_cnt[2] = 0;
            mode = 1;
            P1OUT &= ~(LED_1 | LED_2);
            _delay_cycles(1000000);
        }
        else
        {
            P1OUT ^= LED_1 | LED_2;
        }
        return;
    }
    else
    {
        if (key_press[0])
        {
            P1OUT |= LED_1;
        }
        else
        {
            P1OUT &= ~LED_1;
        }

        if (key_press[1])
        {
            P1OUT |= LED_2;
        }
        else
        {
            P1OUT &= ~LED_2;
        }
    }
}
/* Watchdog Timer interrupt service routine*/
#pragma vector=WDT_VECTOR
__interrupt void watchdog_timer(void)
{
    TA0CCTL1 ^= CCIS0;                        // Create SW capture of CCR1
    __bic_SR_register_on_exit(LPM3_bits);     // Exit LPM3 on reti
}
