/* 
 * File:   boostercomms.h
 * Author: aece-engineering
 *
 * Created on February 12, 2015, 12:43 PM
 */

#ifndef BOOSTERCOMMS_H
#define	BOOSTERCOMMS_H

extern unsigned char boosterCommsData[DATA_TYPE_COUNT][30];
extern unsigned short boosterCommsDataSerial[30];                            //Values read back from boosters

void BoosterDataCommandComms(void);                                             //Controls the booster state machine and reads back data
unsigned char BoosterCommsActive(void);                                         //Check if a booster frame
void BoosterQueryStart(unsigned char);                                          //Start the process of querying a booster for data
void BoosterCommandStart(unsigned short, unsigned char, unsigned char);         //Start the process of querying a booster for a command response
void BoosterCommsDispatcher(void);                                              //The dispatcher manages which values get read when, also does window/serial assignment and serial retrieval
void ResetBoosterStates(void);                                                  //Reset all read booster values to 0
inline void SetCommsLow(void);                                                  //Set the booster comms line low
inline void SetCommsHigh(void);                                                 //Set the booster comms line high

#endif	/* BOOSTERCOMMS_H */

