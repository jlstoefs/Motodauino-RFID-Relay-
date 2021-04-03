/* Example sketch/program showing An Arduino Ignition Control featuring RFID, EEPROM, Relay. 
An RFID tag replaces the motorbike key for the ignition
video available on youtube: xxx
   -------------------------------------------------------------------------------------------
   This code is based on the work of miguelbalboa; This is a MFRC522 library example; for further details and other examples of the original code see: https://github.com/miguelbalboa/rfid
   
This motoduino version is availbale on https://github.com/jlstoefs/Motoduino-RFID-to-Relay-Board
 
 Simple Work Flow (not limited to) :
  +----------------------------------->READ TAGS+^------------------------------------------+
  |                              +--------------------+                                     |
  |                              |                    |                                     |
  |                         +----v-----+        +-----v----+                                |
  |                         |MASTER TAG|        |OTHER TAGS|                                |
  |                         +--+-------+        ++-------------+                            |
  |                            |                 |             |                            |
  |                      +-----v---+        +----v----+   +----v------+                     |
  |         +------------+READ TAGS+---+    |KNOWN TAG|   |UNKNOWN TAG|                     |
  |         |            +-+-------+   |    +-----------+ +------------------+              |
  |         |              |           |                |                    |              |
  |    +----v-----+   +----v----+   +--v--------+     +-v----------+  +------v----+         |
  |    |MASTER TAG|   |KNOWN TAG|   |UNKNOWN TAG|     |GRANT ACCESS|  |DENY ACCESS|         |
  |    +----------+   +---+-----+   +-----+-----+     +-----+------+  +-----+-----+         |
  |                       |               |                 |               |               |
  |       +----+     +----v------+     +--v---+             |               +--------------->
  +-------+EXIT|     |DELETE FROM|     |ADD TO|             |                               |
          +----+     |  EEPROM   |     |EEPROM|             |                               |
                     +-----------+     +------+             +-------------------------------+
   Use a Master Card which act as Programmer then you can able to choose card holders who will granted access or not

 * **Easy User Interface** Just one RFID tag needed whether Delete or Add Tags.
 * **Stores Information on EEPROM** Information stored on non volatile Arduino's EEPROM memory to preserve Key cards' tag and Master Card. EEPROM has unlimited Read cycle but roughly 100,000 limited Write cycle.
 * **Security** To keep it simple we are going to use Tag's Unique IDs. It's simple and not hacker proof.

   Typical RFID reader pin layout used:
   -----------------------------------------------------------------------------------------
               MFRC522      Arduino       Arduino   Arduino    Arduino          Arduino
               Reader/PCD   Uno/101       Mega      Nano v3    Leonardo/Micro   Pro Micro
   Signal      Pin          Pin           Pin       Pin        Pin              Pin
   -----------------------------------------------------------------------------------------
   RST/Reset   RST          9             5         D9         RESET/ICSP-5     RST
   SPI SS      SDA(SS)      10            53        D10        10               10
   SPI MOSI    MOSI         11 / ICSP-4   51        D11        ICSP-4           16
   SPI MISO    MISO         12 / ICSP-1   50        D12        ICSP-1           14
   SPI SCK     SCK          13 / ICSP-3   52        D13        ICSP-3           15

*/
#include <EEPROM.h>     // We are going to read and write PICC's UIDs from/to EEPROM
#include <SPI.h>        // RC522 Module uses SPI protocol
#include <MFRC522.h>  // Library for Mifare RC522 Devices

#define LED_ON HIGH
#define LED_OFF LOW
//Select the desired ignition connection to the relay (normally closed - NC,  or normally open - NO):
#define IGNITION_NC  //comment if ignition wires are connected on normally open pins of the relay
/* connection ignition wires to NC pins of the relay is safer to ride (in case of trouble, ignition stays on in case of arduino failure) but requires a diode in case physical keyv is still used
   Connection ignition wires to NO pins of the relay is safer against theft as relay must be powered in order for the engine to start /ride the bike
NB: NC is only acceptable if the ignition wire is energezed by the same wires as the arduino, and will require a diode on the ignition wire to avoid arduino being reverse powered by ignition wire in case of use of physica key;
without diode, the live ignition wire would power on arduino (due to NC connection), which  would trigger the relay to NO, which will power off the arduino,triggering relay to NC, and cycle would repeat, killing the relay*/ 
#ifdef IGNITION_NC
#define RELAY_INIT LOW //etat de demarrage du relay; pour la securité, le demarreur fonctionne avec le relay hors tension (les fils du dearreur doivent etre reliés sur la position NC)
#else
#define RELAY_INIT HIGH //etat de demarrage du relay; pour la securité, le demarreur fonctionne avec le relay hors tension (les fils du dearreur doivent etre reliés  sur la position NC)
#endif
// defining constants; Pins; 3/5/6/9/10 are PWM pins on arduino EVERY; led pins must be PWM pins
#define relay 4       // Set Relay Pin
#define wipeB 7       // Button pin for WipeMode
    #define BeepPin 2 //Beeper pin
// Create MFRC522 instance.
#define SS_PIN 10
#define RST_PIN 9
MFRC522 mfrc522(SS_PIN, RST_PIN);
// defining global variables
int LedPinsGBR[]={3,5,6}; //Green, Blue and Red led pins; must be PWM pins
bool successRead;    // Variable integer to keep if we have Successful Read from Reader
byte storedCard[4];   // Stores an ID read from EEPROM
byte readCard[4];   // Stores scanned ID read from RFID Module
byte masterCard[4];   // Stores master card's ID read from EEPROM
byte LedLoop=0; //used in program mode to loop the 3 leds without delay
byte ToggleRelay=RELAY_INIT;    // Initial Relay state 
///////////////////////////////////////// Setup ///////////////////////////////////
void setup() {
  
  pinMode(relay, OUTPUT); // relay must be initialized very early in programm as relay not powered allows bike to start (security feature: if no current, bike must continue operating)
  digitalWrite(relay, RELAY_INIT);                            // if IGNITION_NC is selected, bike could start if arduino is off ==> at initilisation, relay must switch to NO.
  pinMode(wipeB, INPUT_PULLUP);                               // Enable pin's pull up resistor for button 
  pinMode (BeepPin,OUTPUT);
  for (int i=0;i<3;i++) pinMode (LedPinsGBR [i],OUTPUT);     // define pinMode OUTPUT for Leds 
 
  Serial.begin(57600);                               // Initialize serial communications with PC
  SPI.begin();                                      // MFRC522 Hardware uses SPI protocol
  mfrc522.PCD_Init();                               // Initialize MFRC522 Hardware
  //mfrc522.PCD_SetAntennaGain(mfrc522.RxGain_max);  //If you set Antenna Gain to Max it will increase reading distance
  Serial.println(F("RFID_Ignition_Moto_V4"));     // sketch name
  Serial.println(F("---------------------"));
  ShowReaderDetails();                              // Show details of PCD - MFRC522 Card Reader details
  Show_EEPROM_Usage();
  for (int i=0; i<3; i++) FadeLoop(i,100,1);//FadeDelay(i,5);//FadeLoop(i,250,1);//
  if (digitalRead(wipeB) == LOW)                      //Wipe Code - If the Button (wipeB) Pressed for 10 sec while setup run (powered on), it wipes EEPROM
        {Serial.println(F("Wipe Button Pressed;all EEPROM data will be deleted in 5 seconds"));
        bool buttonState = monitorWipeButton(5000);    // Give user enough time (5 sec) to cancel operation; if goes to the end, true is returned as buttonState
        if (buttonState == true) Wipe_EEPROM(); // If button still be pressed, wipe EEPROM
        else Serial.println(F("Wiping Cancelled"));     // if monitorWipebutton returns false
        digitalWrite(LedPinsGBR[2], LED_OFF);                  //make sure red led is off
        Serial.println(F("-------------------"));
      }
  // Check if master card defined, if not let user choose a master card; You keep other EEPROM records just write other than 143 to EEPROM address 1
  if (EEPROM.read(1) != 143)                        // EEPROM address 1 should hold magical number which is '143'
        {Serial.println(F("No Master Card Defined"));
        Serial.println(F("Scan a card to define new Master Card"));
        do {
            successRead = getID();            // sets successRead to 1 when we get read from reader otherwise 0
            FadeLoop(1,200,1);
            }
        while (!successRead);                  // Program will not go further while you not get a successful read
    
        for ( byte j = 0; j < 4; j++ ) EEPROM.update( 2 + j, readCard[j] );// Loop 4 times to get the 4 bytes; Write scanned PICC's UID to EEPROM, start from address 2 when j=0)
        EEPROM.update(1, 143);                  // Write to EEPROM we defined Master Card.
        Serial.println(F("Master Card Defined"));
        Serial.println(F("-------------------"));
        }
  Serial.println(F("Master Card's UID"));
  for ( byte i = 0; i < 4; i++ )           // Read Master Card's UID from EEPROM
        {masterCard[i] = EEPROM.read(2 + i);    // print masterCard
        Serial.print(masterCard[i], HEX);
        }
  Serial.println("");
  Serial.println(F("Setup completed; ready to scan"));
  Serial.println(F("----------------"));
}
///////////////////////////////////////// Main Loop ///////////////////////////////////
void loop () {

static boolean programMode = false;  // initialize programming mode to false
static byte GreenLedStatus;     //should be boolean but not digitalWrite (x, TRUE) is not supported by arduino nano EVERY  

  do  {
      successRead = getID();  // sets successRead to 1 when we get read from reader otherwise 0
      if (digitalRead(wipeB) == LOW)           // Check if button is pressed;  if wipe button pressed for 10 seconds initialize Master Card wiping
            {Serial.println(F("Wipe Button Pressed;Master card will be deleted in 5 seconds"));
            bool buttonState = monitorWipeButton(5000); // Give user enough time to cancel operation
            if (buttonState == true) // If button still be pressed, wipe EEPROM cell 1
                  {EEPROM.update(1, 0);                  // Reset Magic Number.
                  Serial.println(F("Master card erased"));
                  Serial.println(F("Please reset to re-program master card"));
                  while (1)  //Dead end: loop stops here                     
                      {for (int i=0;i<3;i++) digitalWrite (LedPinsGBR [i],LED_OFF); 
                      FadeLoop(2,200,1);//FadeDelay(2,20);//FadeLoop(2,200,1);
                      Serial.print(F("."));
                      }        
                  }
            Serial.println(F("Master card erase cancelled"));
            }
      if (programMode) 
                  {FadeLoop(LedLoop,100,0);//for (int i=0; i<3; i++) FadeLoop(i,100,1);//cycleLeds(100); // Program Mode cycles through Green Blue Red waiting to read a new card  ; 
                  if( millis() % 400<200) digitalWrite (BeepPin,HIGH);
                  else digitalWrite (BeepPin,LOW);
                  }
      else 
                  {FadeLoop(1,1500,0);// FadeNoDelay(1,2000);  // FadeLoop(1,2000,0);//  // Normal mode, blue Power LED (RBG=1) is fading
                  digitalWrite(LedPinsGBR[2], LED_OFF);
                  if( ToggleRelay==RELAY_INIT && millis() % 1000<200) digitalWrite (BeepPin,HIGH);
                  else digitalWrite (BeepPin,LOW);
                  }
      }  
  while (!successRead);   //the loop will stop here while we are NOT getting a successful read

  if (programMode) 
        {
        if (isMaster(readCard) )  //When in program mode check First If master card scanned again to exit program mode
              {Serial.println(F("Hello Master - Exiting Program Mode"));
              Serial.println(F("----------------"));
              programMode = false;  //exit programMode
              for (int i=0;i<3;i++) {digitalWrite(LedPinsGBR [i],LED_OFF);}
              digitalWrite(LedPinsGBR[0], GreenLedStatus); 
              return; //recommence au début du loop et ne va pas plus loin
              }
        else  {        //if a Key card is read
              if ( findID(readCard) )  // If scanned card is known delete it
                    {Serial.println(F("removing..."));
                    deleteID(readCard);
                    }
              else                     // If scanned card is not known add it
                    {Serial.println(F("adding..."));
                    writeID(readCard);       // Serial.println(F("Scan a PICC to ADD or REMOVE to EEPROM"));
                    }
              Show_EEPROM_Usage(); 
              Serial.println(F("Scan a card to ADD or REMOVE to EEPROM"));
              }
        }
  else  { // NormalMode
        if (isMaster(readCard))     // If scanned card's ID matches Master Card's ID - enter program mode
              {programMode = true;
              GreenLedStatus=digitalRead(LedPinsGBR[0]);
              Serial.println(F("Hello Master - Entering Program Mode"));
              Serial.println(F("----------------"));
              uint8_t count = EEPROM.read(0);   // Read the first Byte of EEPROM that
              Show_EEPROM_Usage();
              Serial.println(F("Scan a Key card to ADD or REMOVE to EEPROM"));
              Serial.println(F("Scan Master card again to exit program mode"));
              }
        else {                          //if card is not MAster
              if ( findID(readCard) )   //see if the card is in the EEPROM
                    {Serial.println(F("Access granted"));
                    SwitchRelay();      // switch the relay
                    }
              else                    // If card is not in EEPROM, show that the ID was not valid
                    {Serial.println(F("Access denied"));
                    FadeLoop(2,100,5);//denied();
                    digitalWrite (BeepPin,HIGH); 
                    delay(500);
                    digitalWrite (BeepPin,LOW); 
                    }
              }
    Serial.println(F("----------------"));
        }
}
/////////////////////////////////////////  SwitchRelay (Relay+Leds) ///////////////////////////////////
void SwitchRelay () {
  
// static byte ToggleRelay=RELAY_INIT;    // Initial Relay state 

 ToggleRelay = 1-ToggleRelay;                // invert for this run
 digitalWrite(relay, ToggleRelay) ;        // Toggle LOW = relay sur NO (consomme de l'energie)
 for (int i=0;i<3;i++) 
      {digitalWrite (BeepPin,HIGH); 
      delay(50);
      digitalWrite (BeepPin,LOW); 
      delay(50);
      }
 Serial.print(F("ToggleRelay="));
 Serial.println(ToggleRelay);
 if (ToggleRelay==RELAY_INIT) 
      {Serial.println(F("Engine CUT OFF"));
      digitalWrite(LedPinsGBR[0], LED_OFF);
      }
 else 
      {Serial.println(F("Engine ON"));
       digitalWrite(LedPinsGBR[0], LED_ON); 
      }
}

///////////////////////////////////////// boolean getID (0/1 + print ID number) ///////////////////////////////////
bool getID() {

  if ( ! mfrc522.PICC_IsNewCardPresent()) return 0;     //If a new PICC placed to RFID reader
  if ( ! mfrc522.PICC_ReadCardSerial())   return 0;     //Since a PICC placed get Serial and continue   
  Serial.print(F("Card UID: "));
  for ( uint8_t i = 0; i < 4; i++)   // There are Mifare PICCs which have 4 byte or 7 byte UID care;  we assume every PICC as they have 4 byte UID until we support 7 byte PICCs
          {readCard[i] = mfrc522.uid.uidByte[i];
          Serial.print(readCard[i], HEX);
          }
  Serial.println("");
  mfrc522.PICC_HaltA(); // Stop reading
  return 1;
}
//////////////////////////////////////////ShowReaderDetails (Serial.print)///////////////////////////
void ShowReaderDetails() { // Get the MFRC522 software version
  
  byte v = mfrc522.PCD_ReadRegister(mfrc522.VersionReg);
  
  Serial.print(F("MFRC522 Version: 0x"));
  Serial.print(v, HEX);
  if (v == 0x91)      Serial.print(F(" = v1.0"));
  else if (v == 0x92) Serial.print(F(" = v2.0"));
  else                Serial.print(F(" unknown"));
  Serial.println("");
  if ((v == 0x00) || (v == 0xFF))   // When 0x00 or 0xFF is returned, communication probably failed
      {Serial.println(F("WARNING: Communication failure;SYSTEM HALTED"));
      while (true) FadeLoop(2,100,0); // dead end
      }
}
//////////////////////////////////////// readID ( Read ID from EEPROM ans store it in storeCard) //////////////////////////////
void readID( uint8_t number ) {
  
  uint8_t start = (number * 4 ) + 2;    // Figure out starting position
  for ( uint8_t i = 0; i < 4; i++ ) storedCard[i] = EEPROM.read(start + i);   // Loop 4 times to get the 4 Bytes; Assign values read from EEPROM to array
}
///////////////////////////////////////// writeID ( Add ID to EEPROM ) ///////////////////////////////////
void writeID( byte a[] ) {
  
        uint8_t num = EEPROM.read(0);     // Get the numer of used spaces, position 0 stores the number of ID cards
        uint8_t startbyte = ( num * 4 ) + 6;  // Figure out where the next slot starts
        num++;
        EEPROM.update( 0, num );     //  Increment the counter by one & Write the new count to the counter
        for ( uint8_t j = 0; j < 4; j++ ) EEPROM.update( startbyte + j, a[j] );  // Write the array values to EEPROM in the right position
        FadeLoop(0,1000,1);//FadeDelay(0,20); //FadeLoop(0,100,1); //GBR     //successWrite();
        Serial.print(F("card ID added to EEPROM; on slot"));
        Serial.println(num);
}
///////////////////////////////////////// deleteID ( Remove ID from EEPROM )  ///////////////////////////////////
void deleteID( byte a[] ) {
  
        uint8_t num = EEPROM.read(0);   // Get the total number of Key cards, stored in EEPROM position 0
        uint8_t slot= findIDSLOT( a );      // Figure out the slot number of the card  
        const uint8_t looping = ((num - slot) * 4); // the number of bytes used to store Key cards located in EEPROM AFTER the card to be deleted, and which will need to be shifted by 4 bytes 
        uint8_t startByte = ( (slot-1) * 4 ) + 6;      // first EEPROM location of card to be deleted
 
        EEPROM.update( 0, num-1 );   // Write the new nbr of Key cards (i.e. old nbr-1) in EEPROM 0
        for ( byte k = 0; k < 4; k++ )                                              //JL code: only shift location between card to be deleted and the last Key card in the EEPROM
            {EEPROM.update( startByte + k, EEPROM.read(startByte + looping + k));   //JL replacing the card to be deleted with the last card
            EEPROM.update( startByte + looping + k, 0);                             // JL deleting the last 4 bytes as they have been shifted
            }
        FadeLoop(2,1000,1);//FadeDelay(2,20); //FadeLoop(2,100,1); //GBR //successDelete();
        Serial.println(F("Succesfully removed ID record from EEPROM"));
}
///////////////////////////////////////// checkTwo (true/false)  ///////////////////////////////////
boolean checkTwo ( byte a[], byte b[] ) {

boolean match = false;          // initialize card match to false
 if ( a[0] != 0 )      // Make sure there is something in the array first
    match = true;       // Assume they match at first
 for ( uint8_t k = 0; k < 4; k++ )    // Loop 4 times
          {if ( a[k] != b[k] )     // IF a != b then set match = false, one fails, all fail
          match = false;
          }
 if ( match ) return true;        // Check to see if if match is still true
 else return false;
}
///////////////////////////////////////// findIDSLOT (returns int)  ///////////////////////////////////
uint8_t findIDSLOT( byte find[] ) {
  
 uint8_t count = EEPROM.read(0);       // Reads the number of Key cards in EEPROM 0
 for ( uint8_t i = 1; i <= count; i++ )     // Loop once for each EEPROM entry
    {readID(i);                // Read an ID from EEPROM, it is stored in storedCard[4]
    if ( checkTwo( find, storedCard ) )  // Check to see if the storedCard read from EEPROM is the same as the find[] ID card passed
         {return i;         // The slot number of the card
         break;          // Stop looking we found it
         }
    }
}
///////////////////////////////////////// boolean findID (true/false)   ///////////////////////////////////
boolean findID( byte find[] ) {
  
  uint8_t count = EEPROM.read(0);     // Reads the number of Key cards in EEPROM 0
  for ( uint8_t i = 1; i <= count; i++ )     // Loop once for each EEPROM entry
        {readID(i);          // Read an ID from EEPROM, it is stored in storedCard[4]
        if ( checkTwo( find, storedCard ) ) return true;   // Check to see if the storedCard read from EEPROM
        }
  return false;
}
///////////////////////////////////////// boolean isMaster (Check if readCard is masterCard)   ///////////////////////////////////
boolean isMaster( byte test[] ) {
  
  if ( checkTwo( test, masterCard ) )   // Check to see if the ID passed is the master programing card
    return true;
  else
    return false;
}
///////////////////////////////////////// boolean monitorWipeButton (true/false)  ///////////////////////////////////
bool monitorWipeButton(uint32_t interval) {
  
static int Modulo=0;
static int prevModulo=0;
const int freq=500;
int Ledfreq=freq;
byte CountDown=interval/freq; //printing CountDown to reset

uint32_t now = (uint32_t)millis();
while ((uint32_t)millis() - now < interval)  
        {if((uint32_t)millis() - now > interval/2) Ledfreq=freq/4; //LedPinsGBR[2] clignotte 4x plus vite dans la seconde moitié
        prevModulo=Modulo;
        Modulo=millis()% freq;
        FadeLoop(2,Ledfreq,0);//FadeLoop(2,Ledfreq,0);
        if (Modulo<prevModulo)        //each freq
                {if (digitalRead(wipeB) == HIGH) return false; //exit
                Serial.print(CountDown);   //compte a rebours
                for (byte i=1; i<CountDown; i++) Serial.print("*");
                Serial.println("*");
                CountDown--;
                }
        }
return true;
}
////////////////////////////////////////// Wipe_EEPROM   ///////////////////////////////////
void Wipe_EEPROM(){
  
 Serial.println(F("Starting Wiping EEPROM"));
 for (uint16_t x = 0; x < EEPROM.length(); x++) EEPROM.update(x,0);       //If EEPROM address 0, do nothing, already clear, go to the next address in order to save time and reduce writes to EEPROM
 Serial.println(F("EEPROM Successfully Wiped"));
}
///////////////////////////////////////// Show_EEPROM_Usage (Serial.print) ///////////////////////////////////
void Show_EEPROM_Usage() {
  
  Serial.print(F("EEprom.length()= ")); Serial.print(EEPROM.length());Serial.println(F(" bytes"));
  Serial.print(F("Key card slots in EEPROM (excl.master): ")); Serial.println((EEPROM.length()-6)/4);
  Serial.print(F("Used: ")); Serial.println(EEPROM.read(0));
  Serial.print(F("Available: ")); Serial.println((EEPROM.length()-6)/4-EEPROM.read(0));
  Serial.println(F("----------------"));
  }
  //////////////////////////////////////// FadeLoop (phading LED without delay/////////////////////////////////// 
void FadeLoop (int GBR,int PulseLength, byte Repeat) { //Led pulsing; GBR is the led ref, PulseLength is the length of a halve cycle, Repeat is 0 = noDelay or >=1 (number of loops before exiting)
 
  static int prevMod=0; 
  static int Mod=0;
  byte Count=0;
  const byte Brightness=200;
  static bool LedToggle=0;
  int LedValue; //from 0 to 250
  uint32_t StartMillis=0; 
  
  if (Repeat !=0)  //if we have a loop
        {StartMillis=millis();// make sure we do a full increase/fade cycle in case cycle > 0
        LedToggle=0;}
  do 
        {Mod=(millis()-StartMillis)% PulseLength; //va de 0 a pulseLength
        LedValue=map(Mod,0,PulseLength-1,0,Brightness);
        if (Mod<prevMod)                                    //cycle every PulseLength millis, while avoiding delay function
                {LedToggle=1-LedToggle; 
                Count++; 
                LedValue=0;//make sure led is completely off  
                if (LedToggle==0) 
                      {LedLoop++; 
                      }
                  }     
        prevMod=Mod;
        analogWrite(LedPinsGBR[GBR],LedValue+LedToggle*(Brightness-2*LedValue)); 
        } 
  while (Count<(2*Repeat)); //Loop if cycle>0
  LedLoop = LedLoop%3; //makes sure OneTwoThree stays between 0 & 3
}
