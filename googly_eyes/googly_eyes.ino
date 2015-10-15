#include "LedControl.h"

#define DEBUG 1

#define MOVE_EYE_DELAY 200
#define BLINK_EYE_DELAY 10
#define BLINK_EYE_CHANCE 5

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

LedControl lc = LedControl(12, 11, 10, 2); // Pins: DIN,CLK,CS, # of Display connected

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

// Display buffer for rendering eyes
byte eyes[NUM_EYES + 1][NUM_ROWS + 1];

byte positions[MAX_PUPIL_X + 1][MAX_PUPIL_Y + 1][NUM_ROWS + 1];

int intensity, pupil_x, pupil_y, eyelid_y;

void setup() {
  #ifdef DEBUG
    Serial.begin(9600);
    while (!Serial) { }
    Serial.println("STARTING UP");
  #endif

  prerenderPositions();
  
  intensity = 0;
  pupil_x = 0;
  pupil_y = 0;
  eyelid_y = 0;

  for (int i=0; i<NUM_EYES; i++) {
    for (int j=0; j<NUM_ROWS; j++) {
      eyes[i][j] = 0;
    }
    lc.shutdown(i, false);
    lc.setIntensity(i, intensity);
    lc.clearDisplay(i);
  }
  
}

void loop() {

  pupil_x += random(-1, 2);
  pupil_x = constrain(pupil_x, MIN_PUPIL_X, MAX_PUPIL_X);
  pupil_y += random(-1, 2);
  pupil_y = constrain(pupil_y, MIN_PUPIL_Y, MAX_PUPIL_Y);

  // eyelid_y = constrainInt(pupil_y + random(-1, 2), MIN_EYELID_Y, MAX_EYELID_Y);
  // intensity = constrainInt(intensity + random(-1, 2), MIN_INTENSITY, MAX_INTENSITY);

  for (int i=0; i<NUM_EYES; i++) {
    lc.setIntensity(i, intensity);
    renderEyeball(i, pupil_x, pupil_y);
    renderEyelid(i, eyelid_y);
  }

  displayEyes();

  if (random(0, 100) < BLINK_EYE_CHANCE) {
    blinkEyes();
  }

  delay(MOVE_EYE_DELAY);
}

void blinkEyes() {
  for (int j=eyelid_y; j<NUM_ROWS; j++) {
    for (int i=0; i<NUM_EYES; i++) {
      renderEyeball(i, pupil_x, pupil_y);
      renderEyelid(i, j);
    }
    displayEyes();
    delay(BLINK_EYE_DELAY);
  }
  for (int j=NUM_ROWS-1; j>=eyelid_y; j--) {
    for (int i=0; i<NUM_EYES; i++) {
      renderEyeball(i, pupil_x, pupil_y);
      renderEyelid(i, j);
    }
    displayEyes();
    delay(BLINK_EYE_DELAY);
  } 
}

void renderEyeball(int idx, int x, int y) {
  for (int i=0; i<NUM_ROWS; i++) {
    eyes[idx][i] = positions[x][y][i];
  }
}

// Render "eyelid" in display buffer by blacking out rows
void renderEyelid(int idx, int y) {
  for (int i=0; i<y; i++) {
    eyes[idx][i] = 0;
  }
}

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
  for (int i=0; i<NUM_ROWS; i++) {
    positions[x][y][i] = template_eye[i];
  }

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
void displayEyes() {
  for (int j=0; j<NUM_ROWS; j++) {
    for (int i=0; i<NUM_EYES; i++) {
      lc.setRow(i, j, eyes[i][j]);
    }
  }
}