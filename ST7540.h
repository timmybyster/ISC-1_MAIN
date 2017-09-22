/*
 * File:   ST7540.h
 * Author: aece-engineering
 *
 * Created on November 26, 2014, 1:23 PM
 */

#ifndef ST7540_H
#define	ST7540_H

#include <xc.h>

unsigned char InitST7540(void);                                                 //Initialize the modem
void SPIISRHandlerST7540(void);                                                 //Handle SPI interupts
void RXReadyISRHandlerST7540(void);                                             //Handle start of packet interupt
unsigned char LineIdleST7540(void);                                             //Check if the line is idle and ready for TX
void CreateMessageST7540(unsigned short, unsigned short, unsigned char, unsigned char, char *); //Create a message frame from parameters
void StartTransmitST7540(void);                                                 //Start transmission of a frame in memory
unsigned char TransmitBusyST7540(void);                                         //Is the transmission still active?
void ReceiveNewDataST7540(void);                                                //Start waiting for new RX packet
unsigned char DataReadyST7540(void);                                            //Has an RX packet been received
unsigned short PacketReadParamST7540(unsigned char);                            //Read specific parameters from a packet
void RetransmitMessageSt7540(void);                                             //Retransmit received message
char *PacketDataST7540(void);                                                   //Return data buffer address for RX packet

//Internal system flags
#define FLAG_ST7540_TX_ACTIVE       1                                           //Are we currently transmitting?
#define FLAG_ST7540_RX_ACTIVE       2                                           //Are we waiting for an RX packet?
#define FLAG_ST7540_DATA_READY      4                                           //Do we have a valid RX packet in memory?

//Configuration constants
#define ST7540_PREAM_LEN            2                                           //Length of the preamble bytes
#define ST7540_HEADER_LEN           2                                           //Length of the packet header
#define ST7540_MIN_PACKET_LEN       9                                           //Minimum allowed packet length
#define ST7540_MAX_PACKET_LEN       (unsigned long short) 75                    //Maximum allowed packet length
#define ST7540_HEADER               (unsigned long short) 0x9b58                //Packet header for frame detection

//Parameter query commands
#define ST7540_DATA_LEN             0                                           //Return length of the RX data
#define ST7540_SOURCE               1                                           //Return packet source
#define ST7540_DEST                 2                                           //Return packet destination
#define ST7540_NUMBER               3                                           //Return the packet number
#define ST7540_CMD                  4                                           //Return the command
#define ST7540_CRC_VALID            5                                           //Check if the CRC is valid
#define ST7540_NEXT_NUMBER          6                                           //Return the packet number for a built TX frame

//Modem FSK frequency configuration
#define ST7540_FREQ_60KHZ           0b000000000000000000000000
#define ST7540_FREQ_66KHZ           0b000000000000000000000001
#define ST7540_FREQ_72KHZ           0b000000000000000000000010
#define ST7540_FREQ_76KHZ           0b000000000000000000000011
#define ST7540_FREQ_82K05HZ         0b000000000000000000000100
#define ST7540_FREQ_86KHZ           0b000000000000000000000101
#define ST7540_FREQ_110KHZ          0b000000000000000000000110
#define ST7540_FREQ_132K5HZ         0b000000000000000000000111

//Modem baud rate configuration
#define ST7540_BAUD_600             0b000000000000000000000000
#define ST7540_BAUD_1200            0b000000000000000000001000
#define ST7540_BAUD_2400            0b000000000000000000010000
#define ST7540_BAUD_4800            0b000000000000000000011000

//Modem deviation configuration
#define ST7540_DEVIA_05             0b000000000000000000000000
#define ST7540_DEVIA_1              0b000000000000000000100000

//Modem watchdog control
#define ST7540_WATCHDOG_DIS         0b000000000000000000000000
#define ST7540_WATCHDOG_EN          0b000000000000000001000000

//Modem transmission timeout value
#define ST7540_TRANS_TOUT_DIS       0b000000000000000000000000
#define ST7540_TRANS_TOUT_1S        0b000000000000000010000000
#define ST7540_TRANS_TOUT_3S        0b000000000000000100000000
//#define ST7540_TRANS_TOUT_NU        0b000000000000000110000000

//Modem frequency detection time
#define ST7540_FREQ_DET_TIME_05MS   0b000000000000000000000000
#define ST7540_FREQ_DET_TIME_1MS    0b000000000000001000000000
#define ST7540_FREQ_DET_TIME_3MS    0b000000000000010000000000
#define ST7540_FREQ_DET_TIME_5MS    0b000000000000011000000000

//#define ST7540_RESERVED             0b000000000000100000000000

//Modem carrier/preamble detect configuration
#define ST7540_PREAM_WO_COND        0b000000000000000000000000
#define ST7540_PREAM_W_COND         0b000000000001000000000000
#define ST7540_CARRIER_WO_COND      0b000000000010000000000000
#define ST7540_CARRIER_W_COND       0b000000000011000000000000

//Modem a/sync selection
#define ST7540_SYNC                 0b000000000000000000000000
#define ST7540_ASYNC                0b000000000100000000000000

//Modem clock output configuration
#define ST7540_OUTP_CLOCK_16MHZ     0b000000000000000000000000
#define ST7540_OUTP_CLOCK_8MHZ      0b000000001000000000000000
#define ST7540_OUTP_CLOCK_4MHZ      0b000000010000000000000000
#define ST7540_OUTP_CLOCK_OFF       0b000000011000000000000000

//Modem output voltage freeze configuration
#define ST7540_OUTP_VOLT_FRZ_EN     0b000000000000000000000000
#define ST7540_OUTP_VOLT_FRZ_DIS    0b000000100000000000000000

//Modem header recognision configuration
#define ST7540_HEADER_RECOG_DIS     0b000000000000000000000000
#define ST7540_HEADER_RECOG_EN      0b000001000000000000000000

//Modem automatic frame length detection
#define ST7540_FRAME_LEN_CNT_DIS    0b000000000000000000000000
#define ST7540_FRAME_LEN_CNT_EN     0b000010000000000000000000

//Modem header length
#define ST7540_HEADER_LENGTH_8      0b000000000000000000000000
#define ST7540_HEADER_LENGTH_16     0b000100000000000000000000

//Modem extended registers
#define ST7540_EXTENDED_REG_DIS_24  0b000000000000000000000000
#define ST7540_EXTENDED_REG_EN_48   0b001000000000000000000000

//Modem sensitivity selection
#define ST7540_SENSITIV_NORM        0b000000000000000000000000
#define ST7540_SENSITIV_HIGH        0b010000000000000000000000

//Modem input filter configuration
#define ST7540_INP_FILTER_DIS       0b000000000000000000000000
#define ST7540_INP_FILTER_EN        0b100000000000000000000000

//Bit masking values
#define UNSIGNED_LONG_SHORT_23_16   0b111111110000000000000000
#define UNSIGNED_LONG_SHORT_15_8    0b000000001111111100000000
#define UNSIGNED_LONG_SHORT_7_0     0b000000000000000011111111

//CD/PD pin configuration
#define TRIS_n_CD_PD                TRISDbits.TRISD3
//#define ANSEL_n_CD_PD             
#define PORT_n_CD_PD                PORTDbits.RD3
#define LAT_n_CD_PD                 LATDbits.LATD3

//Register/Data pin configuration
#define TRIS_REGnDATA               TRISDbits.TRISD2
//#define ANSEL_REGnDATA            
#define PORT_REGnDATA               PORTDbits.RD2
#define LAT_REGnDATA                LATDbits.LATD2

//RX pin configuration
//#define TRIS_RXD                    TRISCbits.TRISC7
////#define ANSEL_RXD
//#define PORT_RXD                    PORTCbits.RC7
//#define LAT_RXD                     LATCbits.LATC7
//nich change debug
#define TRIS_RXD                    TRISDbits.TRISD6
//#define ANSEL_RXD                   
#define PORT_RXD                    PORTDbits.RD6
#define LAT_RXD                     LATDbits.LATD6


//TX pin configuration
#define TRIS_TXD                    TRISDbits.TRISD4
//#define ANSEL_TXD                 
#define PORT_TXD                    PORTDbits.RD4
#define LAT_TXD                     LATDbits.LATD4

//RX/TX selection configuration
#define TRIS_RXnTX                  TRISCbits.TRISC3
//#define ANSEL_RXnTX
#define PORT_RXnTX                  PORTCbits.RC3
#define LAT_RXnTX                   LATCbits.LATC3

//Band in use/thermal shutdown configuration
#define TRIS_BU_THERM               TRISCbits.TRISC2
#define ANSEL_BU_THERM              ANCON1bits.PCFG11
#define PORT_BU_THERM               PORTCbits.RC2
#define LAT_BU_THERM                LATCbits.LATC2

//Clock input configuration
#define TRIS_CLR_T                  TRISDbits.TRISD5
//#define ANSEL_CLR_T               
#define PORT_CLR_T                  PORTDbits.RD5
#define LAT_CLR_T                   LATDbits.LATD5

//UART/SPI select configuration
#define TRIS_UARTnSPI               TRISCbits.TRISC1
//#define ANSEL_UARTnSPI            
#define PORT_UARTnSPI               PORTCbits.RC1
#define LAT_UARTnSPI                LATCbits.LATC1

//RX IOC (SS) pin configuration
#define TRIS_RXD_IOC                TRISBbits.TRISB4
//#define ANSEL_RXD_IOC             
#define PORT_RXD_IOC                PORTBbits.RP4
#define LAT_RXD_IOC                 LATBbits.LATB4

#endif	/* ST7540_H */

