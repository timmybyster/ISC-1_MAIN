/* 
 * File:   peripherals.h
 * Author: aece-engineering
 *
 * Created on February 12, 2015, 12:40 PM
 */

#ifndef PERIPHERALS_H
#define	PERIPHERALS_H

inline void WaitNewTick(void);                                                  //Wait for the start of a new tick
void WaitTickCount(unsigned short);                                             //Wait x ticks, a tick is 100us
inline void PPSUnlockFunc(void);                                                //Unlock the PPS module for configuration
inline void PPSLockFunc(void);                                                  //Lock the PPS module for configuration
unsigned short ReadAnalogVoltage(unsigned char);                                //Read a specific analog voltage
unsigned char FlashReadAddress(unsigned short);                                 //Read an address from flash
void FlashUnlockSequence(void);                                                 //Flash write unlock sequence
void FlashWriteWord(unsigned short, unsigned char, unsigned char);              //Write a word to flash
void WriteFlashValues(void);                                                    //Write serial numbers to flash
void ReadFlashValues(void);                                                     //Read serial numbers from flash
void InitEarthLeakage(void);                                                    //Initialize the earth leakage read
void ReadEarthLeakage(void);                                                    //Check for an earth leakage problem, should only do this in the booster comms routine
void ReadKeySwitch(void);                                                       //Read the state of the keyswitch

extern unsigned short nextSerialUSG;

#endif	/* PERIPHERALS_H */

