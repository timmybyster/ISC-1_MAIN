/* 
 * File:   main.h
 * Author: aece-engineering
 *
 * Created on January 19, 2015, 9:07 AM
 */

#ifndef MAIN_H
#define	MAIN_H

// PIC18F46J13 Configuration Bit Settings

// 'C' source line config statements

#include <xc.h>

// #pragma config statements should precede project file includes.
// Use project enums instead of #define for ON and OFF.

// CONFIG1L
#pragma config WDTEN = OFF      // Watchdog Timer (Disabled - Controlled by SWDTEN bit)
#pragma config PLLDIV = 2//was 1       // 96MHz PLL Prescaler Selection (PLLSEL=0) (No prescale (4 MHz oscillator input drives PLL directly))
#pragma config CFGPLLEN = OFF   // PLL Enable Configuration Bit (PLL Disabled)
#pragma config STVREN = ON      // Stack Overflow/Underflow Reset (Enabled)
#pragma config XINST = OFF      // Extended Instruction Set (Disabled)

// CONFIG1H
#pragma config CP0 = OFF        // Code Protect (Program memory is not code-protected)

// CONFIG2L
#pragma config OSC = INTOSCPLL  // Oscillator (INTOSCPLL)   ECPLL//
#pragma config SOSCSEL = HIGH   // T1OSC/SOSC Power Selection Bits (High Power T1OSC/SOSC circuit selected)
#pragma config CLKOEC = OFF     // EC Clock Out Enable Bit  (CLKO output disabled on the RA6 pin)
#pragma config FCMEN = ON       // Fail-Safe Clock Monitor (Enabled)
#pragma config IESO = ON        // Internal External Oscillator Switch Over Mode (Enabled)

// CONFIG2H
#pragma config WDTPS = 32768    // Watchdog Postscaler (1:32768)

// CONFIG3L
#pragma config DSWDTOSC = INTOSCREF// DSWDT Clock Select (DSWDT uses INTRC)
#pragma config RTCOSC = INTOSCREF// RTCC Clock Select (RTCC uses INTRC)
#pragma config DSBOREN = ON     // Deep Sleep BOR (Enabled)
#pragma config DSWDTEN = ON     // Deep Sleep Watchdog Timer (Enabled)
#pragma config DSWDTPS = G2     // Deep Sleep Watchdog Postscaler (1:2,147,483,648 (25.7 days))

// CONFIG3H
#pragma config IOL1WAY = OFF    // IOLOCK One-Way Set Enable bit (The IOLOCK bit (PPSCON<0>) can be set and cleared as needed)
#pragma config ADCSEL = BIT10   // ADC 10 or 12 Bit Select (10 - Bit ADC Enabled)
#pragma config PLLSEL =  PLL96   // PLL Selection Bit (Selects 96MHz PLL)
#pragma config MSSP7B_EN = MSK7 // MSSP address masking (7 Bit address masking mode)

// CONFIG4L
#pragma config WPFP = PAGE_63   // Write/Erase Protect Page Start/End Location (Write Protect Program Flash Page 63)
#pragma config WPCFG = OFF      // Write/Erase Protect Configuration Region  (Configuration Words page not erase/write-protected)

// CONFIG4H
#pragma config WPDIS = OFF      // Write Protect Disable bit (WPFP<6:0>/WPEND region ignored)
#pragma config WPEND = PAGE_WPFP// Write/Erase Protect Region Select bit (valid when WPDIS = 0) (Pages WPFP<6:0> through Configuration Words erase/write protected)

//==============================================================================

extern volatile unsigned short long statusFlagsUSLG;

#define DEFAULT_SERIAL_NUMBER           0x3FFE                                  //Serial number used for blank units

#define PROGRAM_SERIAL_NUMBER           0x005D                                 //Serial number programmed onto test units LAST PROGRAMMED 0X006B SN 0107

#define BOOTLOADER_STATE_MEMORY         0x3C00
#define SERIAL_MEMORY                   0x4000

//System flags
#define FLAG_NULL                       0b000000000000000000000000
#define FLAG_TICK                       0b000000000000000000000001              //Clock tick generated (every 100us)
#define FLAG_IBC_COMMS_ACTIVE           0b000000000000000000000010              //Communication to the IBC-1 is active
#define FLAG_MODE_DATAnCOMMAND          0b000000000000000000000100              //IB651 data or command mode
#define FLAG_COMMS_INIT                 0b000000000000000000001000              //Used to keep track of states in booster comms
#define FLAG_EARTH_LEAKAGE              0b000000000000000000010000              //Earth leakage fault
#define FLAG_SWITCH_ARMED               0b000000000000000000100000              //Keyswitch state
#define FLAG_BOOSTER_LINE_FAULT         0b000000000000000001000000              //Booster line fault detected
#define FLAG_PHASE2nPHASE1              0b000000000000000010000000              //Booster comms phase
#define FLAG_FRAME_INVALID              0b000000000000000100000000              //Is received frame invalid
#define FLAG_FRAME_SYNC                 0b000000000000001000000000              //Did we get packet sync
#define FLAG_FRAME_COLLISION            0b000000000000010000000000              //Collision on data frame
#define FLAG_COMMAND_SYNC               0b000000000000100000000000              //Command packet sync
#define FLAG_COMMAND_PARITY             0b000000000001000000000000              //Parity matched
#define FLAG_COMMAND_INVALID            0b000000000010000000000000              //Invalid response from command
#define FLAG_COMMAND_COLLISION          0b000000000100000000000000              //Collision from command
#define FLAG_DISP_ACTIVE                0b000000001000000000000000              //Dispatcher is currently active
#define FLAG_RELAY_CLOSED               0b000000010000000000000000              //Is the shaft relay closed
#define FLAG_SERIAL_ASSIGN_ACT          0b000000100000000000000000              //Booster serial number assignment currently active
#define FLAG_SERIAL_ASSIGN_NEW          0b000001000000000000000000              //Serial numbers assigned since last data request
#define FLAG_RELAY_TIMEOUT              0b000100000000000000000000              //Should the shaft relay timeout if we don't receive a packet in x
#define FLAG_FIRING_MODE                0b001000000000000000000000              //We got a firing command and are staying in this state until we see comms
#define FLAG_BOOSTER_LINE_CUTOFF        0b010000000000000000000000              //Booster line fault detected

//Flags for specific window id
#define FLAG_WIN_NULL                   0b00000000
#define FLAG_WIN_SYNC                   0b00000001                              //Is the current window in sync
#define FLAG_WIN_PARITY                 0b00000010                              //Parity matched
#define FLAG_WIN_INVALID                0b00000100                              //Was the frame invalid
#define FLAG_WIN_COLLISION              0b00001000                              //Was there a collision

//Communication modes for booster protocol
#define MODE_COMMS_IDLE                 0                                       //IB651 comms idle
#define MODE_COMMS_START                1                                       //Generate start pulse
#define MODE_COMMS_CMD                  2                                       //Commands to IB651
#define MODE_COMMS_FRAME                3                                       //Single frame, 30 total
#define MODE_DATA_PHASE                 4                                       //Data mode phase
#define MODE_DATA_LED                   5                                       //LED blink phase
#define MODE_COMMS_END                  6                                       //Generate end pulse
#define MODE_COMMS_LED                  7                                       //Led blink phase 2
#define MODE_CABLE_FAULT                8                                       //Cable fault of < 1k detected....
#define MODE_TEST_PHASE                 9                                       //Cable fault of < 1k detected....

//Present state of the current frame
#define MODE_SYNC_PRE                   0                                       //Wait for start pulse
#define MODE_SYNC_START                 1                                       //Wait for end of start pulse
#define MODE_SYNC_BIT                   2                                       //Bit dead time
#define MODE_SYNC_STOP                  3                                       //Wait for end of stop pulse
#define MODE_SYNC_WAIT                  4                                       //Wait for end of frame
#define MODE_SYNC_CMD_PRE               5 

//Present state of the cable fault frame
#define MODE_CFAULT_PRE                 0                                       //Wait for start pulse
#define MODE_CFAULT_START               1                                       //Wait for end of start pulse
#define MODE_CFAULT_TEST                2                                       //Bit dead time
#define MODE_CFAULT_STOP                3                                       //Wait for end of stop pulse
#define MODE_CFAULT_WAIT                4                                       //Wait for end of frame
#define MODE_CFAULT_WAIT2               5                                       //Wait for end of frame
#define MODE_CFAULT_LED_OFF             6                                       //Wait for end of frame
#define MODE_CFAULT_LED_OFF2            7                                       //Wait for end of frame

//Significant times for cable fault mode
#define TIME_CFAULT_1                   39500                                   //Timeout value for not receiving a packet from IBC
#define TIME_CFAULT_2                   40000                                   //Timeout value for not receiving a packet from IBC
#define TIME_CFAULT_3                   35000                                   //Timeout value for not receiving a packet from IBC
#define TIME_CFAULT_4                   40000                                   //Timeout value for not receiving a packet from IBC
#define TIME_CFAULT_5                   40500                                   //Timeout value for not receiving a packet from IBC

//Significant times test phase
#define TIME_TEST_1                     1000                                   //10ms delay before testing allowing all IB651's to settle
#define TIME_TEST_2                     39600                                 //Wait time before led flashing
#define TIME_TEST_3                     40000                                 //Flashing time

#define TMODE_GENERAL                   0
#define TMODE_ISO_WAIT                  1
#define TMODE_ISO_FLASH1                2
#define TMODE_ISO_WAIT2                 3
#define TMODE_ISO_FLASH3                4
//Significant times for sync mode
#define TIME_SYNC_PRE                   75                                      //Pre start is 7.5ms
#define TIME_SYNC_END                   50                                      //Sync period is 5ms
#define TIME_SYNC_START                 80                                      //Start pulse is 8ms
#define TIME_SYNC_BIT                   60                                      //Bit length is 6ms
#define TIME_SYNC_STOP                  110                                     //Bit stop time is 11ms //80       //Bit stop time is 8ms

//Significant times for blast mode
#define TIME_COMMS_TIMEOUT              40000                                   //Timeout value for not receiving a packet from IBC

//Significant values
#define NUM_MIN_START_LVL               TIME_SYNC_START - 10                    //Minimum high pulses for valid start condition
#define NUM_FRAMES_PER_PHASE            15                                      //Number of comms frames per comms phase
#define NUM_BIT_BIT_LEVEL               30                                      //Number of high reads for low level
#define NUM_BIT_LOW_MAX                 50                                      //Threshold for invalid bit
#define NUM_BIT_HIGH_MIN                10                                      //Threshold for invalid bit
#define NUM_MIN_STOP_LVL                TIME_SYNC_STOP - 10                     //Minimum high pulses for valid stop condition

//Array index for booster data
#define DATA_STATUS                     0
#define DATA_DC_VOLT                    1
#define DATA_AC_VOLT                    2
#define DATA_BL_VOLT                    3
#define DATA_MISSED_FRAMES              4
#define DATA_FAULT_FRAMES               5
#define DATA_WIN_STATUS                 6
#define DATA_DEV_TYPE                   7
#define DATA_TYPE_COUNT                 8

//Query values for booster data mode
#define CMD_DATA_STATUS                 0b000                                   //Command definitions, see docs
#define CMD_DATA_MSR36V                 0b001
#define CMD_DATA_MSR220V                0b010
#define CMD_DATA_BLOOP                  0b011

//Command mode command values
#define CMD_CMD_QUERY_SERIAL            0b000
#define CMD_CMD_ASSIGN_SERIAL           0b001
#define CMD_CMD_WRITE_WINID             0b010
#define CMD_CMD_CALC_RESIS              0b011
#define CMD_CMD_READ_RESIS              0b100
#define CMD_CMD_WIN_SERIAL              0b101
#define CMD_CMD_EXTENDED                0b111                                   //Added as an extended instruction set

//Define Extended instruction command set
#define CMD_EXT_BL_CALIB                0b00001                                 //calibrate to the attached load.

//Define frame response bits
#define FRAME_IB_DEV_TYPE               0b1100000000000000
#define FRAME_IB_ID                     0b0011111000000000
#define FRAME_IB_PARITY                 0b0000000100000000
#define FRAME_IB_PAYLOAD                0b0000000011111111
#define FRAME_CMD_HIGH_BIT              0b100000000000000000000000
#define FRAME_WORD_HIGH_BIT             0b1000000000000000

//Booster comms dispatcher states
#define DISP_GET_STATUS                 0
#define DISP_GET_MSR36V                 1
#define DISP_GET_MSR220V                2
#define DISP_GET_BLOOP                  3
#define DISP_SET_WIN                    4
#define DISP_GET_SERIAL                 5
#define DISP_BL_CALIB                   6

//Read serial states
#define SERIAL_IDLE                     0
#define SERIAL_ACTIVE                   1

//Window assignment states
#define WIN_IDLE                        0
#define WIN_CHECK_SERIAL                1
#define WIN_INIT                        3
#define WIN_ACTIVE                      4
#define WIN_ASSIGN                      5

//Timing for booster pl protocol
#define TIME_COMMS_SLOT_2               2
#define TIME_COMMS_DATA_START           200
#define TIME_COMMS_CMD_START            250
#define TIME_COMMS_BL_START             150
#define TIME_COMMS_FRAME                1300
#define TIME_COMMS_LED                  500
#define TIME_COMMS_LED2                 245         //updated to 25ms
#define TIME_COMMS_DONE                 500
#define TIME_COMMS_END                  300
#define TIME_COMMS_CMD_BIT_Q1           65
#define TIME_COMMS_CMD_BIT_Q2           75
#define TIME_COMMS_CMD_BIT_Q3           100

//ELT testing

#define Counter_ELT_Tests               5           //test 5 times
//Define significant addresses, new ones grow down
#define FLASH_ISC_BOOTLOAD              0xFBFC
#define FLASH_ISC_SERIAL                0xFBFD
#define FLASH_NEXT_SERIAL               0xFBFE

//ADC channel commands
#define ADC_VOLT_KS                     0
#define ADC_VOLT_EL                     1
#define ADC_VOLT_BC                     2

//Significant voltages
#define VOLT_BIT_HIGH                   310                                     //+-1V with 3.3V ref at 10 bits
#define VOLT_BIT_COLLISION              620                                     //+-2V with 3.3V ref at 10 bits
#define VOLT_OFFSET_MAX                 410                                     //+-1.33V with 3.3V ref at 10 bits aka a 40mA offset
#define VOLT_KS_ARMED                   80                                      //Got this experimentally
#define VOLT_EL_HIGH                    620                                     //+-2V with 3.3V ref at 10 bits
#define VOLT_BOOSTER_FAULT_HIGH         74                                      // +-0.238V ~5kOhms 3.3V ref at 10 bits      
#define VOLT_BOOSTER_FAULT_LOW          369                                     // +-1.19V ~1kOhm 3.3V ref at 10 bits    

//cable fault variables
#define CABLE_FAULT_FAILURES            3                                     // +-1.19V ~1kOhm 3.3V ref at 10 bits    

//LED definitions

#define TRIS_STATUS_LED                 TRISAbits.TRISA6
#define TRIS_CFAULT_LED                 TRISBbits.TRISB3
#define TRIS_BLOADER_LED                TRISCbits.TRISC0
#define LAT_STATUS_LED                  LATAbits.LATA6
#define LAT_CFAULT_LED                  LATBbits.LATB3                          //LATCbits.LATC0
#define LAT_BLOADER_LED                 LATCbits.LATC0                          //LATAbits.LATA7

#define TRIS_RELAY_LVL                  TRISDbits.TRISD7
#define LAT_RELAY_LVL                   LATDbits.LATD7
#define PORT_RELAY_LVL                  PORTDbits.RD7

#define TRIS_RELAY_SHF                  TRISAbits.TRISA5
#define LAT_RELAY_SHF                   LATAbits.LATA5
#define PORT_RELAY_SHF                  PORTAbits.RA5

//Comms pin definitions
#define TRIS_DC_TX                      TRISAbits.TRISA2
#define LAT_DC_TX                       LATAbits.LATA2
#define PORT_DC_TX                      PORTAbits.RA2

#define TRIS_DC_nTX                     TRISBbits.TRISB2
#define LAT_DC_nTX                      LATBbits.LATB2
#define PORT_DC_nTX                     PORTBbits.RB2

//Earth leakage pin definitions
#define TRIS_EL_EN                      TRISCbits.TRISC4
#define LAT_EL_EN                       LATCbits.LATC4
#define PORT_EL_EN                      PORTCbits.RC4

#define TRIS_EL_READ                    TRISBbits.TRISB1
#define ANCON_EL_READ                   ANCON1bits.PCFG10                       //Analoge\Digital control

//Rx pin definitions
#define TRIS_DC_RX                      TRISAbits.TRISA1
#define ANCON_DC_RX                     ANCON0bits.PCFG1

#define TRIS_DC_RX_EN                   TRISDbits.TRISD1
#define LAT_DC_RX_EN                    LATDbits.LATD1
#define PORT_DC_RX_EN                   PORTDbits.RD1

//Keyswitch pin definitions
#define TRIS_KEY_EN                     TRISEbits.TRISE0          
#define LAT_KEY_EN                      LATEbits.LATE0

#define TRIS_KEY_READ                   TRISEbits.TRISE1
#define ANCON_KEY_READ                  ANCON0bits.PCFG6

#endif	/* MAIN_H */

