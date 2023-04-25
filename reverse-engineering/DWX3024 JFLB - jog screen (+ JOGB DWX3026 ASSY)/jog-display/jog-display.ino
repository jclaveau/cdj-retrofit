#include <Regexp.h>

// https://playground.arduino.cc/Code/TaskScheduler/
// https://github.com/arkhipenko/TaskScheduler/wiki/API-Task#void-setinterval-unsigned-long-ainterval
#define _TASK_MICRO_RES
#include <TaskScheduler.h>
#include <TaskSchedulerDeclarations.h>
#include <TaskSchedulerSleepMethods.h>


// https://forum.arduino.cc/t/text-and-variable-both-in-display-println/586907/7

// #include "language-tools.h"
#include "SPI.h"

void printLine()
{
  Serial.println();
}

template <typename T, typename... Types>
void printLine(T first, Types... other)
{
  Serial.print(first);
  printLine(other...) ;
}


// unsigned long msBefore;
// unsigned long loopTime;
// bool clockPinState = false;

String serialInput;



int frameLength = 16;
int frameStart = 2;
int frameValues[64] = { // > 16 for experiments
  0, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  // 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  // 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

int frameSentValues[64] = { // > 16 for experiments
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

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
  1 * TASK_IMMEDIATE,
  TASK_FOREVER,
  &handleSerialCommand,
  &ts,
  false,
  NULL,
  &stopHandlingSerialCommand
);

Task renderDisplayTask(
  TASK_ONCE, // delay defined betwee, each non blocking step
  TASK_FOREVER,
  &renderJogDisplayLeft,
  &ts,
  false,
  NULL,
  &stopRenderingJogDisplaySPI
);

Task blinkPinTask(
  TASK_ONCE, // delay defined betwee, each non blocking step
  TASK_FOREVER,
  &blinkPin,
  &ts,
  false,
  NULL,
  &stopBlinkingPin
);

Task latencyTask(
  TASK_ONCE,
  TASK_ONCE,
  &startLatency,
  &ts,
  false,
  NULL,
  &stopLatency
);


void setup() {
  pinMode(6, OUTPUT);
  pinMode(13, OUTPUT);

  setupSerialCommand();
  setupJogDisplaySPI();

  ts.enableAll();
}

void loop() {
  ts.execute();
}


void setupJogDisplaySPI() {
  // SPI.setClockDivider(SPI_CLOCK_DIV128);
  SPI.begin();
  SPI.beginTransaction(SPISettings(20000000, MSBFIRST, SPI_MODE0));
}

#define DISPLAY_LEFT 2
#define DISPLAY_MIDDLE 4
#define DISPLAY_RIGHT 8
int displaySide = DISPLAY_LEFT;
int renderPeriod = 6000; // 10ms is blinking and below reduces contrast with black ones


#define VINYL_LEFT 2;
#define VINYL_RIGHT 4;
#define VINYL_ON 8;
#define VINYL_OFF 16;

struct Config {
  int renderPeriod;
  int blinkPeriod;
  int renderRatios[3];
  int renderRatiosSum;
  int vinyl[2];
  int circleIn[2];
  int circleOut[2];
  int bluePixels[143];
  int redPixels[88];
} config = {
  200, // full render period
  10,
  // {4, 4, 2},
  {5, 5, 0},
  10,
  {0, 0},
  {0, 0},
  {0, 0},
  0,
  0,
};

// int leftFrameInt[16] = {B01100000};
// int rightFrameInt[16] = {B01100000};

// With shades of grey

const uint8_t SHADE_OF_GREY_COUNT = 8;
uint8_t leftFrame[SHADE_OF_GREY_COUNT][16]  = {
  {B00100000},
  {B00100000},
  {B00100000},
  {B00100000},
  {B00100000},
  {B00100000},
  {B00100000},
  {B00100000}
};
uint8_t rightFrame[SHADE_OF_GREY_COUNT][16] = {
  {B01000000},
  {B01000000},
  {B01000000},
  {B01000000},
  {B01000000},
  {B01000000},
  {B01000000},
  {B01000000}
};

// RENDERING
int currentShadeOfGrey = 0;
void renderJogDisplayLeft() {

  // generateFramesFromConfig();

  renderDisplayTask.delay(config.renderPeriod * config.renderRatios[0] / config.renderRatiosSum);

  // int mask = B10111111; // Right side off
  // uint8_t mask = B10111111; // Right side off

  // uint8_t frameToSend[16];
  // for (int i = 0; i < 16; i++) {
  //   int valueToTransfer = leftFrame[currentShadeOfGrey][i];
  //   // if (i == 0) {
  //   //   valueToTransfer &= mask;
  //   // }

  //   frameToSend[i] = valueToTransfer;
  //   // SPI.transfer(valueToTransfer);
  // }
  // SPI.transfer(frameToSend, 16);
  // SPI.transfer(leftFrame[currentShadeOfGrey], 16);
  uint8_t frameToSend[16];
  memcpy(frameToSend, leftFrame[currentShadeOfGrey], 16);
  SPI.transfer(frameToSend, 16);

  renderDisplayTask.setCallback(renderMiddlePixels);
  // renderDisplayTask.setCallback(renderJogDisplayRight);
}

void renderJogDisplayRight() {
  renderDisplayTask.delay(config.renderPeriod * config.renderRatios[1] / config.renderRatiosSum);

  // int mask = B11011111;  // Left side off
  // uint8_t mask = B11011111;  // Left side off

  // uint8_t frameToSend[16];

  // for (int i = 0; i < 16; i++) {
  //   int valueToTransfer = rightFrame[currentShadeOfGrey][i];
  //   // if (i == 0) {
  //   //   valueToTransfer &= mask;
  //   // }

  //   frameToSend[i] = valueToTransfer;

  //   // SPI.transfer(valueToTransfer);
  // }
  // SPI.transfer(frameToSend, 16);

  uint8_t frameToSend[16];
  memcpy(frameToSend, rightFrame[currentShadeOfGrey], 16);
  SPI.transfer(frameToSend, 16);


  // if (currentShadeOfGrey > SHADE_OF_GREY_COUNT - 1) {
  if (currentShadeOfGrey > 4 - 1) {
    currentShadeOfGrey = 0;
  } else {
    currentShadeOfGrey++;
  }

  renderDisplayTask.setCallback(renderJogDisplayLeft);
}

// void sendSPIFrame(uint8_t mask, ) {

//   for (int i = 0; i < 16; i++) {
//     int valueToTransfer = rightFrame[currentShadeOfGrey][i];
//     if (i == 0) {
//       valueToTransfer &= mask;
//     }

//     SPI.transfer(valueToTransfer);
//   }

// }


/**
 * Turn on the center pixels (1 blue top, 1 red top, 2 blues bottom, 2 reds bottom)
 * for a short time to smooth the loss of light due to left/right masking
 */
void renderMiddlePixels() {
  renderDisplayTask.delay(config.renderPeriod * config.renderRatios[2] / config.renderRatiosSum);

  if (config.renderRatios[2] != 0) {
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
  }

  // if (currentShadeOfGrey > SHADE_OF_GREY_COUNT - 1) {
  //   currentShadeOfGrey = 0;
  // } else {
  //   currentShadeOfGrey++;
  // }

  renderDisplayTask.setCallback(renderJogDisplayRight);
}


void stopRenderingJogDisplaySPI() {
  Serial.println("stopRenderingJogDisplaySPI");
}

bool blinkState=false;
void blinkPin() {
  // HIGH / LOW
  blinkState = ! blinkState;
  digitalWrite(13, blinkState);
}

void stopBlinkingPin() {
  blinkState = true;
}




// bool latencyState=false;
void startLatency() {
  // HIGH / LOW
  // latencyState = ! latencyState;
  // digitalWrite(6, latencyState);
  digitalWrite(6, LOW);
  printLine("Latency started");
}

void stopLatency() {
  digitalWrite(6, HIGH);
  printLine("Latency stopped");
}



// COMMANDS

void setupSerialCommand() {
  // Serial.begin(115200);
  Serial.begin(9600);
  Serial.setTimeout(0);
}

void handleSerialCommand() {
  if(Serial.available() > 0) {
    /** Todo
     * Split commands by
     * + config
     *   + debug: dump frames,
     *   +
     */
    printLine("Command: ");

    String command = Serial.readStringUntil(':');
    command.trim();
    if (command == "blink") {
      String period = Serial.readStringUntil(',');
      String delay = Serial.readStringUntil(',');
      period.trim();
      delay.trim();

      // TODO phase par rapport au render
      blinkPinTask.setInterval(period.toInt());

      if (delay != "") {
        printLine("Delay set to ", delay);
        blinkPinTask.delay(delay.toInt());
      }
    }
    else if (command == "latency") {
      long latency = Serial.parseInt();
      printLine("Latency set to ", latency, " ms");

      latencyTask.setInterval(latency * 1000);
      latencyTask.setIterations(2 * TASK_ONCE); // We need 2 runs to wait for an interval
      latencyTask.restart();

      // TODO investigate phase / render for darek pixels
    }
    else if (command == "period") {
      config.renderPeriod = Serial.parseInt();

      float leftTime   = config.renderPeriod * config.renderRatios[0] / config.renderRatiosSum;
      float rightTime  = config.renderPeriod * config.renderRatios[1] / config.renderRatiosSum;
      float middleTime = config.renderPeriod * config.renderRatios[2] / config.renderRatiosSum;
      Serial.print("Left time: "); Serial.print(leftTime); Serial.println(" ms");
      Serial.print("Right time: "); Serial.print(rightTime); Serial.println(" ms");
      Serial.print("Middle time: "); Serial.print(middleTime); Serial.println(" ms");
    }
    else if (command == "render_ratios") {
      String leftRatio = Serial.readStringUntil(',');
      String rightRatio = Serial.readStringUntil(',');
      String middleRatio = Serial.readStringUntil(',');
      leftRatio.trim();
      rightRatio.trim();
      middleRatio.trim();

      config.renderRatios[0] = leftRatio.toInt();
      config.renderRatios[1] = rightRatio.toInt();
      config.renderRatios[2] = middleRatio.toInt();
    }
    // TODO latency
    else if (command == "vinyl") {
      String values[2] = {
        Serial.readStringUntil(','),
        Serial.readStringUntil(','),
      };
      values[0].trim();
      values[1].trim();
      // printLine("VINYL!!!", values[0], ", ", values[1]);

      config.vinyl[0] = values[0].toInt();
      config.vinyl[1] = values[1].toInt();

      // char out[30];
      // snprintf(out, 30, "%d,%d,%d", out1, out2, out3);

      Serial.print("vinyl: ");
      Serial.print(config.vinyl[0]);
      Serial.print(", ");
      Serial.println(config.vinyl[1]);
    }
    else if (command == "circle_in") {
      String values[2] = {
        Serial.readStringUntil(','),
        Serial.readStringUntil(','),
      };
      values[0].trim();
      values[1].trim();

      config.circleIn[0] = values[0].toInt();
      config.circleIn[1] = values[1].toInt();

      // char out[30];
      // snprintf(out, 30, "%d,%d,%d", out1, out2, out3);

      Serial.print("circle_in: ");
      Serial.print(config.circleIn[0]);
      Serial.print(", ");
      Serial.println(config.circleIn[1]);
    }
    else if (command == "circle_out") {
      String values[2] = {
        Serial.readStringUntil(','),
        Serial.readStringUntil(','),
      };
      values[0].trim();
      values[1].trim();

      config.circleOut[0] = values[0].toInt();
      config.circleOut[1] = values[1].toInt();
      Serial.print("circle_out: ");
      Serial.print(config.circleOut[0]);
      Serial.print(", ");
      Serial.println(config.circleOut[1]);
    }
    else if (command == "blue_pixels") {
      String list = Serial.readStringUntil('\n');
      int length = parseCommandList((char *) list.c_str(), config.bluePixels);
      if (length == -1) {
        length = parseCommandRange((char *) list.c_str(), config.bluePixels);
      }
    }
    else if (command == "red_pixels") {
      String list = Serial.readStringUntil('\n');
      int length = parseCommandList((char *) list.c_str(), config.redPixels);
      if (length == -1) {
        length = parseCommandRange((char *) list.c_str(), config.redPixels);
      }
    }

    generateFramesFromConfig();

    if (command == "frame") {
      int inputLength = 0;
      while (String numericValue = Serial.readStringUntil(':')) {
        numericValue.trim();
        // printLine("numericValue: ", numericValue);
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
    else if (command == "frame_buffer") {
      // TODO
    }


    printLine("Left Frames: ");
    for (int i = 0; i < SHADE_OF_GREY_COUNT; i++) {
      Serial.print("Grey "); Serial.print(i);
      Serial.print(" => ");
      Serial.print(byteToString(leftFrame[i][0]));
      for (int j = 1; j < 16; j++) {
        Serial.print(','); Serial.print(leftFrame[i][j]);
      }
      Serial.println();
    }

    printLine("Right Frames: ");
    for (int i = 0; i < SHADE_OF_GREY_COUNT; i++) {
      Serial.print("Grey "); Serial.print(i);
      Serial.print(" => ");
      Serial.print(byteToString(rightFrame[i][0]));
      for (int j = 1; j < 16; j++) {
        Serial.print(','); Serial.print(rightFrame[i][j]);
      }
      Serial.println();
    }
  }
}



MatchState ms;
int parseCommandRange(char * commandString, int * values) {
  ms.Target(commandString);

  // start => end: value
  char result = ms.Match("%s*(%d+)%s*=>%s*(%d+)%s*:%s*(%d+)", 0);
  int start = 0;
  int end = 0;
  int value = 0;
  printLine("Range:", start, " => ", end, ": ", value);
  if (result != REGEXP_MATCHED) {
    return -1;
  }

  char buffer[20];
  ms.GetMatch(buffer);
  start = atoi(ms.GetCapture(buffer, 0));
  end = atoi(ms.GetCapture(buffer, 1));
  value = atoi(ms.GetCapture(buffer, 2));

  printLine("Range:", start, " => ", end, ": ", value);

  for (int i = start; i < end; i++) {
    values[i] = value;
  }

  return end - start;
}


/**
 * Parse a command string like YY[XX,XX,XX] then save XX values
 * at the offset YY into the 'values' parameter.
 *
 * Returns the length of the array change.
 * Returns -1 if the commandString doesn't match the array+offset pattern
 */
int parseCommandList(char * commandString, int * values) {
  ms.Target(commandString);

  // Extracting offset
  char offsetResult = ms.Match ("%s*(%d+)", 0);
  int offset = 0;
  if (offsetResult == REGEXP_MATCHED) {
    char offsetBuffer [4];
    ms.GetMatch(offsetBuffer);
    offset = atoi(ms.GetCapture (offsetBuffer, 0));
  }

  // Extracting array values
  char arrayResult = ms.Match(
    "%s*%[%s*([^%]]+)%]",
    ms.MatchStart + ms.MatchLength
  );

  if (arrayResult != REGEXP_MATCHED) {
    return -1;
  }

  char arrayBuffer [136];
  ms.GetMatch(arrayBuffer);
  const char * arrayPart = ms.GetCapture(arrayBuffer, 0);

  // printLine("offset: ", offset);
  // printLine("arrayPart: ", arrayPart);

  // Extracting array values
  ms.Target((char *) arrayPart);
  int index = 0;
  int i = 0;
  char valueResult;
  char valueBuffer[8];
  do {
    valueResult = ms.Match ("(%d+)%s*,?%s*", index);
    if (valueResult == REGEXP_MATCHED) {
      // Serial.println(ms.GetMatch(valueBuffer));
      // printLine("Captures: ", ms.level);
      ms.GetMatch(valueBuffer);
      for (int j = 0; j < ms.level; j++) {
        values[offset + i] = parseBitsOrInt(ms.GetCapture(valueBuffer, j));
        i++;
      }
      index = ms.MatchStart + ms.MatchLength;
    }
  }
  while (valueResult == REGEXP_MATCHED);

  return i;
}

void generateFramesFromConfig() {

  // TODO
  // + prerender 8 => 16 frames with shade of grey applied
  // + render each frame with a delay
  // + Sync the blink with the render


  // VINYL
  setFramePixelValue(leftFrame,  0, B00001000, config.vinyl[0]);
  setFramePixelValue(rightFrame, 0, B00001000, config.vinyl[1]);

  // CIRCLE IN
  setFramePixelValue(leftFrame,  0, B00000100, config.circleIn[0]);
  setFramePixelValue(rightFrame, 0, B00000100, config.circleIn[1]);

  // CIRCLE OUT
  setFramePixelValue(leftFrame,  0, B00000010, config.circleOut[0]);
  setFramePixelValue(rightFrame, 0, B00000010, config.circleOut[1]);

  // RED PIXELS
  // Byte 0 => 4 mid bottom pixels
  // Byte 9 => 4 mid up pixels

  // The top middle pixel (i=0) is shared between two sides so it must be
  // set on both frames to be at full light
  setFramePixelValue(rightFrame, 13, B00001000, config.redPixels[0]);
  setFramePixelValue(leftFrame, 13, B00001000, config.redPixels[0]);

  for (int i = 1; i < 4; i++) { // Right side
    int onValue;
    switch (i) {
      case 1: onValue = B00010000; break;
      case 2: onValue = B01000000; break;
      case 3: onValue = B10000000; break;
    }
    setFramePixelValue(rightFrame, 13, onValue, config.redPixels[i]);
  }

  for (int i = 4; i < 40; i++) { // Right side
    // Red bytes are B10101010
    int byteNumber;
    byteNumber = i / 4;
    int bitNumber;
    bitNumber = i & B00000011;

    int frameByteNumber;
    frameByteNumber = 11 - byteNumber;
    frameByteNumber += 2; // skip blue only bytes
    int onValue = B00000010 << (bitNumber * 2);
    setFramePixelValue(rightFrame, frameByteNumber, onValue, config.redPixels[i]);
  }

  for (int i = 40; i < 43; i++) {
    // i = 40, 41, 42 (3 last right pixels)
    int onValue;
    switch (i) {
      case 40: onValue = B00000010; break;
      case 41: onValue = B00000100; break;
      case 42: onValue = B00001000; break;
    }
    setFramePixelValue(rightFrame, 3, onValue, config.redPixels[i]);
  }

  for (int i = 43; i < 46; i++) {
    // i = 43, 44, 45 (3 first left pixels)
    int onValue;
    switch (i) {
      case 43: onValue = B00100000; break;
      case 44: onValue = B01000000; break;
      case 45: onValue = B10000000; break;
    }

    setFramePixelValue(leftFrame, 3, onValue, config.redPixels[i]);
  }

  for (int i = 46; i < 82; i++) {
    // Left side. As there are 86 red pixels, we need an offset of 2
    // to apply masks by byte
    int byteNumber;
    byteNumber = (i+2) / 4;
    int bitNumber;
    bitNumber = (i+2) & B00000011;

    int frameByteNumber;
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
    // else {
    //   printLine("ERROR: unhandled byte for red pixels ", byteNumber);
    // }
  }

  for (int i = 82; i < 85; i++) {
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



  // printLine(
  //   "i: ", i,
  //   " => byte: ", byteNumber, ", ", bitNumber, " => ",
  //   frameByteNumber, ", ",
  //   byteToString(leftFrame[frameByteNumber]), ":",
  //   byteToString(rightFrame[frameByteNumber])
  // );



  // BLEU PIXELS
  // printLine("Blue pixels: ");
  // for (int i = 0; i < 16; i++) {
  //   printLine(
  //     i, ": ", byteToString(config.bluePixels[i])
  //   );
  // }
  // The top middle pixel (i=0) is shared between two sides so it must be
  // set on both frames to be at full light
  setFramePixelValue(rightFrame, 15, B00000100, config.bluePixels[0]);
  setFramePixelValue(leftFrame, 15, B00000100, config.bluePixels[0]);

  for (int i = 1; i < 6; i++) {
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

  for (int i = 6; i < 14; i++) {
    int onValue;
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

  for (int i = 14; i < 16; i++) {
    int onValue;
    switch (i-14) {
      case 0: onValue = B00000100; break;
      case 1: onValue = B00100000; break;
    }
    setFramePixelValue(rightFrame, 13, onValue, config.bluePixels[i]);
  }

  for (int i = 16; i < 52; i++) {
    int byteNumber;
    byteNumber = (i-12) / 4;
    int bitNumber;
    bitNumber = (i-12) & B00000011;

    int frameByteNumber;
    frameByteNumber = 11 - byteNumber;
    frameByteNumber += 2; // skip blue only bytes
    // Blue bytes are B01010101
    int onValue = B00000001 << (bitNumber * 2);
    setFramePixelValue(rightFrame, frameByteNumber, onValue, config.bluePixels[i]);
  }

  for (int i = 52; i < 54; i++) {
    int onValue;
    switch (i) {
      case 52: onValue = B00000001; break;
      case 53: onValue = B00010000; break;
    }
    setFramePixelValue(rightFrame, 3, onValue, config.bluePixels[i]);
  }

  for (int i = 54; i < 62; i++) {
    int onValue;
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

  for (int i = 62; i < 68; i++) {
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

  // LEFT SIDE
  for (int i = 68; i < 74; i++) {
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
  setFramePixelValue(leftFrame, 0, B00000001, config.bluePixels[70]);

  for (int i = 74; i < 82; i++) {
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

  for (int i = 82; i < 84; i++) {
    int onValue;
    switch (i) {
      case 82: onValue = B00010000; break;
      case 83: onValue = B00000001; break;
    }
    setFramePixelValue(leftFrame, 3, onValue, config.bluePixels[i]);
  }

  for (int i = 84; i < 120; i++) {
    int byteNumber;
    byteNumber = (i-4) / 4;
    int bitNumber;
    bitNumber = (i-4) & B00000011;

    int frameByteNumber;
    frameByteNumber = byteNumber - 16;
    // Blue bytes are B01010101
    int onValue = B01000000 >> (bitNumber * 2);
    setFramePixelValue(leftFrame, frameByteNumber, onValue, config.bluePixels[i]);
  }

  for (int i = 120; i < 122; i++) {
    int onValue;
    switch (i) {
      case 120: onValue = B00100000; break;
      case 121: onValue = B00000100; break;
    }
    setFramePixelValue(leftFrame, 13, onValue, config.bluePixels[i]);
  }

  for (int i = 122; i < 130; i++) {
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

  for (int i = 130; i < 135; i++) {
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

  /**/
}

// void setFramePixelValue(int frameSide[8][16], int byteNumber, int bitMask, int value) {
void setFramePixelValue(uint8_t frameSide[8][16], int byteNumber, int bitMask, int value) {

  for (int greyLevel = 0; greyLevel < SHADE_OF_GREY_COUNT; greyLevel++) {
    if (value > greyLevel) { // ints are for shades of gray (later)
      // frameSide[byteNumber] |= bitMask;
      frameSide[greyLevel][byteNumber] |= bitMask;
    }
    else {
      // frameSide[byteNumber] &= bitMask ^ B11111111;
      frameSide[greyLevel][byteNumber] &= bitMask ^ B11111111;
    }
  }
}


void stopHandlingSerialCommand() {
  Serial.println("stopHandlingSerialCommand");
}






String byteToString(int value) {
  String out;
  for (int i = 7; i >= 0; i--) {
      bool b = bitRead(value, i);
      out += b ? '1' : '0';
  }

  return out;
}

int parseBitsOrInt(String serialString) {
  int length = serialString.length();
  int result = 0;
  if (length <= 3) {
    result = serialString.toInt();
  }
  else if (length == 8) {
    for(int i = 0; i < length; i++ ) {
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















// !!!!!!!!!!!!!!!! LEGACY CODE


/** /

int startingTime = millis();
int startingPeriodTime = millis();
int lastHighVoltage = millis();

int serial1val = 0;




bool display = true; // left
int spiPeriod = 999; // ms
int lat = 2; // loops
int latPeriodCounter = 0; // periods

int animationPeriod = 5; // ms
int animationPeriodCounter = 0; // periods

int middleLightCounter = 0;
int loopsPerMsCounter = 0;
int loopsPerMs = 0;
int loopsPerPeriodCounter = 0;

void loopBak() {

  // readPins();
  // Serial1.println(0);
    // Serial1.println(serial1val);
  // SPI.transfer(255);


  // clockEveryMS(digitalPin11, 1, false);
  // clockEveryMS(digitalPin11, 300, true);

  if(Serial.available() > 0) {
    // String configString = Serial.readStringUntil(':');
    // frameValues[0] = parseBitsOrInt(configString);

    frameLength = 0;
    while (String numericValue = Serial.readStringUntil(':')) {
      // frameValues[i] = Serial.parseInt();
      // ;
      if (numericValue == "") {
        break;
      }
      frameValues[frameLength] = parseBitsOrInt(numericValue);
      frameSentValues[frameLength] = frameValues[frameLength];
      frameLength++;
    }

    serialInput = Serial.readStringUntil('\n');

    // analogWrite(digitalPin3, frameValues[0]);    // J_LAT  ??    vert  980Hz

    printLine(
      "Period: ", frameValues[0],
      "\nLat: ", frameValues[1],
      "\nLength: ", frameLength - frameStart
    );

    spiPeriod = frameValues[0];
    lat = frameValues[1];

    // for (int i = 1; i < frameLength; i++) {
    //   printLine(
    //     i, ": ", frameValues[i]
    //   );
    //   // Serial.print(i);
    //   // Serial.print(frameValues[i]);
    //   // Serial.print(", ");
    // }

    // SPI.transfer(&frameSentValues[1], frameLength);

    // analogWrite(digitalPin3, 1);    // J_LAT  ??    vert  980Hz
    // analogWrite(digitalPin6, lat);    // J_LAT  ??    vert  980Hz
  }


  int loopTime = millis();
  int timeDiff = loopTime - startingPeriodTime;
  loopsPerMsCounter++;
  if (timeDiff > 1) {
    loopsPerMs = loopsPerMsCounter;
    loopsPerMsCounter = 0;
    startingPeriodTime = loopTime;
  }

  loopsPerPeriodCounter++;
  if (loopsPerPeriodCounter == spiPeriod) {
    loopsPerPeriodCounter = 0;
  }

  // if (timeDiff > spiPeriod || spiPeriod == 0 || loopsPerMsCounter <= 1) {
  if (loopsPerMsCounter <= spiPeriod) {
    // printLine("Diff: ", loopTime - startingPeriodTime, " ms");
    // printLine("leftSide: ", leftSide);
    // startingPeriodTime = loopTime;
    // printLine(startingPeriodTime, "ms");
    int mask=0;
    int mask2;
    // if (displaySide == DISPLAY_LEFT) {
    //   displaySide == DISPLAY_RIGHT;
    //   mask = 191;
    //   mask2 = 170; // only red or blue
    // }
    // else if (displaySide == DISPLAY_RIGHT) {
    //   displaySide == DISPLAY_MIDDLE;
    //   mask = 223;
    //   mask2 = 85; // only red or blue
    // }
    // else if (displaySide == DISPLAY_MIDDLE) {
      displaySide == DISPLAY_LEFT;
      for (int i = 0; i < 16; i++) {
        int valueToTransfer = middleFrame[i];
        if (i == 0 || i == 1 || i == 15) {
          valueToTransfer = middleFrame[i] & frameValues[i + frameStart];
        }
        SPI.transfer(valueToTransfer);
      }
      return;
    // }


    // if (latPeriodCounter < lat / 2) {
    //   digitalWrite(digitalPin6, LOW);
    //   // digitalWrite(digitalPin6, HIGH);
    // }
    // else if (latPeriodCounter >= lat / 2) {
    //   // digitalWrite(digitalPin6, LOW);
    //   digitalWrite(digitalPin6, HIGH);
    // }

    // latPeriodCounter++;
    // if (latPeriodCounter >= lat) {
    //   latPeriodCounter = 0;
    // }




    // frameSentValues[1] = frameValues[1] & mask;
    // frameSentValues[5] = frameValues[5] & mask2;
    // frameSentValues[9] = frameValues[9] & mask2;
    // SPI.transfer(&frameSentValues[1], frameLength-1);


    for (int i = frameStart; i < frameLength; i++) {


      int valueToTransfer = frameValues[i];
      if (i == frameStart + 0) {
        valueToTransfer &= mask;
      }

      if (i == frameStart + 4 || i == frameStart + 8 ) {
        valueToTransfer &= mask2;
      }

      // Serial.print(i);
      // Serial.print(": ");
      // Serial.print(valueToTransfer);
      // Serial.print(", ");

      SPI.transfer(valueToTransfer);
    }
    // Serial.print("\n");
    // printLine(middleLightCounter); // 64/ms
    middleLightCounter = 0;
  }
  // else {
  //   middleLightCounter++;
  //   if (middleLightCounter == 350) {
  //     for (int i = 0; i < 16; i++) {
  //       int valueToTransfer = middleFrame[i];
  //       if (i == 0 || i == 1 || i == 15) {
  //         valueToTransfer = middleFrame[i] & frameValues[i + frameStart];
  //       }
  //       SPI.transfer(valueToTransfer);
  //     }
  //   }
  // }


    // https://makersportal.com/blog/2019/12/15/controlling-arduino-pins-from-the-serial-monitor

  /*
  if (Serial.available()){
    char in_char = Serial.read();
    int in_int = (int)in_char;
    in_int =  in_int * 2;
    // Serial.println(in_char);
    printLine(
      // "Printing: A", in_char, " to J_LAT "
      "Printing: ", in_char, " / ", in_int, " to J_LAT "
    );
    // analogWrite(digitalPin3, in_int); // J_LAT 980Hz
    // digitalWrite(digitalPin3, in_int); // J_LAT 980Hz
    // analogWrite(digitalPin6, in_int); // J_LAT 980Hz


    // digitalWrite(digitalPin6, HIGH); // J_LAT 980Hz
    // delayMicroseconds(1020); // 1 / 980s
    // digitalWrite(digitalPin6, in_int); // J_LAT 980Hz
    if (digitalPinsState[digitalPin6] == HIGH) {
      // printLine("digital pin:", i, " => ", digitalPinsState[i]);
      digitalPinsState[digitalPin6] = LOW;
    }
    else {
      digitalPinsState[digitalPin6] = HIGH;
    }
    digitalWrite(digitalPin6, digitalPinsState[digitalPin6]);

  }
  else {
    // delayMicroseconds(1020); // 1 / 980s
    // analogWrite(digitalPin3, 0); // J_LAT 980Hz
  }
}



/**/
