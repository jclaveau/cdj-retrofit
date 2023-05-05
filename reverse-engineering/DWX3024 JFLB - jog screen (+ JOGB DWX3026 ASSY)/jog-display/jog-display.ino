// https://playground.arduino.cc/Code/TaskScheduler/
// https://github.com/arkhipenko/TaskScheduler/wiki/API-Task#void-setinterval-unsigned-long-ainterval
#define _TASK_MICRO_RES
#include <TaskScheduler.h>
#include <TaskSchedulerDeclarations.h>
#include <TaskSchedulerSleepMethods.h>


// https://forum.arduino.cc/t/text-and-variable-both-in-display-println/586907/7

// #include "language-tools.h"
#include "SPI.h"

typedef void (*NoParamFunction) ();
String blankString;

const uint8_t SHADE_OF_GREY_COUNT = 8; // Bug over 27 on Arduino Leonardo
const uint8_t DISPLAY_FRAME_LENGTH = 16;
typedef uint8_t DisplayFrame[SHADE_OF_GREY_COUNT][DISPLAY_FRAME_LENGTH];

const bool BLINK_ON = HIGH;
const bool BLINK_OFF = LOW;


// void printLine()
// {
//   Serial.println();
// }

// TODO remove this and fix build
template <typename T, typename... Types>
void printLine(T first, Types... other)
{
  Serial.print(first);
  printLine(other...) ;
}

String serialInput = "";

int middleFrame[16] = { // > 16 for experiments
  104, // 01101000
  96, // 01100000 bottom bleus
  0,
  40, // 00101000 bottom reds
  0, 0, 0, 0, 0, 0,
  0, 0, 0,
  8, // 00001000 up red
  0,
  4, // 00000100 up blue
};


Scheduler ts;
Task commandHandlerTask(
  1000,
  TASK_FOREVER,
  &handleSerialCommand,
  &ts,
  false,
  NULL,
  &stopHandlingSerialCommand
);

Task animationTask(
  200000,
  TASK_FOREVER,
  &startAnimation,
  &ts,
  false,
  NULL,
  &stopAnimation
);

Task renderDisplayTask(
  // 1000,
  TASK_ONCE, // delay defined between, each non blocking step
  TASK_FOREVER,
  // 1000,
  &renderJogDisplayLeft,
  &ts,
  false,
  NULL,
  &stopRenderingJogDisplaySPI
);


void setup() {
  pinMode(6, OUTPUT);
  pinMode(13, OUTPUT);

  setupSerialCommand();
  setupJogDisplaySPI();

  prepareFrames();
  generateFramesFromConfig();
  ts.enableAll();
}

void loop() {
  ts.execute();
}


void setupJogDisplaySPI() {
  SPI.begin();
  SPI.beginTransaction(SPISettings(20000000, MSBFIRST, SPI_MODE0));
}


struct Config {
  int renderPeriod;
  int blinkOnDuration;
  int blinkOffDuration;
  int shadesOfGrey;
  int renderRatios[3];
  int renderRatiosSum;
  int latencyDuration;
  int latencySpacing;
  int vinyl[2];
  int circleIn[2];
  int circleOut[2];
  int bluePixels[143];
  int redPixels[88];
  double animationInterval;
  uint8_t maxFrameUpdateStep;
} config = {
  200, // full render period
  // 50, 50,
  100, 1,
  SHADE_OF_GREY_COUNT, // TODO fix it
  // {4, 4, 2},
  {5, 5, 0},
  10,
  1, // latency duration
  0, // 1 by side? depending on shades of grey (3 => 15 shades)
  {SHADE_OF_GREY_COUNT, 1},
  {1, SHADE_OF_GREY_COUNT},
  {SHADE_OF_GREY_COUNT, SHADE_OF_GREY_COUNT},
  0,
  0,
  0 // animation interval (0 = disabled)
};


void dumpConfigToSerial() {

  Serial.println(blankString + "renderPeriod: " + config.renderPeriod + "ms");
  Serial.println(blankString + "blinkOnDuration: " + config.blinkOnDuration + "ms");
  Serial.println(blankString + "blinkOffDuration: " + config.blinkOffDuration + "ms");
  Serial.println(blankString + "shadesOfGrey: " + config.shadesOfGrey);
  Serial.println(blankString + "latency: " + config.latencySpacing + ", " + config.latencyDuration + "us");
  Serial.println(blankString + "vinyl: " + config.vinyl[0] + " " + config.vinyl[1]);
  Serial.println(blankString + "circleIn: " + config.circleIn[0] + " " + config.circleIn[1]);
  Serial.println(blankString + "circleOut: " + config.circleOut[0] + " " + config.circleOut[1]);
  Serial.println(blankString + "animation interval: " + config.animationInterval + "us");

  // Serial.print("bluePixels: "); Serial.println(config.bluePixels);
  // Serial.print("redPixels: "); Serial.println(config.redPixels);
}

DisplayFrame leftFrame  = {};
DisplayFrame rightFrame = {};
// TODO mid frame

void prepareFrames() {
  for (int i=0; i < SHADE_OF_GREY_COUNT; i++) {
    leftFrame[i][0]  = B00100000;
    rightFrame[i][0] = B01000000;
  }
}

// RENDERING
int currentShadeOfGrey = 0;
NoParamFunction afterBlinkStep;



int renderLeftStartTime = 0;
int renderLeftEndTime = 0;
int blinkOnStartTime = 0;
int blinkOnEndTime = 0;
int blinkOffStartTime = 0;
int blinkOffEndTime = 0;
int currentRenderStartTime = 0;
int animationStartTime = 0;
int animationEndTime = 0;

int renderLeftStartTimeTmp = 0;
int renderLeftEndTimeTmp = 0;
int blinkOnStartTimeTmp = 0;
int blinkOnEndTimeTmp = 0;
int blinkOffStartTimeTmp = 0;
int blinkOffEndTimeTmp = 0;
int animationStartTimeTmp = 0;
int animationEndTimeTmp = 0;
uint8_t renderCount=0;

const uint8_t FRAME_UPDATE_STEPS = 58;
int frameUpdateDurations[FRAME_UPDATE_STEPS];

void renderJogDisplayLeft() {
  currentRenderStartTime = micros();
  renderDisplayTask.delay(config.blinkOffDuration);

  digitalWrite(13, BLINK_ON);
  renderLeftStartTime = renderLeftStartTimeTmp;
  renderLeftEndTime = renderLeftEndTimeTmp;
  blinkOnStartTime = blinkOnStartTimeTmp;
  blinkOnEndTime = blinkOnEndTimeTmp;
  blinkOffStartTime = blinkOffStartTimeTmp;
  blinkOffEndTime = blinkOffEndTimeTmp;

  generateFramesFromConfig();


  renderLeftStartTimeTmp = micros();

  renderDisplayTask.setCallback(blink);
  // afterBlinkStep = renderJogDisplayLeft;
  afterBlinkStep = renderJogDisplayRight;

  uint8_t frameToSend[16];
  memcpy(frameToSend, leftFrame[currentShadeOfGrey], 16); // avoid memcpy?
  SPI.transfer(frameToSend, 16);


  renderLeftEndTimeTmp = micros();
  triggerLatency();
}

void renderJogDisplayRight() {
  digitalWrite(13, BLINK_ON);
  renderDisplayTask.delay(config.blinkOffDuration);
  generateFramesFromConfig();
  renderDisplayTask.setCallback(blink);
  // afterBlinkStep = renderJogDisplayLeft;
  afterBlinkStep = renderMiddlePixels;


  uint8_t frameToSend[16];
  memcpy(frameToSend, rightFrame[currentShadeOfGrey], 16);
  SPI.transfer(frameToSend, 16);
  triggerLatency();
}



/**
 * Turn on the center pixels (1 blue top, 1 red top, 2 blues bottom, 2 reds bottom)
 * for a short time to smooth the loss of light due to left/right masking
 */
void renderMiddlePixels() {
  digitalWrite(13, BLINK_ON);
  generateFramesFromConfig();
  renderDisplayTask.setCallback(blink);
  renderDisplayTask.delay(config.blinkOffDuration);
  afterBlinkStep = renderJogDisplayLeft;

  // TODO display mid pixels only here
  // Top blue
  // top red
  // vinyl
  // bottom red
  // bottom blue
  uint8_t frameToSend[16];
  for (int i = 0; i < 16; i++) {
    frameToSend[i] = middleFrame[i];
    if (i == 0 || i == 1 ||  i == 3 || i == 13 || i == 15) {
      frameToSend[i] = middleFrame[i]
        & ( leftFrame[currentShadeOfGrey][i]
          | rightFrame[currentShadeOfGrey][i]
        );
    }
  }
  SPI.transfer(frameToSend, 16);

  triggerLatency();
}


void stopRenderingJogDisplaySPI() {
  Serial.println("stopRenderingJogDisplaySPI");
}

void triggerLatency() {
  if (renderCount >= config.latencySpacing) {
    // renderDisplayTask.setCallback(startLatency);
    // renderDisplayTask.setCallback(afterBlinkStep);
    renderCount = 0;
    startLatency();
  }
  else {
    renderCount++;
    // renderDisplayTask.setCallback(afterBlinkStep);
    blink();
  }
}

// TODO latency during blink?
void blink() {
  currentShadeOfGrey++;
  if (currentShadeOfGrey >= config.shadesOfGrey) {
    currentShadeOfGrey = 0;
  }


  blinkOnStartTimeTmp = micros();
  renderDisplayTask.delay(config.blinkOnDuration);
  digitalWrite(13, BLINK_OFF);
  // if (blinkCount >= config.latencySpacing) {
  //   renderDisplayTask.setCallback(startLatency);
  //   // renderDisplayTask.setCallback(afterBlinkStep);
  //   blinkCount = 0;
  // }
  // else {
  //   blinkCount++;
  //   renderDisplayTask.setCallback(afterBlinkStep);
  // }
  renderDisplayTask.setCallback(afterBlinkStep);
  blinkOnEndTimeTmp = micros();
}



void startLatency() {
  if (config.latencyDuration <= 0) {
    blink();
    return;
  }
  digitalWrite(6, HIGH);
  renderDisplayTask.delay(config.latencyDuration);
  if (config.latencyDuration <= 13) {
    stopLatency();
  }
  else if (config.latencyDuration <= 26) {
    digitalWrite(6, HIGH);
    stopLatency();
  }
  else if (config.latencyDuration <= 39) {
    digitalWrite(6, HIGH);
    digitalWrite(6, HIGH);
    stopLatency();
  }
  else {
    renderDisplayTask.setCallback(stopLatency);
  }
}

void stopLatency() {
  digitalWrite(6, LOW);
  renderDisplayTask.setCallback(blink);
}

uint8_t animationStep = 0;
bool animationDirection = true;
void startAnimation() {
  if (config.animationInterval == 0) {
    return;
  }
  animationTask.delay(config.animationInterval);

  animationStartTimeTmp = micros();

  if (animationDirection) {
    config.circleIn[0] = animationStep;
    config.circleIn[1] = config.shadesOfGrey - animationStep;
  }
  else {
    config.circleIn[0] = config.shadesOfGrey - animationStep;
    config.circleIn[1] = animationStep;
  }

  if (animationStep == config.shadesOfGrey) {
    animationDirection = !animationDirection;
    animationStep = 0;
  }
  else {
    animationStep++;
  }
  animationStartTime = animationStartTimeTmp;
  animationEndTime = micros();
}

void stopAnimation() {
}

void dumpFrameToSerial(DisplayFrame frame) {
  for (int i = 0; i < SHADE_OF_GREY_COUNT; i++) {
    Serial.print(blankString +
      "Grey " + i + " => "
    );
    dumpSPIFrameToSerial(frame[i]);
    Serial.println();
  }
}

void dumpSPIFrameToSerial(uint8_t frame[16]) {
  Serial.print(byteToString(frame[0]));
  for (int j = 1; j < 16; j++) {
    Serial.print(','); Serial.print(frame[j]);
  }
}


// COMMANDS

void setupSerialCommand() {
  // Serial.begin(115200);
  Serial.begin(9600);
  Serial.setTimeout(0);
}

// Required for arduino mega as readStringUntil() returns one char by one char
String readSerialInput() {
  if (Serial.available()) {
    char c = (char) Serial.read();
    if (c == '\n' || c == '\r') {
      String copy = String(serialInput);
      serialInput = String();
      return copy;
    }
    else {
      serialInput += c;
    }
  }
  return String();
}

// Hand made readStringUntil
String readSerialStringUntil(String& serialLine, char delimiter) {
  int index = serialLine.indexOf(delimiter);
  if (index == -1) {
    index = serialLine.length();
  }
  // Serial.println(index);

  String out = serialLine.substring(0, index);
  // Serial.println(out);
  if (index == serialLine.length()) {
    serialLine = String();
  }
  else {
    serialLine.remove(0, index+1);
    // serialLine = serialLine.substring(index+1, serialLine.length());
  }
  // Serial.print("Reste: '"); Serial.print(serialLine); Serial.println("'");
  return out;
}

void handleSerialCommand() {
  String serialLine = readSerialInput();

  if(serialLine.length()) {

    String command = readSerialStringUntil(serialLine, ':');
    command.trim();
    Serial.print(blankString +
      "Command: " + command + "\n"
    );

    // return;

    if (command == "time") {

      Serial.print(blankString +
          "Render duration left: " + (blinkOnStartTime - renderLeftStartTime) + " us\n"
        + "Blink duration: " + (currentRenderStartTime - blinkOnStartTime) + " us\n"
        + "Animation duration: " + (animationEndTime - animationStartTime) + " us\n"
        + "Frame update durations (" + FRAME_UPDATE_STEPS + " / " + config.maxFrameUpdateStep + "): \n"
      );

      for (uint8_t i = 0; i < FRAME_UPDATE_STEPS; i++) {
        Serial.print(blankString + i + ": " + frameUpdateDurations[i] + "us \n");
      }

    }
    else if (command == "dump") {
      Serial.println("Left Frames: ");
      dumpFrameToSerial(leftFrame);

      Serial.println("Right Frames: ");
      dumpFrameToSerial(rightFrame);
    }
    else if (command == "config") {
      dumpConfigToSerial();
    }
    else if (command == "blink") {
      String on = readSerialStringUntil(serialLine, ',');
      String off = readSerialStringUntil(serialLine, ',');
      on.trim();
      off.trim();
      config.blinkOnDuration = on.toInt();
      config.blinkOffDuration = off.toInt();
    }
    else if (command == "shades") {
      String numberOfShadesStr = readSerialStringUntil(serialLine, ',');
      numberOfShadesStr.trim();
      int numberOfShades = numberOfShadesStr.toInt();
      if (numberOfShades >= SHADE_OF_GREY_COUNT) {
        numberOfShades = SHADE_OF_GREY_COUNT;
      }
      config.shadesOfGrey = numberOfShades;
    }
    else if (command == "latency") {
      String spacing = readSerialStringUntil(serialLine, ',');
      String duration = readSerialStringUntil(serialLine, ',');
      spacing.trim();
      duration.trim();
      config.latencyDuration = duration.toInt();
      config.latencySpacing = spacing.toInt();
    }
    else if (command == "vinyl") {
      String values[2] = {
        readSerialStringUntil(serialLine, ','),
        readSerialStringUntil(serialLine, ','),
      };
      values[0].trim();
      values[1].trim();

      config.vinyl[0] = values[0].toInt();
      config.vinyl[1] = values[1].toInt();
    }
    else if (command == "circle_in") {
      String values[2] = {
        readSerialStringUntil(serialLine, ','),
        readSerialStringUntil(serialLine, ','),
      };
      values[0].trim();
      values[1].trim();

      config.circleIn[0] = values[0].toInt();
      config.circleIn[1] = values[1].toInt();
    }
    else if (command == "circle_out") {
      String values[2] = {
        readSerialStringUntil(serialLine, ','),
        readSerialStringUntil(serialLine, ','),
      };
      values[0].trim();
      values[1].trim();

      config.circleOut[0] = values[0].toInt();
      config.circleOut[1] = values[1].toInt();
    }
    else if (command == "blue_pixels") {
      String list = readSerialStringUntil(serialLine, '\n');
      uint8_t length = parseCommandRange(list, config.bluePixels);
      // TODO
      // + split parsing command and saving values
      // + save a list of values
    }
    else if (command == "red_pixels") {
      String list = readSerialStringUntil(serialLine, '\n');
      uint8_t length = parseCommandRange(list, config.redPixels);
    }

    else if (command == "animation") {
      String value = readSerialStringUntil(serialLine, '\n');
      config.animationInterval = value.toDouble();
    }

    if (command == "frame") {
      int inputLength = 0;
      while (String numericValue = readSerialStringUntil(serialLine, ':')) {
        numericValue.trim();
        if (numericValue == "") {
          break;
        }

        // config.redPixels[start + inputLength] = parseBitsOrInt(numericValue);
        for(int greyLevel = 0; greyLevel < SHADE_OF_GREY_COUNT; greyLevel++) {
          leftFrame[greyLevel][inputLength]  = parseBitsOrInt(numericValue);
          rightFrame[greyLevel][inputLength] = parseBitsOrInt(numericValue);
        }
        inputLength++;
      }
    }
  }
}



// TODO split between command layer and vaue update layer
int parseCommandRange(String commandString, int * values) {

  Serial.println(blankString + "parseCommandRange: " +  commandString);

  // start => end: value
  uint8_t arrowPos = commandString.indexOf("=>");
  if (arrowPos == -1) {
    Serial.println(blankString + "No '=>' found in: " + commandString);
    return -1;
  }
  uint8_t start = commandString.substring(0, arrowPos).toInt();
  uint8_t end = commandString.substring(arrowPos+2).toInt();

  uint8_t colonPos = commandString.indexOf(':');
  if (colonPos == -1) {
    Serial.println(blankString + "No ':' found in: " + commandString);
    return -1;
  }
  uint8_t value = commandString.substring(colonPos+1).toInt();

  Serial.println(blankString + "Range:" + start + " => " +  end + ": " + value);

  for (int i = start; i < end; i++) {
    values[i] = value;
  }

  return end - start;
}



void updateFirstByte() {
   // VINYL
  setFramePixelValue(leftFrame,  0, B00001000, config.vinyl[0]);
  setFramePixelValue(rightFrame, 0, B00001000, config.vinyl[1]);

  // CIRCLE IN
  setFramePixelValue(leftFrame,  0, B00000100, config.circleIn[0]);
  setFramePixelValue(rightFrame, 0, B00000100, config.circleIn[1]);

  // CIRCLE OUT
  setFramePixelValue(leftFrame,  0, B00000010, config.circleOut[0]);
  setFramePixelValue(rightFrame, 0, B00000010, config.circleOut[1]);
}

void updateRightRedPixelsByte_13() {
  // The top middle pixel (i=0) is shared between two sides so it must be
  // set on both frames to be at full light
  setFramePixelValue(rightFrame, 13, B00001000, config.redPixels[0]);
  setFramePixelValue(leftFrame, 13, B00001000, config.redPixels[0]);

  for (uint8_t i = 1; i < 4; i++) { // Right side
    int onValue;
    switch (i) {
      case 1: onValue = B00010000; break;
      case 2: onValue = B01000000; break;
      case 3: onValue = B10000000; break;
    }
    setFramePixelValue(rightFrame, 13, onValue, config.redPixels[i]);
  }
}

void updateRight44RedPixelsByte(uint8_t start, uint8_t end) {
  // bytes 4 => 12
  // for (uint8_t i = 4; i < 40; i++) {
  for (uint8_t i = start; i < end; i++) {
    // Red bytes are B10101010
    uint8_t byteNumber;
    byteNumber = i / 4;
    uint8_t bitNumber;
    bitNumber = i & B00000011;

    uint8_t frameByteNumber;
    frameByteNumber = 11 - byteNumber;
    frameByteNumber += 2; // skip blue only bytes
    uint8_t onValue = B00000010 << (bitNumber * 2);
    setFramePixelValue(rightFrame, frameByteNumber, onValue, config.redPixels[i]);
  }
}

void updateRightRedPixelsByte_3() {
  for (uint8_t i = 40; i < 43; i++) {
    // i = 40, 41, 42 (3 last right pixels)
    int onValue;
    switch (i) {
      case 40: onValue = B00000010; break;
      case 41: onValue = B00000100; break;
      case 42: onValue = B00001000; break;
    }
    setFramePixelValue(rightFrame, 3, onValue, config.redPixels[i]);
  }
}

void updateLeftRedPixelsByte_3() {
  for (uint8_t i = 43; i < 46; i++) {
    // i = 43, 44, 45 (3 first left pixels)
    int onValue;
    switch (i) {
      case 43: onValue = B00100000; break;
      case 44: onValue = B01000000; break;
      case 45: onValue = B10000000; break;
    }

    setFramePixelValue(leftFrame, 3, onValue, config.redPixels[i]);
  }
}

void updateLeft44RedPixelsByte(uint8_t start, uint8_t end) {
  // bytes 4 => 12
  // for (uint8_t i = 46; i < 82; i++) {
  for (uint8_t i = start; i < end; i++) {
    // Left side. As there are 86 red pixels, we need an offset of 2
    // to apply masks by byte
    uint8_t byteNumber;
    byteNumber = (i+2) / 4;
    uint8_t bitNumber;
    bitNumber = (i+2) & B00000011;

    uint8_t frameByteNumber;
    frameByteNumber = byteNumber - 10;
    frameByteNumber += 2; // skip blue only bytes

    if (byteNumber > 11 && byteNumber < 21) {
      int onValue;
      // Red bytes are B10101010

      // int onValue = B00000010 << (bitNumber * 2);
      switch (bitNumber) {
        case 0: onValue = B10000000; break;
        case 1: onValue = B00100000; break;
        case 2: onValue = B00001000; break;
        case 3: onValue = B00000010; break; // ??
      }

      setFramePixelValue(leftFrame, frameByteNumber, onValue, config.redPixels[i]);
    }
  }
}


void updateLeftRedPixelsByte_13() {
  for (uint8_t i = 82; i < 85; i++) {
    // i = 43, 44, 45 (3 first left pixels)
    int onValue;
    switch (i) {
      case 82: onValue = B10000000; break;
      case 83: onValue = B00000001; break;
      case 84: onValue = B00000010; break;
      // case 87: onValue = B00001000; break;
    }

    setFramePixelValue(leftFrame, 13, onValue, config.redPixels[i]);
  }
}



void updateRightBluePixelsByte_15() {
  for (uint8_t i = 1; i < 6; i++) {
    int onValue;
    switch (i) {
      case 1: onValue = B00001000; break;
      case 2: onValue = B00010000; break;
      case 3: onValue = B00100000; break;
      case 4: onValue = B01000000; break;
      case 5: onValue = B10000000; break;
    }
    setFramePixelValue(rightFrame, 15, onValue, config.bluePixels[i]);
  }
}

void updateRightBluePixelsByte_14(uint8_t start, uint8_t end) {
  // for (uint8_t i = 6; i < 14; i++) {
  for (uint8_t i = start; i < end; i++) {
    uint8_t onValue;
    switch (i-6) {
      case 0: onValue = B00000001; break;
      case 1: onValue = B00000010; break;
      case 2: onValue = B00000100; break;
      case 3: onValue = B00001000; break;
      case 4: onValue = B00010000; break;
      case 5: onValue = B00100000; break;
      case 6: onValue = B01000000; break;
      case 7: onValue = B10000000; break;
    }
    setFramePixelValue(rightFrame, 14, onValue, config.bluePixels[i]);
  }
}

void updateRightBluePixelsByte_13() {
  for (uint8_t i = 14; i < 16; i++) {
    uint8_t onValue;
    switch (i-14) {
      case 0: onValue = B00000100; break;
      case 1: onValue = B00100000; break;
    }
    setFramePixelValue(rightFrame, 13, onValue, config.bluePixels[i]);
  }
}

void updateRight44BluePixelsByte(uint8_t start, uint8_t end) {
  // bytes 4 => 12
  // for (uint8_t i = 16; i < 52; i++) {
  for (uint8_t i = start; i < end; i++) {
    uint8_t byteNumber;
    byteNumber = (i-12) / 4;
    uint8_t bitNumber;
    bitNumber = (i-12) & B00000011;

    uint8_t frameByteNumber;
    frameByteNumber = 11 - byteNumber;
    frameByteNumber += 2; // skip blue only bytes
    // Blue bytes are B01010101
    uint8_t onValue = B00000001 << (bitNumber * 2);
    setFramePixelValue(rightFrame, frameByteNumber, onValue, config.bluePixels[i]);
  }
}

void updateRightBluePixelsByte_3() {
  for (uint8_t i = 52; i < 54; i++) {
    int onValue;
    switch (i) {
      case 52: onValue = B00000001; break;
      case 53: onValue = B00010000; break;
    }
    setFramePixelValue(rightFrame, 3, onValue, config.bluePixels[i]);
  }
}

void updateRightBluePixelsByte_2(uint8_t start, uint8_t end) {
  // for (uint8_t i = 54; i < 62; i++) {
  for (uint8_t i = start; i < end; i++) {
    uint8_t onValue;
    switch (i-54) {
      case 0: onValue = B00000001; break;
      case 1: onValue = B00000010; break;
      case 2: onValue = B00000100; break;
      case 3: onValue = B00001000; break;
      case 4: onValue = B00010000; break;
      case 5: onValue = B00100000; break;
      case 6: onValue = B01000000; break;
      case 7: onValue = B10000000; break;
    }
    setFramePixelValue(rightFrame, 2, onValue, config.bluePixels[i]);
  }
}

void updateRightBluePixelsByte_1() {
  for (uint8_t i = 62; i < 68; i++) {
    int onValue;
    switch (i-62) {
      case 0: onValue = B00000001; break;
      case 1: onValue = B00000010; break;
      case 2: onValue = B00000100; break;
      case 3: onValue = B00001000; break;
      case 4: onValue = B00010000; break;
      case 5: onValue = B00100000; break;
    }
    setFramePixelValue(rightFrame, 1, onValue, config.bluePixels[i]);
  }
}


void updateLeftBluePixelsByte_0_1() {
  setFramePixelValue(leftFrame, 0, B00000001, config.bluePixels[70]);

  for (uint8_t i = 68; i < 74; i++) {
    int onValue;
    switch (i-68) {
      case 0: onValue = B01000000; break;
      case 1: onValue = B10000000; break;
      case 2: onValue = B00000100; break;
      // special case for i == 68: in byte 0
      case 4: onValue = B00000010; break;
      case 5: onValue = B00000001; break;
    }
    setFramePixelValue(leftFrame, 1, onValue, config.bluePixels[i]);
  }
}

void updateLeftBluePixelsByte_2(uint8_t start, uint8_t end) {
  // for (uint8_t i = 74; i < 82; i++) {
  for (uint8_t i = start; i < end; i++) {
    int onValue;
    switch (i-74) {
      case 0: onValue = B10000000; break;
      case 1: onValue = B01000000; break;
      case 2: onValue = B00100000; break;
      case 3: onValue = B00010000; break;
      case 4: onValue = B00001000; break;
      case 5: onValue = B00000100; break;
      case 6: onValue = B00000010; break;
      case 7: onValue = B00000001; break;
    }
    setFramePixelValue(leftFrame, 2, onValue, config.bluePixels[i]);
  }
}

void updateLeftBluePixelsByte_3() {
  for (uint8_t i = 82; i < 84; i++) {
    int onValue;
    switch (i) {
      case 82: onValue = B00010000; break;
      case 83: onValue = B00000001; break;
    }
    setFramePixelValue(leftFrame, 3, onValue, config.bluePixels[i]);
  }
}

void updateLeft44BluePixelsByte(uint8_t start, uint8_t end) {
  // bytes 4 => 12
  // for (uint8_t i = 84; i < 120; i++) {
  for (uint8_t i = start; i < end; i++) {
    uint8_t byteNumber;
    byteNumber = (i-4) / 4;
    uint8_t bitNumber;
    bitNumber = (i-4) & B00000011;

    uint8_t frameByteNumber;
    frameByteNumber = byteNumber - 16;
    // Blue bytes are B01010101
    uint8_t onValue = B01000000 >> (bitNumber * 2);
    setFramePixelValue(leftFrame, frameByteNumber, onValue, config.bluePixels[i]);
  }
}



void updateLeftBluePixelsByte_13() {
  for (uint8_t i = 120; i < 122; i++) {
    uint8_t onValue;
    switch (i) {
      case 120: onValue = B00100000; break;
      case 121: onValue = B00000100; break;
    }
    setFramePixelValue(leftFrame, 13, onValue, config.bluePixels[i]);
  }
}

void updateLeftBluePixelsByte_14(uint8_t start, uint8_t end) {
  // for (uint8_t i = 122; i < 130; i++) {
  for (uint8_t i = start; i < end; i++) {
    int onValue;
    switch (i-122) {
      case 0: onValue = B10000000; break;
      case 1: onValue = B01000000; break;
      case 2: onValue = B00100000; break;
      case 3: onValue = B00010000; break;
      case 4: onValue = B00001000; break;
      case 5: onValue = B00000100; break;
      case 6: onValue = B00000010; break;
      case 7: onValue = B00000001; break;
    }
    setFramePixelValue(leftFrame, 14, onValue, config.bluePixels[i]);
  }
}

void updateLeftBluePixelsByte_15() {
  for (uint8_t i = 130; i < 135; i++) {
    int onValue;
    switch (i-130) {
      case 0: onValue = B10000000; break;
      case 1: onValue = B01000000; break;
      case 2: onValue = B00100000; break;
      case 3: onValue = B00000001; break;
      case 4: onValue = B00000010; break;
    }
    setFramePixelValue(leftFrame, 15, onValue, config.bluePixels[i]);
  }
}


uint8_t updateStep = 0;
void generateFramesFromConfig() {
  int frameUpdateStartTimeTmp = micros();
  // Serial.println(blankString + "generateFramesFromConfig: " + updateStep);

  updateStep++;
  uint8_t onStep = 0;

  if (updateStep == (onStep += 1)) {
    // Serial.println(blankString + "updateFirstByte");
    updateFirstByte();
  }
  else if (updateStep == (onStep += 1)) {
    updateRightRedPixelsByte_13();
  }

  else if (updateStep == (onStep += 1)) {
    // updateRightRedPixelsByte_4_12();
    updateRight44RedPixelsByte(4, 8);
  }
  else if (updateStep == (onStep += 1)) {
    // updateRightRedPixelsByte_4_12();
    updateRight44RedPixelsByte(8, 12);
  }
  else if (updateStep == (onStep += 1)) {
    // updateRightRedPixelsByte_4_12();
    updateRight44RedPixelsByte(12, 16);
  }
  else if (updateStep == (onStep += 1)) {
    // updateRightRedPixelsByte_4_12();
    updateRight44RedPixelsByte(16, 20);
  }
  else if (updateStep == (onStep += 1)) {
    // updateRightRedPixelsByte_4_12();
    updateRight44RedPixelsByte(20, 24);
  }
  else if (updateStep == (onStep += 1)) {
    // updateRightRedPixelsByte_4_12();
    updateRight44RedPixelsByte(24, 28);
  }
  else if (updateStep == (onStep += 1)) {
    // updateRightRedPixelsByte_4_12();
    updateRight44RedPixelsByte(28, 32);
  }
  else if (updateStep == (onStep += 1)) {
    // updateRightRedPixelsByte_4_12();
    updateRight44RedPixelsByte(32, 36);
  }
  else if (updateStep == (onStep += 1)) {
    // updateRightRedPixelsByte_4_12();
    updateRight44RedPixelsByte(36, 40);
  }

  else if (updateStep == (onStep += 1)) {
    updateRightRedPixelsByte_3();
  }
  else if (updateStep == (onStep += 1)) {
    updateLeftRedPixelsByte_3();
  }

  else if (updateStep == (onStep += 1)) {
    // updateLeftRedPixelsByte_4_12();
    updateLeft44RedPixelsByte(46, 50);
  }
  else if (updateStep == (onStep += 1)) {
    // updateLeftRedPixelsByte_4_12();
    updateLeft44RedPixelsByte(50, 54);
  }
  else if (updateStep == (onStep += 1)) {
    // updateLeftRedPixelsByte_4_12();
    updateLeft44RedPixelsByte(54, 58);
  }
  else if (updateStep == (onStep += 1)) {
    // updateLeftRedPixelsByte_4_12();
    updateLeft44RedPixelsByte(58, 62);
  }
  else if (updateStep == (onStep += 1)) {
    // updateLeftRedPixelsByte_4_12();
    updateLeft44RedPixelsByte(62, 66);
  }
  else if (updateStep == (onStep += 1)) {
    // updateLeftRedPixelsByte_4_12();
    updateLeft44RedPixelsByte(66, 70);
  }
  else if (updateStep == (onStep += 1)) {
    // updateLeftRedPixelsByte_4_12();
    updateLeft44RedPixelsByte(70, 74);
  }
  else if (updateStep == (onStep += 1)) {
    // updateLeftRedPixelsByte_4_12();
    updateLeft44RedPixelsByte(74, 78);
  }
  else if (updateStep == (onStep += 1)) {
    // updateLeftRedPixelsByte_4_12();
    updateLeft44RedPixelsByte(78, 82);
  }

  else if (updateStep == (onStep += 1)) {
    updateLeftRedPixelsByte_13();
  }
  else if (updateStep == (onStep += 1)) {
    // The top middle pixel (i=0) is shared between two sides so it must be
    // set on both frames to be at full light
    setFramePixelValue(rightFrame, 15, B00000100, config.bluePixels[0]);
    setFramePixelValue(leftFrame, 15, B00000100, config.bluePixels[0]);
  }
  else if (updateStep == (onStep += 1)) {
    updateRightBluePixelsByte_15();
  }
  else if (updateStep == (onStep += 1)) {
  // for (uint8_t i = 6; i < 14; i++) {
    updateRightBluePixelsByte_14(6, 10);
  }
  else if (updateStep == (onStep += 1)) {
    updateRightBluePixelsByte_14(10, 14);
  }
  else if (updateStep == (onStep += 1)) {
    updateRightBluePixelsByte_13();
  }

  else if (updateStep == (onStep += 1)) {
    updateRight44BluePixelsByte(16, 20);
  }
  else if (updateStep == (onStep += 1)) {
    updateRight44BluePixelsByte(20, 24);
  }
  else if (updateStep == (onStep += 1)) {
    updateRight44BluePixelsByte(24, 28);
  }
  else if (updateStep == (onStep += 1)) {
    updateRight44BluePixelsByte(28, 32);
  }
  else if (updateStep == (onStep += 1)) {
    updateRight44BluePixelsByte(32, 36);
  }
  else if (updateStep == (onStep += 1)) {
    updateRight44BluePixelsByte(36, 40);
  }
  else if (updateStep == (onStep += 1)) {
    updateRight44BluePixelsByte(40, 44);
  }
  else if (updateStep == (onStep += 1)) {
    updateRight44BluePixelsByte(44, 48);
  }
  else if (updateStep == (onStep += 1)) {
    updateRight44BluePixelsByte(48, 52);
  }

  else if (updateStep == (onStep += 1)) {
    updateRightBluePixelsByte_3();
  }
  else if (updateStep == (onStep += 1)) { // 39
    // for (uint8_t i = 54; i < 62; i++) {
    updateRightBluePixelsByte_2(54, 58);
  }
  else if (updateStep == (onStep += 1)) {
    updateRightBluePixelsByte_2(58, 62);
  }
  else if (updateStep == (onStep += 1)) {
    updateRightBluePixelsByte_1();
  }
  else if (updateStep == (onStep += 1)) {
    updateLeftBluePixelsByte_0_1();
  }
  else if (updateStep == (onStep += 1)) { // ~43
    // for (uint8_t i = 74; i < 82; i++) {
    updateLeftBluePixelsByte_2(74, 78);
  }
  else if (updateStep == (onStep += 1)) {
    updateLeftBluePixelsByte_2(78, 82);
  }
  else if (updateStep == (onStep += 1)) {
    updateLeftBluePixelsByte_3();
  }

  else if (updateStep == (onStep += 1)) {
    updateLeft44BluePixelsByte(84, 88);
  }
  else if (updateStep == (onStep += 1)) {
    updateLeft44BluePixelsByte(88, 92);
  }
  else if (updateStep == (onStep += 1)) {
    updateLeft44BluePixelsByte(92, 96);
  }
  else if (updateStep == (onStep += 1)) {
    updateLeft44BluePixelsByte(96, 100);
  }
  else if (updateStep == (onStep += 1)) {
    updateLeft44BluePixelsByte(100, 104);
  }
  else if (updateStep == (onStep += 1)) {
    updateLeft44BluePixelsByte(104, 108);
  }
  else if (updateStep == (onStep += 1)) {
    updateLeft44BluePixelsByte(108, 112);
  }
  else if (updateStep == (onStep += 1)) {
    updateLeft44BluePixelsByte(112, 116);
  }
  else if (updateStep == (onStep += 1)) {
    updateLeft44BluePixelsByte(116, 120);
  }

  else if (updateStep == (onStep += 1)) {
    updateLeftBluePixelsByte_13();
  }
  else if (updateStep == (onStep += 1)) {
  // for (uint8_t i = 122; i < 130; i++) {
    updateLeftBluePixelsByte_14(122, 126);
  }
  else if (updateStep == (onStep += 1)) {
    updateLeftBluePixelsByte_14(126, 130);
  }
  else if (updateStep == (onStep += 1)) {
    updateLeftBluePixelsByte_15();
  }
  else {
    config.maxFrameUpdateStep = onStep;
    updateStep = 0;
  }

  frameUpdateDurations[updateStep] = micros() - frameUpdateStartTimeTmp;
}

// void setFramePixelValue(int frameSide[8][16], int byteNumber, int bitMask, int value) {
void setFramePixelValue(DisplayFrame frameSide, int byteNumber, int bitMask, int value) {

  // Serial.println(value);
  for (uint8_t greyLevel = 0; greyLevel < config.shadesOfGrey; greyLevel++) {
    if (value > greyLevel) { // ints are for shades of gray (later)
      frameSide[greyLevel][byteNumber] |= bitMask;
      // Serial.print("enable: ");
      // Serial.println(byteToString(bitMask));
    }
    else {
      frameSide[greyLevel][byteNumber] &= bitMask ^ B11111111;
      // Serial.print("disable: ");
      // Serial.println(byteToString(bitMask));
    }
  }
}


void stopHandlingSerialCommand() {
  Serial.println("stopHandlingSerialCommand");
}






String byteToString(int value) {
  String out;
  for (uint8_t i = 7; i >= 0; i--) {
      bool b = bitRead(value, i);
      out += b ? '1' : '0';
  }

  return out;
}

uint8_t parseBitsOrInt(String serialString) {
  uint8_t length = serialString.length();
  uint8_t result = 0;
  if (length <= 3) {
    result = serialString.toInt();
  }
  else if (length == 8) {
    for(uint8_t i = 0; i < length; i++ ) {
      // char c = serialString[i];
      // char incomingBytes [1] = {
      //   serialString[i]
      // };
      // int incomingBit = atoi(incomingBytes);
      int incomingBit = serialString.substring(i, i + 1).toInt();
      bitWrite(result, 7 - i, incomingBit);
    }
  }
  else {
    result = -1;
  }

  return result;
}
