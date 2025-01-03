/*
  ProjectorStepper
  
  Runs a 16mm film projector in single cell steps so we can photograph each cell and
  ultimately convert the physical film to digital video.

  There are a couple of relays that run the motor, clutch and brake that we need to control.
  We also need to monitor an optical sensor to determine when the cell is in place.

  Communication is via the serial port to an attached laptop that also runs the software that
  then captures the photos.

  Pius Ott - piusott@gmail.com - 20240510
*/

#include "Constants.h"

// Override << operator to allow us to easily concatenate and format strings to write to the serial port
template <typename T>
Print& operator<<(Print& printer, T value)
{
    printer.print(value);
    return printer;
}

// -------------------------------------------------------------------------------------------------

// Send a command or notification to the laptop via the USB serial port
void sendCommand(String cmd)
{
  Serial << CMD_SEND << cmd << "\n";
}

// -------------------------------------------------------------------------------------------------

// Send an error response to the laptop.
void sendError(String errorCode)
{
  Serial << RSP_ERR << errorCode << "\n";
}

// -------------------------------------------------------------------------------------------------

// Send a debug string to the laptop. This is for testing only, so we only send it while we're in
// debug mode
void debugLog(String msg)
{
  if (debugMode)
    Serial << "Log: " << msg << "\n";
}

// -------------------------------------------------------------------------------------------------

// Check if we have a command and if so, return it
String getCommand()
{
  String cmd = "";
  int cmdLength = CMD_RECEIVE.length();
  int charsAvailable = Serial.available();
  if (charsAvailable < cmdLength)
    return cmd;   // No data available, or not enough to form a full command yet. Return

  int charsToRead = cmdLength;
  // Check if we have a newline character. If so it should indicate that we have a full command received
  for (; charsToRead < charsAvailable; charsToRead++)
  {
    if (Serial.peek() == '\n')
    {
      // New line received. Break out of the loop
      break;
    }
  }

  // There appears to be command data. Read it and make sure it's a command
  if (charsToRead > cmdLength)
  {
    String inData = Serial.readStringUntil('\n');
    if (inData.startsWith(CMD_RECEIVE))
    {
      // Looks like a command. Strip the prefix
      cmd = inData.substring(cmdLength);
    }
  }

  return cmd;
}

// -------------------------------------------------------------------------------------------------

/*
  Move the film forward to the next cell. This means we need to make sure the motor is on and
  currently running forward. Also that the Film Interrupt switch is not set. If that's the case we
  then trigger the Clutch relay until the optical sensor is triggered again, which means the shutter
  has been passed. 
*/
void nextCell()
{
  debugLog("Moving to next cell.");

  int motorOn, motorDirection, clutchState, sensorState, interruptSwitch;

  motorOn = digitalRead(PIN_MOTORON);

  // We only need to read one. We can assume that both are always the same value
  motorDirection = digitalRead(PIN_MOTORDIRECTION1);

  clutchState = digitalRead(PIN_CLUTCH);
  sensorState = digitalRead(PIN_OPTICALSENSOR);
  interruptSwitch = digitalRead(PIN_FILMINTERRUPT);

  if (interruptSwitch == SWITCHOFF)
  {
    // Most likely the film is not loaded, or not loaded correctly.
    sendError("Film interrupt switch is set.");
    return;
  }

  if (clutchState == SWITCHON)
  {
    sendError("Film is already running.");
    return;
  }

  // Make sure the motor is going to move forward
  digitalWrite(PIN_MOTORDIRECTION1, FORWARD);
  digitalWrite(PIN_MOTORDIRECTION2, FORWARD);

  if (motorOn == 0) // motor is not on, switch it on
  {
    debugLog("Turning motor on to move to next cell");
    digitalWrite(PIN_MOTORON, SWITCHON);
  }

  // looks like we're good to go
  debugLog("Starting clutch.");
  digitalWrite(PIN_CLUTCH, SWITCHON);

  // Initially the shutter should be blocking the sensor, so the first few reads will be still low. We then
  // need to cycle through the sensor being high and once it gets low again, stop the film moving. So in
  // normal circumstances we read low-high-low and stop the film again. However we also need to account for 
  // instances where the film is out of position. So it's possible that the initial state is already high.
  // In that case we cycle through a high-low-high-low. This means we'll miss a cell, but we can probably
  // live with that.
  int lastSensorState = sensorState;
  int sensorLowCount = 0;
  int timeOut = 0;

  // The motor moves pretty slow, so we give it around 2 second before we give up on the sensor
  while(timeOut++ < 800)    // 800P * 5mS - don't know why this ends up as 2 seconds, but that's what I measured
  {
    sensorState = digitalRead(PIN_OPTICALSENSOR);
    delay(5);

    // Film is not moving smoothly. Stop process.
    interruptSwitch = digitalRead(PIN_FILMINTERRUPT);
    if (interruptSwitch == SWITCHOFF)
    {
      sendError("Film interrupt switch is set.");
      return;
    }

    if (sensorState != lastSensorState)
    {
      lastSensorState = sensorState;

      if (sensorState == 0)
      {
        sensorLowCount++;
        //debugLog("Sensor state: " + String(sensorState) + " - count: " + sensorLowCount);
        debugLog("sensorLowCount: " + String(sensorLowCount));
      }
      // TODO: Need to test exactly what value this should be. Most likely 2
      if (sensorLowCount >= 2)
      {
        // turn off clutch
        digitalWrite(PIN_CLUTCH, SWITCHOFF);
        digitalWrite(PIN_MOTORON, SWITCHOFF); // although probably not strictly needed, I like the quiet...
        sendCommand(CMD_ATCELL);
        debugLog("We're at the next cell. Stopping clutch.");
        return;
      }
    }
  }

  // timeout, turn off clutch. We're assuming something's gone wrong so we don't want to keep moving the
  // film from here. Also turn off the motor, just for some peace and quiet
  digitalWrite(PIN_CLUTCH, SWITCHOFF);
  digitalWrite(PIN_MOTORON, SWITCHOFF);
  sendError("Timeout while moving to next cell. Stopping clutch.");
}

// -------------------------------------------------------------------------------------------------

/*
  Rewind the film. This simply means we reverse the motor direction.
  TODO : establish if the clutch needs to be switched on for this too.
*/
void rewind()
{
  debugLog("Rewinding the film");

  int motorOn, clutchState;

  motorOn = digitalRead(PIN_MOTORON);

  clutchState = digitalRead(PIN_CLUTCH);
  if (clutchState == SWITCHON)
  {
    digitalWrite(PIN_CLUTCH, SWITCHOFF);  // I don't know if this is needed, or if it even makes a difference
    debugLog("Turning clutch off.");
  }

  // set motor to run in reverse
  digitalWrite(PIN_MOTORDIRECTION1, BACKWARD);
  digitalWrite(PIN_MOTORDIRECTION2, BACKWARD);

  if (motorOn == SWITCHOFF) // motor is not on, switch it on
  {
    debugLog("Turning motor on to rewind the film.");
    digitalWrite(PIN_MOTORON, SWITCHON);
  }
}

// -------------------------------------------------------------------------------------------------

/*
  Turn the motor on. This is always going to set the motor running forward. To run it in reverse,
  use the Rewind command.
  If the motor is already running, we're just making sure it runs forward.
  Note that we always turn the clutch off. We just want to turn on the motor. The clutch is
  controlled through the nextCell command.
  To be honest, I don't know if this function is needed at all. I just put it in because this is
  one of the switch settings on the projector allow. But I can't see a real use case for our
  requirements here.
*/
void motorOn()
{
  debugLog("Switching the motor on.");

  int clutchState = digitalRead(PIN_CLUTCH);
  if (clutchState == SWITCHON)
  {
    digitalWrite(PIN_CLUTCH, SWITCHOFF);  // I don't know if this is needed, or if it even makes a difference
    debugLog("Turning clutch off.");
  }

  // set motor to run forward
  digitalWrite(PIN_MOTORDIRECTION1, FORWARD);
  digitalWrite(PIN_MOTORDIRECTION2, FORWARD);

  int motorOn = digitalRead(PIN_MOTORON);
  if (motorOn == SWITCHOFF) // motor is not on, switch it on
  {
    debugLog("Turning motor on.");
    digitalWrite(PIN_MOTORON, SWITCHON);
  }
  else
    debugLog("Nothing to do, motor is already on.");
}

// -------------------------------------------------------------------------------------------------

/*
  Turn the motor off.
  I don't know if this function is needed either. Although maybe there is a use case to have it so
  we can turn off the motor again after rewinding the film.
*/
void motorOff()
{
  debugLog("Switching the motor off.");

  int motorOn = digitalRead(PIN_MOTORON);
  if (motorOn == SWITCHON)  // motor is on, switch it off
  {
    debugLog("Turning motor off.");
    digitalWrite(PIN_MOTORON, SWITCHOFF);
  }
  else
    debugLog("Nothing to do, motor is already off.");
}

// -------------------------------------------------------------------------------------------------

/*
  Check and report the state of the optic sensor. This function is only used for testing.
*/
void checkOpticSensor()
{
  debugLog("Testing optic sensor state.");

  // TODO: this should really run until we send a command to stop it, or send any other command. But
  //       for the moment I'm just doing this the easy way to let me quickly test if the circuit
  //       board is actually working. I'm just running the check for a couple of seconds

  int i = 0;
  int sensorState;
  while (i++ < 20)
  {
    sensorState = digitalRead(PIN_OPTICALSENSOR);
    if (sensorState == 0)
      debugLog("Sensor is low");
    else
      debugLog("Sensor is high");

    delayAndCmdCheck(200);
  }
}

// -------------------------------------------------------------------------------------------------

void sendAck()
{
  sendCommand(CMD_OK);
}

// -------------------------------------------------------------------------------------------------

/*
  Function that runs a delay but also keeps checking the serial input to see if we've received a
  command. If we have, and we're not currently executing in an uninterruptable command, then we
  handle that new command.
*/
void delayAndCmdCheck(int duration)
{
  const int minDelay = 10;    // minimum delay period is 10mS
  int remainingDelay = duration;

  while(remainingDelay > minDelay)
  {
    delay(minDelay);
    if (inPriorityCommand == 0)
    {
      String cmd = getCommand();
      if (cmd.length() > 0)
      {
        handleCommand(cmd);
        return;
      }
    }
    remainingDelay -= minDelay;
  }

  // there is still a little bit of delay left to do. Handle this here.
  if (remainingDelay > 0)
    delay(remainingDelay);
}

// -------------------------------------------------------------------------------------------------

void handleCommand(String cmd)
{
  if (cmd == CMD_OK)
    return;   // Nothing to do

  if (cmd == CMD_NEXTCELL)
  {
    nextCell();
  }
  else if (cmd == CMD_REWIND)
  {
    rewind();
  }
  else if (cmd == CMD_MOTORON)
  {
    motorOn();
  }
  else if (cmd == CMD_MOTOROFF)
  {
    motorOff();
  }
  else if (cmd == CMD_TESTOPTO)
  {
    checkOpticSensor();
  }
  else if (cmd == CMD_PING)
  {
    // Nothing to do, we just send an Ack back
  }
  else
    return;   // Not a valid command. Do not send acknowledgement

  sendAck();
  return;
}

// -------------------------------------------------------------------------------------------------

void setup() 
{
  // initialize serial communication at 9600 bits per second
  Serial.begin(9600);

  // For the moment, allow debug messages to be sent to the laptop
  debugMode = 1;

  // Set up pins to their respective functions
  pinMode(PIN_CLUTCH, OUTPUT);       // Clutch/brake relay
  pinMode(PIN_MOTORON, OUTPUT);         // Motor relay
  pinMode(PIN_MOTORDIRECTION1, OUTPUT);  // Motor direction relay 1
  pinMode(PIN_MOTORDIRECTION2, OUTPUT);  // Motor direction relay 2. We use two relays to reverse polarity
  pinMode(PIN_FILMINTERRUPT, INPUT);    // Film tension breaker switch
  pinMode(PIN_OPTICALSENSOR, INPUT);   // Optical sensor to determine film position

  pinMode(LED_BUILTIN, OUTPUT);     // Use the built in LED as a quick indicator for various tests

  // Signal to the laptop that we're ready now
  sendCommand(CMD_READY);
}

// -------------------------------------------------------------------------------------------------

// Main code that runs repeatedly
void loop() 
{
  String cmd = getCommand();
  if (cmd.length() > 0)
  {
    // This should probably be removed too
    digitalWrite(LED_BUILTIN, 1);
    delay(100);
    digitalWrite(LED_BUILTIN, 0);

    // TEMP ONLY - Remove this!
    //Serial << "Received: " << cmd << "\n";
    // TEMP ONLY - Remove this!

    handleCommand(cmd);
  }
}

// -------------------------------------------------------------------------------------------------
