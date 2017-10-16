/* 
 * File:   mastercomms.h
 * Author: aece-engineering
 *
 * Created on February 12, 2015, 11:28 AM
 */

#ifndef MASTERCOMMS_H
#define	MASTERCOMMS_H

void ProcessMasterComms(void);                                                  //Wait for and process a command
void ClearPastValues(void);                                                     //Clear past values sent to IBC
void ClearPastValue(unsigned char);                                             //Clear past value sent to IBC
void ClearPacketNumbers(void);                                                  //Reset all packet numbers
void ForceFiringMode(unsigned char);                                            //Force the unit into firing mode
void SetIsolationRelay(unsigned char);                                          //Change the state of the isolation relay

extern unsigned short iscSerialUSG;
extern unsigned short masterCommsTimeoutUSG;

//Commmands/responses from and to the IBC
#define MASTER_CMD_DEFAULT          0b00000000
#define MASTER_CMD_DC_VOLT          0b00000001
#define MASTER_CMD_AC_VOLT          0b00000010
#define MASTER_CMD_BLAST_VLS        0b00000011
//#define MASTER_CMD_TEMP             0b00000100
#define MASTER_CMD_CLOSE_RLY        0b00000101
#define MASTER_CMD_OPEN_RLY         0b00000110
#define MASTER_CMD_GET_SERIAL       0b00000111
#define MASTER_CMD_BLAST            0b00100101
#define MASTER_CMD_PING             0b00101001
#define MASTER_CMD_BL               0b00110001
//#define MASTER_CMD_DISARM         0b00110000
#define MASTER_CMD_BUSY             0b00001111
#define MASTER_CMD_SRL_CHNG         0b00001000
#define MASTER_CMD_SET_SERIAL       0b00001001

//Broadcast and force bits of the command byte
#define MASTER_BIT_BCAST            0b10000000
#define MASTER_BIT_FORCE            0b01000000

//Master comms flags
#define FLAGS_MASTER_BCAST          0b00000001
#define FLAGS_MASTER_DIRECTED       0b00000010
#define FLAGS_MASTER_FORCE          0b00000100

//Payload data array index
#define MASTER_PAYLOAD_DEFAULT      0
#define MASTER_PAYLOAD_DC           1
#define MASTER_PAYLOAD_AC           2
#define MASTER_PAYLOAD_BLAST        3
#define MASTER_PAYLOAD_COUNT        4

//ISC status bits to return to IBC
#define FLAGS_ISC_KEYSWITCH         0b10000000
#define FLAGS_ISC_RELAY             0b01000000
#define FLAGS_ISC_ARMED             0b00100000
#define FLAGS_ISC_CABLE_FAULT       0b00010000
#define FLAGS_ISC_EARTH_LEAKAGE     0b00001000

//Compare window for checking changed data
#define MASTER_WINDOW_VOLTAGE       10

//Array index into packet number array
#define MASTER_PACKET_NUMBER        0
#define MASTER_PACKET_SERIAL        1
#define MASTER_PACKET_COUNT         2

//The packet number window width
#define MASTER_WINDOW_COUNT         10

//Linear feedback shift register mangling bits
#define MASTER_MANGLE_B0            0b0000000000000001
#define MASTER_MANGLE_B1            0b0000000000000010
#define MASTER_MANGLE_B2            0b0000000000000100
#define MASTER_MANGLE_B4            0b0000000000010000
#define MASTER_MANGLE_B6            0b0000000001000000
#define MASTER_MANGLE_B10           0b0000010000000000
#define MASTER_MANGLE_B12           0b0001000000000000
#define MASTER_MANGLE_HIGH          0b1000000000000000
#define MASTER_MANGLE_BITS          0b0000001111111111

//Command values for entering/leaving firing mode
#define MASTER_FIRING_ACTIVE        0
#define MASTER_FIRING_INACTIVE      1

#endif	/* MASTERCOMMS_H */

