 /* 
 * File:   main.c
 * Author: aece-engineering
 *
 * Created on January 19, 2015, 9:06 AM
 *
 * Main code base for new ISC-1 new hardware, all feature should eventually
 * be here.
 * 
 * 17/05/2016 - Found the system to be running at 72 Mhz causing the PPL to be running out of spec.
  *             System corrected by running it at 48Mhz and adjusting 100us timer
  *  
 */

#include "main.h"
#include "ST7540.h"
#include "mastercomms.h"
#include "peripherals.h"
#include "ST7540.h"
#include "boostercomms.h"

volatile unsigned short long statusFlagsUSLG;                                            //Global flags reflecting the system state
//unsigned long modemTimeoutCounter;
//unsigned char modemTimeoutFlag;

void InitSystem(void);                                                          //Initialize the peripherals
void InitStates(void);                                                          //Initialize the system states
void InitST7540Wrapper(void);                                                   //Wrapper for modem initialization

void interrupt isr(void){
    unsigned char discardUC;

    if(PIR1bits.TMR2IF){                                                        //100us have passed
        PIR1bits.TMR2IF = 0;
//        modemTimeoutCounter++;                                                      //increment the modem Timeout Counter
        
        if(++masterCommsTimeoutUSG == TIME_COMMS_TIMEOUT){                      //Has a comms timeout occured
            ForceFiringMode(MASTER_FIRING_ACTIVE);                              //Go to firing mode
//            if(statusFlagsUSLG & FLAG_RELAY_TIMEOUT)//{                            //If the level relay was closed and we lost comms
//                    SetIsolationRelay(MASTER_CMD_OPEN_RLY);                         //Open the relay again
            statusFlagsUSLG &= ~FLAG_IBC_COMMS_ACTIVE;                          //Set comms as inactive
            masterCommsTimeoutUSG = 0;                                          //And clear the timeout
        }
    
//        if (modemTimeoutCounter >= 50000){             // If there has been no Comms for 5 seconds = 50 000 x 100us
//            if(!(statusFlagsUSLG&FLAG_FIRING_MODE) && modemTimeoutFlag != 1)  //if it is timeout check and its not currently in firing mode       
//                modemTimeoutFlag = 1;                                         // this will try to reset if unit does not receive firing command.
//        }
        statusFlagsUSLG |= FLAG_TICK;                                           //Tick flag generation
        BoosterDataCommandComms();                                              //Manage booster comms state machine
        BoosterCommsDispatcher();                                               //Call the booster dispatcher
        
    }

    if(PIR3bits.SSP2IF){                                                        //SPI byte TX or RX done
        SPIISRHandlerST7540();                                                  //Handle SPI comms
    }

    if(INTCONbits.RBIF){                                                        //IOC on SS pin
        discardUC = PORT_RXD_IOC;                                               
        RXReadyISRHandlerST7540();                                              //Should we receive it?
        INTCONbits.RBIF = 0;
    }
}

//Todo: turn off dispatcher/booster comms on line fault?
////When do we turn it on again? Probably don't want to turn it off.
void main(void){
    InitStates();                                                               //Initialize the system states
    InitSystem();                                                               //Initialize all onboard peripherals
    
    InitST7540Wrapper();                                                        //Initialize the modem  

    SetCommsHigh();                                                             //Set the booster comms line high

    statusFlagsUSLG |= FLAG_DISP_ACTIVE;                                        //Activate the booster dispatcher

    while(1){                                                                    //Sit and wait for packets from the IBC
        ProcessMasterComms();
        
//        if(modemTimeoutFlag){                                                   //If there has been no comms for 5 seconds
//            //LAT_STATUS_LED = 1;
////            modemTimeoutCounter = 0;                                            //Clear the Flag and counter so we only do this once.
//            InitST7540Wrapper();                                                //Re-initialize the modem
//        }
    }
}

void InitSystem(void){

    //Note, SPI, SPI pins, and IOC on RB4 enabled in ST7540 init code

    //Configure system clock
    OSCTUNEbits.PLLEN = 1;                                                      //Enable PLL
    OSCCONbits.IRCF = 0b110;                                                    //Set clock to 4mhz

    //Configure TMR2
    PR2 = 150;//216                                                                  //Set match on 100us
    T2CONbits.T2OUTPS = 0b0111;                                                 //Postscaler set to 8
    PIR1bits.TMR2IF = 0;                                                        //Clear int flag
    PIE1bits.TMR2IE = 1;                                                        //Enable TMR2 INT
    T2CONbits.TMR2ON = 1;                                                       //Enable TMR2
    INTCONbits.PEIE = 1;                                                        //Enable PI
    INTCONbits.GIE = 1;                                                         //Enable global int

    //Configure ADC
    ADCON1bits.ACQT = 0b001;                                                    //Set acquisition time to 2 TAD
    ADCON1bits.ADCS = 0b110;                                                    //Set ADC clock to FOSC/64, datasheet isn't clear on this
    ADCON1bits.ADFM = 1;                                                        //Right justify result
    ADCON0bits.ADON = 1;                                                        //Turn on ADC
    
    TRISAbits.TRISA5 = 1;
    
    //Pull pins B6 and B7 to ground and ensure they are not floating.
    TRISBbits.TRISB7 =0;
    TRISBbits.TRISB6 =0;
    LATBbits.LATB7 =0;
    LATBbits.LATB6 =0;
    
    //Configure LEDs
    TRIS_STATUS_LED = 0;
    TRIS_CFAULT_LED = 0;
    TRIS_BLOADER_LED = 0;
    LAT_STATUS_LED = 0;
    LAT_CFAULT_LED = 0;
    LAT_BLOADER_LED = 0;


    //Configure earth leakage pins
    TRIS_EL_EN = 0;
    LAT_EL_EN = 0;
    
    //Configure relays
    TRIS_RELAY_LVL = 0;
    LAT_RELAY_LVL = 0;
    
    TRIS_RELAY_SHF = 0;
    LAT_RELAY_SHF = 0;
    statusFlagsUSLG |= FLAG_RELAY_CLOSED;

    //Configure IB651 power line
    TRIS_DC_TX = 0;
    LAT_DC_TX = 0;

    TRIS_DC_nTX = 0;
    LAT_DC_nTX = 1;

    TRIS_DC_RX_EN = 0;
    LAT_DC_RX_EN = 1;

    TRIS_EL_READ = 1;
    ANCON_EL_READ = 0;

    //Configure booster comms adc pin
    TRIS_DC_RX = 1;
    ANCON_DC_RX = 0;

    TRIS_KEY_READ = 1;
    ANCON_KEY_READ = 0;  

        //Configure keyswitch pins
    TRIS_KEY_EN = 0;
    LAT_KEY_EN = 0;
    
    //Turn relays on
    LAT_RELAY_LVL = 1;
}

void InitStates(void){

    ResetBoosterStates();                                                       //Reset all boster data
    ReadFlashValues();                                                          //Read our configs from flash
    ClearPacketNumbers();                                                       //Reset all packet numbers
}

void InitST7540Wrapper(void){
    unsigned char initCountUC;
    
    for(initCountUC = 0; initCountUC < 5; initCountUC++){                       //Try up to five times to initialize the modem
        if(InitST7540()){
            //LAT_BLOADER_LED = 1;
            statusFlagsUSLG |= FLAG_IBC_COMMS_ACTIVE;
            break;
        }
    }
}

