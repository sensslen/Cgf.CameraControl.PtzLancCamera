#include <LibLanc.h>
#include <TimerOne.h>

Lanc lanc(13, 12);
#define PAN_LEFT_PIN  5
#define PAN_RIGHT_PIN  3
#define TILT_UP_PIN  6
#define TILT_DOWN_PIN  9

typedef struct sPwmState {
  bool pinState;
  uint8_t value;
} tPwmState;

typedef void (*tReceptionState)(uint8_t revieced);

void Reception_WaitForStart(uint8_t received);
void Reception_Receiving(uint8_t received);
void Reception_ReceptionComplete(uint8_t received);

uint8_t receiveString[6];
uint8_t receivePosition;
tReceptionState receptionState;
int lastCommandRequest;

tPwmState panLeftState = {false, 0};
tPwmState panRightState = {false, 0};
tPwmState tiltUpState =  {false, 0};
tPwmState tiltDownState =  {false, 0};
uint8_t pwmCounter = 0;

void setup(void)
{
  Serial.begin(500000);

  pinMode(PAN_LEFT_PIN, OUTPUT);
  pinMode(PAN_RIGHT_PIN, OUTPUT);
  pinMode(TILT_UP_PIN, OUTPUT);
  pinMode(TILT_DOWN_PIN, OUTPUT);

  Timer1.initialize(200);
  Timer1.attachInterrupt(panTiltExecute); // blinkLED to run every 0.15 seconds

  lanc.begin();

  receptionState = Reception_WaitForStart;
  lastCommandRequest = 0;

  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB
  }
}

void loop(void)
{
  getCurrentCommand();
  lanc.loop();
}

void requestCommand() {
  lastCommandRequest = millis();
  Serial.println("v1");
}

/**
   Ask the counterpart connected via Serial port for the next commands to execute.
   Commands are formatted as CSV and should follow this pattern:
   |     Pan     |    Tilt     |   Zoom  |  Focus  |
   | -255 .. 255 | -255 .. 255 | -8 .. 8 | -1 .. 1 |

   This function processes the input and issues the according commands

   @note Please note that focus commands override zoom commands
*/
void getCurrentCommand() {
  // read things
  while (Serial.available() > 0) {
    receptionState((uint8_t)Serial.read());
  }

  // process when receptionComplete
  if (receptionState == Reception_ReceptionComplete) {
    processPan();
    processTilt();
    processZoom();
    processFocus();
    receptionState = Reception_WaitForStart;
    requestCommand();
  }

  // get next command if last reception is too long waited for
  if ((millis() - lastCommandRequest) > 1000) {
    requestCommand();
  }
}

int decodePanTilt(uint8_t * data) {
  uint8_t lowValue = data[1] - '@';
  uint8_t highValue = data[0] - '@';
  bool negative = highValue & (1 << 4);
  int retval = ((highValue % 16) << 5) + lowValue;
  return (negative) ? -retval : retval;
}

int decodeZoom(uint8_t * data) {
  uint8_t value = data[0] - '@';
  bool negative = (value & (1 << 4)) > 0;
  int retval = value % 16;
  return (negative) ? -retval : retval;
}

void decodeFocus(uint8_t * data, int * focusValue, bool * toggleAutofocus) {
  uint8_t value = data[0] - '@';
  bool negative = (value & (1 << 4)) > 0;
  *focusValue = (negative) ? value & (1 << 0) : value & (1 << 0);
  * toggleAutofocus = (value & (1 << 1)) != 0;
}

void processPan() {
  int value = decodePanTilt(&receiveString[0]);

  if (value < 0) {
    panLeftState.value = 0;
    panRightState.value = -value;
  } else {
    panRightState.value = 0;
    panLeftState.value = value;
  }
}

void processTilt() {
  int value = decodePanTilt(&receiveString[2]);

  if (value < 0) {
    tiltUpState.value = 0;
    tiltDownState.value = -value;
  } else {
    tiltDownState.value = 0;
    tiltUpState.value = value;
  }
}

void processZoom() {
  int value = decodeZoom(&receiveString[4]);

  // A value of zero will already clear the command
  // therefore no need here to clear manually
  lanc.Zoom(value);
}

void processFocus() {
  bool toggleAutofocus = false;
  int focusValue = 0;
  decodeFocus(&receiveString[5], &focusValue, &toggleAutofocus);

  if (toggleAutofocus) {
    lanc.AutoFocus();
  } else if (focusValue > 0) {
    lanc.Focus(false);
  } else if (focusValue < 0) {
    lanc.Focus(true);
  }
  // Don't clear command. This command unfortunately
  // overwrites the Zoom command since it is processed later.
  // (unfortunately the lanc protocol does not allow simultanous operation
  // of both commands)
}

void Reception_WaitForStart(uint8_t received) {
  if (received == '?') {
    receptionState = Reception_Receiving;
    receivePosition = 0;
  }
}
void Reception_Receiving(uint8_t received) {
  if (received == '\n') {
    receptionState = Reception_ReceptionComplete;
  } else if (receivePosition < (sizeof(receiveString) / sizeof(receiveString[0]))) {
    receiveString[receivePosition] = received;
    receivePosition ++;
  }
}
void Reception_ReceptionComplete(uint8_t received) {
  (void) received;
  // do nothing intentionally
}

inline void executePwm(tPwmState * state, int pin) {
  bool newState = state->value > pwmCounter;
  if (state->pinState != newState) {
    state->pinState = newState;
    digitalWrite(pin, state->pinState);
  }
}

void panTiltExecute(void)
{
  pwmCounter ++;
  executePwm(&panLeftState, PAN_LEFT_PIN);
  executePwm(&panRightState, PAN_RIGHT_PIN);
  executePwm(&tiltUpState, TILT_UP_PIN);
  executePwm(&tiltDownState, TILT_DOWN_PIN);
}
