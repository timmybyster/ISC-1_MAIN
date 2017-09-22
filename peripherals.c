/* 
 * File:   peripherals.c
 * Author: aece-engineering
 *
 * Created on January 20, 2015, 10:57 AM
 */

#include "main.h"
#include "peripherals.h"
#include "mastercomms.h"

unsigned short nextSerialUSG;                                                   //Next serial number to be assigned to IB651
extern unsigned short commandBLCalibFlag;                                       //calib debug
extern unsigned char  ISO_COUNTER;                                               //isolated 60s counter
extern unsigned short counterELTTests;                                      //Track number of ELT tests performed
extern unsigned short counterELTFailures;                                   //Track number of failures

inline void WaitNewTick(void){                                                  
    while(!(statusFlagsUSLG & FLAG_TICK));                                      //Wait for the start of a new tick
    statusFlagsUSLG &= ~FLAG_TICK;                                              //Unset it and return
}

void WaitTickCount(unsigned short tickCountUS){                                 //Wait a certain number of ticks
    while(tickCountUS--)
        WaitNewTick();
}

inline void PPSUnlockFunc(void){                                                //Unlock PPS to assign modem pins, probably don't even need this
    INTCONbits.GIE = 0;
    EECON2 = 0x55;
    EECON2 = 0xAA;
    PPSCONbits.IOLOCK = 0;
    INTCONbits.GIE = 1;
}

inline void PPSLockFunc(void){                                                  //Relock PPS after assignment
    INTCONbits.GIE = 0;
    EECON2 = 0x55;
    EECON2 = 0xAA;
    PPSCONbits.IOLOCK = 1;
    INTCONbits.GIE = 1;
}

unsigned short ReadAnalogVoltage(unsigned char channelC){                       //Read either the keyswitch, earth leakage or booster comms pins

    switch(channelC){
        case(ADC_VOLT_KS):
            ADCON0bits.CHS = 0b0110;                                            //Select RE1, AN6, KS Read, pin  as input
            break;
        case(ADC_VOLT_EL):
            ADCON0bits.CHS = 0b1010;                         //was 0111 now on B1   //Select RE2, AN7, EL Test, pin  as input
            break;
        case(ADC_VOLT_BC):
            ADCON0bits.CHS = 0b0001;                                            //Select RA1, AN1, BC, pin 20 as input
            break;
    }

    ADCON0bits.GO_nDONE = 1;                                                    //Start conversion
    while(ADCON0bits.GO_nDONE);                                                 //Wait for conversion to finish

    return ADRES;                                                               //Return ADC value
}

void InitEarthLeakage(void){
    counterELTFailures =0;                                                      //clear the ELT counters
    counterELTTests=0;                                                          //clear the ELT counters
    LAT_DC_RX_EN = 0;                                                           //Disable the receive sense
    LAT_EL_EN = 1;                                                              //Enable EL test
}

void ReadEarthLeakage(void){
    unsigned short earthLeakageValueUS;

    counterELTTests++;
    earthLeakageValueUS = ReadAnalogVoltage(ADC_VOLT_EL);                       //Read the EL test pin
    
    if(earthLeakageValueUS > VOLT_EL_HIGH)                                      //Is the value too high?
        counterELTFailures++;                                                   //Increment the counter.... if all tests fail then fault condition
    else
        counterELTFailures==0;                                                  //Else set counter to zero and thus wont trigger alarm
    
    if(counterELTTests == Counter_ELT_Tests){
        LAT_EL_EN = 0;                                                              //Disable EL test
        LAT_DC_RX_EN = 1;                                                           //Enable receive sense

        if(counterELTFailures == counterELTTests)                                   //do all tests done fail? if 1 passes then it wont work
            statusFlagsUSLG |= FLAG_EARTH_LEAKAGE;                                  //Mark as earth leakage fault
        else
            statusFlagsUSLG &= ~FLAG_EARTH_LEAKAGE;                                 //Else mark as all good
    }
}

void ReadKeySwitch(void){                                                       //Read the status of the keyswitch
    unsigned short keySwitchValueUS;

    LAT_KEY_EN = 1;                                                             //Enable keyswitch output
    keySwitchValueUS = ReadAnalogVoltage(ADC_VOLT_KS);                          //Read the input value
    LAT_KEY_EN = 0;                                                             //Disable keyswitch output

    NOP();

    if(keySwitchValueUS > VOLT_KS_ARMED){                                        //Was the value high
        ISO_COUNTER = 0;
        statusFlagsUSLG &= ~FLAG_SWITCH_ARMED;
    }
        else
        statusFlagsUSLG |= FLAG_SWITCH_ARMED;                                   //Else, it's isolated
}

unsigned char FlashReadAddress(unsigned short flashAddressUS){

    TBLPTRU = 0;                                                                //Configure flash read address
    TBLPTRH = flashAddressUS >> 8;
    TBLPTRL = flashAddressUS;

#asm
    TBLRD                                                                       //Read back the location
#endasm

    return TABLAT;                                                              //Return read value
}

void FlashEraseBlock(unsigned short flashAddressUS){
    unsigned char intStatusUC;

    intStatusUC = INTCONbits.GIE;                                               //Save GIE state
    INTCONbits.GIE = 0;

    TBLPTRU = 0;                                                                //Configure address bytes
    TBLPTRH = (flashAddressUS >> 8) & ~0b11;
    TBLPTRU = 0;

    EECON1bits.WREN = 1;                                                        //Erase flash block
    EECON1bits.FREE = 1;
    FlashUnlockSequence();
    EECON1bits.WREN = 0;

    if(intStatusUC)                                                             //Restore GIE state if needed
        INTCONbits.GIE = 1;
}

void FlashUnlockSequence(void){                                                 //Unlock flash for writing
    EECON2 = 0x55;
    EECON2 = 0xAA;
    EECON1bits.WR = 1;
}

void FlashWriteWord(unsigned short flashAddressUS, unsigned char lsbUC, unsigned char msbUC){
    unsigned char intStatusUC;

    intStatusUC = INTCONbits.GIE;                                               //Save GIE state
    INTCONbits.GIE = 0;

    TBLPTRU = 0;                                                                //Configure address bytes
    TBLPTRH = flashAddressUS >> 8;
    TBLPTRL = flashAddressUS;

    TABLAT = lsbUC;                                                             //Load LSB of word

#asm
    TBLWT*+                                                                     //Save value and increment address
#endasm

    TABLAT = msbUC;                                                             //Load MSB of word

#asm
    TBLWT*                                                                      //Save value and don't increment
#endasm

    EECON1bits.WPROG = 1;                                                       //Write latches to flash
    EECON1bits.WREN = 1;
    FlashUnlockSequence();
    EECON1bits.WPROG = 0;
    EECON1bits.WREN = 0;

    if(intStatusUC)                                                             //Restore GIE state if needed
        INTCONbits.GIE = 1;
}

void WriteFlashValues(void){
    FlashEraseBlock(FLASH_NEXT_SERIAL);                                         //Erase the block
    FlashWriteWord(FLASH_NEXT_SERIAL, nextSerialUSG, nextSerialUSG >> 8);       //Write next IB651 serial to flash
    FlashWriteWord(FLASH_ISC_SERIAL, iscSerialUSG, iscSerialUSG >> 8);          //Write current ISC serial to flash
}

void ReadFlashValues(void){
    nextSerialUSG = FlashReadAddress(FLASH_NEXT_SERIAL);                        //Read IB651 LSB serial bits
    nextSerialUSG |= ((unsigned short) FlashReadAddress(FLASH_NEXT_SERIAL+1)) << 8;//Read IB651 USB serial bits
    iscSerialUSG = FlashReadAddress(FLASH_ISC_SERIAL);                          //Read ISC LSB serial bits
    iscSerialUSG |= ((unsigned short) FlashReadAddress(FLASH_ISC_SERIAL+1)) << 8;//Read ISC USB serial bits

    if(nextSerialUSG == 0xFFFF){                                                //Did we read default values?
        nextSerialUSG = 1;                                                      //First IB651 serial should be 1
        iscSerialUSG = PROGRAM_SERIAL_NUMBER;                                   //ISC serial should be default
        WriteFlashValues();                                                     //Save these values
    }
}