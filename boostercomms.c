/*
 * File:   boostercomms.c
 * Author: aece-engineering
 *
 * Created on November 24, 2014, 12:52 PM
 */

#include "main.h"
#include "peripherals.h"
#include "boostercomms.h"
#include "mastercomms.h"

unsigned char commsStatusUCG;                                                   //Current status of booster comms
unsigned short long commsTXBitsUSLG;                                            //Bits to TX to boosters
unsigned char commsDataModeUCG;                                                 //What command is being requested from boosters
unsigned short commandResponseUS;                                               //Response received from booster for command
unsigned short commandBLCalibFlag;                                               //Response received from booster for command
unsigned char  ISO_COUNTER;                                                     //isolated 60s counter
unsigned short counterELTTests;                                      //Track number of ELT tests performed
unsigned short counterELTFailures;                                   //Track number of failures
unsigned short commsOffsetADCValueUS;                                        //Comms line voltage ofset level (cable fault, general consumtion)
unsigned char  cableFaultCounter;                                        //Comms line voltage ofset level (cable fault, general consumtion)
unsigned char  QueryFailureCounter;

unsigned char missingSerialWinUCS;
unsigned char statusGetSerialUCS;
unsigned char dispStatusUCS;
unsigned char boosterCountUC;       //debug
//unsigned short activeWindowCounter;                                               //Response received from booster for command

//unsigned char boosterCommsData[DATA_TYPE_COUNT][30];                            //Values read back from boosters
unsigned char boosterCommsData[DATA_TYPE_COUNT][30];                            //Values read back from boosters
unsigned short boosterCommsDataSerial[30];                            //Values read back from boosters

void ProcessRXFrame(unsigned short, unsigned char);                             //Process the data received from boosters
unsigned char CheckBoosterRXParity(unsigned short);                             //Check if the received data parity was valid
unsigned char CheckDataCollision(void);                                         //Check if any collisions occured during last data request phase
unsigned char NewBoosterAdded(void);                                            //Check if a new booster has been added to the line
unsigned char SetWindowID(void);                                                //Assign window IDs to the entire line
unsigned char WindowIDBacktrack(unsigned char *, unsigned short *, unsigned char *); //Part of the algo for above function
void AssignSerialNumber(void);                                                  //Assign a serial number to new blank units
void RunBLCalibration(void);                                                  //Assign a serial number to new blank units
unsigned char QueryWinSerial(void);                                             //Query all active window IDs for their serial number
unsigned char LowestMissingSerial(void);                                        //Retrieve the window ID for the first unit on the line we don't have a serial number for
void ResetSingleBoosterState(unsigned char boosterNumberUC);                    //Reset the data associated with a single booster
void CheckLineFault(void);                                                      //Check if there is a line fault on the booster line

void BoosterDataCommandComms(void){
    static unsigned short counterCommsStatusUSS;                                //Track duration of phase
    static unsigned short counterSyncStatusUSS;                                 //Track duration inside sync
    static unsigned char commsBitsSentUCS;                                      //How many cmd bits have we sent
    static unsigned char framesReceivedPhaseUCS;                                //How many receive frames have elapsed
    //static unsigned short counterTestingUSS;
    unsigned short commsADCValueUS;                                             //Comms line voltage level
    static unsigned char syncModeUCS;                                           //Current state of sync mode
    static unsigned char bitsReadUCS;                                           //How many bits have we read
    static unsigned char lvlCountUCS;                                           //How many high levels have we read
    static unsigned short recvFrameUSS;                                         //Received frame
    static unsigned char cableFaultModeUCS;                                           //cable fault mode
    static unsigned char cableTestUCS;

    //counterTestingUSS++;
    counterCommsStatusUSS++;
    counterSyncStatusUSS++;
    switch(commsStatusUCG){
        case(MODE_COMMS_IDLE):
            return;
        case(MODE_COMMS_START):                                                 //Start of booster comms phase
            if(!(statusFlagsUSLG & FLAG_COMMS_INIT)){
                statusFlagsUSLG |= FLAG_COMMS_INIT;                             //Mark init as done
                counterCommsStatusUSS = 0;
                //counterTestingUSS = 0;
                SetCommsLow();                                                  //And set comms low to start sync pulse
            }else if(((statusFlagsUSLG & FLAG_MODE_DATAnCOMMAND) && (counterCommsStatusUSS == TIME_COMMS_DATA_START)) || //If going into data mode and data start pulse time elapsed
                    (!(statusFlagsUSLG & FLAG_MODE_DATAnCOMMAND) && (counterCommsStatusUSS == TIME_COMMS_CMD_START))){ //Or going into command mode and cmd start pulse time elapsed
                commsStatusUCG = MODE_COMMS_CMD;                                //Start sending command bits
                statusFlagsUSLG &= ~FLAG_COMMS_INIT;                            //Unmark init flag
                counterCommsStatusUSS = 0;                                      //Reset the timing counter
                SetCommsHigh();                                                 //And set line high to finish start pulse
            }
            break;
        case(MODE_COMMS_CMD):                                                   //Command bits to boosters
            if(!(statusFlagsUSLG & FLAG_COMMS_INIT)){
                statusFlagsUSLG |= FLAG_COMMS_INIT;
                if(statusFlagsUSLG & FLAG_MODE_DATAnCOMMAND)                    //If in data mode
                    commsTXBitsUSLG <<= 21;                                     //Shift bits to properly format for TX
                commsBitsSentUCS = 0;                                           //Clear bits sent variable
            }else{
                if(counterCommsStatusUSS == TIME_COMMS_CMD_BIT_Q1){             //Transmit bit value to boosters
                    if(!(commsTXBitsUSLG & FRAME_CMD_HIGH_BIT))
                        SetCommsLow();
                }else if(counterCommsStatusUSS == TIME_COMMS_CMD_BIT_Q2){
                    SetCommsLow();
                }else if(counterCommsStatusUSS == TIME_COMMS_CMD_BIT_Q3){       //After bit was sent
                    commsTXBitsUSLG <<= 1;                                      //Shift in next bit
                    commsBitsSentUCS++;
                    SetCommsHigh();
                    if((statusFlagsUSLG & FLAG_MODE_DATAnCOMMAND) && (commsBitsSentUCS == 3)){ // If in data mode and we sent 3 bits
                        commsStatusUCG = MODE_DATA_PHASE;                       //Start reading booster responses
                        statusFlagsUSLG &= ~FLAG_COMMS_INIT;
                    }else if(!(statusFlagsUSLG & FLAG_MODE_DATAnCOMMAND) && (commsBitsSentUCS == 24)){ //If in cmd mode and we sent 24 bits
                        commsStatusUCG = MODE_COMMS_FRAME;                      //Start reading booster responses
                        statusFlagsUSLG &= ~FLAG_COMMS_INIT;
                        framesReceivedPhaseUCS = 0;
                        syncModeUCS = MODE_SYNC_CMD_PRE;// MODE_SYNC_PRE;
                    }
                    counterCommsStatusUSS = 0;                                  //Reset counter, either for next bits or next state
                }
            }
            break;
        case(MODE_COMMS_FRAME):                                                 //Sync on and read data for current frame
            commsADCValueUS = ReadAnalogVoltage(ADC_VOLT_BC);
            switch(syncModeUCS){                
                case(MODE_SYNC_CMD_PRE):
                    if(counterCommsStatusUSS > (TIME_SYNC_PRE - 10) && (framesReceivedPhaseUCS == 0)) //account for cappacitence by only looking after 6.5ms
                        syncModeUCS = MODE_SYNC_PRE;                 //of 7.5ms low. 
                    else if(framesReceivedPhaseUCS != 0)
                        syncModeUCS = MODE_SYNC_PRE;                 //otherwise go strait in.
                    break;                                          // this was implemented due to capacitence on a line that causes
                                                                        //a high pulse that the ISC sees as a unit start bit.
                case(MODE_SYNC_PRE):
                    //LAT_BLOADER_LED = 1;
                    if((commsADCValueUS-commsOffsetADCValueUS) > VOLT_BIT_HIGH){                        //Was the voltage high enough to be a unit in TX
                        syncModeUCS = MODE_SYNC_START;                          //If so we start frame RX
                        statusFlagsUSLG |= FLAG_FRAME_SYNC;                     //And mark the frame as synched
                        counterSyncStatusUSS = 0;
                        lvlCountUCS = 1;                                        //Mark that we have 1 adc reading thats high enough
                    }else if(counterCommsStatusUSS > (TIME_SYNC_PRE + TIME_SYNC_END)) //If we failed to sync in a reasonable time
                        syncModeUCS = MODE_SYNC_WAIT;                           //Go wait for next frame
                    break;
                case(MODE_SYNC_START):                                          //Wait for and validate start pulse
                    if((commsADCValueUS-commsOffsetADCValueUS) > VOLT_BIT_HIGH)                         //If voltage is high enough
                        lvlCountUCS++;                                          //Add to our high count
                    if((commsADCValueUS-commsOffsetADCValueUS) > VOLT_BIT_COLLISION){                   //If value is high enough for collision
                        syncModeUCS = MODE_SYNC_WAIT;                           //Go wait for next frame
                        statusFlagsUSLG |= FLAG_FRAME_COLLISION;                //And mark that there was a collision
                    }
                    if(counterSyncStatusUSS == TIME_SYNC_START){                //If the end pulse is done
                        if(lvlCountUCS < NUM_MIN_START_LVL){                    //Check if we received enough high readings
                            LAT_STATUS_LED = 1; 
                            syncModeUCS = MODE_SYNC_WAIT;                       //If not, wait for next frame
                            statusFlagsUSLG |= FLAG_FRAME_INVALID;              //And mark frame as invalid
                        }else{
                            syncModeUCS = MODE_SYNC_BIT;                        //If valid frame, start reading bits
                            counterSyncStatusUSS = 0;
                            recvFrameUSS = 0;
                            bitsReadUCS = 0;
                            lvlCountUCS = 0;
                        }
                    }
                    break;
                case(MODE_SYNC_BIT):                                            //Read data bits from boosters
                    if((commsADCValueUS-commsOffsetADCValueUS) > VOLT_BIT_HIGH)                         //If voltage is high enough
                        lvlCountUCS++;                                          //Add to our high count
                    if(counterSyncStatusUSS == TIME_SYNC_BIT){                  //If the bit is done
                        recvFrameUSS <<= 1;                                     //Shift our input buffer
                        if((lvlCountUCS < NUM_BIT_HIGH_MIN) || (lvlCountUCS > NUM_BIT_LOW_MAX)){ //And check if bit was invalid
                            syncModeUCS = MODE_SYNC_WAIT;                       //If so, wait for next frame
                            statusFlagsUSLG |= FLAG_FRAME_INVALID;              //And mark the frame as invalid
                        }else if(lvlCountUCS < NUM_BIT_BIT_LEVEL){              //Else if bit was valid
                            recvFrameUSS |= 1;                                  //Mark it as either a 1
                        }                                                       //Or let it default to 0
                        if(++bitsReadUCS == 16)                                 //If we have read 16 bits
                            syncModeUCS = MODE_SYNC_STOP;                       //Wait for stop pulse
                        lvlCountUCS = 0;
                        counterSyncStatusUSS = 0;
                    }
                    break;
                case(MODE_SYNC_STOP):                                           //Wait for stop pulse
                    if((commsADCValueUS-commsOffsetADCValueUS) > VOLT_BIT_HIGH)                         //If voltage is high enough
                        lvlCountUCS++;                                          //Add to our high count
                    if((commsADCValueUS-commsOffsetADCValueUS) > VOLT_BIT_COLLISION){                   //If value is high enough for collision
                        syncModeUCS = MODE_SYNC_WAIT;                           //Go wait for next frame
                        statusFlagsUSLG |= FLAG_FRAME_COLLISION;                //And mark that there was a collision
                    }
                    if(counterSyncStatusUSS == TIME_SYNC_STOP){                 //If the stop pulse is done
                        if(lvlCountUCS < NUM_MIN_STOP_LVL)                      //Check if it was invalid
                            statusFlagsUSLG |= FLAG_FRAME_INVALID;              //And mark it if it was
                        syncModeUCS = MODE_SYNC_WAIT;                           //Either way, go wayt for next frame
                    }
                    break;
                case(MODE_SYNC_WAIT):                                           //Wait for next frame and process window location/frame
                    if(counterCommsStatusUSS == TIME_COMMS_FRAME){              //Wait until the end of the frame
                        ProcessRXFrame(recvFrameUSS, framesReceivedPhaseUCS+1); //And process the received data
                        if(statusFlagsUSLG & FLAG_MODE_DATAnCOMMAND){           //If in data mode
                            commsStatusUCG = MODE_DATA_PHASE;                   //Read next data frame
                        }else{
                            commsStatusUCG = MODE_COMMS_END;                    //Else goto comms end
                            SetCommsLow();
                        }
                        counterCommsStatusUSS = 0;
                        recvFrameUSS = 0;
                        framesReceivedPhaseUCS++;
                        statusFlagsUSLG &= ~FLAG_FRAME_SYNC;                    //Clear values for reading next frame if needed
                        statusFlagsUSLG &= ~FLAG_FRAME_INVALID;
                        statusFlagsUSLG &= ~FLAG_FRAME_COLLISION;                        
                    }
                    break;
            }
            break;
        case(MODE_DATA_PHASE):                                                  //Phase for receiving 15/15 data frames
            if(!(statusFlagsUSLG & FLAG_COMMS_INIT)){
                statusFlagsUSLG |= FLAG_COMMS_INIT;
                framesReceivedPhaseUCS = 0;                                     //Reset the number of frames we have received
            }else{
                if(framesReceivedPhaseUCS == NUM_FRAMES_PER_PHASE){             //If we have received all for this phase
                    if(statusFlagsUSLG & FLAG_PHASE2nPHASE1){                   //And we are in the second phase
                        commsStatusUCG = MODE_COMMS_END;                        //Goto comms end
                        statusFlagsUSLG &= ~FLAG_COMMS_INIT;
                        statusFlagsUSLG &= ~FLAG_PHASE2nPHASE1;
                        SetCommsLow();
                    }else{                                                      //If we are in the first phase
                        commsStatusUCG = MODE_DATA_LED;                         //Go blink an LED, which will return us here
                        framesReceivedPhaseUCS = 0;
                    }
                }else{                                                          //If we need to receive more frames
                    commsStatusUCG = MODE_COMMS_FRAME;                          //Go get the next one
                    syncModeUCS = MODE_SYNC_CMD_PRE;// MODE_SYNC_PRE;                                //Set us as not synched
                }
            }
            break;
        case(MODE_DATA_LED):                                                    //Blink led inbetween data phases
            if(counterCommsStatusUSS == TIME_COMMS_SLOT_2){
                if(statusFlagsUSLG & FLAG_FIRING_MODE)                          //Leave the LED alone in firing mode
                    1;

                //else if(!(statusFlagsUSLG & FLAG_BOOSTER_LINE_FAULT) || !(statusFlagsUSLG & FLAG_IBC_COMMS_ACTIVE)){ //OLD - KEEP
                else if(!(statusFlagsUSLG & FLAG_BOOSTER_LINE_FAULT) && (statusFlagsUSLG & FLAG_IBC_COMMS_ACTIVE)&&!(statusFlagsUSLG & FLAG_SWITCH_ARMED)&&!(statusFlagsUSLG & FLAG_EARTH_LEAKAGE)){  //We do not have a Line Fault and coms is ok
                    LAT_STATUS_LED = 1;         
                }                
                else if((statusFlagsUSLG & FLAG_BOOSTER_LINE_FAULT) || (statusFlagsUSLG & FLAG_EARTH_LEAKAGE)){
                    LAT_CFAULT_LED = 1;                    
                    1;
                }   //let cable fault have higher priority in flash routine.
                else if (!(statusFlagsUSLG & FLAG_IBC_COMMS_ACTIVE)||(statusFlagsUSLG & FLAG_SWITCH_ARMED)){           //We do not have COMMS or key is isolated
                    LAT_BLOADER_LED = 1;
                }
            }else if(counterCommsStatusUSS == TIME_COMMS_LED){                  //After LED blink phase
                commsStatusUCG = MODE_DATA_PHASE;                               //Go to phase 2 of data comms
                statusFlagsUSLG |= FLAG_PHASE2nPHASE1;
                counterCommsStatusUSS = 0;
                if(!(statusFlagsUSLG & FLAG_FIRING_MODE)){                      //If we aren't in firing mode, turn off the leds
                    LAT_STATUS_LED = 0;
                    LAT_CFAULT_LED = 0;
                    LAT_BLOADER_LED = 0;                    
                }
            }
            break;
        case(MODE_COMMS_END):                                                   //End pulse phase
            if(counterCommsStatusUSS == TIME_COMMS_END){                        //If end pulse done
                commsStatusUCG = MODE_COMMS_LED;                                //Goto blink LED and measure line
                counterCommsStatusUSS = 0;
                SetCommsHigh();                                                 //And set line high to finish stop pulse
            }
            break;
        case(MODE_COMMS_LED):                                                   //Blink final led and measure line
            if(!(statusFlagsUSLG & FLAG_COMMS_INIT)){
                statusFlagsUSLG |= FLAG_COMMS_INIT;
                if(statusFlagsUSLG & FLAG_FIRING_MODE)                          //Leave the LED alone in firing mode
                    1;               
                
                //else if((statusFlagsUSLG & FLAG_BOOSTER_LINE_FAULT) || !(statusFlagsUSLG & FLAG_IBC_COMMS_ACTIVE)){                
                else if((statusFlagsUSLG & FLAG_BOOSTER_LINE_FAULT) || (statusFlagsUSLG & FLAG_EARTH_LEAKAGE) || !(statusFlagsUSLG & FLAG_IBC_COMMS_ACTIVE)){

                    if (statusFlagsUSLG & FLAG_MODE_DATAnCOMMAND){  //This if statement added so that it flashes correctly when there is a CF.
                        LAT_CFAULT_LED = 1;                         //Goes into Command mode directly after a CF which causes another blink
                    }
                    else                   
                        LAT_BLOADER_LED = 1;
                }
                else if(!(statusFlagsUSLG & FLAG_MODE_DATAnCOMMAND))            //Blink blue in command mode                    
                        LAT_BLOADER_LED = 1;
                else{
                    LAT_STATUS_LED = 1;   
                }
                //ReadKeySwitch();                                                //Read keyswitch  
                //CheckLineFault();                                               //Check for a booster line fault
                InitEarthLeakage();                                             //Check for an earth leakage fault
                counterELTTests =0;                                             //ensure the counter starts at zero
            //}else if(counterCommsStatusUSS == TIME_COMMS_LED2){//}else if(counterCommsStatusUSS >= TIME_COMMS_LED2 && counterELTTests <=Counter_ELT_Tests){
            }else if(counterCommsStatusUSS >= TIME_COMMS_LED2 && counterELTTests <=Counter_ELT_Tests){//
                ReadEarthLeakage();                                             //Read earth leakage fault after x
            }else if(counterCommsStatusUSS == TIME_COMMS_DONE){                 //After idle pulse
                statusFlagsUSLG &= ~FLAG_COMMS_INIT;
                commsStatusUCG = MODE_TEST_PHASE;                               //Set comms as idle and wait for dispatcher
                cableTestUCS = TMODE_GENERAL;                       //go to start test.
                counterCommsStatusUSS = 0;
                if(!(statusFlagsUSLG & FLAG_FIRING_MODE)){                      //If we aren't in firing mode, turn off the leds
                    LAT_STATUS_LED = 0;
                    LAT_CFAULT_LED = 0;
                    LAT_BLOADER_LED = 0;
                }
                //read the default adc value just before starting comms, used for determining collistion detection and a high
                //aka the base current draw due to boosters being connected.
//                commsOffsetADCValueUS = ReadAnalogVoltage(ADC_VOLT_BC)-20;
//                if(commsOffsetADCValueUS>600){
//                    commsOffsetADCValueUS = 0;                     // cap this reading at the offset amount
//                }
//                else if(commsOffsetADCValueUS>VOLT_OFFSET_MAX){
//                    commsOffsetADCValueUS = VOLT_OFFSET_MAX;                     // cap this reading at the offset amount
//                }
            }
            break;
        case(MODE_CABLE_FAULT):
            counterCommsStatusUSS++;
            switch(cableFaultModeUCS){
                case(MODE_CFAULT_PRE):
                    counterCommsStatusUSS = 0;              //restart the 5 second counter
                    cableFaultModeUCS = MODE_CFAULT_WAIT;
                    break;
                case(MODE_CFAULT_WAIT):
                    if(counterCommsStatusUSS == TIME_CFAULT_1){                  //2s elapsed
                        cableFaultModeUCS = MODE_CFAULT_LED_OFF;                  //
                        LAT_CFAULT_LED = 1;                                     //turn on the cable fault LED
                    }
                    break;
                case(MODE_CFAULT_LED_OFF):                                        
                    if(counterCommsStatusUSS == TIME_CFAULT_2){                  //2s elapsed
                        cableFaultModeUCS = MODE_CFAULT_WAIT2;                  //turn line on to test
                        LAT_CFAULT_LED = 0;                                     //turn off the cable fault LED
                        counterCommsStatusUSS=0;
                    }
                    break;
                case(MODE_CFAULT_WAIT2):
                    if(counterCommsStatusUSS == TIME_CFAULT_3)                  //2s elapsed
                        cableFaultModeUCS = MODE_CFAULT_START;                  //turn line on to test
                    break;
                case(MODE_CFAULT_START):                                        
                    SetCommsHigh();                                             //Turn power on to perform test again
                    cableFaultModeUCS = MODE_CFAULT_TEST;                       //go to test phase
                    break;
                case(MODE_CFAULT_TEST):
                    if(counterCommsStatusUSS == TIME_CFAULT_4){                 // if 500ms elapsed since power provided then test.
                        CheckLineFault();                                               //Check for a booster line fault
                        cableFaultModeUCS = MODE_CFAULT_STOP;                       //go to stop test.
                        LAT_CFAULT_LED = 1;                                     //turn on the cable fault LED
                    }
                    break;
                case(MODE_CFAULT_STOP):
                    if(counterCommsStatusUSS == TIME_CFAULT_5){                 // if 50ms
                        LAT_CFAULT_LED = 0;                                     //turn off the cable fault LED
                        cableFaultModeUCS = MODE_CFAULT_PRE;                    //ensure that it goes to start again
                        if(statusFlagsUSLG & FLAG_BOOSTER_LINE_CUTOFF)
                            SetCommsLow();                                              //Turn power off to protect
                        else
                            commsStatusUCG = MODE_COMMS_IDLE;                  //go back to normal communicaiton as fault has been corrected or improved.
                                                                               //above the cutoff limit.
                    }
                    break;
            }
            break;
        case(MODE_TEST_PHASE):
            counterCommsStatusUSS++;
            switch(cableTestUCS){
                case(TMODE_GENERAL):
                    if(counterCommsStatusUSS == TIME_TEST_1){                  //10ms elapsed delay for all IB651's to settle 
                        if((statusFlagsUSLG & FLAG_MODE_DATAnCOMMAND)){                       // only check if in data mode
                            ReadKeySwitch();                                                //Read keyswitch  
                            CheckLineFault();                                               //Check for a booster line fault
                        }
                        if ((statusFlagsUSLG & FLAG_SWITCH_ARMED) && ISO_COUNTER == 0){
                            counterCommsStatusUSS = 0;                          //restart 2s counter
                            cableTestUCS = TMODE_ISO_WAIT;                     //send to second phase
                            SetCommsLow();                                 //Turn power off
                            LAT_RELAY_LVL = 0;                              //switch the booster line to the firing signal
                        }
                        else if(statusFlagsUSLG & FLAG_BOOSTER_LINE_CUTOFF){
                            SetCommsLow();                                              //Turn off power to the level to protect TP42C.
                            commsStatusUCG = MODE_CABLE_FAULT;                          //cable fault go to cable fault mode
                            cableFaultModeUCS = MODE_CFAULT_PRE;
                        }
                        else
                        commsStatusUCG = MODE_COMMS_IDLE;                               //Set comms as idle and wait for dispatcher
                    }
                    break;
                case(TMODE_ISO_WAIT):
                        if(counterCommsStatusUSS == TIME_TEST_2){               //2s elapsed
                        ReadKeySwitch();                                        //Read keyswitch  
                            if((statusFlagsUSLG & FLAG_SWITCH_ARMED))
                                LAT_STATUS_LED = 1;                             //turn on green LED for GREEN-BLUE routine
                            else{
                                commsStatusUCG = MODE_COMMS_IDLE;               //Set comms as idle and wait for dispatcher
                                cableTestUCS = TMODE_GENERAL;                     
                                LAT_RELAY_LVL = 1;                              //switch the booster line on
                                SetCommsHigh();                                 //Turn power on as it returns to normal operation
                            }
                        }
                        else if(counterCommsStatusUSS == TIME_TEST_3){          //2s elapsed
                            LAT_STATUS_LED = 0;                                 //turn off green LED for GREEN-BLUE routine
                            counterCommsStatusUSS = 0;                          //restart 2s counter
                            cableTestUCS = TMODE_ISO_WAIT2;                     //send to second phase
                        }
                    break;
                case(TMODE_ISO_WAIT2):
                        if(counterCommsStatusUSS == TIME_TEST_2){               //1.95s elapsed
                        ReadKeySwitch();                                        //Read keyswitch  
                            if((statusFlagsUSLG & FLAG_SWITCH_ARMED) && (ISO_COUNTER < 15)) // device armed and <60 seconds have passed/
                                LAT_BLOADER_LED = 1;                             //turn on green LED for GREEN-BLUE routine
                            else{
                                commsStatusUCG = MODE_COMMS_IDLE;               //Set comms as idle and wait for dispatcher
                                cableTestUCS = TMODE_GENERAL;                     
                                LAT_RELAY_LVL = 1;                              //switch the booster line on and return to normal operation
                                SetCommsHigh();                                 //Turn power on as it returns to normal operation
                            }
                        }
                        else if(counterCommsStatusUSS == TIME_TEST_3){          //2s elapsed 50ms flash
                            LAT_BLOADER_LED = 0;                                 //turn off green LED for GREEN-BLUE routine
                            cableTestUCS = TMODE_ISO_WAIT;                     //send to second phase
                            ISO_COUNTER++;
                            counterCommsStatusUSS = 0;                          //restart 2s counter
                        }
                    break;
            }
            break;
    }
}

void CheckLineFault(void){
    unsigned short lineVoltUS;

    lineVoltUS = ReadAnalogVoltage(ADC_VOLT_BC);                                //Read the line current on the booster line
    //calculate the ofset for a unit when calcualting a 1 and 0
    commsOffsetADCValueUS = 0;
    for(int i = 100; i<=500; i=i+100){                                          //for every 100 counts higher increase by 50
        if(lineVoltUS>=i){                                                      //up to a max of 500 counts which is 1.3V
            commsOffsetADCValueUS=commsOffsetADCValueUS+60;                     //value will never exceed a 1V offset deduction at a max offset value of 1.6V
        }
        else{
            i=600;                                                             //else leave
        }
    }
    int activeWindowCounter=0;
    
    for(int i = 0; i < 30; i++){                                                //check how many frames had activity
        if(!(boosterCommsData[DATA_MISSED_FRAMES][i] == 10))//(!(boosterCommsData[DATA_MISSED_FRAMES][i] == 10))
            activeWindowCounter++;                                              //if a frame had activity then it must be accounted for
    }
    
    if(lineVoltUS > (VOLT_BOOSTER_FAULT_HIGH+(activeWindowCounter*11))){              //If this current is over 60mA
        if(cableFaultCounter<CABLE_FAULT_FAILURES)                              //If its less then the failures then keep incrementing
            cableFaultCounter++;                                                //increment the cable fault counter
        if(cableFaultCounter==CABLE_FAULT_FAILURES){                            //if we have 3 consecutive failures then indicate.
            statusFlagsUSLG |= FLAG_BOOSTER_LINE_FAULT;                             //Report a fault
            if(lineVoltUS > (VOLT_BOOSTER_FAULT_LOW+(activeWindowCounter*11)))
            statusFlagsUSLG |= FLAG_BOOSTER_LINE_CUTOFF;                        //Report a fault
            else
                statusFlagsUSLG &= ~FLAG_BOOSTER_LINE_CUTOFF;                       //Else report all fine
    
        }
    }
    else{
        cableFaultCounter=0;
        statusFlagsUSLG &= ~FLAG_BOOSTER_LINE_FAULT;                            //Else report all fine
        statusFlagsUSLG &= ~FLAG_BOOSTER_LINE_CUTOFF;
    }
}

unsigned char BoosterCommsActive(void){
    if(commsStatusUCG != MODE_COMMS_IDLE)                                       //Check to see if the comms active bit is set
        return 1;                                                               //And return its status
    return 0;
}

void BoosterQueryStart(unsigned char queryValUC){
    if(BoosterCommsActive())                                                    //Should only allow starting a new command if one isn't in progress
        return;
    statusFlagsUSLG |= FLAG_MODE_DATAnCOMMAND;                                  //Set to data mode
    commsTXBitsUSLG = queryValUC;                                               //Configure command and bits to send
    commsDataModeUCG = queryValUC;
    commsStatusUCG = MODE_COMMS_START;                                          //Start the booster data query
}

void BoosterCommandStart(unsigned short destSerialUS, unsigned char destCommandUC, unsigned char destPayloadUC){
    if(BoosterCommsActive())                                                    //Should only allow starting a new command if one isn't in progress
        return;
    statusFlagsUSLG &= ~FLAG_MODE_DATAnCOMMAND;                                 //Set to command mode
    commsTXBitsUSLG = (((unsigned short long) destSerialUS) << 8) | (destCommandUC << 5) | destPayloadUC; //Set bits to send
    if(CheckBoosterRXParity(destSerialUS) ^ CheckBoosterRXParity(destCommandUC) ^ CheckBoosterRXParity(destPayloadUC)) //Calculate parity for this frame
        commsTXBitsUSLG |= FRAME_CMD_HIGH_BIT;
    commsStatusUCG = MODE_COMMS_START;                                          //Start the booster command query
}

inline void SetCommsLow(void){
    LAT_DC_TX = 0;                                                              //Disable current source
    LAT_DC_nTX = 1;                                                             //Pull line down
}

inline void SetCommsHigh(void){
    LAT_DC_nTX = 0;                                                             //Disable line pull down
    LAT_DC_TX = 1;                                                              //Enable current source
}

unsigned char CheckBoosterRXParity(unsigned short recvFrameUS){                 //Check the parity bit of a RX frame
    unsigned char bitCountUC;
    unsigned char bitParityUC;
    unsigned short tmpRecvFrameUS = recvFrameUS;

    for(bitCountUC = 0, bitParityUC = 0; bitCountUC < 16; bitCountUC++){        //Count the number of bits set
        (tmpRecvFrameUS & 1)?bitParityUC++:1;
        tmpRecvFrameUS >>= 1;
    }

    if(bitParityUC % 2)                                                         //If the parity is odd
        return 1;                                                               //Return 1
    else
        return 0;                                                               //If not, return 0
}

void ProcessRXFrame(unsigned short recvFrameUS, unsigned char frameNumUC){
    unsigned char parityCheckUC;

    if(statusFlagsUSLG & FLAG_MODE_DATAnCOMMAND){                               //If we are in data mode
        if(statusFlagsUSLG & FLAG_PHASE2nPHASE1)                                //Get the index for our current frame, depending on phase, see docs
            frameNumUC += NUM_FRAMES_PER_PHASE;

        boosterCommsData[DATA_WIN_STATUS][frameNumUC-1] = FLAG_WIN_NULL;        //Clear last window ID status flags
        if(statusFlagsUSLG & FLAG_FRAME_SYNC)
            boosterCommsData[DATA_WIN_STATUS][frameNumUC-1] |= FLAG_WIN_SYNC;   //Did we get a sync in this window ID?
        if(!(parityCheckUC = CheckBoosterRXParity(recvFrameUS)))
            boosterCommsData[DATA_WIN_STATUS][frameNumUC-1] |= FLAG_WIN_PARITY; //Did the parity match?
        if(statusFlagsUSLG & FLAG_FRAME_INVALID)
            boosterCommsData[DATA_WIN_STATUS][frameNumUC-1] |= FLAG_WIN_INVALID;//Was it an invalid frame?
        if(statusFlagsUSLG & FLAG_FRAME_COLLISION)
            boosterCommsData[DATA_WIN_STATUS][frameNumUC-1] |= FLAG_WIN_COLLISION;//Was there a collision

        if(!(statusFlagsUSLG & FLAG_FRAME_SYNC)){                               //If we didn't sync
            if(boosterCommsDataSerial[frameNumUC-1]){             //And there was a unit in this ID
                boosterCommsData[DATA_MISSED_FRAMES][frameNumUC-1]++;           //Increment the missed packet count
                if(boosterCommsData[DATA_MISSED_FRAMES][frameNumUC-1] == 10){   //If it misses 10
                    ResetSingleBoosterState(frameNumUC-1);                      //Assume it is offline and reset its data
                    statusFlagsUSLG |= FLAG_SERIAL_ASSIGN_NEW;                  //Mark the unit as missing for the master comms
                    ClearPastValue(frameNumUC);                                 //And clear all the past values sent to the IBC
                }
            }
            return;                                                             //Process next frame
        }else if((frameNumUC != ((recvFrameUS & FRAME_IB_ID) >> 9)) ||          //If the frame number doesn't match the window ID
                parityCheckUC ||                                                //Or the parity failed
                (statusFlagsUSLG & FLAG_FRAME_INVALID)){                        //Or the frame was invalid
            if(boosterCommsData[DATA_FAULT_FRAMES][frameNumUC-1] < 10)          //If we have received less than ten of these frames
                boosterCommsData[DATA_FAULT_FRAMES][frameNumUC-1]++;            //Increment the window ID counter
            boosterCommsData[DATA_WIN_STATUS][frameNumUC-1] |= FLAG_WIN_INVALID;//And mark the window ID frame as invalid
            return;                                                             //Process next frame
        }

        boosterCommsData[DATA_FAULT_FRAMES][frameNumUC-1] = 0;                  //Valid frame, so reset fault counter
        boosterCommsData[DATA_MISSED_FRAMES][frameNumUC-1] = 0;                 //As well as missed frame counter

        boosterCommsData[DATA_DEV_TYPE][frameNumUC-1] = (recvFrameUS & FRAME_IB_DEV_TYPE) >> 14; //Save the device type
        switch(commsDataModeUCG){
            case(CMD_DATA_STATUS):                                              //Check what value was requested
                boosterCommsData[DATA_STATUS][frameNumUC-1] = (recvFrameUS & FRAME_IB_PAYLOAD); //And save it in the correct place
                break;
            case(CMD_DATA_MSR36V):
                boosterCommsData[DATA_DC_VOLT][frameNumUC-1] = (recvFrameUS & FRAME_IB_PAYLOAD); //And save it in the correct place
                break;
            case(CMD_DATA_MSR220V):
                boosterCommsData[DATA_AC_VOLT][frameNumUC-1] = (recvFrameUS & FRAME_IB_PAYLOAD); //And save it in the correct place
                break;
            case(CMD_DATA_BLOOP):
                boosterCommsData[DATA_BL_VOLT][frameNumUC-1] = (recvFrameUS & FRAME_IB_PAYLOAD); //And save it in the correct place
                break;
        }
    }else{                                                                      //If we were in command mode instead
        statusFlagsUSLG &= ~(FLAG_COMMAND_SYNC | FLAG_COMMAND_PARITY | FLAG_COMMAND_INVALID | FLAG_COMMAND_COLLISION); //Clear the status flags from the last response
        if(statusFlagsUSLG & FLAG_FRAME_SYNC)
            statusFlagsUSLG |= FLAG_COMMAND_SYNC;                               //And mark the flags if we had a sync
        if(!CheckBoosterRXParity(recvFrameUS))
            statusFlagsUSLG |= FLAG_COMMAND_PARITY;                             //And a parity match
        if(statusFlagsUSLG & FLAG_FRAME_INVALID)
            statusFlagsUSLG |= FLAG_COMMAND_INVALID;                            //And an invalid response
        if(statusFlagsUSLG & FLAG_FRAME_COLLISION)
            statusFlagsUSLG |= FLAG_COMMAND_COLLISION;                          //Or a collision
        commandResponseUS = recvFrameUS;                                        //And save the complete response for the dispatcher
    }
}

void ResetSingleBoosterState(unsigned char boosterNumberUC){                    //Reset all values associated with a single booster
    boosterCommsData[DATA_MISSED_FRAMES][boosterNumberUC] = 10;
    boosterCommsData[DATA_FAULT_FRAMES][boosterNumberUC] = 0;
    boosterCommsData[DATA_WIN_STATUS][boosterNumberUC] = 0;
    boosterCommsData[DATA_DEV_TYPE][boosterNumberUC] = 0;
    boosterCommsData[DATA_STATUS][boosterNumberUC] = 0;
    boosterCommsData[DATA_DC_VOLT][boosterNumberUC] = 0;
    boosterCommsData[DATA_AC_VOLT][boosterNumberUC] = 0;
    boosterCommsData[DATA_BL_VOLT][boosterNumberUC] = 0;
    boosterCommsDataSerial[boosterNumberUC] = 0;
}

void ResetBoosterStates(void){                                                  //Reset all values associated with all boosters
    unsigned char boosterCountUC;

    for(boosterCountUC = 0; boosterCountUC < 30; boosterCountUC++)
        ResetSingleBoosterState(boosterCountUC);
}

unsigned char CheckDataCollision(void){                                         //Check if any of the window IDs had a collision
    unsigned char boosterCountUC;

    for(boosterCountUC = 0; boosterCountUC < 28; boosterCountUC++){
        if(boosterCommsData[DATA_WIN_STATUS][boosterCountUC] & FLAG_WIN_COLLISION)
            return 1;
    }

    return 0;
}

unsigned char LowestIdleWindow(void){                                           //Return the lost unused window ID
    unsigned char boosterCountUC;
    
    for(boosterCountUC = 1; boosterCountUC < 30; boosterCountUC++)//changed to only check and assign from window 1
        if(!(boosterCommsDataSerial[boosterCountUC]))//(boosterCommsData[DATA_MISSED_FRAMES][boosterCountUC] == 10)
            return boosterCountUC;
        
    return boosterCountUC;
}

unsigned char NewBoosterAdded(void){                                            //Check if there is a booster in the last frame but we have open window IDs

    if((LowestIdleWindow() < 29) && (boosterCommsData[DATA_MISSED_FRAMES][29] != 10))
        return 1;
    return 0;
}

unsigned char WindowIDBacktrack(unsigned char *currentBitUC, unsigned short *currentSerialUS, unsigned char *statusSetIDUC){

    while(*currentBitUC && (*currentSerialUS & (1 << *currentBitUC))){          //Backtrack to first 0 bit or start
        *currentSerialUS &= ~(*currentSerialUS & (1 << *currentBitUC));
        (*currentBitUC)--;
    }
    if(!(*currentBitUC) && (*currentSerialUS & (1 << *currentBitUC))){          //If at start and bit is 1, we're done
        *statusSetIDUC = WIN_IDLE;
        return 1;
    }else{                                                                      //Else, we at 0 bit at start or not
        *currentSerialUS |= (1 << *currentBitUC);
        BoosterCommandStart(*currentSerialUS, CMD_CMD_QUERY_SERIAL, *currentBitUC);
        return 0;
    }
}

void AssignWindowID(void){
    unsigned char newWindowUC;
    unsigned short serialNumberUS;

    newWindowUC = LowestIdleWindow();                                           //Get the lowest unused window ID
    serialNumberUS = (commandResponseUS & ~FRAME_WORD_HIGH_BIT);                //Retrieve the serial number and blank the parity bit
    boosterCommsDataSerial[newWindowUC] = serialNumberUS;         //Save the serial number in the window ID
    boosterCommsData[DATA_MISSED_FRAMES][newWindowUC] = 0;                      //Mark the unit

    BoosterCommandStart(serialNumberUS, CMD_CMD_WRITE_WINID, newWindowUC+1);    //Assign the unit a window id
}

void AssignSerialNumber(void){
    BoosterCommandStart(nextSerialUSG++, CMD_CMD_ASSIGN_SERIAL, 0);             //Assign the next serial number and increment
    WriteFlashValues();                                                         //Then save this value to flash
}

void RunBLCalibration(void){
    //unsigned short serialNumberUS;
    BoosterCommandStart(boosterCommsDataSerial[0], CMD_CMD_EXTENDED, CMD_EXT_BL_CALIB );             //Tell IB651 to run calibration routine
}

//Todo: this can be improved by checking all responses from clients, currently
//only checking bitmask responses for invalid responses. Should do window id
//and serial assignment as well.
unsigned char SetWindowID(void){
    static unsigned char statusSetIDUCS;
    static unsigned char currentBitUCS;                                         //Current bit, 0 is first
    static unsigned short currentSerialUSS;

    //if sync and parity fail or invalid packet and not collision, resend
    
    //if no responses from 0 value, try 1
    //if no respones from 1 value, backtrack
      //if prev 0 bit, increment
      //if prev 1 bit, backtrack?
        //if at first bit and its 1, we're done? set mode idle
    //if single response, assign window id
      //if 0 bit, increment
      //if 1 bit, backtrack
        //if prev 0 bit, increment
        //if prev 1 bit, backtrack?
          //if at first bit and its 1, we're done? set mode idle
    //if multiple responses, next bit from 0

    if(statusSetIDUCS == WIN_ACTIVE){
        NOP();
        if((statusFlagsUSLG & FLAG_COMMAND_SYNC) &&                             //If single packet received but invalid or parity error
          !(statusFlagsUSLG & FLAG_COMMAND_COLLISION) &&
           ((statusFlagsUSLG & FLAG_COMMAND_INVALID) ||!(statusFlagsUSLG & FLAG_COMMAND_PARITY))){
            BoosterCommandStart(currentSerialUSS, CMD_CMD_QUERY_SERIAL, currentBitUCS);
            return 0;
        }
        if(!(statusFlagsUSLG & FLAG_COMMAND_SYNC)){                             //If no packet received
            if(!(currentSerialUSS & (1 << currentBitUCS))){                     //No sync and current bit is 0
                currentSerialUSS |= (1 << currentBitUCS);
                BoosterCommandStart(currentSerialUSS, CMD_CMD_QUERY_SERIAL, currentBitUCS);
                return 0;
            }else                                                               //No sync and current bit is 1
                return WindowIDBacktrack(&currentBitUCS, &currentSerialUSS, &statusSetIDUCS);
        }else if(!(statusFlagsUSLG & FLAG_COMMAND_COLLISION)){                  //Sync and not collision
            AssignWindowID();                                                   //Assign window ID, WOOP!
            statusSetIDUCS = WIN_ASSIGN;
        }else{                                                                  //Sync and collision
            if(++currentBitUCS == 14){                                          //If we have a collision up to the last serial
                statusSetIDUCS = WIN_IDLE;
                return 1;                                                       //Give up and go cry in the corner
            }
            BoosterCommandStart(currentSerialUSS, CMD_CMD_QUERY_SERIAL, currentBitUCS); //query the next bit
        }
    }else if(statusSetIDUCS == WIN_ASSIGN){                                     //If we tried to assign a window ID
        statusSetIDUCS = WIN_ACTIVE;
        if(!(currentSerialUSS & (1 << currentBitUCS))){                         //If zero, increment
            currentSerialUSS |= (1 << currentBitUCS);
            BoosterCommandStart(currentSerialUSS, CMD_CMD_QUERY_SERIAL, currentBitUCS); //Check the next bit
            return 0;
        }else
            return WindowIDBacktrack(&currentBitUCS, &currentSerialUSS, &statusSetIDUCS); //Or backtrack
    }else if(statusSetIDUCS == WIN_INIT){                                       //When starting to assign new window ids
        ResetBoosterStates();                                                   //Reset all old booster values
        statusFlagsUSLG |= FLAG_SERIAL_ASSIGN_ACT;                              //And mark that we are busy, incase the IBC asks us for something
        currentBitUCS = 0;                                                      //Start at bit 0
        currentSerialUSS = 0;                                                   //For serial number 0
        statusSetIDUCS = WIN_ACTIVE;
        BoosterCommandStart(0, CMD_CMD_QUERY_SERIAL, 0);                        //And query any with this mask to respond
    }else if(statusSetIDUCS == WIN_CHECK_SERIAL){
        if((statusFlagsUSLG & FLAG_COMMAND_SYNC) && !(statusFlagsUSLG & FLAG_COMMAND_COLLISION))
            AssignSerialNumber();                                               //If any unit has the default serial, assign a new one
        else if(statusFlagsUSLG & FLAG_COMMAND_COLLISION){                      //If multiple units have the default serial
            statusSetIDUCS = WIN_IDLE;                                          
            return 1;                                                           //Give up and go cry in the corner
        }
        statusSetIDUCS = WIN_INIT;                                              //Then start assigning window ids
    }else{                                                                      //On entry
        statusSetIDUCS = WIN_CHECK_SERIAL;                                      //Check if any unit has the default serial number
        BoosterCommandStart(DEFAULT_SERIAL_NUMBER, CMD_CMD_QUERY_SERIAL, 13);
    }

    return 0;
}

unsigned char LowestMissingSerial(void){
//    unsigned char boosterCountUC;
                                                                                //only check from window 1 as add only adds from window 1, window 0 reserved
    for(boosterCountUC = 1; boosterCountUC < 30; boosterCountUC++)              //Check all window ids
        if((boosterCommsData[DATA_MISSED_FRAMES][boosterCountUC] != 10) &&      //Without the default frame count
           !boosterCommsDataSerial[boosterCountUC])               //And a black serial number
            return boosterCountUC;                                              //If so, return the window id
    return boosterCountUC;                                                      //Else, return 30
}

unsigned char QueryWinSerial(void){
//    static unsigned char statusGetSerialUCS = SERIAL_IDLE;
//    static unsigned char missingSerialWinUCS;
    unsigned short serialNumberUS;

    
    if(statusGetSerialUCS == SERIAL_ACTIVE){
        NOP();
        if(!(statusFlagsUSLG & FLAG_COMMAND_SYNC) ||                             //If there is a problem with the response
          (statusFlagsUSLG & FLAG_COMMAND_COLLISION) ||
           (statusFlagsUSLG & FLAG_COMMAND_INVALID) ||!(statusFlagsUSLG & FLAG_COMMAND_PARITY)){
            //missingSerialWinUCS = LowestMissingSerial();                            //And query the first unit without a serial
            if(QueryFailureCounter>10){
                statusGetSerialUCS = SERIAL_IDLE; 
                return 1;
            }
            QueryFailureCounter++;
            BoosterCommandStart(0, CMD_CMD_WIN_SERIAL, missingSerialWinUCS+1);    //Query it again
            return 0;
        }else if((statusFlagsUSLG & FLAG_COMMAND_SYNC) && (statusFlagsUSLG & FLAG_COMMAND_PARITY) && !(statusFlagsUSLG & FLAG_COMMAND_INVALID)){                          //If the response was good
            QueryFailureCounter=0;
            serialNumberUS = (commandResponseUS & ~FRAME_WORD_HIGH_BIT);
            boosterCommsDataSerial[missingSerialWinUCS] = serialNumberUS; //Save the serial number
            ClearPastValue(missingSerialWinUCS);                              //Clear all the past values sent to the IBC
            if((missingSerialWinUCS = LowestMissingSerial()) < 30){
                BoosterCommandStart(0, CMD_CMD_WIN_SERIAL, missingSerialWinUCS+1); //And query the next unit without a serial
                return 0;
            }
        }
        for(char i=1;i<29;i++){
            for(char j=i+1;j<29;j++){
                if((boosterCommsDataSerial[i]==boosterCommsDataSerial[j])&&!(boosterCommsDataSerial[i]==0)){
                    ResetSingleBoosterState(i);
                    ResetSingleBoosterState(j);       
                    BoosterCommandStart(0, CMD_CMD_WIN_SERIAL, i+1); //And query the next unit without a serial 
                    return 0;
                }
            }
        }
        
        statusGetSerialUCS = SERIAL_IDLE;                                       //If we queried all of them
        return 1;                                                               //Return with a grin on our faces
    }else{                                                                      //On entry
        statusFlagsUSLG |= FLAG_SERIAL_ASSIGN_ACT;                              //Mark that we are reassigning serials, in case the IBC asks
        statusGetSerialUCS = SERIAL_ACTIVE;
            if((missingSerialWinUCS = LowestMissingSerial()) < 30){
                QueryFailureCounter=0;
                BoosterCommandStart(0, CMD_CMD_WIN_SERIAL, missingSerialWinUCS+1); //And query the next unit without a serial
                return 0;
            }
        return 1;
    }
}

void BoosterCommsDispatcher(void){
    //static unsigned char dispStatusUCS = DISP_GET_STATUS;

    if(BoosterCommsActive() || !(statusFlagsUSLG & FLAG_DISP_ACTIVE))           //If comms is active or dispatcher inactive
        return;                                                                 //Then don't let the dispatcher continue

    NOP();

    if((dispStatusUCS == DISP_SET_WIN) || CheckDataCollision() || NewBoosterAdded()){ //If window assignment is active, we had a collision, or a new booster was added
        if(SetWindowID()){                                                      //Start or continue window id assignment
            dispStatusUCS = DISP_GET_STATUS;                                    //When done, continue dispatcher data query
            statusFlagsUSLG |= FLAG_SERIAL_ASSIGN_NEW;                          //Mark that we have new window ids, so the master comms mode can tell the IBC
            statusFlagsUSLG &= ~FLAG_SERIAL_ASSIGN_ACT;                         //Tell it window assignment is done
            ClearPastValues();                                                  //And clear any past values we have
        }else{
            dispStatusUCS = DISP_SET_WIN;                                       //If not done, carry on
            return;
        }
    }

    if((dispStatusUCS == DISP_GET_SERIAL) || (LowestMissingSerial() < 30)){     //Are we querying for serials or know we have a unit with an unknown serial
        if(QueryWinSerial()){                                                   //If so, go query more units for serials
            dispStatusUCS = DISP_GET_STATUS;                                    //When done, continue the dispatcher
            statusFlagsUSLG |= FLAG_SERIAL_ASSIGN_NEW;                          //Mark that we have new serials
            statusFlagsUSLG &= ~FLAG_SERIAL_ASSIGN_ACT;                         //And that we aren't busy with the query anymore
        }else{
            dispStatusUCS = DISP_GET_SERIAL;                                    //If not done, carry on
            return;
        }
    }

    switch(dispStatusUCS){                                                      //Dispatcher query routine, request value, then change state to request next value when done
        case(DISP_GET_STATUS):
            BoosterQueryStart(CMD_DATA_STATUS);
            dispStatusUCS = DISP_GET_MSR220V;
            break;
        case(DISP_GET_MSR220V):
            BoosterQueryStart(CMD_DATA_MSR220V);
            dispStatusUCS = DISP_GET_BLOOP;
            break;
        case(DISP_GET_BLOOP):
            BoosterQueryStart(CMD_DATA_BLOOP);
            dispStatusUCS = DISP_GET_MSR36V;
            break;
        case(DISP_GET_MSR36V):
            BoosterQueryStart(CMD_DATA_MSR36V);
            dispStatusUCS = DISP_GET_STATUS;
            break;
    }
}