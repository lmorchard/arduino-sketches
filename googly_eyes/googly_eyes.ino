#include "LedControl.h"

#define DEBUG 1

#define INTENSITY 0

#define MOVE_EYE_DELAY 200
#define BLINK_EYE_DELAY 15
#define BLINK_EYE_CHANCE 5

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

#define MIN_EYELID_Y 0
#define MAX_EYELID_Y 3

#define MIN_INTENSITY 1
#define MAX_INTENSITY 15

// Pins: DIN,CLK,CS, # of Display connected
LedControl lc = LedControl(12, 11, 10, 2); 

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

#define END_OF_ANIM -1

#define WAIT_1 50
#define WAIT_2 100
#define WAIT_3 200
#define WAIT_4 400
#define WAIT_5 800
#define WAIT_LONG 1500

#define BLINK_EYE 16

#define OP_MOVE 10
#define OP_BLINK 20

int ANIM_STARE[][5] = {
  {2,2,2,2,WAIT_2},{2,2,2,2,WAIT_2},{2,2,2,2,WAIT_2},{2,2,2,2,WAIT_2},{2,2,2,2,WAIT_2},
  {2,2,2,2,WAIT_2},{2,2,2,2,WAIT_2},{2,2,2,2,WAIT_2},{2,2,2,2,WAIT_2},{2,2,2,2,WAIT_2},
  {BLINK_EYE,0,BLINK_EYE,0,0},
  {2,2,2,2,WAIT_2},{2,2,2,2,WAIT_2},{2,2,2,2,WAIT_2},{2,2,2,2,WAIT_2},{2,2,2,2,WAIT_2},
  {2,2,2,2,WAIT_2},{2,2,2,2,WAIT_2},{2,2,2,2,WAIT_2},{2,2,2,2,WAIT_2},{2,2,2,2,WAIT_2},
  {BLINK_EYE,0,BLINK_EYE,0,0}, 
  {END_OF_ANIM,0,0,0,0}
};

int ANIM_SIDE_TO_SIDE[][5]  = {
  {2,2,2,2,WAIT_3}, {1,2,1,2,WAIT_3}, {0,2,0,2,WAIT_4}, {1,2,1,2,WAIT_3}, {2,2,2,2,500},
  {3,2,3,2,WAIT_3}, {4,2,4,2,WAIT_4}, {3,2,3,2,WAIT_3}, 
  {2,2,2,2,WAIT_5}, 
  {END_OF_ANIM,0,0,0,0}
};

int ANIM_EYE_ROLL[][5] = {
  {2,2,2,2,WAIT_1}, {1,2,1,2,WAIT_1}, {0,2,0,2,WAIT_1}, {0,1,0,1,WAIT_1}, {0,0,0,0,WAIT_1}, {1,0,1,0,WAIT_1},
  {2,0,2,0,WAIT_1}, {3,0,3,0,WAIT_1}, {4,0,4,0,WAIT_1}, {4,1,4,1,WAIT_1}, {4,2,4,2,WAIT_1}, {3,2,3,2,WAIT_1},
  {2,2,2,2,WAIT_5}, 
  {END_OF_ANIM,0,0,0,0}
};

int ANIM_CROSSED[][5] = {
  {2,2,2,2,WAIT_1}, {3,2,1,2,WAIT_3}, {4,2,0,2,WAIT_LONG}, {3,2,1,2,WAIT_3}, {2,2,2,2,WAIT_LONG}, 
  {END_OF_ANIM,0,0,0,0}
};

int* animations[] = {
  *ANIM_STARE,
  *ANIM_SIDE_TO_SIDE,
  *ANIM_SIDE_TO_SIDE,
  *ANIM_SIDE_TO_SIDE,
  *ANIM_EYE_ROLL,
  *ANIM_CROSSED
};

// Display buffer for rendering eyes
byte eyes[NUM_EYES + 1][NUM_ROWS + 1];

// Prerendered positions of the eyeball & pupil
byte positions[MAX_PUPIL_X + 1][MAX_PUPIL_Y + 1][NUM_ROWS + 1];

void setup() {
  #ifdef DEBUG
    Serial.begin(9600);
    while (!Serial) { }
    Serial.println("STARTING UP");
  #endif
  prerenderPositions();
  resetDisplay();  
}

void loop() {
  runAnimation(animations[random(0, sizeof(animations) / sizeof(int*))]);
}

void runAnimation(int* step_ptr) {
  while (1) {
    int left_x = step_ptr[0];
    int left_y = step_ptr[1];
    int right_x = step_ptr[2];
    int right_y = step_ptr[3];
    int wait = step_ptr[4];
    
    if (left_x == END_OF_ANIM) { 
      break;
    } else if (left_x == BLINK_EYE && right_x == BLINK_EYE) {
      blinkEyes(1, 1);
    } else {
      copyEyeballPosition(EYE_LEFT, left_x, left_y);
      copyEyeballPosition(EYE_RIGHT, right_x, right_y);
      updateDisplay();
    }

    delay(wait);
    step_ptr += 5;
  }
}

// Blink by rapidly dropping and raising the "eyelid"
void blinkEyes(int do_left, int do_right) {
  int y;
  for (int idx = -NUM_ROWS; idx <= NUM_ROWS; idx++) {
    y = NUM_ROWS - abs(idx);
    copyEyeballPosition(EYE_LEFT, 2, 2);
    copyEyeballPosition(EYE_RIGHT, 2, 2);
    if (do_left) { renderEyelid(EYE_LEFT, y); }
    if (do_right) { renderEyelid(EYE_RIGHT, y); }
    updateDisplay();
    delay(BLINK_EYE_DELAY);
  }
}

// Ensure the displays are on, intense, and cleared.
void resetDisplay() {
  for (int i=0; i<NUM_EYES; i++) {
    lc.shutdown(i, false);
    lc.setIntensity(i, INTENSITY);
    lc.clearDisplay(i);
  }  
}

// Set the eyeball by copying a prerendered position 
void copyEyeballPosition(int idx, int x, int y) {
  memcpy(eyes[idx], positions[x][y], NUM_ROWS);
}

// Render "eyelid" in display buffer by blacking out rows
void renderEyelid(int idx, int y) {
  for (int i=0; i<y; i++) {
    eyes[idx][i] = 0;
  }
}

// Fill the set of prerendered positions based on
// all possible x/y pairs
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
  for (int j=0; j<NUM_ROWS; j++) {
    for (int i=0; i<NUM_EYES; i++) {
      lc.setRow(i, j, eyes[i][j]);
    }
  }
}
