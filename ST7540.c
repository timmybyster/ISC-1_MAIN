/*
 * File:   ST7540.c
 * Author: aece-engineering
 *
 * Created on November 26, 2014, 3:40 PM
 *
 * When the unit is powered on it generates a continious clock stream, this
 * means that we either have to make sure our SPI module is in sync when we want
 * to transmit for initialization or only turn it on after we have signalled
 * that we are initializing, we use this second approach.
 *
 * Module works with header detection and no packet length detection, because:
 *
 * With packet length detection SS is held low from when the header is detected
 * until n bytes have been read, this means that the unit is held in RX until
 * n bytes have been read regardless of how many bytes were sent. The clock is
 * generated continiously while the read operation is in progress but not
 * otherwise. This enables the use of the SS pin as would normally be done but
 * forces us to wait the full length of time the biggest packet would take.
 *
 * Without packet length detection SS is pulsed when a valid header is observed
 * and it is up to the program to distinguish how many bytes to read. The clock
 * is generated continiously regardless if there is data on the line or not.
 * This means we don't have to wait for a full frame but can't use the SS pin as
 * normal.
 *
 * Since a header can be detected in the middle of an frame if sync isn't spot
 * on we have to start the SPI module only when a header is observed, we can't
 * afford the wait time of using length detection (which would have allowed
 * using the SS pin normally) so instead monitor the state of the SS pin and
 * active the SPI module when a valid header pulse is received.
 *
 * Ideally we would want to use an int on chage pin but ISC-1V1 hardware didn't
 * allow this so the pin was monitored every 100us.
 *
 * === 20 Jan 2015 ===
 *
 * Hit another little snag on ISC-1V2, SPI doesn't work in slave mode if SS is
 * disabled. SPI module doesn't even turn on. Workaround is to leave SS assigned
 * to RP31, which is the default PPS assignment.
 *
 * === 5 Feb 2015 ===
 *
 * Ok, two part bug, different pics react differently to interupt flags that
 * aren't cleared once the int is generated. Should thus not if-elseif in an
 * ISR, always use if, if, if. Problem is that SPI int flag gets set regardless
 * whether the int is enabled or not, causing or init routine to enter an SPI
 * int, fixed this by checking if ints are enabled within the spi isr.
 *
 * === 24 Feb 2014 ===
 *
 * Polling while waiting to TX means we miss packets on the line, should redo
 * this lib to use a ring buffer for RX that is always active if not full.
 */

//void interrupt isr(void){
//    if(PIR3bits.SSP2IF){
//        SPIISRHandlerST7540();                                                  //Next RX/TX SPI byte
//    }
//
//    if(INTCONbits.RBIF){
//        discardUC = PORT_RXD_IOC;                                               //Incoming frame from modem
//        RXReadyISRHandlerST7540();                                              //Should we receive it?
//        INTCONbits.RBIF = 0;
//    }
//}

#include "ST7540.h"
#include "crc16.h"
#include "main.h"
#include "peripherals.h"

unsigned char flagST7540UCG;                                                    //Store the internal state of the library
unsigned char bufferTXST7540UCAG[ST7540_MAX_PACKET_LEN+ST7540_HEADER_LEN+ST7540_PREAM_LEN]; //Store the TX packet while transmitting
unsigned char bufferRXST7540UCAG[ST7540_MAX_PACKET_LEN];                        //Stores the last received frame, only valid when not waiting for a new packet
unsigned char bufferTXLenUCG;                                                   //Length of the TX buffer
unsigned char bufferTXNextUCG;                                                  //Next TX byte in the buffer
unsigned char bufferRXLenUCG;                                                   //Length of the RX buffer
unsigned char bufferRXNextUCG;                                                  //Next location for received byte
unsigned char packetNumberUCG = 0;                                              //Packet number for next packet

void InitST7540Pins(void);                                                      //Initialize the peripherals needed by the modem
void WriteConfigST7540(unsigned long short, unsigned long short);               //Write config settings to the modem
void ReadConfigST7540(unsigned long short *, unsigned long short *);            //Read config settings from the modem
unsigned char CheckCRCST7540(void);                                             //Check if the received CRC is valid
char *PacketDataST7540(void);                                                   //Return a pointer to the data buffer of RX packet

unsigned char InitST7540(void){
    unsigned long short frameDataTXUS;                                          //Frame config bits to modem
    unsigned long short frameDataRXUS = 0;                                      //Frame config bits from modem
    unsigned long short configDataTXUS;                                         //Config data to modem
    unsigned long short configDataRXUS = 0;                                     //Config data from modem

    InitST7540Pins();                                                           //Configure required pins

    frameDataTXUS = /*(ST7540_MAX_PACKET_LEN << 16) |*/ ST7540_HEADER;          //Build bits 48-24
    configDataTXUS = ST7540_FREQ_110KHZ |                                       //Build bits 23-0
                   ST7540_BAUD_2400 |                                           //Default
                   ST7540_DEVIA_05 |                                            //Default
                   ST7540_WATCHDOG_DIS |
                   ST7540_TRANS_TOUT_1S |                                       //Default
                   ST7540_FREQ_DET_TIME_1MS |                                   //Default
                   ST7540_PREAM_W_COND |
                   ST7540_SYNC |
                   ST7540_OUTP_CLOCK_8MHZ |// was 16MHz
                   ST7540_OUTP_VOLT_FRZ_DIS |                                   //Default
                   ST7540_HEADER_RECOG_EN |
                   ST7540_FRAME_LEN_CNT_DIS |                                   //Default
                   ST7540_HEADER_LENGTH_16 |                                    //Default
                   ST7540_EXTENDED_REG_EN_48 |
                   ST7540_SENSITIV_HIGH |                                       
                   ST7540_INP_FILTER_DIS;                                       //Default

    SSP2STATbits.SMP = 0;                                                       //Datasheet says we should clear this in slave mode
    SSP2STATbits.CKE = 1;                                                       //SPI TX on high to low
    SSP2CON1bits.CKP = 0;                                                       //Clock idle low
    SSP2CON1bits.SSPM = 0b0100;                                                 //SPI Slave mode, SSX enabled

//    WriteConfigST7540(frameDataTXUS, configDataTXUS);
//    WriteConfigST7540(frameDataTXUS, configDataTXUS);
//    while(1){
//        WriteConfigST7540(frameDataTXUS, configDataTXUS);                     
//        ReadConfigST7540(&frameDataRXUS, &configDataRXUS);
//        WaitTickCount(500);
//    }

    WriteConfigST7540(frameDataTXUS, configDataTXUS);                           //First write to enable extended registers
    WriteConfigST7540(frameDataTXUS, configDataTXUS);                           //Second write to fill in extended registers
    ReadConfigST7540(&frameDataRXUS, &configDataRXUS);                          //Read back config to confirm

    if((frameDataTXUS != frameDataRXUS) && (configDataTXUS != configDataRXUS))
        return 0;                                                               //Return false on error

    PIR3bits.SSP2IF = 0;                                                        //Clear SPI int flag
    PIE3bits.SSP2IE = 1;                                                        //Enable SPI ints
    INTCONbits.PEIE = 1;                                                        //Enable peripheral ints

    return 1;                                                                   //Return true if all went well
}

void WriteConfigST7540(unsigned long short frameDataUS, unsigned long short configDataUS){
    unsigned char discardUC;
    unsigned char bitsSentUC;

    bufferTXST7540UCAG[0] = (frameDataUS & UNSIGNED_LONG_SHORT_23_16) >> 16;    //Format config data for TX
    bufferTXST7540UCAG[1] = (frameDataUS & UNSIGNED_LONG_SHORT_15_8) >> 8;
    bufferTXST7540UCAG[2] = frameDataUS & UNSIGNED_LONG_SHORT_7_0;
    bufferTXST7540UCAG[3] = (configDataUS & UNSIGNED_LONG_SHORT_23_16) >> 16;
    bufferTXST7540UCAG[4] = (configDataUS & UNSIGNED_LONG_SHORT_15_8) >> 8;
    bufferTXST7540UCAG[5] = configDataUS & UNSIGNED_LONG_SHORT_7_0;

    LAT_REGnDATA = 1;                                                           //Register access
    LAT_RXnTX = 0;                                                              //Write to register
    while(PORT_CLR_T);                                                          //Wait for idle clock
    SSP2CON1bits.SSPEN = 1;                                                     //Enable SPI

    for(bitsSentUC = 0; bitsSentUC < 6; bitsSentUC++){
        discardUC = SSP2BUF;                                                    //Clear BF by reading
        SSP2BUF = bufferTXST7540UCAG[bitsSentUC];                               //Send next byte
        while(!SSP2STATbits.BF);                                                //Wait till TX done
    }

    LAT_REGnDATA = 0;                                                           //Data access
    LAT_RXnTX = 1;                                                              //Receive mode
    SSP2CON1bits.SSPEN = 0;                                                     //Disable SPI
    WaitTickCount(20);                                                          //Wait 2ms (max delay, 600 buad)
}

void ReadConfigST7540(unsigned long short *frameDataUS, unsigned long short *configDataUS){
    unsigned char bitsReceivedUC;
    unsigned char discardUC;

    LAT_REGnDATA = 1;                                                           //Register access
    LAT_RXnTX = 1;                                                              //Read from register
    while(PORT_CLR_T);                                                          //Wait for idle clock
    SSP2CON1bits.SSPEN = 1;                                                     //Enable SPI

    discardUC = SSP2BUF;                                                        //Clear BF by reading
    SSP2BUF = 0;                                                                //Send blank value
    for(bitsReceivedUC = 0; bitsReceivedUC < 6; bitsReceivedUC++){
        while(!SSP2STATbits.BF);                                                //Wait till RX done
        bufferRXST7540UCAG[bitsReceivedUC] = SSP2BUF;                           //Save received byte
        SSP2BUF = 0;                                                            //Send blank value
    }

    LAT_REGnDATA = 0;                                                           //Data access
    SSP2CON1bits.SSPEN = 0;                                                     //Disable SPI

    *frameDataUS = 0;
    *configDataUS = 0;
            
    *frameDataUS |= ((unsigned long short) bufferRXST7540UCAG[0] << 16);        //Format data for RX compare
    *frameDataUS |= ((unsigned long short) bufferRXST7540UCAG[1] << 8);
    *frameDataUS |= bufferRXST7540UCAG[2];
    *configDataUS |= ((unsigned long short) bufferRXST7540UCAG[3] << 16);
    *configDataUS |= ((unsigned long short) bufferRXST7540UCAG[4] << 8);
    *configDataUS |= bufferRXST7540UCAG[5];
}

void SPIISRHandlerST7540(void){
    unsigned char dataReadUC;

    if(!PIE3bits.SSP2IE){                                                       //Do we want to trigger on SPI
        PIR3bits.SSP2IF = 0;                                                    //If not, just return. Do this because SPI int flag is set even when ints aren't enabled
        return;
    }

    dataReadUC = SSP2BUF;
    SSP2BUF = 0;

    if(flagST7540UCG & FLAG_ST7540_TX_ACTIVE){                                  //Are we in TX mode
        if(bufferTXNextUCG > bufferTXLenUCG){                                   //Done transmitting buffer?
            flagST7540UCG &= ~FLAG_ST7540_TX_ACTIVE;                            //Disable TX mode
            SSP2CON1bits.SSPEN = 0;                                             //Disable SPI module
            PIR3bits.SSP2IF = 0;                                                //Clear int flag
            LAT_RXnTX = 1;                                                      //Change to read data
            return;
        }
        SSP2BUF = bufferTXST7540UCAG[bufferTXNextUCG++];                        //Send next byte
    }else if(flagST7540UCG & FLAG_ST7540_RX_ACTIVE){                            //Are we in RX mode
        bufferRXST7540UCAG[bufferRXNextUCG++] = dataReadUC;                     //Read next byte
        if(bufferRXST7540UCAG[0] == bufferRXNextUCG){                           //Entire packet read?
            flagST7540UCG |= FLAG_ST7540_DATA_READY;                            //Mark data as ready
            flagST7540UCG &= ~FLAG_ST7540_RX_ACTIVE;                            //Disable RX mode
            SSP2CON1bits.SSPEN = 0;                                             //Disable SPI module
            PIR3bits.SSP2IF = 0;                                                //Clear int flag
            return;
        }
    }

    PIR3bits.SSP2IF = 0;                                                        //Clear int flag
}

void RXReadyISRHandlerST7540(void){
    if((flagST7540UCG & FLAG_ST7540_RX_ACTIVE) && !PORT_RXD_IOC){                //If in RX mode and get negative SS pulse
        SSP2CON1bits.SSPEN = 1;                                                 //Enable SPI       
    }
}

unsigned char LineIdleST7540(void){
    if(PORT_BU_THERM)                                                           //Is the line active
        return 0;                                                               //Return not idle
    return 1;                                                                   //Return idle
}

void RetransmitMessageSt7540(void){
    unsigned short packetSourceUS;
    unsigned short packetDestUS;
    unsigned char commandUC;
    unsigned char oldPacketNumberUC;
    //unsigned char thisPacketNumberUC;
    unsigned char packetDataLenUC;
    char *dataBuf;

    oldPacketNumberUC = packetNumberUCG;                                        //Save our packet number for later

    packetSourceUS = PacketReadParamST7540(ST7540_SOURCE);                      //Get the packet source
    packetDestUS = PacketReadParamST7540(ST7540_DEST);                          //Get the packet destination
    packetNumberUCG = PacketReadParamST7540(ST7540_NUMBER);                     //Get the packet number
    commandUC = PacketReadParamST7540(ST7540_CMD);                              //Get the command
    packetDataLenUC = PacketReadParamST7540(ST7540_DATA_LEN);                   //Get the data length
    dataBuf = PacketDataST7540();                                               //Get the payload

    CreateMessageST7540(packetSourceUS, packetDestUS, commandUC, packetDataLenUC, dataBuf); //Create the packet
    while(!LineIdleST7540() || TransmitBusyST7540());                           //Wait till line is idle
    StartTransmitST7540();                                                      //Send broadcast packet on its way

    packetNumberUCG = oldPacketNumberUC;                                        //Restore our packet number
}

void CreateMessageST7540(unsigned short packetSourceUS, unsigned short packetDestUS, unsigned char commandUC, unsigned char dataLenUC, char *dataBuf){
    unsigned char dataBufLocUC;
    unsigned short packetCRCUS;

    bufferTXST7540UCAG[0] = 0xAA;                                               //Preamble
    bufferTXST7540UCAG[1] = 0xAA;
    bufferTXST7540UCAG[2] = ST7540_HEADER >> 8;                                 //Packet header
    bufferTXST7540UCAG[3] = ST7540_HEADER;
    bufferTXST7540UCAG[4] = ST7540_MIN_PACKET_LEN + dataLenUC;                  //Packet length
    bufferTXST7540UCAG[5] = packetSourceUS >> 8;                                //Packet source
    bufferTXST7540UCAG[6] = packetSourceUS;
    bufferTXST7540UCAG[7] = packetDestUS >> 8;                                  //Packet destination
    bufferTXST7540UCAG[8] = packetDestUS;
    bufferTXST7540UCAG[9] = packetNumberUCG++;                                  //Packet number
    bufferTXST7540UCAG[10] = commandUC;                                         //Command

    for(dataBufLocUC = 0; dataBufLocUC < dataLenUC; dataBufLocUC++)             //Packet payload
        bufferTXST7540UCAG[dataBufLocUC + 11] = dataBuf[dataBufLocUC];
    dataBufLocUC += 11;

    packetCRCUS = CRC16(bufferTXST7540UCAG + 4, ST7540_MIN_PACKET_LEN + dataLenUC - 2); //Packet CRC                                                    //Calcualte CRC
    bufferTXST7540UCAG[dataBufLocUC++] = packetCRCUS >> 8;
    bufferTXST7540UCAG[dataBufLocUC] = packetCRCUS;
    bufferTXLenUCG = dataBufLocUC;                                              //Length of frame
}

void StartTransmitST7540(void){
    unsigned char discardUC;

    flagST7540UCG |= FLAG_ST7540_TX_ACTIVE;                                     //Set TX state as active
    LAT_RXnTX = 0;                                                              //Start TX
    while(PORT_CLR_T);
    SSP2CON1bits.SSPEN = 1;                                                     //Enable SPI module
    discardUC = SSP2BUF;                                                        //Clear BF just in case
    SSP2BUF = bufferTXST7540UCAG[0];                                            //Write first byte
    bufferTXNextUCG = 1;                                                        //Set index to second byte
}

unsigned char TransmitBusyST7540(void){
    return (flagST7540UCG & FLAG_ST7540_TX_ACTIVE);                             //Are we transmitting
}

void ReceiveNewDataST7540(void){
    unsigned char discardUC;

    flagST7540UCG |= FLAG_ST7540_RX_ACTIVE;                                     //Set RX state as active
    flagST7540UCG &= ~FLAG_ST7540_DATA_READY;                                   //Mark data as not ready
    discardUC = SSP2BUF;                                                        //Clear BF just in case
    SSP2BUF = 0;                                                                //Write null byte
    bufferRXNextUCG = 0;                                                        //Set index
}

unsigned short PacketReadParamST7540(unsigned char paramName){
    unsigned short retValueUS = 0;

    switch(paramName){
        case(ST7540_DATA_LEN):                                                  //Retrieve length of the RX data
            return bufferRXST7540UCAG[0] - ST7540_MIN_PACKET_LEN;
        case(ST7540_SOURCE):                                                    //Retrieve the source address
            retValueUS |= ((unsigned short) bufferRXST7540UCAG[1] << 8);
            retValueUS |= bufferRXST7540UCAG[2];
            return retValueUS;
        case(ST7540_DEST):                                                      //Retrieve the destination address
            retValueUS |= ((unsigned short) bufferRXST7540UCAG[3] << 8);
            retValueUS |= bufferRXST7540UCAG[4];
            return retValueUS;
        case(ST7540_NUMBER):                                                    //Retrieve the packet number
            return bufferRXST7540UCAG[5];
        case(ST7540_CMD):                                                       //Retrieve the command
            return bufferRXST7540UCAG[6];
        case(ST7540_CRC_VALID):                                                 //Does the CRC match
            return CheckCRCST7540();
        case(ST7540_NEXT_NUMBER):
            return bufferTXST7540UCAG[9];                                       //Packet number for next TX packet, above commands are all for RX
    }

    return 0;
}

char *PacketDataST7540(void){
    return (bufferRXST7540UCAG + 7);                                            //Return address to start of data
}

unsigned char DataReadyST7540(void){
    return (flagST7540UCG & FLAG_ST7540_DATA_READY);                            //Is the received data ready
}

unsigned char CheckCRCST7540(void){
    unsigned short expectedCRCUS;
    unsigned short receivedCRCUS = 0;
    unsigned char packetLenUC;

    packetLenUC = bufferRXST7540UCAG[0]-2;                                      //Packet length minus CRC len
    expectedCRCUS = CRC16(bufferRXST7540UCAG, packetLenUC);                     //Calculate the expected CRC
    receivedCRCUS |= ((unsigned short) bufferRXST7540UCAG[packetLenUC] << 8);   //Extract received CRC
    receivedCRCUS |= bufferRXST7540UCAG[packetLenUC+1];

    return (expectedCRCUS == receivedCRCUS);                                    //Compare to the received CRC
}

void InitST7540Pins(void){

    PPSUnlockFunc();                                                            //Enable PPS remap

    //PPS inputs
    RPINR21 = 23;     //was 18                                                          //Set SDI2 from RC6
    RPINR22 = 22;                                                               //Set SCK2IN from RD5
    //RPINR23 = 20;                                                             //Set SS2IN from RD3
    //RPINR23 = 
    //Leave SS2IN assigned to RP31, needed for SPI workaround, see notes

    //PPS outputs
    RPOR21 = 10;                                                                //Set SDO2 from RD4
    //RPOR22 = 11;                                                              //Set SCK2 from RD5

    PPSLockFunc();                                                              //Disable PPS remap

    //Carrier/Preamble detect pin as input
    TRIS_n_CD_PD = 1;
    //ANSEL_n_CD_PD = 0;

    //SS IOC pin as input
    TRIS_RXD_IOC = 1;
    //ANSEL_RXD_IOC = 1;

    //IOC enable for SS pin (note, we use two pins)
    INTCONbits.RBIE = 1;                                                        //Enable IOC for RXD_IOC (RB4)

    //Register or data select as output
    TRIS_REGnDATA = 0;
    //ANSEL_REGnDATA = 0;
    LAT_REGnDATA = 0;

    //RX pin as input
    TRIS_RXD = 1;
//    ANSEL_RXD = 0;

    //TX pin as output
    TRIS_TXD = 0;
    //ANSEL_TXD = 0;
    LAT_TXD = 0;

    //RX or TX select as output
    TRIS_RXnTX = 0;
    //ANSEL_RXnTX = 0;
    LAT_RXnTX = 1;

    //Band in use/thermal shutdown, as input
    TRIS_BU_THERM = 1;
    ANSEL_BU_THERM = 1;                                                         //Stupid non standard analog pin assignment

    //Clock input as input
    TRIS_CLR_T = 1;
    //ANSEL_CLR_T = 0;

    //UART/SPI select as output
    TRIS_UARTnSPI = 0;
    //ANSEL_UARTnSPI = 0;
    LAT_UARTnSPI = 0;
}