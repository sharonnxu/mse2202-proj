#include <Servo.h>
#include <EEPROM.h>
#include <uSTimer2.h>
#include <CharliePlexM.h>
#include <Wire.h>
#include <I2CEncoder.h>

// Uncomment keywords to enable debugging output

//#define DEBUG_MODE_DISPLAY
//#define DEBUG_MOTORS
//#define DEBUG_LINE_TRACKERS
//#define DEBUG_ENCODERS
//#define DEBUG_ULTRASONIC_FRONT
//#define DEBUG_ULTRASONIC_RIGHT
//#define DEBUG_ULTRASONIC_LEFT
//#define DEBUG_LINE_TRACKER_CALIBRATION
//#define DEBUG_MOTOR_CALIBRATION

boolean bt_Motors_Enabled = true;

//port pin constants
const int ci_Ultrasonic_Data_Front = 2;   //input plug
const int ci_Ultrasonic_Ping_Front = 3;   //output plug
//right side ultrasonic
const int ci_Ultrasonic_Data_Right = 4;   //input plug
const int ci_Ultrasonic_Ping_Right = 5;   //output plug
//left side ultrasonic
const int ci_Ultrasonic_Data_Left = 6;   //input plug
const int ci_Ultrasonic_Ping_Left = 7;   //output plug

//assigned following LED pins randomly because did not want them to be implemented in process (they act as dummy variables)
const int ci_Charlieplex_LED1 = 99;//4
const int ci_Charlieplex_LED2 = 99;//5
const int ci_Charlieplex_LED3 = 99;//6
const int ci_Charlieplex_LED4 = 99;//7

const int ci_Mode_Button = 0;//button to start robot either in mode 1 or 2
const int ci_Right_Motor = 8;//right motor that controls right wheel
const int ci_Left_Motor = 9;//left motor that controls left wheel
const int ci_LeftPivot_Motor = 13;//left servo motor that is attached to left side of pivot
const int ci_RightPivot_Motor = 12;//right servo motor that is attached to right side of pivot
const int ci_Hall_Effect = A0;//hall effect sensor input
const int ci_Right_Line_Tracker = A1;//right line tracker at front of robot
const int ci_Middle_Line_Tracker = A2; // line tracker attached at tip of arm where magnet is
const int ci_Left_Line_Tracker = A3;//left line tracker at the front of robot
const int ci_I2C_SDA = A4;         // I2C data = white
const int ci_I2C_SCL = A5;         // I2C clock = yellow

const int ci_servo_turntable = 1;//turn table that rotates arm
const int ci_servo_arm = 11;//rotation of arm that turns it vertically upward by 180 degrees
const int ci_servo_magnet = 10;//servo that controls arm that holds magnet

// EEPROM addresses
const int ci_Left_Line_Tracker_Dark_Address_L = 0;
const int ci_Left_Line_Tracker_Dark_Address_H = 1;
const int ci_Left_Line_Tracker_Light_Address_L = 2;
const int ci_Left_Line_Tracker_Light_Address_H = 3;
const int ci_Middle_Line_Tracker_Dark_Address_L = 4;
const int ci_Middle_Line_Tracker_Dark_Address_H = 5;
const int ci_Middle_Line_Tracker_Light_Address_L = 6;
const int ci_Middle_Line_Tracker_Light_Address_H = 7;
const int ci_Right_Line_Tracker_Dark_Address_L = 8;
const int ci_Right_Line_Tracker_Dark_Address_H = 9;
const int ci_Right_Line_Tracker_Light_Address_L = 10;
const int ci_Right_Line_Tracker_Light_Address_H = 11;
const int ci_Left_Motor_Offset_Address_L = 12;
const int ci_Left_Motor_Offset_Address_H = 13;
const int ci_Right_Motor_Offset_Address_L = 14;
const int ci_Right_Motor_Offset_Address_H = 15;

const int ci_Left_Motor_Stop = 1500;        // 200 for brake mode; 1500 for stop
const int ci_Right_Motor_Stop = 1500;
const int ci_Display_Time = 500;
const int ci_Line_Tracker_Calibration_Interval = 100;
const int ci_Line_Tracker_Cal_Measures = 20;
const int ci_Line_Tracker_Tolerance = 50;   // May need to adjust this
const int ci_Motor_Calibration_Cycles = 3;
const int ci_Motor_Calibration_Time = 5000;

//variables
byte b_LowByte;
byte b_HighByte;

//following used to hold distance obtained from the three ultrasonics on robot
unsigned long ul_Echo_Time_Front;
unsigned long ul_Echo_Time_Right;
unsigned long ul_Echo_Time_Left;

unsigned int ui_Left_Line_Tracker_Data;
unsigned int ui_Middle_Line_Tracker_Data;
unsigned int ui_Right_Line_Tracker_Data;
unsigned int ui_Motors_Speed = 1900;        // Default run speed
unsigned int ui_Left_Motor_Speed;
unsigned int ui_Right_Motor_Speed;
long l_Left_Motor_Position;
long l_Right_Motor_Position;

unsigned long ul_3_Second_timer = 0;
unsigned long ul_Display_Time;
unsigned long ul_Calibration_Time;
unsigned long ui_Left_Motor_Offset;
unsigned long ui_Right_Motor_Offset;

unsigned int ui_Cal_Count;
unsigned int ui_Cal_Cycle;
unsigned int ui_Left_Line_Tracker_Dark;
unsigned int ui_Left_Line_Tracker_Light;
unsigned int ui_Middle_Line_Tracker_Dark;
unsigned int ui_Middle_Line_Tracker_Light;
unsigned int ui_Right_Line_Tracker_Dark;
unsigned int ui_Right_Line_Tracker_Light;
unsigned int ui_Line_Tracker_Tolerance;

unsigned int  ui_Robot_State_Index = 0;
//0123456789ABCDEF
unsigned int  ui_Mode_Indicator[6] = {
  0x00,    //B0000000000000000,  //Stop
  0x00FF,  //B0000000011111111,  //Run
  0x0F0F,  //B0000111100001111,  //Calibrate line tracker light level
  0x3333,  //B0011001100110011,  //Calibrate line tracker dark level
  0xAAAA,  //B1010101010101010,  //Calibrate motors
  0xFFFF   //B1111111111111111   //Unused
};

unsigned int  ui_Mode_Indicator_Index = 0;

//display Bits 0,1,2,3, 4, 5, 6,  7,  8,  9,  10,  11,  12,  13,   14,   15
int  iArray[16] = {
  1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 65536
};
int  iArrayIndex = 0;
int mag_field = 0; //hall-effect sensor value
boolean bt_Heartbeat = true;
boolean bt_3_S_Time_Up = false;
boolean bt_Do_Once = false;
boolean bt_Cal_Initialized = false;

Servo servo_RightMotor;
Servo servo_LeftMotor;

Servo servo_turntable, servo_arm, servo_magnet;
Servo servo_LeftPivot;
Servo servo_RightPivot;
I2CEncoder encoder_RightMotor;
I2CEncoder encoder_LeftMotor;

int pivot_pos = 28;//initialize initial pivot position

/*****************************************************************SETUP************************************************************/
void setup() {

  Wire.begin();

  //setup ultrasonic
  pinMode(ci_Ultrasonic_Ping_Front, OUTPUT);
  pinMode(ci_Ultrasonic_Data_Front, INPUT);
  pinMode(ci_Ultrasonic_Ping_Right, OUTPUT);
  pinMode(ci_Ultrasonic_Data_Right, INPUT);
  pinMode(ci_Ultrasonic_Ping_Left, OUTPUT);
  pinMode(ci_Ultrasonic_Data_Left, INPUT);

  CharliePlexM::setBtn(ci_Charlieplex_LED1, ci_Charlieplex_LED2,
                       ci_Charlieplex_LED3, ci_Charlieplex_LED4, ci_Mode_Button);

  // set up drive motors
  pinMode(ci_Right_Motor, OUTPUT);
  servo_RightMotor.attach(ci_Right_Motor);
  pinMode(ci_Left_Motor, OUTPUT);
  servo_LeftMotor.attach(ci_Left_Motor);

  //set up servos
  pinMode(ci_servo_turntable, OUTPUT);
  pinMode(ci_servo_arm, OUTPUT);
  pinMode(ci_servo_magnet, OUTPUT);

  // set up encoders. Must be initialized in order that they are chained together,
  // starting with the encoder directly connected to the Arduino. See I2CEncoder docs
  // for more information
  encoder_LeftMotor.init(1.0 / 3.0 * MOTOR_393_SPEED_ROTATIONS, MOTOR_393_TIME_DELTA);
  encoder_LeftMotor.setReversed(false);  // adjust for positive count when moving forward
  encoder_RightMotor.init(1.0 / 3.0 * MOTOR_393_SPEED_ROTATIONS, MOTOR_393_TIME_DELTA);
  encoder_RightMotor.setReversed(true);  // adjust for positive count when moving forward

  // set up line tracking sensors
  pinMode(ci_Right_Line_Tracker, INPUT);
  pinMode(ci_Middle_Line_Tracker, INPUT);
  pinMode(ci_Left_Line_Tracker, INPUT);
  ui_Line_Tracker_Tolerance = ci_Line_Tracker_Tolerance;

  //set up hall effect sensor
  pinMode(ci_Hall_Effect, INPUT);

  // read saved values from EEPROM
  b_LowByte = EEPROM.read(ci_Left_Line_Tracker_Dark_Address_L);
  b_HighByte = EEPROM.read(ci_Left_Line_Tracker_Dark_Address_H);
  ui_Left_Line_Tracker_Dark = word(b_HighByte, b_LowByte);
  b_LowByte = EEPROM.read(ci_Left_Line_Tracker_Light_Address_L);
  b_HighByte = EEPROM.read(ci_Left_Line_Tracker_Dark_Address_H);
  ui_Left_Line_Tracker_Light = word(b_HighByte, b_LowByte);
  b_LowByte = EEPROM.read(ci_Middle_Line_Tracker_Dark_Address_L);
  b_HighByte = EEPROM.read(ci_Left_Line_Tracker_Dark_Address_H);
  ui_Middle_Line_Tracker_Dark = word(b_HighByte, b_LowByte);
  b_LowByte = EEPROM.read(ci_Middle_Line_Tracker_Light_Address_L);
  b_HighByte = EEPROM.read(ci_Left_Line_Tracker_Dark_Address_H);
  ui_Middle_Line_Tracker_Light = word(b_HighByte, b_LowByte);
  b_LowByte = EEPROM.read(ci_Right_Line_Tracker_Dark_Address_L);
  b_HighByte = EEPROM.read(ci_Left_Line_Tracker_Dark_Address_H);
  ui_Right_Line_Tracker_Dark = word(b_HighByte, b_LowByte);
  b_LowByte = EEPROM.read(ci_Right_Line_Tracker_Light_Address_L);
  b_HighByte = EEPROM.read(ci_Left_Line_Tracker_Dark_Address_H);
  ui_Right_Line_Tracker_Light = word(b_HighByte, b_LowByte);
  b_LowByte = EEPROM.read(ci_Left_Motor_Offset_Address_L);
  b_HighByte = EEPROM.read(ci_Left_Motor_Offset_Address_H);
  ui_Left_Motor_Offset = word(b_HighByte, b_LowByte);
  b_LowByte = EEPROM.read(ci_Right_Motor_Offset_Address_L);
  b_HighByte = EEPROM.read(ci_Right_Motor_Offset_Address_H);
  ui_Right_Motor_Offset = word(b_HighByte, b_LowByte);

  //attach servo motors being used
  servo_turntable.attach(ci_servo_turntable);
  servo_magnet.attach(ci_servo_magnet);
  //set up arm motor
  servo_arm.attach(ci_servo_arm);
  //setup pivot leftarm motors
  servo_LeftPivot.attach(ci_LeftPivot_Motor);
  //setup pivot rightarm motors
  servo_RightPivot.attach(ci_RightPivot_Motor);

  //initialize servo positions
  servo_RightPivot.write(pivot_pos);
  servo_LeftPivot.write(180 - pivot_pos);
  servo_turntable.write(41);
  servo_arm.write(170);
  servo_magnet.write(115);//change to 115 min, 150 max

}
/*********************************************************************END SETUP**************************************************************/

//declare variables
int p = 0;
int q = 0;

//used for timing purposes in code
int previous = 0;
int current = 0;
int current1 = 0;
int previous1 = 0;
int previous_ping = 0;
int current_ping = 0;
int previous_scan = 0;
int current_scan = 0;

//used to read hall effect sensor changes inmagnetic flux
int previous_magflux = analogRead(ci_Hall_Effect);
int current_magflux = analogRead(ci_Hall_Effect);

//used for timing purposes in code
int previous_detect = 0;
int current_detect = 0;

bool holding_tesseract = false;
bool temp = false;
bool drive = true;//robot moves forward when this is true
int bot_speed = 1700;
int bot_stop = 1500;
bool start_turn = false;//used to indicate when turn has started or not
bool end_turn = true;//used to indicate when turn has ended or not
int num_turns = 0;//turn count
int num_scans = 0;//scan count
int tesseract_count = 0;//amount of tesseracts collected count
int left_wheel = 0;//used to calculated wheel rotation while turning
int right_wheel = 0;//used to calculated wheel rotation while turning
int left_wheel_prev = 0;//used to hold previous encoder value of left wheel
int right_wheel_prev = 0;//used to hold previous encoder value of right wheel
//use these variables for navigation (mapping the area)
int x = 0;
int y = 0;
int mode2counter = 0;
int timer = 1;
bool timer1 = false;
bool timer2 = false;
bool lo = false;
bool timer3 = false; bool timer4 = false; bool timer5 = false; bool timer6 = false; bool timer7 = false;
bool timer9 = false; bool timer8 = false;
void readLineTrackers()
{
  ui_Left_Line_Tracker_Data = analogRead(ci_Left_Line_Tracker);
  ui_Middle_Line_Tracker_Data = analogRead(ci_Middle_Line_Tracker);
  ui_Right_Line_Tracker_Data = analogRead(ci_Right_Line_Tracker);

#ifdef DEBUG_LINE_TRACKERS
  Serial.print("Trackers: Left = ");
  Serial.print(ui_Left_Line_Tracker_Data, DEC);
  Serial.print(", Middle = ");
  Serial.print(ui_Middle_Line_Tracker_Data, DEC);
  Serial.print(", Right = ");
  Serial.println(ui_Right_Line_Tracker_Data, DEC);
#endif

}

int pos = 40;//used for initial position of scanning

void scan() {//rotates arm to scan for good tesseracks

  if (num_scans % 2 == 0) { //scan by moving arm leftward

    pos++;
    servo_turntable.write(pos);
    if (pos >= 120) { //180 means maximum to left side of robot
      previous = current;//reset timer for travelling forward
      drive = true;//drive forward again after completing one scan
      num_scans++;//one scan has been completed
    }
  }
  else { //alternate to rightward scanning every other scan
    pos--;
    servo_turntable.write(pos);
    if (pos <= 40) { //0 means maximum to right side of robot
      drive = true;//drive forward again after completing one scan
      previous = current;//reset timer for travelling forward
      num_scans++;//one scan has been completed
    }
  }
}

void Ping_Left()
{
  //Ping Ultrasonic
  //Send the Ultrasonic Range Finder a 10 microsecond pulse per tech spec
  digitalWrite(ci_Ultrasonic_Ping_Left, HIGH);
  delayMicroseconds(10);  //The 10 microsecond pause where the pulse in "high"
  digitalWrite(ci_Ultrasonic_Ping_Left, LOW);
  //use command pulseIn to listen to Ultrasonic_Data pin to record the
  //time that it takes from when the Pin goes HIGH until it goes LOW
  ul_Echo_Time_Left = pulseIn(ci_Ultrasonic_Data_Left, HIGH, 10000);


#ifdef DEBUG_ULTRASONIC_LEFT
  // Print Sensor Readings
  Serial.print("Time (microseconds): ");
  Serial.print(ul_Echo_Time_Left, DEC);
  Serial.print(", cm: ");
  Serial.println(ul_Echo_Time_Left / 24); //divide time by 58 to get distance in cm
#endif

}

void Ping_Right()
{
  //Ping Ultrasonic
  //Send the Ultrasonic Range Finder a 10 microsecond pulse per tech spec
  digitalWrite(ci_Ultrasonic_Ping_Right, HIGH);
  delayMicroseconds(10);  //The 10 microsecond pause where the pulse in "high"
  digitalWrite(ci_Ultrasonic_Ping_Right, LOW);
  //use command pulseIn to listen to Ultrasonic_Data pin to record the
  //time that it takes from when the Pin goes HIGH until it goes LOW
  ul_Echo_Time_Right = pulseIn(ci_Ultrasonic_Data_Right, HIGH, 10000);


#ifdef DEBUG_ULTRASONIC_RIGHT
  // Print Sensor Readings
  Serial.print("Time (microseconds): ");
  Serial.print(ul_Echo_Time_Right, DEC);
  Serial.print(", cm: ");
  Serial.println(ul_Echo_Time_Right / 24); //divide time by 58 to get distance in cm
#endif

}

void Ping_Front()
{
  //Ping Ultrasonic
  //Send the Ultrasonic Range Finder a 10 microsecond pulse per tech spec
  digitalWrite(ci_Ultrasonic_Ping_Front, HIGH);
  delayMicroseconds(10);  //The 10 microsecond pause where the pulse in "high"
  digitalWrite(ci_Ultrasonic_Ping_Front, LOW);
  //use command pulseIn to listen to Ultrasonic_Data pin to record the
  //time that it takes from when the Pin goes HIGH until it goes LOW
  ul_Echo_Time_Front = pulseIn(ci_Ultrasonic_Data_Front, HIGH, 10000);

#ifdef DEBUG_ULTRASONIC_FRONT
  // Print Sensor Readings
  Serial.print("Time (microseconds): ");
  Serial.print(ul_Echo_Time_Front, DEC);
  Serial.print(", cm: ");
  Serial.println(ul_Echo_Time_Front / 24); //divide time by 58 to get distance in cm
#endif

}

void travel() {//makes robot travel straight, main movement function of robot

  if (num_turns % 2 == 0) {//use LEFT ultrasonic to detect wall and travel in a straight path
    if ((ul_Echo_Time_Left / 24) >= (10.5 + (25 * num_turns))) { //if moving AWAY FROM wall turn left
      servo_LeftMotor.writeMicroseconds(bot_speed - 5); //left motor moves slower
      servo_RightMotor.writeMicroseconds(bot_speed);
    }
    else if ((ul_Echo_Time_Left / 24) <= (9.5 + (25 * num_turns))) { //if moving TOWARD wall, turn right
      servo_LeftMotor.writeMicroseconds(bot_speed);
      servo_RightMotor.writeMicroseconds(bot_speed - 1); //right motor moves slower. This is small because robot leans to the right
    }
    else {//when in correct position do not adjust speed
      servo_LeftMotor.writeMicroseconds(bot_speed);
      servo_RightMotor.writeMicroseconds(bot_speed);
    }
  }

  else { //use RIGHT ultrasonic to detect wall and travel in a straight path
    if ((ul_Echo_Time_Right / 24) >= (10.5 + (25 * num_turns))) { //if moving TOWARD wall, turn right
      servo_LeftMotor.writeMicroseconds(bot_speed);
      servo_RightMotor.writeMicroseconds(bot_speed - 10); //right motor moves slower
    }
    else if ((ul_Echo_Time_Right / 24) <= (9.5 + (25 * num_turns))) { //if moving AWAY FROM wall, turn left
      servo_LeftMotor.writeMicroseconds(bot_speed - 10); //left motor moves slower
      servo_RightMotor.writeMicroseconds(bot_speed);
    }
    else {//when in correct position do not adjust speed
      servo_LeftMotor.writeMicroseconds(bot_speed);
      servo_RightMotor.writeMicroseconds(bot_speed);
    }
  }

}

void turn() {//turns on either end when it reaches the end wall
  //read encoder position
  left_wheel = encoder_LeftMotor.getRawPosition();
  right_wheel = encoder_RightMotor.getRawPosition();

  if (num_turns % 2 == 0) {
    servo_LeftMotor.writeMicroseconds(bot_speed);
    servo_RightMotor.writeMicroseconds(bot_stop);
  }
  else {
    servo_LeftMotor.writeMicroseconds(bot_stop);
    servo_RightMotor.writeMicroseconds(bot_speed);
  }

  if ((abs(left_wheel - left_wheel_prev) >= 1700) || (abs(right_wheel - right_wheel_prev) >= 1700)) { //when 180 degree turn is complete
    //reinitialize turn variables to stop turning
    start_turn = false;
    end_turn = true;
    num_turns++;//one turn has been completed
  }

}

//using the following step variables inside of gohome function
//this allows it to keep track of process of going home after picking up a tesseract
bool step1 = false;
bool step2 = false;
bool step3 = false;
bool step4 = false;
bool step5 = false;
bool step6 = false;
bool step7 = false;
bool step8 = false;
bool step9 = false;
bool step10 = false;
bool step11 = false;
bool step12 = false;

bool clockwise1 = true;

void GoHome() { //after tesserack is collected, goes home to place tesseract and then returns to previous position
  //read encoder position
  left_wheel = encoder_LeftMotor.getRawPosition();
  right_wheel = encoder_RightMotor.getRawPosition();

  if (step1 == true) {//make a 90 degree turn to the wall that is next to home and parallel to path of motion
    if (num_turns % 2 == 0) { // make a 90 degree leftward because left side is facing home
      servo_LeftMotor.writeMicroseconds(bot_stop - 100);//moves left wheel backward
      servo_RightMotor.writeMicroseconds(bot_stop + 100);//moves right wheel forward
      //turn 90 degrees left ward
    }

    else { // make a 90 degree rightward because right side is facing home

      servo_LeftMotor.writeMicroseconds(bot_stop + 100);//moves left wheel forward
      servo_RightMotor.writeMicroseconds(bot_stop - 100);//moves right wheel backward
      //turn 90 degrees right
    }
    //do two 90 degree turns to go home....450 might not be correct
    if ((abs(left_wheel - left_wheel_prev) >= 440) || (abs(right_wheel - right_wheel_prev) >= 440)) { //when quarter turn is complete might need to adjust value here
      servo_LeftMotor.writeMicroseconds(bot_stop);
      servo_RightMotor.writeMicroseconds(bot_stop);
      step1 = false;
      step2 = true;
      previous = current;
      delay(500);
    }

  }
  else if (step2 == true) {//move forward until reached 27 cm away from the wall
    servo_LeftMotor.writeMicroseconds(bot_speed);
    servo_RightMotor.writeMicroseconds(bot_speed);

    if (((ul_Echo_Time_Front / 24) <= 27) && ((ul_Echo_Time_Front / 24) > 0)) { //reached 27 cm away from wall
      servo_LeftMotor.writeMicroseconds(bot_stop);
      servo_RightMotor.writeMicroseconds(bot_stop);
      left_wheel_prev = encoder_LeftMotor.getRawPosition();
      right_wheel_prev = encoder_RightMotor.getRawPosition();
      step2 = false;
      step3 = true;
    }
  }
  else if (step3 == true) {//do a 90 degree left
    servo_LeftMotor.writeMicroseconds(bot_stop - 100);//moves left wheel backward
    servo_RightMotor.writeMicroseconds(bot_stop + 100);//moves right wheel forward

    if ((abs(left_wheel - left_wheel_prev) >= 440) || (abs(right_wheel - right_wheel_prev) >= 440)) { //when quarter turn is complete
      servo_LeftMotor.writeMicroseconds(bot_stop);//stop robot
      servo_RightMotor.writeMicroseconds(bot_stop);
      step3 = false;//continue to next step
      step4 = true;
      delay(500);
    }
  }
  else if (step4 == true) {//move forward until "32 cm" from wall
    servo_LeftMotor.writeMicroseconds(bot_speed);
    servo_RightMotor.writeMicroseconds(bot_speed);

    if (((ul_Echo_Time_Front / 24) <= 32) && ((ul_Echo_Time_Front / 24) > 0)) { //reached 32 cm away from wall
      servo_LeftMotor.writeMicroseconds(bot_stop);
      servo_RightMotor.writeMicroseconds(bot_stop);
      step4 = false;//continue to next step
      step5 = true;
    }
  }
  else if (step5 == true) {//drop tesseract
    for (int i = pos; i >= 23; i--) {//drops it slowly because of delay
      servo_turntable.write(i);
      delay(25);
    }
    for (int i = pivot_pos; i >= 45; i--) {//move pivot arm to be low enough to drop tesseract
      pivot_pos = i;
      servo_RightPivot.write(pivot_pos);
      servo_LeftPivot.write(180 - pivot_pos);//right and left pivot rotate in opposing directions
      delay(25);//this moves the arm slowly
    }
    servo_magnet.write(150);//drop
    delay(2500);//allow some time for the detachment of tesseract from magnet
    servo_magnet.write(115);//return to orignal posittion
    left_wheel_prev = encoder_LeftMotor.getRawPosition();//read encoder position to aid in turning 180degrees in next step
    right_wheel_prev = encoder_RightMotor.getRawPosition();
    step5 = false;
    step6 = true;

  }
  else if (step6 == true) {
    servo_LeftMotor.writeMicroseconds(bot_stop - 100);//moves left wheel backward
    servo_RightMotor.writeMicroseconds(bot_stop + 100);//moves right wheel forward
    //turn 90 degrees left ward

    //complete a 180 degree turn to return to original travelling and scanning of tesseracts
    if ((abs(left_wheel - left_wheel_prev) >= 920) || (abs(right_wheel - right_wheel_prev) >= 920)) { //when half turn is complete
      servo_LeftMotor.writeMicroseconds(bot_stop);
      servo_RightMotor.writeMicroseconds(bot_stop);
      for (int i = pivot_pos; i >= 28; i--) {//return arm to original scanning position
        pivot_pos = i;
        servo_RightPivot.write(pivot_pos);
        servo_LeftPivot.write(180 - pivot_pos);
        delay(25);
      }
      step6 = false;
      holding_tesseract = false;//reset variables to indicate that robot is back to scanning mode
      previous_magflux = analogRead(ci_Hall_Effect);//reinitilize hall effect readings
      current_magflux = previous_magflux;
      num_turns = 0;//reset number of turns

    }
  }

}


///////Below functions are for mode 2
void Mode2Scan()
{
  if (clockwise1 == true)
  { for (pos = 40; pos <= 120; pos += 1) { // goes from 0 degrees to 180 degrees
      // in steps of 1 degree
      { servo_turntable.write(pos);              // tell servo to go to position in variable 'pos'
        delay(75);
        current_magflux = analogRead(ci_Hall_Effect);
        if (abs(current_magflux - previous_magflux) > 15)
        {
          timer2 = true;
          break;
        }
      }  // waits 15ms for the servo to reach the position

    }
    clockwise1 = false;
  }
  else {
    for (pos = 120; pos >= 40; pos -= 1) { // goes from 0 degrees to 180 degrees
      // in steps of 1 degree
      { servo_turntable.write(pos);              // tell servo to go to position in variable 'pos'
        delay(75);
        current_magflux = analogRead(ci_Hall_Effect);
        if (abs(current_magflux - previous_magflux) > 15)
        {

          timer2 = true;
          break;
        }
      }  // waits 15ms for the servo to reach the position

    }
    clockwise1 = true;
  }
}
void goback(int a)
{
  while (current - previous > a * 1000)
  {
    servo_RightMotor.writeMicroseconds(1350);
    servo_LeftMotor.writeMicroseconds(1350);
  }
}




/************************************************************* MAIN LOOP *****************************************************************/
void loop() {

  if ((millis() - ul_3_Second_timer) > 3000)
  {
    bt_3_S_Time_Up = true;
  }

  // button-based mode selection
  if (CharliePlexM::ui_Btn)
  {
    if (bt_Do_Once == false)
    {
      bt_Do_Once = true;
      ui_Robot_State_Index++;
      ui_Robot_State_Index = ui_Robot_State_Index & 7;
      ul_3_Second_timer = millis();
      bt_3_S_Time_Up = false;
      bt_Cal_Initialized = false;
      ui_Cal_Cycle = 0;
    }
  }
  else
  {
    bt_Do_Once = LOW;
  }

  // check if drive motors should be powered
  //bt_Motors_Enabled = digitalRead(ci_Motor_Enable_Switch);

  // modes
  // 0 = default after power up/reset
  // 1 = Press mode button once to enter. Run robot Mode 1.
  // 2 = Press mode button twice to enter. Run robot mode 2.
  // 3 = Press mode button three times to enter. Calibrate line tracker dark level.
  // 4 = Press mode button four times to enter. Calibrate motor speeds to drive straight.
  switch (ui_Robot_State_Index)
  {
    case 0:    //Robot stopped
      {
        readLineTrackers();
        Ping_Front();
        servo_LeftMotor.writeMicroseconds(ci_Left_Motor_Stop);
        servo_RightMotor.writeMicroseconds(ci_Right_Motor_Stop);
        encoder_LeftMotor.zero();
        encoder_RightMotor.zero();
        ui_Mode_Indicator_Index = 0;

        previous_magflux = analogRead(ci_Hall_Effect);
        current_magflux = previous_magflux;

        break;
      }

    case 1:    //Robot Run after 3 seconds
      {
        if (bt_3_S_Time_Up)
        {
          readLineTrackers();

#ifdef DEBUG_ENCODERS
          l_Left_Motor_Position = encoder_LeftMotor.getRawPosition();
          l_Right_Motor_Position = encoder_RightMotor.getRawPosition();

          Serial.print("Encoders L: ");
          Serial.print(l_Left_Motor_Position);
          Serial.print(", R: ");
          Serial.println(l_Right_Motor_Position);
#endif

          // set motor speeds
          ui_Left_Motor_Speed = constrain(ui_Motors_Speed + ui_Left_Motor_Offset, 1600, 2100);
          ui_Right_Motor_Speed = constrain(ui_Motors_Speed + ui_Right_Motor_Offset, 1600, 2100);

          /*****************************************************************************************************************************************
            Main operation code HERE
            Implementation of mode 1 operations of MSE 2202 Project
            /**************************************************************************************************************************************/




          if (ui_Left_Line_Tracker_Data > 950)//when left line tracker has passed over black line
          {
            p = 100;
          }
          if (ui_Middle_Line_Tracker_Data > 950)//when right line tracker has passed over blacked line
          {
            q = 100;
          }
          if ((p == 100) && (q == 100))
          {
            holding_tesseract = true;//return to home position to rescan entire area after line tracker has scanned both middle lines
          }

          else {

            //timer variables
            current = millis();
            current_scan = millis();
            current_ping = millis();
            current_detect = millis();

            //readjust pivot position every loop
            servo_RightPivot.write(pivot_pos);
            servo_LeftPivot.write(180 - pivot_pos);

            if ((current_detect - previous_detect) > 50) {//read and reset hall effect sensor every 50 milliseconds
              current_magflux = analogRead(ci_Hall_Effect);
              previous_detect = current_detect;
            }

            // ping every time it has stopped scanning or it is holding a tesseract (this way the arm is not in the way of the front ultrasonic)

            if ((((current_ping - previous_ping) > 500) && (drive == true)) || holding_tesseract == true) {
              Ping_Front();
              Ping_Right();
              Ping_Left();
              previous_ping = current_ping;
            }

            if (holding_tesseract == true) {//when tesseract is picked up go to home positon and drop off tesseract
              GoHome();

            }
            else if (abs(current_magflux - previous_magflux) > 15) {//when good is picked up tesseract
              holding_tesseract = true;
              step1 = true;//initiates first step of GoHome function

              left_wheel_prev = encoder_LeftMotor.getRawPosition();//read encoder positoin of wheels
              right_wheel_prev = encoder_RightMotor.getRawPosition();
              for (int i = pivot_pos; i <= 90; i++) {//raise arm upward out of the way of the front ultrasonic
                pivot_pos = i;
                delay(25);
              }


            }

            else if (drive == true) {//drive forward
              
              if (((ul_Echo_Time_Front / 24) <= 45) && (end_turn == true)) {
                start_turn = true;
                left_wheel_prev = encoder_LeftMotor.getRawPosition();
                right_wheel_prev = encoder_RightMotor.getRawPosition();
                end_turn = false;
              }

              if ((current - previous) > 300) { //stop after 0.3 second to scan and then it loops bakc to drive forward again
                drive = false;
              }
              else {
                if (start_turn == true)//if we are starting a turn call turn function
                  turn();
                else//otherwise continue driving forward and scanning area
                  travel();
              }

            }

            else if (drive == false) {//when robot has stopped to scan for tesseract
              servo_LeftMotor.writeMicroseconds(bot_stop);//robot stops for scanning
              servo_RightMotor.writeMicroseconds(bot_stop);

              if ((current_scan - previous_scan) > 40) {//move scan arm slowly in increments of 0.04 seconds
                scan();
                current_scan = millis();//reset timer values for scan() function
                previous_scan = current_scan;
              }

            }

          }

#ifdef DEBUG_MOTORS
          Serial.print("Motors enabled: ");
          Serial.print(bt_Motors_Enabled);
          Serial.print(", Default: ");
          Serial.print(ui_Motors_Speed);
          Serial.print(", Left = ");
          Serial.print(ui_Left_Motor_Speed);
          Serial.print(", Right = ");
          Serial.println(ui_Right_Motor_Speed);
#endif
          ui_Mode_Indicator_Index = 1;

          break;
        }


      /*********************************Mode 2 coding***********************/
      case 2:
        {
          if (bt_3_S_Time_Up)
          {
            current1 = millis();
            Ping_Front();
            //   servo_RightPivot.write(pivot_pos);
            //  servo_LeftPivot.write(180 - pivot_pos);
            if (timer <= 4)   //5 tesseracts
            {
              if (timer9 == false)
              {
                for (int i = pivot_pos; i <= 40; i++)
                {
                  pivot_pos = i;
                  servo_RightPivot.write(pivot_pos);
                  servo_LeftPivot.write(180 - pivot_pos);
                }
                timer9 = true;
                //    previous1 = current1;
              }

              //          Step1: make sure arm is at the same height as platform

              if ((timer1 == false) && (timer9 == true))  // Step2: car moving forward, close to platform
              {
                if ((ul_Echo_Time_Front / 24) <= 32 && (ul_Echo_Time_Front / 24) > 0)
                {
                  servo_RightMotor.writeMicroseconds(1500);
                  servo_LeftMotor.writeMicroseconds(1500);
                  // previous1 = current1;
                  timer1 = true;
                }
                else
                {
                  servo_LeftMotor.writeMicroseconds(1750);
                  servo_RightMotor.writeMicroseconds(1750);
                }
              }

              if ((timer1 == true) && (timer2 == false))
              {
                Mode2Scan();
                previous1 = current1; //Step 3: Check if pick up tesseracts, if not keep scanning until hall-effect sensor changes
              }

              if ((timer2 == true) && (timer3 == false))
              {
                if ((current1 - previous1) >= 3850)
                {
                  servo_RightMotor.writeMicroseconds(1500);
                  servo_LeftMotor.writeMicroseconds(1500);
                  previous1 = current1;
                  timer3 = true;
                }
                else
                {
                  servo_RightMotor.writeMicroseconds(bot_stop - 100);//moves left wheel backward
                  servo_LeftMotor.writeMicroseconds(bot_stop + 100);
                }
                //Step4: turn 90 degree clockwise
              }
              if ((timer3 == true) && (timer4 == false))
              {
                timer4 = true;
              }


              if ((timer4 == true) && (timer5 == false)) //lower arm and go forward
              {
                if (lo == false)
                {
                  for (int p = 180; p >= 80; p = p - 1)
                  {
                    servo_arm.write(p);
                    delay(15);
                  }
                  for (int r = 150; r <= 180; r = r + 1)     //lower arm, only execute once
                  {
                    servo_LeftPivot.write(r);
                    servo_RightPivot.write(180 - r);
                    delay(15);
                  }
                  for (int r = 43; r <= 80; r++)
                  {
                    servo_turntable.write(r);
                    delay(15);
                  }
                  previous1 = current1;
                  lo = true;
                }


                else

                  if ((current1 - previous1) >= 5000)
                  {
                    servo_RightMotor.writeMicroseconds(1500);
                    servo_LeftMotor.writeMicroseconds(1500);
                    previous1 = current1;
                    timer5 = true;
                  }
                  else
                  {
                    servo_LeftMotor.writeMicroseconds(1750);
                    servo_RightMotor.writeMicroseconds(1750);
                  }
              }

              if ((timer5 == true) && (timer6 == false))
              {
                for (int r = 0; r <= 80; r = r + 1)     //pivot arm, only execute once
                {
                  servo_RightPivot.write(r);
                  servo_LeftPivot.write(180 - r);
                  delay(15);
                }

                for (int d = 170; d >= 20; d = d - 1)    //flip arm, make tesseract face upside
                {
                  servo_arm.writeMicroseconds(d);
                  delay(15);
                }
                for (int pos1 = 40; pos1 <= timer * 15 + 40; pos1 += 1 )                                 //turntable rotates every time robot releases tesseracts
                {
                  servo_turntable.write(pos);
                  delay(15);
                }
                // for(int pos2 = 115; pos2 <= 150; pos2 += 1)                           //spin the magnet arm, release magnet,BOO!!!
                servo_magnet.write(150);//drop
                delay(2500);

                timer6 = true;

              }

              if ((timer6 == true) && (timer8 == false)) //robot back to default mode
              {

                if (temp == false)
                {
                  servo_magnet.write(115);//drop
                  delay(1000);
                  for (int pos1 = timer * 15 + 40; pos1 >= 41; pos1 -= 1 )                             //turntable rotates every time robot releases tesseracts
                  {
                    servo_turntable.write(pos);
                    delay(15);
                  }
                  for (int d = 20; d <= 170; d = d + 1)  //flip arm, make tesseract face upside
                  {
                    servo_arm.write(d);
                    delay(15);
                  }
                  for (int r = 100; r >= 28; r = r - 1)    //pivot arm, only execute once
                  {
                    servo_RightPivot.write(r);
                    servo_LeftPivot.write(180 - r);
                    delay(15);
                  }
                  temp = true;
                  previous1 = current1;
                }
                else
                {
                  if ((current1 - previous1) > 3500)
                  {
                    servo_RightMotor.writeMicroseconds(1500);
                    servo_LeftMotor.writeMicroseconds(1500);
                    previous1 = current1;
                    timer8 = true;
                  }
                  else
                  {
                    servo_RightMotor.writeMicroseconds(1350);
                    servo_LeftMotor.writeMicroseconds(1350);
                  }
                }
              }
              if ((timer8 == true) && (timer7 == false))
              {
                if ((current1 - previous1) >= 1850)
                {
                  servo_RightMotor.writeMicroseconds(1500);
                  servo_LeftMotor.writeMicroseconds(1500);
                  previous1 = current1;
                  timer7 = true;
                }
                else
                {
                  servo_RightMotor.writeMicroseconds(bot_stop + 100);//moves left wheel backward
                  servo_LeftMotor.writeMicroseconds(bot_stop - 100);
                }
              }
              if (timer7 == true)
              {
                //reset timer variables
                timer++;
                timer1 = false;
                timer2 = false;
                timer3 = false;
                timer4 = false;
                timer5 = false;
                timer6 = false;
                timer7 = false;
                timer8 = false;
                current_magflux = analogRead(ci_Hall_Effect);
                previous_magflux = current_magflux;
                previous1 = current1;
              }
            }


            ui_Mode_Indicator_Index = 2;
            break;
          }
        case 3:    // Calibrate line tracker dark levels after 3 seconds
          {
            if (bt_3_S_Time_Up)
            {
              if (!bt_Cal_Initialized)
              {
                bt_Cal_Initialized = true;
                ui_Left_Line_Tracker_Dark = 0;
                ui_Middle_Line_Tracker_Dark = 0;
                ui_Right_Line_Tracker_Dark = 0;
                ul_Calibration_Time = millis();
                ui_Cal_Count = 0;
              }
              else if ((millis() - ul_Calibration_Time) > ci_Line_Tracker_Calibration_Interval)
              {
                ul_Calibration_Time = millis();
                readLineTrackers();
                ui_Left_Line_Tracker_Dark += ui_Left_Line_Tracker_Data;
                ui_Middle_Line_Tracker_Dark += ui_Middle_Line_Tracker_Data;
                ui_Right_Line_Tracker_Dark += ui_Right_Line_Tracker_Data;
                ui_Cal_Count++;
              }
              if (ui_Cal_Count == ci_Line_Tracker_Cal_Measures)
              {
                ui_Left_Line_Tracker_Dark /= ci_Line_Tracker_Cal_Measures;
                ui_Middle_Line_Tracker_Dark /= ci_Line_Tracker_Cal_Measures;
                ui_Right_Line_Tracker_Dark /= ci_Line_Tracker_Cal_Measures;
#ifdef DEBUG_LINE_TRACKER_CALIBRATION
                Serial.print("Dark Levels: Left = ");
                Serial.print(ui_Left_Line_Tracker_Dark, DEC);
                Serial.print(", Middle = ");
                Serial.print(ui_Middle_Line_Tracker_Dark, DEC);
                Serial.print(", Right = ");
                Serial.println(ui_Right_Line_Tracker_Dark, DEC);
#endif
                EEPROM.write(ci_Left_Line_Tracker_Dark_Address_L, lowByte(ui_Left_Line_Tracker_Dark));
                EEPROM.write(ci_Left_Line_Tracker_Dark_Address_H, highByte(ui_Left_Line_Tracker_Dark));
                EEPROM.write(ci_Middle_Line_Tracker_Dark_Address_L, lowByte(ui_Middle_Line_Tracker_Dark));
                EEPROM.write(ci_Middle_Line_Tracker_Dark_Address_H, highByte(ui_Middle_Line_Tracker_Dark));
                EEPROM.write(ci_Right_Line_Tracker_Dark_Address_L, lowByte(ui_Right_Line_Tracker_Dark));
                EEPROM.write(ci_Right_Line_Tracker_Dark_Address_H, highByte(ui_Right_Line_Tracker_Dark));
                ui_Robot_State_Index = 0;    // go back to Mode 0
              }
              ui_Mode_Indicator_Index = 3;
            }
            break;
          }

        case 4:    //Calibrate motor straightness after 3 seconds.
          {
            if (bt_3_S_Time_Up)
            {
              if (!bt_Cal_Initialized)
              {
                bt_Cal_Initialized = true;
                encoder_LeftMotor.zero();
                encoder_RightMotor.zero();
                ul_Calibration_Time = millis();
                servo_LeftMotor.writeMicroseconds(ui_Motors_Speed);
                servo_RightMotor.writeMicroseconds(ui_Motors_Speed);
              }
              else if ((millis() - ul_Calibration_Time) > ci_Motor_Calibration_Time)
              {
                servo_LeftMotor.writeMicroseconds(ci_Left_Motor_Stop);
                servo_RightMotor.writeMicroseconds(ci_Right_Motor_Stop);
                l_Left_Motor_Position = encoder_LeftMotor.getRawPosition();
                l_Right_Motor_Position = encoder_RightMotor.getRawPosition();
                if (l_Left_Motor_Position > l_Right_Motor_Position)
                {
                  // May have to update this if different calibration time is used
                  ui_Right_Motor_Offset = 0;
                  ui_Left_Motor_Offset = (l_Left_Motor_Position - l_Right_Motor_Position) / 4;
                }
                else
                {
                  // May have to update this if different calibration time is used
                  ui_Right_Motor_Offset = (l_Right_Motor_Position - l_Left_Motor_Position) / 4;
                  ui_Left_Motor_Offset = 0;
                }

#ifdef DEBUG_MOTOR_CALIBRATION
                Serial.print("Motor Offsets: Left = ");
                Serial.print(ui_Left_Motor_Offset);
                Serial.print(", Right = ");
                Serial.println(ui_Right_Motor_Offset);
#endif
                EEPROM.write(ci_Right_Motor_Offset_Address_L, lowByte(ui_Right_Motor_Offset));
                EEPROM.write(ci_Right_Motor_Offset_Address_H, highByte(ui_Right_Motor_Offset));
                EEPROM.write(ci_Left_Motor_Offset_Address_L, lowByte(ui_Left_Motor_Offset));
                EEPROM.write(ci_Left_Motor_Offset_Address_H, highByte(ui_Left_Motor_Offset));

                ui_Robot_State_Index = 0;    // go back to Mode 0
              }
#ifdef DEBUG_MOTOR_CALIBRATION
              Serial.print("Encoders L: ");
              Serial.print(encoder_LeftMotor.getRawPosition());
              Serial.print(", R: ");
              Serial.println(encoder_RightMotor.getRawPosition());
#endif
              ui_Mode_Indicator_Index = 4;
            }
            break;
          }
        }

        if ((millis() - ul_Display_Time) > ci_Display_Time)
        {
          ul_Display_Time = millis();

#ifdef DEBUG_MODE_DISPLAY
          Serial.print("Mode: ");
          Serial.println(ui_Mode_Indicator[ui_Mode_Indicator_Index], DEC);
#endif

        }
      }
  }
}
