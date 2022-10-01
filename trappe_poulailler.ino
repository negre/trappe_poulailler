#define USE_SUNSET_LIB

#ifdef USE_SUNSET_LIB
#include <sunset.h>
#else
#include <Dusk2Dawn.h>
#endif

#define USE_PCF85063TP
//#define USE_DS1307

#include <Servo.h>

#ifdef USE_DS1307
#include <RTClib.h>
RTC_DS1307 rtc;
#endif

#ifdef USE_PCF85063TP
#include <PCF85063TP.h>
PCD85063TP rtc;
#endif

#define PIN_SERVO 9
#define PIN_MOTOR_UP 6
#define PIN_MOTOR_DOWN 7
#define PIN_SWITCH_UP 2
#define PIN_SWITCH_DOWN 3
#define PIN_SWITCH_MANUAL 8

#define SERVO_LOCK 0
#define SERVO_UNLOCK 180

const int OPEN_TIMEOUT = 12000;
const int UNLOCK_TIMEOUT = 4000;
const int CLOSE_TIMEOUT = 12000;

#define LAT 45.23
#define LON 5.88

#define SUNSET_OFFSET 10

enum State
{
  OPENING,
  LOCKING,
  OPENED,
  UNLOCKING_STEP1,
  UNLOCKING_STEP2,
  CLOSING,
  CLOSED,
  ERROR,
};



Servo servo;
#ifdef USE_SUNSET_LIB
SunSet location(LAT, LON, 2);
#else
Dusk2Dawn location(LAT, LON, 2);
#endif
char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
char stateStr[8][16] = {"OPENING", "LOCKING", "OPENED", "UNLOCKING_STEP1", "UNLOCKING_STEP2", "CLOSING", "CLOSED", "ERROR"};

volatile State state = CLOSED;

unsigned long stateTime;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(57600);
#ifdef USE_DS1307
  if (! rtc.begin()) {
    Serial.println("Couldn't find RTC");
    while (1);
  }
#endif
#ifdef USE_PCF85063TP
  rtc.begin();
#endif

  // uncomment to set the RTC time
#ifdef USE_DS1307
   //rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  //rtc.adjust(DateTime(2021, 12, 20, 13, 23, 0));
#endif
#ifdef USE_PCF85063TP
//  rtc.stopClock();
//  rtc.fillByYMD(2022,10,1);
//  rtc.fillByHMS(14,35,00);
//  rtc.fillDayOfWeek(SAT);//Saturday
//  rtc.setTime();//write time to the RTC chip
//  rtc.startClock();
#endif

  pinMode(PIN_SWITCH_UP, INPUT_PULLUP);
  pinMode(PIN_SWITCH_DOWN, INPUT_PULLUP);
  pinMode(PIN_SWITCH_MANUAL, INPUT_PULLUP);
  
  pinMode(PIN_MOTOR_UP, OUTPUT);
  pinMode(PIN_MOTOR_DOWN, OUTPUT);

  motorStop();
  
  attachInterrupt(digitalPinToInterrupt(PIN_SWITCH_UP), switchUpHandler, RISING);
  attachInterrupt(digitalPinToInterrupt(PIN_SWITCH_DOWN), switchDownHandler, RISING);

  stateTime = millis();
}

void loop() {
  // put your main code here, to run repeatedly:
#if USE_DS1307
  DateTime now = rtc.now();
  Serial.print(now.day(), DEC);
  Serial.print('/');
  Serial.print(now.month(), DEC);
  Serial.print('/');
  Serial.print(now.year(), DEC);
  Serial.print(' ');
  
  //  Serial.print(" (");
  //  Serial.print(daysOfTheWeek[now.dayOfTheWeek()]);
  //  Serial.print(") ");
  Serial.print(now.hour(), DEC);
  Serial.print(':');
  Serial.print(now.minute(), DEC);
  Serial.print(':');
  Serial.print(now.second(), DEC);
  Serial.println();

#endif

#ifdef USE_PCF85063TP
  rtc.getTime();

  Serial.print(rtc.dayOfMonth, DEC);
  Serial.print('/');
  Serial.print(rtc.month, DEC);
  Serial.print('/');
  Serial.print(rtc.year+2000, DEC);
  Serial.print(' ');
  Serial.print(rtc.hour, DEC);
  Serial.print(':');
  Serial.print(rtc.minute, DEC);
  Serial.print(':');
  Serial.print(rtc.second, DEC);
  Serial.println();
  
#endif
  
  //  Serial.println();
  //
  //  Serial.print("SunRise : ");
  //  Serial.print(sunrise / 60);
  //  Serial.print("h ");
  //  Serial.print(sunrise % 60);
  //  Serial.println("min");
  //
  //
  //  Serial.print("SunSet : ");
  //  Serial.print(sunset / 60);
  //  Serial.print("h ");
  //  Serial.print(sunset % 60);
  //  Serial.println("min");

  printState(state);
  Serial.println();
  
  switch (state)
  {
    case OPENING:
      if (millis() - stateTime > OPEN_TIMEOUT)
      {
        handleError();
      }
      break;
    case LOCKING:
      //printState(state);
      //Serial.println();
      lock();
      state = OPENED;
      //printState(state);
      //Serial.println();
      break;
    case OPENED:
      {
        //DateTime now = rtc.now();
        
#if USE_DS1307
        int current_min = 60 * now.hour() + now.minute();

#ifdef USE_SUNSET_LIB
        location.setCurrentDate(now.year(), now.month(), now.day());
        int sunrise = location.calcSunrise();
        int sunset = SUNSET_OFFSET + location.calcCivilSunset();
#else
        int sunrise = location.sunrise(now.year(), now.month(), now.day(), false);
        int sunset = SUNSET_OFFSET + location.sunset(now.year(), now.month(), now.day(), false);
#endif
#endif
   
#ifdef USE_PCF85063TP
        int current_min = 60 * rtc.hour + rtc.minute;

#ifdef USE_SUNSET_LIB
        location.setCurrentDate(rtc.year, rtc.month, rtc.dayOfMonth);
        int sunrise = location.calcSunrise();
        int sunset = SUNSET_OFFSET + location.calcCivilSunset();
#else
        int sunrise = location.sunrise(rtc.year()+2000, rtc.month, rtc.dayOfMonth, false);
        int sunset = SUNSET_OFFSET + location.sunset(rtc.year+2000, rtc.month, rtc.dayOfMonth, false);
#endif   
#endif  
        
        Serial.print("closing scheduled at ");
        Serial.print(sunset/60);
        Serial.print("h");
        Serial.print(sunset%60);
        Serial.println();
        if (current_min > sunset || current_min<sunrise || digitalRead(PIN_SWITCH_MANUAL)==LOW)
        {
          //unlock trap
          state = UNLOCKING_STEP1;
          stateTime = millis();
          if(isUp()){
            state = UNLOCKING_STEP2;
          }else{
            motorUp();
          }
          //printState(state);
          //Serial.println();
        } else
        {
          delay(1000);
        }
      }
      break;
    case UNLOCKING_STEP1:
      if (millis() - stateTime > UNLOCK_TIMEOUT)
      {
        handleError();
      }
      break;
    case UNLOCKING_STEP2:
      printState(state);
      Serial.println();
      unlock();
      stateTime = millis();
      state = CLOSING;
      if(isDown())
      {
        state=CLOSED;
      }else{
        motorDown();
      }
      printState(state);
      Serial.println();
      break;
    case CLOSING:
      if (millis() - stateTime > CLOSE_TIMEOUT)
      {
        motorStop();
        stateTime = millis();
        state=CLOSED;
      }
      break;
    case CLOSED:
      {
#if USE_DS1307
        int current_min = 60 * now.hour() + now.minute();

#ifdef USE_SUNSET_LIB
        location.setCurrentDate(now.year(), now.month(), now.day());
        int sunrise = location.calcSunrise();
        int sunset = SUNSET_OFFSET + location.calcCivilSunset();
#else
        int sunrise = location.sunrise(now.year(), now.month(), now.day(), false);
        int sunset = SUNSET_OFFSET + location.sunset(now.year(), now.month(), now.day(), false);
#endif   
#endif

#ifdef USE_PCF85063TP
        int current_min = 60 * rtc.hour + rtc.minute;

#ifdef USE_SUNSET_LIB
        location.setCurrentDate(rtc.year, rtc.month, rtc.dayOfMonth);
        int sunrise = location.calcSunrise();
        int sunset = SUNSET_OFFSET + location.calcCivilSunset();
#else
        int sunrise = location.sunrise(rtc.year()+2000, rtc.month, rtc.dayOfMonth, false);
        int sunset = SUNSET_OFFSET + location.sunset(rtc.year+2000, rtc.month, rtc.dayOfMonth, false);
#endif   
#endif  

        Serial.print("Opening scheduled at ");
        Serial.print(sunrise/60);
        Serial.print("h");
        Serial.print(sunrise%60);
        Serial.println();
        
        if ((current_min > sunrise && current_min < sunset) || digitalRead(PIN_SWITCH_MANUAL)==LOW)
        {
          //unlock trap
          unlock();
          state = OPENING;
          stateTime = millis();
          if(isUp()){
            state = LOCKING;
          }else{
            motorUp();
          }
          //printState(state);
          //Serial.println();
        } else
        {
          delay(1000);
        }
      }
      break;
    case ERROR:
    {
      if(digitalRead(PIN_SWITCH_MANUAL)==LOW)
      {
        state = CLOSED;
      }else{
        delay(1000);
      }
      break;
    }
    default:
      delay(1000);
      break;
  }

  Serial.println();
  //delay(10);
}

bool isUp()
{
  return digitalRead(PIN_SWITCH_UP)==HIGH;
}

bool isDown()
{
  return digitalRead(PIN_SWITCH_DOWN)==HIGH;
}

void motorUp()
{
  digitalWrite(PIN_MOTOR_DOWN, HIGH);
  digitalWrite(PIN_MOTOR_UP, LOW);
}

void motorDown()
{
  digitalWrite(PIN_MOTOR_UP, HIGH);
  digitalWrite(PIN_MOTOR_DOWN, LOW);
}

void motorStop()
{
  digitalWrite(PIN_MOTOR_UP, HIGH);
  digitalWrite(PIN_MOTOR_DOWN, HIGH);
}

void lock()
{
  servo.attach(PIN_SERVO);
  servo.write(SERVO_LOCK);
  delay(4000);
  servo.detach();
}

void unlock()
{
  servo.attach(PIN_SERVO);
  servo.write(SERVO_UNLOCK);
  delay(4000);
  servo.detach();
}

void switchUpHandler()
{
  // security stop Motor Up
  digitalWrite(PIN_MOTOR_UP, HIGH);

  // Process state machine
  if (state == OPENING)
  {
    motorStop();
    state = LOCKING;
  } else if (state == UNLOCKING_STEP1)
  {
    motorStop();
    state = UNLOCKING_STEP2;
  }
}

void switchDownHandler()
{
  // security stop Motor Down
  digitalWrite(PIN_MOTOR_DOWN, HIGH);

  // Process state machine
  if (state == CLOSING)
  {
    motorStop();
    state = CLOSED;
    Serial.println("CLOSED");
  }
}

void printState(State s)
{
  Serial.print(stateStr[s]);
}

void handleError()
{
  motorStop();
  State prev_state = state;
  state = ERROR;
  Serial.print("ERROR during ");
  printState(prev_state);
  Serial.println();
}
