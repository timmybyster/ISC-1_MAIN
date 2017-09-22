/* 
 * File:   mastercomms.c
 * Author: aece-engineering
 *
 * Created on February 12, 2015, 10:53 AM
 */

#include "main.h"
#include "ST7540.h"
#include "mastercomms.h"
#include "boostercomms.h"
#include "peripherals.h"

unsigned char masterFlagsUCG;                                                   //Stores the flags for the modem comms manager
unsigned short masterCommsTimeoutUSG = 0;                                       //Counter to monitor potential comms timeout, needs comms ever x seconds or we go into firing mode
unsigned short iscSerialUSG;                                                    //Current ISC serial number
unsigned short pastBoosterValuesUSAG[MASTER_PAYLOAD_COUNT][31];                 //Booster values already sent to IBC
unsigned short packetNumbersUSAG[MASTER_PACKET_COUNT][31];                      //Packet number of ISC units and IBC

extern unsigned char  ISO_COUNTER;                                               //isolated 60s counter
//extern unsigned char  modemTimeoutFlag;                                         // flag indicating coms received.
//extern unsigned long  modemTimeoutCounter;                                      // ensure the counter gets cleared aech time

void ProcessPingCMD(unsigned short);                                            //Respond to a ping command
void ProcessSerialCMD(unsigned short);                                          //Respond to a serial number request command
void CreateAndSend(unsigned short, unsigned short, unsigned char, unsigned char, char *); //Create a packet and send it with collision avoidance
void ReturnBoosterData(unsigned char, unsigned short);                          //Respond to booster data request
signed char IdlePacketSlot(void);                                               //Find an idle packet number slot
void ClearPacketNumbers(void);                                                  //Clear all saved packet numbers
void SavePacketNumber(unsigned short, unsigned char);                           //Save the packet number for a specific serial
short GetLastPacketNumber(unsigned short);                                      //Get the last packet number from a specirfic serial
void CollisionCheck(void);                                                      //Collision avoidance routine
signed char GetPacketSlot(unsigned short);                                      //Get the location of a packet number for a given serial in the array
void SetSerialNumber(unsigned short);                                           //Set the serial number for an ISC with the default serial
unsigned char UpdatePacketNumber(unsigned short, short);                        //Check if a packet number should be updated and update if so

void ProcessMasterComms(void){
    unsigned char recvCmdUC;
    unsigned short destSerialUS;
    unsigned short respSerialUS;
    unsigned short currentPacketNumberUS;
    unsigned short pastPacketNumberUS;

    masterFlagsUCG = 0;                                                         //Clear the master flags for each received packet
    ReceiveNewDataST7540();                                                     //Start waiting for a new packet

    while(!DataReadyST7540());                                                  //Did we receive a new packet
    if(!PacketReadParamST7540(ST7540_CRC_VALID))                                //Was the CRC valid
        return;
        
    ForceFiringMode(MASTER_FIRING_INACTIVE);                                    //Cancel firing mode if needed
    statusFlagsUSLG |= FLAG_IBC_COMMS_ACTIVE;                                   //Mark the comms as active
    masterCommsTimeoutUSG = 0;                                                  //Clear the comms timeout
//    modemTimeoutFlag = 0;                                                       //Clear the flag to allow for a reset later if needed.
//    modemTimeoutCounter = 0;                                                    //claer the counter so it doesnt auto trigger
    
    recvCmdUC = PacketReadParamST7540(ST7540_CMD);                              //Retrieve the packet command
    destSerialUS = PacketReadParamST7540(ST7540_DEST);                          //Retrieve the packet destination
    respSerialUS = PacketReadParamST7540(ST7540_SOURCE);                        //Retrieve the packet source

    NOP();

    if(recvCmdUC & MASTER_BIT_BCAST){                                           //Was it a broadcast packet?
        masterFlagsUCG |= FLAGS_MASTER_BCAST;                                   //Mark the flags to reflect broadcast
        recvCmdUC &= ~MASTER_BIT_BCAST;                                         //Unset the broadcast bit in the command
    }

    if(recvCmdUC & MASTER_BIT_FORCE){                                           //Was it a forced data request?
        masterFlagsUCG |= FLAGS_MASTER_FORCE;                                   //Mark the flags to reflect force
        recvCmdUC &= ~MASTER_BIT_FORCE;                                         //Unset the force bit in the command
    }

    if(destSerialUS)                                                            //Was it an undirected request?
        masterFlagsUCG |= FLAGS_MASTER_DIRECTED;                                //Mark the flags to reflect undirected

    if(!destSerialUS || (destSerialUS == iscSerialUSG)){                        //Was the packet sent to us or undirected?
        currentPacketNumberUS = PacketReadParamST7540(ST7540_NUMBER);           //Retrieve the packet number
        if(!UpdatePacketNumber(respSerialUS, currentPacketNumberUS))            //Is this packet old?
            return;                                                             //Don't process if so
        switch(recvCmdUC){
            case(MASTER_CMD_DEFAULT):
            case(MASTER_CMD_DC_VOLT):
            case(MASTER_CMD_AC_VOLT):
            case(MASTER_CMD_BLAST_VLS):
//          case(MASTER_CMD_TEMP):
                ReturnBoosterData(recvCmdUC, respSerialUS);                     //Respond with booster data if requested
                break;
            case(MASTER_CMD_CLOSE_RLY):
                SetIsolationRelay(recvCmdUC);                                   //Set level relay to requested state
                CreateAndSend(iscSerialUSG, respSerialUS, recvCmdUC, 0, 0);
            case(MASTER_CMD_OPEN_RLY):
                SetIsolationRelay(recvCmdUC);                                   //Set level relay to requested state
                CreateAndSend(iscSerialUSG, respSerialUS, recvCmdUC, 0, 0);
                break;
            case(MASTER_CMD_GET_SERIAL):
                ProcessSerialCMD(respSerialUS);                                 //Respond with serial numbers if requested
                break;
            case(MASTER_CMD_SET_SERIAL):
                SetSerialNumber(respSerialUS);                                  //Set ISC serial number if instructed
                break;
            case(MASTER_CMD_BLAST):
                ForceFiringMode(MASTER_FIRING_ACTIVE);                          //Force the ISC into firing mode
                //if(statusFlagsUSLG & FLAG_SWITCH_ARMED){
                    statusFlagsUSLG |= FLAG_FIRING_MODE;                        //Mark us as being in firing mode
                    //Stupid bloody freaking compiler, mucks up below if we don't cast
                    statusFlagsUSLG &= ~((unsigned short long) FLAG_DISP_ACTIVE); //Disable the dispatcher
                    LAT_CFAULT_LED = 1;                                         //And turn on the blasting LED                    
                //}
                break;
            case(MASTER_CMD_PING):
                NOP();
                ProcessPingCMD(respSerialUS);                                   //Respond to ping if requested
                break;
                //Software blasting routines below, not implemented yet
//            case(MASTER_CMD_ARM):
                //remember to set flag when arming
//                break;
//            case(MASTER_CMD_DISARM):
                //remember to clear flag when disarming
//                break;
        }
    }else{                                                                      //If packet wasn't for us
        currentPacketNumberUS = PacketReadParamST7540(ST7540_NUMBER);
        if(UpdatePacketNumber(respSerialUS, currentPacketNumberUS) &&           //Update the packet number if it's new
          (masterFlagsUCG & FLAGS_MASTER_BCAST)){                               //If broadcast and packet updated
            CollisionCheck();                                                   //Do some collision avoidance
            RetransmitMessageSt7540();                                          //And broadcast the packet again
        }
    }
}

unsigned char UpdatePacketNumber(unsigned short serialNumberUS, short packetNumberS){
    signed short pastPacketNumberS;
    
    pastPacketNumberS = GetLastPacketNumber(serialNumberUS);                    //Read the last packet number from this serial
    if((packetNumberS > pastPacketNumberS) ||                                   //If it is not within the old packet window
       (packetNumberS < (pastPacketNumberS - MASTER_WINDOW_COUNT)) ||
       (pastPacketNumberS == (-MASTER_WINDOW_COUNT-1))){
       SavePacketNumber(serialNumberUS, packetNumberS);                         //Update the packet number in memory
       return 1;                                                                //And return that we updated
    }
    return 0;                                                                   //Else return that it's an old packet
}

void ForceFiringMode(unsigned char forceModeUC){

    if(forceModeUC == MASTER_FIRING_ACTIVE && !(statusFlagsUSLG & FLAG_BOOSTER_LINE_CUTOFF)){           //If entering firing mode, dont release if in cutoff point <1kOhm resistance
        LAT_RELAY_LVL = 0;                                                      //And switch the booster line to the firing signal
        ResetBoosterStates();                                                   //Clear all old booster values in memory
        ClearPastValues();                                                      //And clear the old values we have sent to the IBC
    }else{                                                                      //If leaving firing mode
        statusFlagsUSLG |= FLAG_IBC_COMMS_ACTIVE;                               //Mark the comms as active
        statusFlagsUSLG &= ~FLAG_FIRING_MODE;                                   //Mark us as having left firing mode
        statusFlagsUSLG |= FLAG_DISP_ACTIVE;                                    //Enable the booster dispatcher
        if(!(statusFlagsUSLG & FLAG_SWITCH_ARMED) ||((statusFlagsUSLG & FLAG_SWITCH_ARMED)&& (ISO_COUNTER >= 15)))
            LAT_RELAY_LVL = 1;                                                      //And set the relay to allow booster comms
        LAT_CFAULT_LED = 0;                                                     //Turn off the firing mode led        
    }
}

void SetSerialNumber(unsigned short respSerialUS){
    unsigned short *dataBufUSP;

    if(iscSerialUSG == DEFAULT_SERIAL_NUMBER){                                  //If we get a change serial command and we have the default serial
        dataBufUSP = (unsigned short *) PacketDataST7540();                     //Retrieve the new serial
        iscSerialUSG = (dataBufUSP[0] << 8) | (dataBufUSP[0] >> 8);             //And sort out the indians.
        if(!iscSerialUSG){                                                      //If it is the undirected serial
            iscSerialUSG = DEFAULT_SERIAL_NUMBER;                               //Ignore it
            CreateAndSend(iscSerialUSG, respSerialUS, MASTER_CMD_SET_SERIAL, 0, 0); //And respond with our old serial
            return;
        }
        WriteFlashValues();                                                     //If not save the new serial
        CreateAndSend(iscSerialUSG, respSerialUS, MASTER_CMD_SET_SERIAL, 2, (char *) &iscSerialUSG); //And reply with it
    }else
        CreateAndSend(iscSerialUSG, respSerialUS, MASTER_CMD_SET_SERIAL, 0, 0); //If we don't have the default serial, respond with it instead
}

unsigned short MangleSerial(unsigned short mangleValueUS){
    unsigned char cycleCountUC;
    unsigned char xorOutputUC;
    unsigned char b0UC, b1UC, b2UC, b4UC, b6UC, b10UC, b12UC;

    NOP();

    for(cycleCountUC = 0; cycleCountUC < 160; cycleCountUC++){                  //Linear feedback shift register to mangle the serial number and get us a semi random value per unit
        b0UC = (mangleValueUS & MASTER_MANGLE_B0)?1:0;
        b1UC = (mangleValueUS & MASTER_MANGLE_B1)?1:0;
        b2UC = (mangleValueUS & MASTER_MANGLE_B2)?1:0;
        b4UC = (mangleValueUS & MASTER_MANGLE_B4)?1:0;
        b6UC = (mangleValueUS & MASTER_MANGLE_B6)?1:0;
        b10UC = (mangleValueUS & MASTER_MANGLE_B10)?1:0;
        b12UC = (mangleValueUS & MASTER_MANGLE_B12)?1:0;
        xorOutputUC = b0UC ^ b1UC ^ b2UC ^ b4UC ^ b6UC ^ b10UC ^ b12UC;
        mangleValueUS = (mangleValueUS >> 1) | (xorOutputUC?MASTER_MANGLE_HIGH:0);
        if(!mangleValueUS)
            mangleValueUS = 1;
    }

    NOP();

    return mangleValueUS;                                                       //Return the mangled value
}

void CollisionCheck(void){
    unsigned short delayValueUS;

    delayValueUS = MangleSerial(iscSerialUSG) & MASTER_MANGLE_BITS;             //Calculate random delay based on serial number
    do{
        WaitTickCount(delayValueUS);                                            //Wait this long
    }while(!LineIdleST7540());                                                  //And redo until line is idle
}

void ClearPacketNumbers(void){
    unsigned char iscCountUC;

    for(iscCountUC = 0; iscCountUC < 31; iscCountUC++){                         //Clear all packet numbers stored internally
        packetNumbersUSAG[MASTER_PACKET_SERIAL][iscCountUC] = 0;
        packetNumbersUSAG[MASTER_PACKET_NUMBER][iscCountUC] = 0;
    }
}

signed char IdlePacketSlot(void){
    signed char iscCountC;

    for(iscCountC = 0; iscCountC < 31; iscCountC++){                            //Look for an idle packet slot in memory
        if(!packetNumbersUSAG[MASTER_PACKET_SERIAL][iscCountC])
            return iscCountC;                                                   //And return it if found
    }
    
    return -1;                                                                  //Return -1 if no idle packet slot left, ie, more than 31 units on the line
}

signed char GetPacketSlot(unsigned short srcSerialUS){
    signed char saveSlotC;

    for(saveSlotC = 0; saveSlotC < 31; saveSlotC++){                            //Search for the packet slot occupied by a given serial number
        if(packetNumbersUSAG[MASTER_PACKET_SERIAL][saveSlotC] == srcSerialUS)
            return saveSlotC;                                                   //And return it if found
    }

    return -1;                                                                  //Or return -1 to indicate not found
}

void SavePacketNumber(unsigned short srcSerialUS, unsigned char packetNumberUC){
    signed char saveSlotC;

    saveSlotC = GetPacketSlot(srcSerialUS);                                     //Get the packet slot for a given serial
    if(saveSlotC == -1){                                                        //If it wasn't found
        saveSlotC = IdlePacketSlot();                                           //Get the next idle packet slot
        if(saveSlotC == -1){                                                    //If no packets slots are idle
            ClearPacketNumbers();                                               //Reset all packet slots
            saveSlotC = IdlePacketSlot();                                       //And get the first idle one
        }
    }
    
    packetNumbersUSAG[MASTER_PACKET_SERIAL][saveSlotC] = srcSerialUS;           //Then save the requested packet number and serial number
    packetNumbersUSAG[MASTER_PACKET_NUMBER][saveSlotC] = packetNumberUC;
}

short GetLastPacketNumber(unsigned short srcSerialUS){
    unsigned char iscCountUC;

    for(iscCountUC = 0; iscCountUC < 31; iscCountUC++){                         //Get the last packet number from a specific serial
        if(packetNumbersUSAG[MASTER_PACKET_SERIAL][iscCountUC] == srcSerialUS)
            return packetNumbersUSAG[MASTER_PACKET_NUMBER][iscCountUC];         //And return it if found
    }
    return (-MASTER_WINDOW_COUNT-1);                                            //If not return out of bound packet number
}

void ClearPastValue(unsigned char valuePosUC){
    pastBoosterValuesUSAG[MASTER_PAYLOAD_DEFAULT][valuePosUC] = 0;              //Clear the past values for a specific unit
    pastBoosterValuesUSAG[MASTER_PAYLOAD_DC][valuePosUC] = 0;
    pastBoosterValuesUSAG[MASTER_PAYLOAD_AC][valuePosUC] = 0;
    pastBoosterValuesUSAG[MASTER_PAYLOAD_BLAST][valuePosUC] = 0;
}

void ClearPastValues(void){
    unsigned char boosterCountUC;

    for(boosterCountUC = 0; boosterCountUC < 31; boosterCountUC++)              //Clear past values for all units
        ClearPastValue(boosterCountUC);
}

void SetIsolationRelay(unsigned char relayStateUC){
    NOP();
    if(relayStateUC == MASTER_CMD_OPEN_RLY){                                    //If we get an open relay command
        statusFlagsUSLG &= ~FLAG_RELAY_CLOSED;                                  //Mark it as open
        statusFlagsUSLG &= ~FLAG_RELAY_TIMEOUT;                                 //And unset the timeout check
        LAT_RELAY_SHF = 1;                                                      //Then open the relay
    }else if(relayStateUC == MASTER_CMD_CLOSE_RLY){                             //If we get a close relay command
        statusFlagsUSLG |= FLAG_RELAY_CLOSED;                                   //Mark it as close
        statusFlagsUSLG |= FLAG_RELAY_TIMEOUT;                                  //Enable the timeout check
        LAT_RELAY_SHF = 0;                                                      //And close the relay
    }
}

void ReturnBoosterData(unsigned char commandValUC, unsigned short respSerialUS){
    unsigned char boosterCountUC;
    unsigned short allRespUSA[31];
    unsigned short partRespUSA[31];
    unsigned char iscStatusUC = 0;
    unsigned char wordCountUC;
    unsigned char respCountUC;
    unsigned char dataValUC;
    unsigned short windowCompareUS = 0;

    NOP();

    if(statusFlagsUSLG & FLAG_SERIAL_ASSIGN_NEW){                               //Has the booster serial numbers changed since last data request
        CreateAndSend(iscSerialUSG, respSerialUS, MASTER_CMD_SRL_CHNG, 0, 0);   //If so report it and refuse to serve data until serial numbers are read again
        return;
    }

    switch(commandValUC){                                                       //Which data do we want to return
        case(MASTER_CMD_DEFAULT):
            dataValUC = MASTER_PAYLOAD_DEFAULT;
            break;
        case(MASTER_CMD_DC_VOLT):
            dataValUC = MASTER_PAYLOAD_DC;
            break;
        case(MASTER_CMD_AC_VOLT):
            dataValUC = MASTER_PAYLOAD_AC;
            break;
        case(MASTER_CMD_BLAST_VLS):
            dataValUC = MASTER_PAYLOAD_BLAST;
            break;
    }

    for(boosterCountUC = 0; boosterCountUC < 31; boosterCountUC++){             //Clear the all and partial response buffers
        allRespUSA[boosterCountUC] = 0;
        partRespUSA[boosterCountUC] = 0;
    }

    allRespUSA[0] = 0b0100000000000000;                                         //Format the ISC response
    if(dataValUC == MASTER_PAYLOAD_DEFAULT){                                    //For default data if needed
        if(statusFlagsUSLG & FLAG_SWITCH_ARMED)
            iscStatusUC |= FLAGS_ISC_KEYSWITCH;
        if(statusFlagsUSLG & FLAG_RELAY_CLOSED)
            iscStatusUC |= FLAGS_ISC_RELAY;
        //if(statusFlagsUSLG & FLAG_BOOSTERS_ARMED)
        //    iscStatusUC |= FLAGS_ISC_ARMED;
        if(statusFlagsUSLG & FLAG_BOOSTER_LINE_FAULT)
            iscStatusUC |= FLAGS_ISC_CABLE_FAULT;
        if(statusFlagsUSLG & FLAG_EARTH_LEAKAGE)
            iscStatusUC |= FLAGS_ISC_EARTH_LEAKAGE;
        allRespUSA[0] |= iscStatusUC;
    }

    for(boosterCountUC = 0, wordCountUC = 1; boosterCountUC < 30; boosterCountUC++){ //Then build up an array of all the requested data
        if(boosterCommsDataSerial[boosterCountUC]){
            allRespUSA[wordCountUC++] = 0b0010000000000000 |
                                         ((unsigned short) (boosterCountUC+1) << 8) |
                                          boosterCommsData[dataValUC][boosterCountUC];
        }
    }

    NOP();

    if(masterFlagsUCG & FLAGS_MASTER_FORCE){                                    //If the request was for all data
        commandValUC |= MASTER_BIT_FORCE;                                       //Reset the force bit in the command
        CreateAndSend(iscSerialUSG, respSerialUS, commandValUC, wordCountUC * 2, (char *) allRespUSA);  //And return all data
    }else{                                                                      //If the request was for changed data
        if(commandValUC != MASTER_CMD_DEFAULT)                                  //Set our compare window based on the command
            windowCompareUS = MASTER_WINDOW_VOLTAGE;
        for(boosterCountUC = 0, respCountUC = 0; boosterCountUC < wordCountUC; boosterCountUC++){ //If any of the data changed since the last request
            if((pastBoosterValuesUSAG[dataValUC][boosterCountUC] < (allRespUSA[boosterCountUC] - windowCompareUS)) ||
               (pastBoosterValuesUSAG[dataValUC][boosterCountUC] > (allRespUSA[boosterCountUC] + windowCompareUS))){
                partRespUSA[respCountUC++] = allRespUSA[boosterCountUC];        //Save it to return to the IBC
            }
        }
        CreateAndSend(iscSerialUSG, respSerialUS, commandValUC, respCountUC * 2, (char *) partRespUSA);  //Return all changed data
    }

    for(boosterCountUC = 0; boosterCountUC < wordCountUC; boosterCountUC++)     //Then save the current data as past data
        pastBoosterValuesUSAG[dataValUC][boosterCountUC] =  allRespUSA[boosterCountUC];
}

void CreateAndSend(unsigned short packetSourceUS, unsigned short packetDestUS, unsigned char commandUC, unsigned char dataLenUC, char *dataBuf){
    unsigned short nextPacketNumberUS;

    if(masterFlagsUCG & FLAGS_MASTER_BCAST)                                     //If the request packet was a broadcast
        commandUC |= MASTER_BIT_BCAST;                                          //Reset the broadcast bit
    CreateMessageST7540(packetSourceUS, packetDestUS, commandUC, dataLenUC, dataBuf); //Create the message in memory
    nextPacketNumberUS = PacketReadParamST7540(ST7540_NEXT_NUMBER);             //Retrieve the outbound serial number
    SavePacketNumber(packetSourceUS, nextPacketNumberUS);                       //And update our own packet number list so we don't broadcast our own packet in the future
    if(!(masterFlagsUCG & FLAGS_MASTER_DIRECTED) ||                             //If it was a broadcast or undirected message
        (masterFlagsUCG & FLAGS_MASTER_BCAST))
        CollisionCheck();                                                       //Do some collision avoidance
    while(!LineIdleST7540() || TransmitBusyST7540());                           //Wait for the line to go idle
    StartTransmitST7540();                                                      //And send it on its way
}

void ProcessSerialCMD(unsigned short respSerialUS){
    unsigned short serialListUSA[30];
    unsigned char boosterCountUC;
    unsigned char boosterIndexUC;

    if(statusFlagsUSLG & FLAG_SERIAL_ASSIGN_ACT)                                //If the boosters are currently assigning or transmitting serial numbers
        CreateAndSend(iscSerialUSG, respSerialUS, MASTER_CMD_BUSY, 0, 0);       //Tell the IBC to bugger off
    
    for(boosterCountUC = 0, boosterIndexUC = 0; boosterCountUC < 30; boosterCountUC++){             //Check through all boster mem
        if(boosterCommsDataSerial[boosterCountUC])                //If we have a serial number for it
            serialListUSA[boosterIndexUC++] = boosterCommsDataSerial[boosterCountUC]; //Save it for TX
    }
    statusFlagsUSLG &= ~FLAG_SERIAL_ASSIGN_NEW;                                 //Mark the serial numbers as read
    CreateAndSend(iscSerialUSG, respSerialUS, MASTER_CMD_GET_SERIAL, boosterIndexUC * 2, (char *) serialListUSA); //And return a list of serial numbers
}

void ProcessPingCMD(unsigned short respSerialUS){
    CreateAndSend(iscSerialUSG, respSerialUS, MASTER_CMD_PING, 0, 0);           //Respond to pin command
}