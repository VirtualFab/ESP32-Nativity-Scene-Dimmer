//////////////////////////////////////////////
//        RemoteXY include library          //
//////////////////////////////////////////////

// RemoteXY select connection mode and include library 
#define REMOTEXY_MODE__ESP32CORE_BLUETOOTH
#include <BluetoothSerial.h>

#include <RemoteXY.h>

// RemoteXY connection settings 
#define REMOTEXY_BLUETOOTH_NAME "RemoteXY"


// RemoteXY configurate  
#pragma pack(push, 1)
uint8_t RemoteXY_CONF[] =
  { 255,3,0,0,0,73,0,13,13,1,
  2,0,21,3,22,11,2,26,31,31,
  79,78,0,79,70,70,0,3,132,14,
  28,33,9,2,26,129,0,14,20,34,
  6,17,65,110,105,109,97,122,105,111,
  110,101,0,4,192,0,49,62,7,2,
  26,129,0,15,41,34,6,17,76,117,
  109,105,110,111,115,105,116,195,160,0 };
  
// this structure defines all the variables and events of your control interface 
struct {

    // input variables
  uint8_t RaSwitch1; // =1 if switch ON and =0 if OFF 
  uint8_t RaAnimazione; // =0 if select position A, =1 if position B, =2 if position C, ... 
  int8_t slider_1; // =0..100 slider position 

    // other variable
  uint8_t connect_flag;  // =1 if wire connected, else =0 

} RemoteXY;
#pragma pack(pop)

/////////////////////////////////////////////
//           END RemoteXY include          //
/////////////////////////////////////////////

#include <OneWire.h>
#include <DallasTemperature.h>

#define VERBOSE true
#define TIME_RES_MILLIS 10 

// LED ------------------------------------------------------
#define NUMLEDS 7
#define MAXANIMATIONSTEPS 10
enum {
  ENDANIMATION=0,
  FADEUP,
  FADEDOWN,
  ON,
  OFF
};

//uint8_t LedPin[NUMLEDS] ={2,15,5,18,19,21,22,23,32,33,25,26,27,14,12};
uint8_t LedPin[NUMLEDS]   ={2,15,5,18,21,22,32};
uint8_t intLedCurAniIdx[NUMLEDS];
uint16_t intLedTimeCounter[NUMLEDS];
uint8_t aLedProgram[][NUMLEDS][MAXANIMATIONSTEPS][2]={
{
   { {FADEUP,10}, {ON,75}, {FADEDOWN,5}, {OFF, 30} }
  ,{ {OFF,10}, {FADEUP,10},{ON, 55}, {FADEDOWN,5}, {OFF,35} }
  ,{ {OFF,20}, {FADEUP,10},{ON, 40}, {FADEDOWN,5}, {OFF,45} }
  ,{ {OFF,30}, {FADEUP, 5},{ON, 30}, {FADEDOWN,5}, {OFF,50} }
  ,{ {OFF,35}, {FADEUP, 5},{ON, 25}, {FADEDOWN,5}, {OFF,50} }
  ,{ {OFF,40}, {FADEUP, 5},{ON, 20}, {FADEDOWN,5}, {OFF,50} }
  ,{ {OFF,45}, {FADEUP, 5},{ON, 25}, {FADEDOWN,5}, {OFF,40} }
}
,{
   { {ON, 1},{OFF, 1} }
  ,{ {ON, 1},{OFF, 1} }
  ,{ {ON, 1},{OFF, 1} }
  ,{ {ON, 1},{OFF, 1} }
  ,{ {ON, 1},{OFF, 1} }
  ,{ {ON, 1},{OFF, 1} }
  ,{ {ON, 1},{OFF, 1} }
}
,{
   { {FADEUP, 5},{FADEDOWN, 5} }
  ,{ {FADEUP, 5},{FADEDOWN, 5} }
  ,{ {FADEUP, 5},{FADEDOWN, 5} }
  ,{ {FADEUP, 5},{FADEDOWN, 5} }
  ,{ {FADEUP, 5},{FADEDOWN, 5} }
  ,{ {FADEUP, 5},{FADEDOWN, 5} }
  ,{ {FADEUP, 5},{FADEDOWN, 5} }
}
,{
   { {ON, 1} }
  ,{ {ON, 1} }
  ,{ {ON, 1} }
  ,{ {ON, 1} }
  ,{ {ON, 1} }
  ,{ {ON, 1} }
  ,{ {ON, 1} }
}
};
#define NUMLEDPROGRAMS sizeof(aLedProgram)/NUMLEDS/MAXANIMATIONSTEPS/2

uint8_t intLedCurPgm=1;
float LedPWM[NUMLEDS];
float LedFadeIncrement[NUMLEDS];

// LED PWM Settings 
#define PWMFREQ 5000 // 5 KHz 
#define PWMRESOLUTION 10 // 0 to 2^10-1=1023
const int MAX_DUTY_CYCLE = (int)(pow(2, PWMRESOLUTION) - 1);
int intBrightness;

// PUSHBUTTON ----------------------------------------------
#define PUSHBUTTONPIN 0 //13 // use 0 for GPIO0, the on-board pushbutton
int intPushButtonPrevState=HIGH;
int intPushButtonCounter;
#define PUSHBUTTONLONGPRESSHUNDR 200 // 2 seconds for a long press state

// DS18B20 -------------------------------------------------
// GPIO where the DS18B20 is connected to
#define oneWireBus 4     

// Setup a oneWire instance to communicate with any OneWire devices
OneWire oneWire(oneWireBus);

// Pass our oneWire reference to Dallas Temperature sensor 
DallasTemperature sensors(&oneWire);

float temperatureC;

// FAN ------------------------------------------------------
#define FAN_PIN 33
#define FAN_PWMCHAN 15       // use the last one, leaving all the other 0 to 15 to LEDs
#define FAN_PWM_RESOLUTION 6 // 64 different fan speeds are enough
#define FAN_PWM_FREQ 10000   // Use a higher frequency for the fan, otherwise you can hear it whisteling
const int FAN_PWM_MAX=(pow(2, FAN_PWM_RESOLUTION) - 1);
#define FAN_TEMP_MAX 40      // Fan hysteresys: Temperature at which to start the fan
#define FAN_TEMP_MIN 35      //                 Temperature at which to stop the fan

int intSecHundr,lastTimerValue;
int intLastTempRead;

// ------------------------------------------------------------------------------
void dbgprintf(const char *format, ...) { // Generic debug sprintf function capable to output to serial in one line
// ------------------------------------------------------------------------------
#if VERBOSE
  char strDebugLine[256];
  va_list args;
  va_start(args, format);
  vsnprintf(strDebugLine, 255, format, args);
  Serial.println(strDebugLine);
  va_end(args);
#endif
}

// ------------------------------------------------------------------------------
void LedProgramInit() { // LED internal counters and animation init
// ------------------------------------------------------------------------------
  for(uint8_t i=0;i<NUMLEDS;i++) {
    intLedCurAniIdx[i]=255;
    intLedTimeCounter[i]=1;
    LedPWM[i]=0;
    ledcWrite(i, LedPWM[i]);
  }
}

// ------------------------------------------------------------------------------
void setup() {
// ------------------------------------------------------------------------------
  // LED PWM pins and lib initialization ----------------------------------------
  for(uint8_t i=0;i<NUMLEDS;i++) {
    ledcSetup(i, PWMFREQ, PWMRESOLUTION);
    pinMode(LedPin[i], OUTPUT);
    ledcAttachPin(LedPin[i], i);
  }
  LedProgramInit();
  
  // FAN PWM pin initialization -------------------------------------------------
  ledcSetup(FAN_PWMCHAN, 10000, FAN_PWM_RESOLUTION);
  pinMode(FAN_PIN, OUTPUT);
  ledcAttachPin(FAN_PIN, FAN_PWMCHAN);
  
  // Start the Serial Monitor
  Serial.begin(115200);
  // Start the DS18B20 sensor
  sensors.begin();
  sensors.setWaitForConversion(false); // Function requestTemperature() etc returns immediately 

  pinMode(PUSHBUTTONPIN, INPUT);

  RemoteXY_Init (); 

  dbgprintf("READY");
}

//boolean blnEnabled=false;

// ------------------------------------------------------------------------------
void loop() {
// ------------------------------------------------------------------------------
#define PUSHBUTTONLONGPRESS 200 // 2 seconds for a long press state

  RemoteXY_Handler();
  
  if(millis()-lastTimerValue>=10) { // 10 milliseconds=1 hundredth of a second
    lastTimerValue=millis();
    intSecHundr++;

    /*// Push button handling -----------------------------------------
    int intPushButtonState = digitalRead(PUSHBUTTONPIN);
    if(intPushButtonState != intPushButtonPrevState) { 
      dbgprintf("Button changed state: %i", intPushButtonState);
      intPushButtonPrevState = intPushButtonState;
      intPushButtonCounter=PUSHBUTTONLONGPRESSHUNDR; // Reinit counter
      if(intPushButtonState==HIGH) { // It is now HIGH, it was LOW but no long press so it's a 
        // Normal press:
        blnEnabled = !blnEnabled; // Toggle enable
        dbgprintf("Enabled: %i", blnEnabled);
        LedProgramInit();
      }
    } else if(intPushButtonState==LOW) {
      //dbgprintf("Button state: %i intPushButtonCounter=%i", intPushButtonState,intPushButtonCounter);
      if(intPushButtonCounter>0) {
        intPushButtonCounter--;
        if(intPushButtonCounter==0) { // It has been LOW for intPushButtonCounter hundredths/sec = Long push 
          intPushButtonPrevState = intPushButtonState; // Cheat this routine so next loop it doesn't retrigger
          if(++intLedCurPgm > NUMLEDPROGRAMS-1) intLedCurPgm=0; // Select next program in sequence, looping through them
          LedProgramInit();
          dbgprintf("Long Press! Start Program #%i", intLedCurPgm);
          blnEnabled = true; 
        }
      }
    }*/


    if(RemoteXY.RaSwitch1==0) { // Power OFF
      LedProgramInit(); // shut down LEDs
    } else {
      if(intLedCurPgm != RemoteXY.RaAnimazione) {
        LedProgramInit(); 
      }
      intLedCurPgm=RemoteXY.RaAnimazione;
      intBrightness=MAX_DUTY_CYCLE*(100-RemoteXY.slider_1)/100;
      // LED handling -------------------------------------------------
      for(uint8_t i=0;i<NUMLEDS;i++) {
        if(intLedTimeCounter[i]>0) {
          if(--intLedTimeCounter[i]==0) {
            intLedCurAniIdx[i]++;
            if(aLedProgram[intLedCurPgm][i][intLedCurAniIdx[i]][0]==ENDANIMATION) {
              intLedCurAniIdx[i]=0;
            } 
            intLedTimeCounter[i]=aLedProgram[intLedCurPgm][i][intLedCurAniIdx[i]][1]*100; // Seconds->Hundredths
            LedFadeIncrement[i]=intBrightness/(((float)intLedTimeCounter[i]/TIME_RES_MILLIS*10));
            if(i==0){
              dbgprintf("Led %i changed animation step: %i Counter=%i FadeIncrement=%f", i, intLedCurAniIdx[i], intLedTimeCounter[i], LedFadeIncrement[i]);
            }
          }
        }
        switch(aLedProgram[intLedCurPgm][i][intLedCurAniIdx[i]][0]) {
          case OFF:
            LedPWM[i]=0;
            break;
          case ON:
              LedPWM[i]=intBrightness;
            break;
          case FADEUP:
            if(intBrightness-LedPWM[i]>LedFadeIncrement[i]) {
              LedPWM[i]+=LedFadeIncrement[i];
            } else {
              LedPWM[i]=intBrightness;
            }
            break;
          case FADEDOWN:
            if(LedPWM[i]>LedFadeIncrement[i]) {
              LedPWM[i]-=LedFadeIncrement[i];
            } else {
              LedPWM[i]=0;
            }
            break;
        }
        ledcWrite(i, LedPWM[i]);
        if(i==0){
          //dbgprintf("Ani=%i LedPWM[i]=%f Count=%i LedFadeIncrement[i]=%f",intLedCurAniIdx[i],LedPWM[i],intLedTimeCounter[i],LedFadeIncrement[i]);
        }
      } // for(i) NUMLEDS
    } // if(blnEnabled)
    
    //dbgprintf("intSecHundr=%i",intSecHundr);

    // FAN handling -------------------------------------------------
    if(abs(intSecHundr-intLastTempRead)>=100) { // Conversion on DS18B20 takes about 750ms so read it each 1 second interval
      intLastTempRead=intSecHundr;
      temperatureC = sensors.getTempCByIndex(0);
      sensors.requestTemperatures(); // Start a new reading 
      //dbgprintf("temperatureC=%f ºC",temperatureC);
      int tempScale=temperatureC;
      if(tempScale>FAN_TEMP_MAX) tempScale=FAN_TEMP_MAX;
      if(tempScale<FAN_TEMP_MIN) tempScale=FAN_TEMP_MIN;
      int intPwmFan=(int)(tempScale-FAN_TEMP_MIN)*(FAN_PWM_MAX/(FAN_TEMP_MAX-FAN_TEMP_MIN));
      dbgprintf("%f °C PWM=%i tempScale=%i PWM_MAX=%i intSecHundr=%i",temperatureC,intPwmFan,tempScale,FAN_PWM_MAX,intSecHundr);
      ledcWrite(FAN_PWMCHAN, intPwmFan);
    }
  }
  
  delay(TIME_RES_MILLIS);
}
