#include <LedControl.h>
#include <NESpad.h>

// put your own strobe/clock/data pin numbers here -- see the pinout in readme.txt
NESpad nintendo = NESpad(2,3,4);

// Pins: DIN,CLK,CS, # of Display connected
LedControl lc = LedControl(12, 11, 10, 2); 

#define POT_PIN 0

#define DEBUG 1

#define INTENSITY 0
#define INTENSITY_MIN 1
#define INTENSITY_MAX 15

#define EYE_LEFT 0
#define EYE_RIGHT 1

#define NUM_EYES 2
#define NUM_COLS 8
#define NUM_ROWS 8

#define MARGIN_X 1
#define MARGIN_Y 1

#define PUPIL_COLS 2
#define PUPIL_ROWS 2

#define PUPIL_X_MIN 0
#define PUPIL_X_MAX NUM_COLS - PUPIL_COLS - MARGIN_X * 2

#define PUPIL_Y_MIN 0
#define PUPIL_Y_MAX NUM_ROWS - PUPIL_ROWS - MARGIN_Y * 2

#define PUPIL_X_CENTER 2
#define PUPIL_Y_CENTER 2

#define EYELID_MIN 0
#define EYELID_MAX NUM_ROWS

#define LOOP_DELAY 25

#define IDLE_TIME_MAX 2000
#define IDLE_MOVE_DELAY_MIN 200
#define IDLE_MOVE_DELAY_MAX 1000

#define IDLE_BLINK_CHANCE 15

// Templates for pupil and eyeball
const byte template_pupil = B11000000;
const byte template_eye[8] = {
  B00111100,
  B01111110,
  B11111111,
  B11111111,
  B11111111,
  B11111111,
  B01111110,
  B00111100
};

byte idlePads[] = {
  0,
  NES_UP,
  NES_DOWN,
  NES_LEFT,
  NES_RIGHT,
  NES_UP | NES_LEFT,
  NES_UP | NES_RIGHT,
  NES_DOWN | NES_LEFT,
  NES_DOWN | NES_RIGHT,
  NES_RIGHT | NES_START,
  NES_UP | NES_RIGHT | NES_START,
  NES_DOWN | NES_RIGHT | NES_START
};

// Display buffer for rendering eyes
byte eyes[NUM_EYES + 1][NUM_ROWS + 1];

// Prerendered positions of the eyeball & pupil
byte positions[PUPIL_X_MAX + 1][PUPIL_Y_MAX + 1][NUM_ROWS + 1];

#define POS_X 0
#define POS_Y 1
#define POS_EYELID 2

int current_pos[NUM_EYES][3];
int target_pos[NUM_EYES][3];
int min_pos[] = { PUPIL_X_MIN, PUPIL_Y_MIN, EYELID_MIN };
int max_pos[] = { PUPIL_X_MAX, PUPIL_Y_MAX, EYELID_MAX };
int step_pos[] = { 1, 1, 3 };

int i, current, target, pot, pad, idle_pad, idle_time, idle_move_delay;

void setup() {
  #ifdef DEBUG
    Serial.begin(9600);
    while (!Serial) { }
    Serial.println("STARTING UP");
  #endif

  idle_time = 0;
  idle_move_delay = 0;
  prerenderPositions();
  resetDisplay();
  updateDisplay();
}

void loop() {

  // Adjust intensity based on the potentiometer position
  pot = map(analogRead(POT_PIN), 0, 1023, 0, 15);
  setDisplayIntensity(pot);

  // Grab gamepad state
  pad = nintendo.buttons();

  // If the gamepad is entirely neutral, increment the idle timer.
  idle_time = (pad != 0) ? 0 : idle_time + LOOP_DELAY;

  // If we're past the max idle time, start performing idle animations
  if (idle_time >= IDLE_TIME_MAX) {
    
    // Constrain the idle timer to max value, so we don't
    // just keep incrementing and overflow the variable
    idle_time = IDLE_TIME_MAX;
    
    // Tick down the delay timer until the next idle move.
    idle_move_delay -= LOOP_DELAY;
    
    if (idle_move_delay <= 0) {
      setRandomIdlePad();
    }

    // Set the idle animation pad state as if it were user input
    pad = idle_pad;
    
  }

  setTargetsFromPad(pad);
  animateToTargets();
  updateDisplay();

  delay(LOOP_DELAY);
}

byte setRandomIdlePad() {
  idle_pad = idlePads[random(0, sizeof(idlePads))];
  idle_move_delay = random(IDLE_MOVE_DELAY_MIN, IDLE_MOVE_DELAY_MAX);
  
  // Blink quickly, every now and then
  if (random(0, 100) < IDLE_BLINK_CHANCE) {
    idle_pad |= NES_B | NES_A;
    idle_move_delay = 100;
  }
}

void setTargetsFromPad(byte pad) {

  // B & A close left & right eyelids respectively
  target_pos[EYE_LEFT][POS_EYELID] = (pad & NES_B) ? EYELID_MAX : EYELID_MIN;
  target_pos[EYE_RIGHT][POS_EYELID] = (pad & NES_A) ? EYELID_MAX : EYELID_MIN;

  // Vertical eye movement on up/down/center
  target_pos[EYE_LEFT][POS_Y] = target_pos[EYE_RIGHT][POS_Y] =
    (pad & NES_UP)   ? PUPIL_Y_MIN :
    (pad & NES_DOWN) ? PUPIL_Y_MAX :
                       PUPIL_Y_CENTER;

  // Horizontal eye movement on left/right/center
  target_pos[EYE_LEFT][POS_X] = target_pos[EYE_RIGHT][POS_X] =
    (pad & NES_LEFT)  ? PUPIL_X_MIN :
    (pad & NES_RIGHT) ? PUPIL_X_MAX :
                        PUPIL_X_CENTER;

  // Start button inverts horizontal axis - e.g. for crossed eyes
  if (pad & NES_START) {
    target_pos[EYE_RIGHT][POS_X] = abs(PUPIL_X_MAX - target_pos[EYE_RIGHT][POS_X]);
  }

  // Select button inverts vertical axis - e.g. for crazy eyes
  if (pad & NES_SELECT) {
    target_pos[EYE_RIGHT][POS_Y] = abs(PUPIL_Y_MAX - target_pos[EYE_RIGHT][POS_Y]);
  }
  
}

// Move current eye & eyelid positions one step toward target positions
void animateToTargets() {
  for (int eye_idx=0; eye_idx<NUM_EYES; eye_idx++) {
    for (int k=0; k<3; k++) {
      current = current_pos[eye_idx][k];
      target = target_pos[eye_idx][k];
      if (current != target) {
        current += (current < target) ? step_pos[k] : 0-step_pos[k];
        current_pos[eye_idx][k] = constrain(current, min_pos[k], max_pos[k]);
      }
    }
  }
}

// Render "eyelid" in display buffer by blacking out rows
void renderEyelid(int idx, int y) {
  for (int i=0; i<y; i++) {
    eyes[idx][i] = 0;
  }
}

// Ensure the displays are on, intense, and cleared.
void resetDisplay() {
  for (int i=0; i<NUM_EYES; i++) {
    lc.shutdown(i, false);
    lc.setIntensity(i, INTENSITY);
    lc.clearDisplay(i);
    current_pos[i][POS_X] = target_pos[i][POS_X] = PUPIL_X_CENTER;
    current_pos[i][POS_Y] = target_pos[i][POS_Y] = PUPIL_Y_CENTER;
    current_pos[i][POS_EYELID] = target_pos[i][POS_EYELID] = 0;
  }  
}

// Set the eyeball by copying a prerendered position 
void copyEyeballPosition(int idx, int x, int y) {
  memcpy(eyes[idx], positions[x][y], NUM_ROWS);
}

// Render one eyeball by copying in blank template and 
// blacking out the pupil at correct location
void renderPosition(int x, int y) {
  // Copy in the blank eyeball template
  memcpy(positions[x][y], template_eye, NUM_ROWS);

  // Use binary right shift to adjust pupil to x position
  byte pupil = template_pupil >> (x + MARGIN_X);

  // Use XOR to black out the pupil in the appropriate rows
  int start_y = y + MARGIN_Y;
  int end_y = start_y + PUPIL_ROWS;
  for (int i=start_y; i<end_y; i++) {
    positions[x][y][i] ^= pupil;
  }
}

// Fill the set of prerendered positions for all x/y pairs
// Probably a gross over-optimization, but kind of hoping
// doing less math on every eye position will save battery life
void prerenderPositions() {
  for (int x=0; x<=PUPIL_X_MAX; x++) {
    for (int y=0; y<=PUPIL_Y_MAX; y++) {
      renderPosition(x, y);
    }
  }
}

// Set display intensity
void setDisplayIntensity(int intensity) {
  for (i=0; i<NUM_EYES; i++) {
    lc.setIntensity(i, intensity);
  }  
}

// Set the LED contents from the display buffers
void updateDisplay() {
  for (int i=0; i<NUM_EYES; i++) {
    copyEyeballPosition(i, current_pos[i][POS_X], current_pos[i][POS_Y]);
    renderEyelid(i, current_pos[i][POS_EYELID]);
  }
  for (int j=0; j<NUM_ROWS; j++) {
    for (int i=0; i<NUM_EYES; i++) {
      lc.setRow(i, j, eyes[i][j]);
    }
  }
}
