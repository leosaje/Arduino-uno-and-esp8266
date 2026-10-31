
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Servo.h>

// ==================================================
// PIN CONFIGURATION
// ==================================================

// L298N Motor Driver
#define ENA 6
#define IN1 7
#define IN2 8
#define IN3 9
#define IN4 10
#define ENB 11

// Other Components
#define VACUUM_PIN 5
#define SERVO_PIN 3
#define TRIG_PIN 4
#define ECHO_PIN A2

// ==================================================
// ROBOT SETTINGS
// ==================================================

const int AUTO_SPEED = 180;
const int MANUAL_SPEED = 210;

const int OBSTACLE_DISTANCE = 25;

const int SERVO_CENTER = 90;
const int SERVO_LEFT = 150;
const int SERVO_RIGHT = 30; 

const bool SERVO_REVERSE = true;

const unsigned long MANUAL_TIMEOUT = 1800;

const unsigned long SESSION_LIMIT =
  2UL * 60UL * 60UL * 1000UL;

const unsigned long WARNING_TIME =
  20UL * 60UL * 1000UL;

// ==================================================
// OBJECTS
// ==================================================

LiquidCrystal_I2C lcd(0x27, 16, 2);
Servo scanner;

// ==================================================
// ROBOT VARIABLES
// ==================================================

bool autoMode = false;
bool vacuumOn = false;
bool manualMoving = false;

bool sessionActive = false;
bool warningSent = false;
bool timeLimitReached = false;

unsigned long sessionStart = 0;
unsigned long lastManualCommand = 0;
unsigned long lastStatusUpdate = 0;
unsigned long lastLCDUpdate = 0;
unsigned long lastAutoStep = 0;

int leftDistance = 400;
int frontDistance = 400;
int rightDistance = 400;

String currentStatus = "READY";

// ==================================================
// MOTOR CONTROL
// ==================================================

void setMotor(int leftSpeed, int rightSpeed) {

  leftSpeed = constrain(leftSpeed, -255, 255);
  rightSpeed = constrain(rightSpeed, -255, 255);

  // LEFT MOTOR
  if (leftSpeed > 0) {
    digitalWrite(IN1, HIGH);
    digitalWrite(IN2, LOW);
  }
  else if (leftSpeed < 0) {
    digitalWrite(IN1, LOW);
    digitalWrite(IN2, HIGH);
  }
  else {
    digitalWrite(IN1, LOW);
    digitalWrite(IN2, LOW);
  }

  // RIGHT MOTOR
  if (rightSpeed > 0) {
    digitalWrite(IN3, HIGH);
    digitalWrite(IN4, LOW);
  }
  else if (rightSpeed < 0) {
    digitalWrite(IN3, LOW);
    digitalWrite(IN4, HIGH);
  }
  else {
    digitalWrite(IN3, LOW);
    digitalWrite(IN4, LOW);
  }

  analogWrite(ENA, abs(leftSpeed));
  analogWrite(ENB, abs(rightSpeed));
}

void stopDrive() {
  setMotor(0, 0);
}

void moveForward(int speedValue) {
  setMotor(speedValue, speedValue);
}

void moveBackward(int speedValue) {
  setMotor(-speedValue, -speedValue);
}

void turnLeft(int speedValue) {
  setMotor(-speedValue, speedValue);
}

void turnRight(int speedValue) {
  setMotor(speedValue, -speedValue);
}

// ==================================================
// VACUUM CONTROL
// ==================================================

void setVacuum(bool enabled) {

  vacuumOn = enabled;

  digitalWrite(VACUUM_PIN, enabled ? HIGH : LOW);

  Serial.println(enabled ? "VACUUM:ON" : "VACUUM:OFF");
}

// ==================================================
// CLEANING SESSION TIMER
// ==================================================

void startSessionIfNeeded() {

  if (!sessionActive && (autoMode || vacuumOn)) {

    sessionActive = true;
    sessionStart = millis();

    warningSent = false;
    timeLimitReached = false;
  }
}

void updateSessionTimer() {

  if (!autoMode && !vacuumOn) {

    sessionActive = false;
    warningSent = false;
    timeLimitReached = false;

    return;
  }

  startSessionIfNeeded();

  if (!sessionActive || timeLimitReached) {
    return;
  }

  unsigned long elapsed = millis() - sessionStart;

  if (elapsed >= SESSION_LIMIT) {

    autoMode = false;
    manualMoving = false;

    stopDrive();
    setVacuum(false);

    timeLimitReached = true;
    currentStatus = "TIME LIMIT";

    Serial.println("STATUS:TIME_LIMIT");
    Serial.println("ALERT:RUN_TIME_LIMIT_REACHED");

    return;
  }

  unsigned long remaining = SESSION_LIMIT - elapsed;

  if (remaining <= WARNING_TIME && !warningSent) {

    warningSent = true;

    Serial.println("ALERT:20_MINUTES_REMAINING");
  }
}

// ==================================================
// ULTRASONIC SENSOR
// ==================================================

int getDistance() {

  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);

  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);

  digitalWrite(TRIG_PIN, LOW);

  unsigned long duration =
    pulseIn(ECHO_PIN, HIGH, 25000);

  if (duration == 0) {
    return 400;
  }

  int distance = duration * 0.0343 / 2;

  if (distance <= 0 || distance > 400) {
    return 400;
  }

  return distance;
}

// ==================================================
// SERVO SCANNING
// ==================================================

int moveServoAndMeasure(int angle) {

  int physicalAngle = SERVO_REVERSE
    ? 180 - angle
    : angle;

  scanner.write(physicalAngle);

  delay(300);

  return getDistance();
}

void scanObstacleSides() {

  // Look LEFT
  leftDistance =
    moveServoAndMeasure(SERVO_LEFT);

  // Look RIGHT
  rightDistance =
    moveServoAndMeasure(SERVO_RIGHT);

  // Return to CENTER
  scanner.write(SERVO_CENTER);

  delay(200);

  Serial.print("SENSOR:");
  Serial.print(leftDistance);
  Serial.print(",");
  Serial.print(frontDistance);
  Serial.print(",");
  Serial.println(rightDistance);
}

// ==================================================
// AUTOMATIC CLEANING
// ==================================================

void automaticStep() {

  if (!autoMode || timeLimitReached) {
    return;
  }

  if (millis() - lastAutoStep < 100) {
    return;
  }

  lastAutoStep = millis();

  // Keep servo facing forward
  scanner.write(SERVO_CENTER);

  // Check front obstacle
  frontDistance = getDistance();

  // Send front sensor reading
  Serial.print("SENSOR:");
  Serial.print(leftDistance);
  Serial.print(",");
  Serial.print(frontDistance);
  Serial.print(",");
  Serial.println(rightDistance);

  // NORMAL MOVEMENT
  if (frontDistance > OBSTACLE_DISTANCE) {

    moveForward(AUTO_SPEED);

    return;
  }

  // OBSTACLE DETECTED
  stopDrive();

  delay(200);

  // Scan left and right only now
  scanObstacleSides();

  // CHOOSE CLEARER DIRECTION
  if (leftDistance > rightDistance &&
      leftDistance > OBSTACLE_DISTANCE) {

    turnLeft(AUTO_SPEED);

    delay(450);
  }
  else if (rightDistance > OBSTACLE_DISTANCE) {

    turnRight(AUTO_SPEED);

    delay(450);
  }
  else {

    // BOTH SIDES BLOCKED
    moveBackward(AUTO_SPEED);

    delay(500);

    turnRight(AUTO_SPEED);

    delay(600);
  }

  stopDrive();

  scanner.write(SERVO_CENTER);
}

// ==================================================
// COMMAND HANDLER
// ==================================================

void handleCommand(String command) {

  command.trim();
  command.toUpperCase();

  if (command.startsWith("CMD:")) {
    command = command.substring(4);
    command.trim();
  }

  // HEARTBEAT
  if (command == "HB" ||
      command == "HEARTBEAT") {

    if (manualMoving) {
      lastManualCommand = millis();
    }

    return;
  }

  // STATUS REQUEST
  if (command == "STATUS") {
    sendStatus();
    return;
  }

  // STOP
  if (command == "STOP") {

    autoMode = false;
    manualMoving = false;

    stopDrive();

    currentStatus = "STOPPED";

    Serial.println("STATUS:STOPPED");

    return;
  }

  if (timeLimitReached) {
    return;
  }

  // FORWARD
  if (command == "FORWARD") {

    autoMode = false;
    manualMoving = true;

    lastManualCommand = millis();

    moveForward(MANUAL_SPEED);

    currentStatus = "MANUAL";
  }

  // BACKWARD
  else if (command == "BACKWARD") {

    autoMode = false;
    manualMoving = true;

    lastManualCommand = millis();

    moveBackward(MANUAL_SPEED);

    currentStatus = "MANUAL";
  }

  // LEFT
  else if (command == "LEFT") {

    autoMode = false;
    manualMoving = true;

    lastManualCommand = millis();

    turnLeft(MANUAL_SPEED);

    currentStatus = "MANUAL";
  }

  // RIGHT
  else if (command == "RIGHT") {

    autoMode = false;
    manualMoving = true;

    lastManualCommand = millis();

    turnRight(MANUAL_SPEED);

    currentStatus = "MANUAL";
  }

  // VACUUM ON
  else if (command == "VACUUM_ON") {

    setVacuum(true);

    startSessionIfNeeded();
  }

  // VACUUM OFF
  else if (command == "VACUUM_OFF") {

    setVacuum(false);
  }

  // AUTO ON
  else if (command == "AUTO_ON" ||
           command == "AUTO") {

    manualMoving = false;

    autoMode = true;

    currentStatus = "AUTO";

    lastAutoStep = 0;

    startSessionIfNeeded();

    Serial.println("STATUS:AUTO");
  }

  // AUTO OFF
  else if (command == "AUTO_OFF") {

    autoMode = false;

    stopDrive();

    currentStatus = "READY";

    Serial.println("STATUS:READY");
  }
}

// ==================================================
// ESP8266 SERIAL READER
// ==================================================

void readESPCommands() {

  static String input = "";

  while (Serial.available()) {

    char incoming = Serial.read();

    if (incoming == '\n' || incoming == '\r') {

      if (input.length() > 0) {

        handleCommand(input);

        input = "";
      }
    }

    else if (input.length() < 40) {

      input += incoming;
    }

    else {

      input = "";
    }
  }
}

// ==================================================
// STATUS REPORT
// ==================================================

void sendStatus() {

  unsigned long remaining =
    SESSION_LIMIT / 1000;

  if (sessionActive && !timeLimitReached) {

    unsigned long elapsed =
      (millis() - sessionStart) / 1000;

    if (elapsed < SESSION_LIMIT / 1000) {

      remaining =
        SESSION_LIMIT / 1000 - elapsed;
    }
    else {

      remaining = 0;
    }
  }

  Serial.print("STATUS:");
  Serial.println(currentStatus);

  Serial.print("REMAINING:");
  Serial.println(remaining);

  Serial.println(
    vacuumOn ? "VACUUM:ON" : "VACUUM:OFF"
  );

  Serial.print("SENSOR:");
  Serial.print(leftDistance);
  Serial.print(",");
  Serial.print(frontDistance);
  Serial.print(",");
  Serial.println(rightDistance);
}

// ==================================================
// LCD DISPLAY
// ==================================================

void updateLCD() {

  if (millis() - lastLCDUpdate < 500) {
    return;
  }

  lastLCDUpdate = millis();

  lcd.setCursor(0, 0);

  if (autoMode) {
    lcd.print("AUTO CLEANING   ");
  }
  else if (manualMoving) {
    lcd.print("MANUAL CONTROL  ");
  }
  else if (timeLimitReached) {
    lcd.print("TIME LIMIT      ");
  }
  else {
    lcd.print("CLEANING ROBOT  ");
  }

  lcd.setCursor(0, 1);

  if (vacuumOn) {
    lcd.print("Vacuum: ON      ");
  }
  else {
    lcd.print("Vacuum: OFF     ");
  }
}

// ==================================================
// SETUP
// ==================================================

void setup() {

  pinMode(ENA, OUTPUT);
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  pinMode(ENB, OUTPUT);

  pinMode(VACUUM_PIN, OUTPUT);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  digitalWrite(VACUUM_PIN, LOW);

  Serial.begin(9600);

  scanner.attach(SERVO_PIN);
  scanner.write(SERVO_CENTER);

  lcd.init();
  lcd.backlight();

  lcd.setCursor(0, 0);
  lcd.print("Cleaning Robot");

  lcd.setCursor(0, 1);
  lcd.print("System Ready");

  stopDrive();

  delay(1000);

  currentStatus = "READY";

  Serial.println("STATUS:READY");

  sendStatus();
}

// ==================================================
// MAIN LOOP
// ==================================================

void loop() {

  // Read commands from ESP8266
  readESPCommands();

  // Manual movement timeout
  if (manualMoving &&
      millis() - lastManualCommand > MANUAL_TIMEOUT) {

    manualMoving = false;

    stopDrive();

    currentStatus = "READY";

    Serial.println("STATUS:READY");
  }

  // Update runtime timer
  updateSessionTimer();

  // Automatic obstacle avoidance
  if (autoMode && !timeLimitReached) {

    automaticStep();
  }

  // LCD
  updateLCD();

  // Send status every 2 seconds
  if (millis() - lastStatusUpdate >= 2000) {

    lastStatusUpdate = millis();

    sendStatus();
  }
}
