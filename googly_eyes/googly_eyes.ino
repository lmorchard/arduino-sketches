#include "LedControl.h"

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

#define OP_MOVE 10
#define OP_BLINK 20
#define OP_WANDER 30
#define OP_END 99

#define WAIT_QUICK 50
#define WAIT_FAST 200
#define WAIT_MED 400
#define WAIT_SLOW 800
#define WAIT_HOLD 2000
#define WAIT_RANDOM -999

#define WANDER_WAIT 100
#define MIN_WANDER_STEPS 10
#define MAX_WANDER_STEPS 25

#define BLINK_WAIT 5

#define RANDOM_WAIT_MIN 750
#define RANDOM_WAIT_MAX 1500

struct AnimationStep {
  int op_left;
  int left_x;
  int left_y;
  int op_right;
  int right_x;
  int right_y;
  int wait;
};

AnimationStep ANIM_STARE[] = {
  {OP_MOVE,2,2,OP_MOVE,2,2,WAIT_RANDOM},
  {OP_BLINK,2,2,OP_BLINK,2,2,WAIT_QUICK},
  {OP_END,0,0,0,0,0,0}
};

AnimationStep ANIM_SIDE_TO_SIDE[] = {
  {OP_MOVE,2,2,OP_MOVE,2,2,WAIT_QUICK},
  {OP_MOVE,1,2,OP_MOVE,1,2,WAIT_QUICK},
  {OP_MOVE,0,2,OP_MOVE,0,2,WAIT_HOLD},
  {OP_MOVE,1,2,OP_MOVE,1,2,WAIT_QUICK},
  {OP_MOVE,2,2,OP_MOVE,2,2,WAIT_QUICK},
  {OP_MOVE,3,2,OP_MOVE,3,2,WAIT_QUICK},
  {OP_MOVE,4,2,OP_MOVE,4,2,WAIT_HOLD},
  {OP_MOVE,3,2,OP_MOVE,3,2,WAIT_QUICK},
  {OP_MOVE,2,2,OP_MOVE,2,2,WAIT_QUICK},
  {OP_END,0,0,0,0,0,0}
};

AnimationStep ANIM_ROLL[] = {
  {OP_MOVE,2,2,OP_MOVE,2,2,WAIT_QUICK},
  {OP_MOVE,1,2,OP_MOVE,1,2,WAIT_QUICK},
  {OP_MOVE,0,2,OP_MOVE,0,2,WAIT_QUICK},
  {OP_MOVE,0,1,OP_MOVE,0,1,WAIT_QUICK},
  {OP_MOVE,0,0,OP_MOVE,0,0,WAIT_QUICK},
  {OP_MOVE,1,0,OP_MOVE,1,0,WAIT_QUICK},
  {OP_MOVE,2,0,OP_MOVE,2,0,WAIT_QUICK},
  {OP_MOVE,3,0,OP_MOVE,3,0,WAIT_QUICK},
  {OP_MOVE,4,0,OP_MOVE,4,0,WAIT_QUICK},
  {OP_MOVE,4,1,OP_MOVE,4,1,WAIT_QUICK},
  {OP_MOVE,4,2,OP_MOVE,4,2,WAIT_QUICK},
  {OP_MOVE,3,2,OP_MOVE,3,2,WAIT_QUICK},
  {OP_MOVE,2,2,OP_MOVE,2,2,WAIT_HOLD},
  {OP_END,0,0,0,0,0,0}
};

AnimationStep ANIM_CROSSED[] = {
  {OP_MOVE,2,2,OP_MOVE,2,2,WAIT_QUICK},
  {OP_MOVE,3,2,OP_MOVE,1,2,WAIT_QUICK},
  {OP_MOVE,4,2,OP_MOVE,0,2,WAIT_HOLD},
  {OP_MOVE,3,2,OP_MOVE,1,2,WAIT_QUICK},
  {OP_MOVE,2,2,OP_MOVE,2,2,WAIT_HOLD}, 
  {OP_END,0,0,0,0,0,0}
};

AnimationStep ANIM_DERP[] = {
  {OP_MOVE,2,2,OP_MOVE,2,2,WAIT_QUICK},
  {OP_MOVE,1,2,OP_MOVE,3,2,WAIT_QUICK},
  {OP_MOVE,0,2,OP_MOVE,4,2,WAIT_HOLD},
  {OP_MOVE,1,2,OP_MOVE,3,2,WAIT_QUICK},
  {OP_MOVE,2,2,OP_MOVE,2,2,WAIT_HOLD}, 
  {OP_END,0,0,0,0,0,0}
};

AnimationStep ANIM_LOOK_NOSE[] = {
  {OP_MOVE,2,2,OP_MOVE,2,2,WAIT_QUICK},
  {OP_MOVE,3,3,OP_MOVE,1,3,WAIT_QUICK},
  {OP_MOVE,4,4,OP_MOVE,0,4,WAIT_HOLD},
  {OP_BLINK,4,4,OP_BLINK,0,4,WAIT_RANDOM},
  {OP_MOVE,3,3,OP_MOVE,1,3,WAIT_QUICK},
  {OP_MOVE,2,2,OP_MOVE,2,2,WAIT_QUICK},
  {OP_END,0,0,0,0,0,0}
};

AnimationStep ANIM_LOOK_BROW[] = {
  {OP_MOVE,2,2,OP_MOVE,2,2,WAIT_QUICK},
  {OP_MOVE,3,1,OP_MOVE,1,1,WAIT_QUICK},
  {OP_MOVE,4,0,OP_MOVE,0,0,WAIT_HOLD},
  {OP_BLINK,4,0,OP_BLINK,0,0,WAIT_RANDOM},
  {OP_MOVE,3,1,OP_MOVE,1,1,WAIT_QUICK},
  {OP_MOVE,2,2,OP_MOVE,2,2,WAIT_QUICK},
  {OP_END,0,0,0,0,0,0}
};

AnimationStep ANIM_WINK_LEFT[] = {
  {OP_BLINK,2,2,OP_MOVE,2,2,WAIT_RANDOM},
  {OP_END,0,0,0,0,0,0}
};

AnimationStep ANIM_WINK_RIGHT[] = {
  {OP_MOVE,2,2,OP_BLINK,2,2,WAIT_RANDOM},
  {OP_END,0,0,0,0,0,0}
};

AnimationStep ANIM_DOWN_LEFT[] = {
  {OP_MOVE,2,2,OP_MOVE,2,2,WAIT_QUICK},
  {OP_MOVE,1,3,OP_MOVE,1,3,WAIT_QUICK},
  {OP_MOVE,0,4,OP_MOVE,0,4,WAIT_HOLD},
  {OP_MOVE,1,3,OP_MOVE,1,3,WAIT_QUICK},
  {OP_MOVE,2,2,OP_MOVE,2,2,WAIT_QUICK},
  {OP_END,0,0,0,0,0,0}
};

AnimationStep ANIM_DOWN_RIGHT[] = {
  {OP_MOVE,2,2,OP_MOVE,2,2,WAIT_QUICK},
  {OP_MOVE,3,3,OP_MOVE,3,3,WAIT_QUICK},
  {OP_MOVE,4,4,OP_MOVE,4,4,WAIT_HOLD},
  {OP_MOVE,3,3,OP_MOVE,3,3,WAIT_QUICK},
  {OP_MOVE,2,2,OP_MOVE,2,2,WAIT_QUICK},
  {OP_END,0,0,0,0,0,0}
};

AnimationStep ANIM_UP_LEFT[] = {
  {OP_MOVE,2,2,OP_MOVE,2,2,WAIT_QUICK},
  {OP_MOVE,1,1,OP_MOVE,1,1,WAIT_QUICK},
  {OP_MOVE,0,0,OP_MOVE,0,0,WAIT_HOLD},
  {OP_MOVE,1,1,OP_MOVE,1,1,WAIT_QUICK},
  {OP_MOVE,2,2,OP_MOVE,2,2,WAIT_QUICK},
  {OP_END,0,0,0,0,0,0}
};

AnimationStep ANIM_UP_RIGHT[] = {
  {OP_MOVE,2,2,OP_MOVE,2,2,WAIT_QUICK},
  {OP_MOVE,3,1,OP_MOVE,3,1,WAIT_QUICK},
  {OP_MOVE,4,0,OP_MOVE,4,0,WAIT_HOLD},
  {OP_MOVE,3,1,OP_MOVE,3,1,WAIT_QUICK},
  {OP_MOVE,2,2,OP_MOVE,2,2,WAIT_QUICK},
  {OP_END,0,0,0,0,0,0}
};

AnimationStep ANIM_WANDER[] = {
  {OP_WANDER,0,0,OP_WANDER,0,0,WAIT_QUICK},
  {OP_END,0,0,0,0,0,0}
};

AnimationStep* animations[] = {
  ANIM_STARE,
  ANIM_STARE,
  ANIM_STARE,
  ANIM_STARE,
  ANIM_STARE,
  ANIM_STARE,
  ANIM_STARE, 
  ANIM_STARE, 
  ANIM_STARE, 
  ANIM_STARE, 
  ANIM_SIDE_TO_SIDE, 
  ANIM_SIDE_TO_SIDE, 
  ANIM_SIDE_TO_SIDE, 
  ANIM_SIDE_TO_SIDE, 
  ANIM_ROLL,
  ANIM_ROLL,
  ANIM_ROLL,
  ANIM_ROLL,
  ANIM_WANDER,
  ANIM_CROSSED,
  ANIM_DERP,
  ANIM_LOOK_NOSE,
  ANIM_LOOK_BROW,
  ANIM_WINK_LEFT,
  ANIM_WINK_RIGHT,
  ANIM_UP_LEFT,
  ANIM_UP_RIGHT,
  ANIM_DOWN_LEFT,
  ANIM_DOWN_RIGHT
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
  runAnimation(animations[random(0, sizeof(animations) / sizeof(AnimationStep*))]);
}

// This is a total mess, and it shames me.
void runAnimation(struct AnimationStep* step_ptr) {
  while (1) {
    AnimationStep curr = *step_ptr;

    if (curr.op_left == OP_END) { break; }

    if (curr.op_left == OP_BLINK && curr.op_right == OP_BLINK) {
      blinkEyes(1, 1, curr.left_x, curr.left_y, curr.right_x, curr.right_y);
    } else if (curr.op_left == OP_WANDER && curr.op_right == OP_WANDER) {
      wanderEyes();
    } else {
      if (curr.op_left == OP_MOVE) {
        copyEyeballPosition(EYE_LEFT, curr.left_x, curr.left_y);
      }
      if (curr.op_right == OP_MOVE) {
        copyEyeballPosition(EYE_RIGHT, curr.right_x, curr.right_y);
      }
      updateDisplay();
      if (curr.op_left == OP_BLINK) {
        blinkEyes(1, 0, curr.left_x, curr.left_y, curr.right_x, curr.right_y);      
      }
      if (curr.op_right == OP_BLINK) {
        blinkEyes(0, 1, curr.left_x, curr.left_y, curr.right_x, curr.right_y);      
      }
    }

    if (curr.wait == WAIT_RANDOM) {
      delay(random(RANDOM_WAIT_MIN, RANDOM_WAIT_MAX));
    } else {
      delay(curr.wait);
    }
    
    step_ptr += 1;
  }
}

// Blink by rapidly dropping and raising the "eyelid"
void blinkEyes(int do_left, int do_right, int left_x, int left_y, int right_x, int right_y) {
  int y;
  for (int idx = -NUM_ROWS; idx <= NUM_ROWS; idx++) {
    y = NUM_ROWS - abs(idx);
    if (do_left) { 
      copyEyeballPosition(EYE_LEFT, left_x, left_y);
      renderEyelid(EYE_LEFT, y); 
    }
    if (do_right) { 
      copyEyeballPosition(EYE_RIGHT, right_x, right_y);
      renderEyelid(EYE_RIGHT, y); 
    }
    updateDisplay();
    delay(BLINK_WAIT);
  }
}

// Random wandering eyes
void wanderEyes() {
  int x = 2;
  int y = 2;
  int max_steps = random(MIN_WANDER_STEPS, MAX_WANDER_STEPS);

  for (int steps = 0; steps < max_steps; steps++) {
    x += random(-1, 2);
    x = constrain(x, MIN_PUPIL_X, MAX_PUPIL_X);
    y += random(-1, 2);
    y = constrain(y, MIN_PUPIL_Y, MAX_PUPIL_Y);
    
    copyEyeballPosition(EYE_LEFT, x, y);
    copyEyeballPosition(EYE_RIGHT, x, y);
    updateDisplay();
    delay(WANDER_WAIT);
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
  for (int j=0; j<NUM_ROWS; j++) {
    for (int i=0; i<NUM_EYES; i++) {
      lc.setRow(i, j, eyes[i][j]);
    }
  }
}
