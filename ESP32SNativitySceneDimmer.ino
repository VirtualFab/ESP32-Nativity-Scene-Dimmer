#include <OneWire.h>
#include <DallasTemperature.h>

#define VERBOSE true
#define TIME_RES_MILLIS 10 

// LED ------------------------------------------------------
#define NUMLEDS 8
#define MAXANIMATIONSTEPS 10
enum {
  ENDANIMATION=0,
  FADEUP,
  FADEDOWN,
  ON,
  OFF
};

//uint8_t LedPin[NUMLEDS] ={2,15,5,18,19,21,36,39,32,33,25,26,27,14,12};
uint8_t LedPin[NUMLEDS]   ={2,15,5,18,19,21,36,39};
uint8_t intLedCurAniIdx[NUMLEDS];
uint16_t intLedTimeCounter[NUMLEDS];
uint8_t aLedProgram[][NUMLEDS][MAXANIMATIONSTEPS][2]={
{
   { {OFF, 1},{FADEUP,15},{ON, 1}, {FADEDOWN,15}, {OFF, 5} }
  ,{ {OFF,32},{FADEUP,15},{ON, 1}, {FADEDOWN,15}, {OFF,96} }
  ,{ {OFF,64},{FADEUP,35},{ON, 1}, {FADEDOWN,15}, {OFF,64} }
  ,{ {OFF,96},{FADEUP,45},{ON, 1}, {FADEDOWN,15}, {OFF,32} }
}
,{
   { {ON, 1},{OFF, 1} }
  ,{ {ON, 1},{OFF, 1} }
  ,{ {ON, 1},{OFF, 1} }
  ,{ {ON, 1},{OFF, 1} }
}
,{
   { {FADEUP, 5},{FADEDOWN, 5} }
  ,{ {FADEUP, 5},{FADEDOWN, 5} }
  ,{ {FADEUP, 5},{FADEDOWN, 5} }
  ,{ {FADEUP, 5},{FADEDOWN, 5} }
}
};
#define NUMLEDPROGRAMS sizeof(aLedProgram)/NUMLEDS/MAXANIMATIONSTEPS/2

uint8_t intLedCurPgm=0;
//uint8_t LedFadeSpeed=10;
float LedPWM[NUMLEDS];
float LedFadeIncrement[NUMLEDS];

/* LED PWM Settings */
#define PWMFREQ 5000 /* 5 KHz */
#define PWMRESOLUTION 10
const int MAX_DUTY_CYCLE = (int)(pow(2, PWMRESOLUTION) - 1);

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
#define FAN_PIN 32 
#define FAN_PWMCHAN 15       // use the last one, leaving all the other 15 to LEDs
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
  dbgprintf("READY");
  dbgprintf("intAniSize0=%i",NUMLEDPROGRAMS);
}

boolean blnEnabled=false;

// ------------------------------------------------------------------------------
void loop() {
// ------------------------------------------------------------------------------
#define PUSHBUTTONLONGPRESS 200 // 2 seconds for a long press state
  
  if(millis()-lastTimerValue>=10) { // 10 milliseconds=1 hundredth of a second
    lastTimerValue=millis();
    intSecHundr++;

    // Push button handling -----------------------------------------
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
    }

    if(blnEnabled) {
      // LED handling -------------------------------------------------
      for(uint8_t i=0;i<NUMLEDS;i++) {
        if(intLedTimeCounter[i]>0) {
          if(--intLedTimeCounter[i]==0) {
            intLedCurAniIdx[i]++;
            if(aLedProgram[intLedCurPgm][i][intLedCurAniIdx[i]][0]==ENDANIMATION) {
              intLedCurAniIdx[i]=0;
            } 
            intLedTimeCounter[i]=aLedProgram[intLedCurPgm][i][intLedCurAniIdx[i]][1]*100; // Seconds->Hundredths
            LedFadeIncrement[i]=MAX_DUTY_CYCLE/(((float)intLedTimeCounter[i]/TIME_RES_MILLIS*10));
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
            LedPWM[i]=MAX_DUTY_CYCLE;
            break;
          case FADEUP:
            if(MAX_DUTY_CYCLE-LedPWM[i]>LedFadeIncrement[i]) {
              LedPWM[i]+=LedFadeIncrement[i];
            } else {
              LedPWM[i]=MAX_DUTY_CYCLE;
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
