#include <LedControl.h>
#include <NESpad.h>

// put your own strobe/clock/data pin numbers here -- see the pinout in readme.txt
NESpad nintendo = NESpad(2,3,4);

// Pins: DIN,CLK,CS, # of Display connected
LedControl lc = LedControl(12, 11, 10, 2); 

#define POT_PIN 0

#define DEBUG 1

#define INTENSITY 0

#define EYE_LEFT 0
#define EYE_RIGHT 1

#define NUM_EYES 2
#define NUM_COLS 8
#define NUM_ROWS 8

#define MARGIN_X 1
#define MARGIN_Y 1

#define PUPIL_COLS 2
#define PUPIL_ROWS 2

#define MIN_PUPIL_X 0
#define MAX_PUPIL_X NUM_COLS - PUPIL_COLS - MARGIN_X * 2

#define MIN_PUPIL_Y 0
#define MAX_PUPIL_Y NUM_ROWS - PUPIL_ROWS - MARGIN_Y * 2

#define MIN_EYELID 0
#define MAX_EYELID NUM_ROWS

#define CENTER_PUPIL_X 2
#define CENTER_PUPIL_Y 2

#define MIN_INTENSITY 1
#define MAX_INTENSITY 15

#define LOOP_DELAY 25

#define MAX_IDLE_TIME 3000
#define MIN_IDLE_MOVE_DELAY 200
#define MAX_IDLE_MOVE_DELAY 1000

#define IDLE_BLINK_CHANCE 7

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
  NES_UP, NES_DOWN, NES_LEFT, NES_RIGHT,
  NES_UP | NES_LEFT, NES_UP | NES_RIGHT,
  NES_DOWN | NES_LEFT, NES_DOWN | NES_RIGHT,
  NES_LEFT | NES_START, NES_RIGHT | NES_START
};

// Display buffer for rendering eyes
byte eyes[NUM_EYES + 1][NUM_ROWS + 1];

// Prerendered positions of the eyeball & pupil
byte positions[MAX_PUPIL_X + 1][MAX_PUPIL_Y + 1][NUM_ROWS + 1];

#define X_POS 0
#define Y_POS 1
#define EYELID_POS 2

int current_pos[NUM_EYES][3];
int target_pos[NUM_EYES][3];
int min_pos[] = { MIN_PUPIL_X, MIN_PUPIL_Y, MIN_EYELID };
int max_pos[] = { MAX_PUPIL_X, MAX_PUPIL_Y, MAX_EYELID };
int scale_pos[] = { 1, 1, 3 };

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
  pot = map(analogRead(POT_PIN), 0, 1023, 0, 15);
  setDisplayIntensity(pot);

  pad = nintendo.buttons();
  idle_time = (pad != 0) ? 0 : idle_time + LOOP_DELAY;

  if (idle_time > MAX_IDLE_TIME) {
    idle_move_delay -= LOOP_DELAY;
    if (idle_move_delay <= 0) {
      setRandomIdlePad();
    }
    pad = idle_pad;
  }
  
  setTargetsFromPad(pad);
  animateToTargets();
  updateDisplay();

  delay(LOOP_DELAY);
}

byte setRandomIdlePad() {
  idle_pad = idlePads[random(0, sizeof(idlePads))];
  idle_move_delay = random(MIN_IDLE_MOVE_DELAY, MAX_IDLE_MOVE_DELAY);
  
  // Blink quickly, every now and then
  if (random(0, 100) < IDLE_BLINK_CHANCE) {
    idle_pad |= NES_B | NES_A;
    idle_move_delay = 100;
  }
}

void setTargetsFromPad(byte pad) {

  target_pos[EYE_LEFT][EYELID_POS] = (pad & NES_B) ? MAX_EYELID : MIN_EYELID;
  target_pos[EYE_RIGHT][EYELID_POS] = (pad & NES_A) ? MAX_EYELID : MIN_EYELID;
  
  if (pad & NES_UP) {
    target_pos[EYE_LEFT][Y_POS] = MIN_PUPIL_Y;
    target_pos[EYE_RIGHT][Y_POS] = MIN_PUPIL_Y;
  } else if (pad & NES_DOWN) {
    target_pos[EYE_LEFT][Y_POS] = MAX_PUPIL_Y;
    target_pos[EYE_RIGHT][Y_POS] = MAX_PUPIL_Y;
  } else {
    target_pos[EYE_LEFT][Y_POS] = CENTER_PUPIL_Y;
    target_pos[EYE_RIGHT][Y_POS] = CENTER_PUPIL_Y;
  }
  
  if (pad & NES_LEFT) {
    target_pos[EYE_LEFT][X_POS] = MIN_PUPIL_X;
    target_pos[EYE_RIGHT][X_POS] = MIN_PUPIL_X;
  } else if (pad & NES_RIGHT) {
    target_pos[EYE_LEFT][X_POS] = MAX_PUPIL_X;
    target_pos[EYE_RIGHT][X_POS] = MAX_PUPIL_X;
  } else {
    target_pos[EYE_LEFT][X_POS] = CENTER_PUPIL_X;
    target_pos[EYE_RIGHT][X_POS] = CENTER_PUPIL_X;
  }

  if (pad & NES_START) {
    target_pos[EYE_RIGHT][X_POS] = abs(4 - target_pos[EYE_RIGHT][X_POS]);
  }

  if (pad & NES_SELECT) {
    target_pos[EYE_RIGHT][Y_POS] = abs(4 - target_pos[EYE_RIGHT][Y_POS]);
  }
  
}

void setDisplayIntensity(int intensity) {
  for (i=0; i<NUM_EYES; i++) {
    lc.setIntensity(i, intensity);
  }  
}

void animateToTargets() {
  for (int eye_idx=0; eye_idx<NUM_EYES; eye_idx++) {
    for (int k=0; k<3; k++) {
      current = current_pos[eye_idx][k];
      target = target_pos[eye_idx][k];
      if (current != target) {
        current += (current < target) ? scale_pos[k] : 0-scale_pos[k];
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
    current_pos[i][X_POS] = CENTER_PUPIL_X;
    current_pos[i][Y_POS] = CENTER_PUPIL_Y;
    current_pos[i][EYELID_POS] = 0;
    target_pos[i][X_POS] = CENTER_PUPIL_X;
    target_pos[i][Y_POS] = CENTER_PUPIL_Y;
    target_pos[i][EYELID_POS] = 0;
  }  
}

// Set the eyeball by copying a prerendered position 
void copyEyeballPosition(int idx, int x, int y) {
  memcpy(eyes[idx], positions[x][y], NUM_ROWS);
}

// Fill the set of prerendered positions for all x/y pairs
// Probably a gross over-optimization, but kind of hoping
// doing less math on every eye position will save battery life
void prerenderPositions() {
  for (int x=0; x<=MAX_PUPIL_X; x++) {
    for (int y=0; y<=MAX_PUPIL_Y; y++) {
      renderPosition(x, y);
    }
  }
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

// Set the LED contents from the display buffers
void updateDisplay() {
  for (int i=0; i<NUM_EYES; i++) {
    copyEyeballPosition(i, current_pos[i][X_POS], current_pos[i][Y_POS]);
    renderEyelid(i, current_pos[i][EYELID_POS]);
  }
  for (int j=0; j<NUM_ROWS; j++) {
    for (int i=0; i<NUM_EYES; i++) {
      lc.setRow(i, j, eyes[i][j]);
    }
  }
}
