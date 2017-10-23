#include <NESpad.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Max72xxPanel.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266mDNS.h>
#include <DNSServer.h>            //Local DNS Server used for redirecting all requests to the configuration portal
#include <ESP8266WebServer.h>     //Local WebServer used to serve the configuration portal
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager WiFi Configuration Magic
#include <FS.h>
#include <WebSockets.h>
#include <WebSocketsClient.h>
#include <WebSocketsServer.h>

#define POT_PIN 0

#define DEBUG 1

#define SERVER_NAME "pumpkin"
#define SERVER_PORT 80
#define SERVER_WEBSOCKET_PORT 81

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

#define MOUTH_MIN 0
#define MOUTH_MAX 3

#define IDLE_TIME_MAX 2000
#define IDLE_MOVE_DELAY_MIN 200
#define IDLE_MOVE_DELAY_MAX 1000

#define IDLE_BLINK_CHANCE 15
#define IDLE_MOUTH_CHANCE 40

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

const byte mouth[4][8] = {
  {
    B11111111,
    B11111110,
    B11111100,
    B11111000,
    B11110000,
    B11100000,
    B11000000,
    B10000000
  },
  {
    B11110001,
    B11110001,
    B11110001,
    B11110001,
    B11111011,
    B11111011,
    B11111011,
    B11111011,
  },
  {
    B10001111,
    B10001111,
    B10001111,
    B10001111,
    B11011111,
    B11011111,
    B11011111,
    B11011111,
  },
  {
    B11111111,
    B01111111,
    B00111111,
    B00011111,
    B00001111,
    B00000111,
    B00000011,
    B00000001
  },
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
  NES_DOWN | NES_RIGHT | NES_START,
  NES_UP | NES_SELECT,
  NES_DOWN | NES_SELECT,
  NES_LEFT | NES_SELECT,
  NES_RIGHT | NES_SELECT,
  NES_UP | NES_LEFT | NES_SELECT,
  NES_UP | NES_RIGHT | NES_SELECT,
  NES_DOWN | NES_LEFT | NES_SELECT,
  NES_DOWN | NES_RIGHT | NES_SELECT,
  NES_RIGHT | NES_START | NES_SELECT,
  NES_UP | NES_RIGHT | NES_START | NES_SELECT,
  NES_DOWN | NES_RIGHT | NES_START | NES_SELECT
};

// Display buffer for rendering eyes
byte eyes[NUM_EYES + 1][NUM_ROWS + 1];

// Prerendered positions of the eyeball & pupil
byte positions[PUPIL_X_MAX + 1][PUPIL_Y_MAX + 1][NUM_ROWS + 1];

#define POS_X 0
#define POS_Y 1
#define POS_EYELID 2
#define POS_MOUTH 3
#define NUM_POS 4

int current_pos[NUM_EYES][NUM_POS];
int target_pos[NUM_EYES][NUM_POS];
int min_pos[] = { PUPIL_X_MIN, PUPIL_Y_MIN, EYELID_MIN, MOUTH_MIN };
int max_pos[] = { PUPIL_X_MAX, PUPIL_Y_MAX, EYELID_MAX, MOUTH_MAX };
int step_pos[] = { 1, 1, 3, 1 };

// Attach CS to this pin, DIN to MOSI and CLK to SCK (cf http://arduino.cc/en/Reference/SPI )
// CS pin, # horiz displays, # vert displays
Max72xxPanel matrix = Max72xxPanel(12, 6, 1);

// put your own strobe/clock/data pin numbers here -- see the pinout in readme.txt
NESpad nintendo = NESpad(5, 4, 16);

// Create an instance of the server
// specify the port to listen on as an argument
ESP8266WebServer server ( SERVER_PORT );
WebSocketsServer webSocket = WebSocketsServer(SERVER_WEBSOCKET_PORT);

WiFiManager wifiManager;

int i, current, target, pot, pad, web_pad, idle_pad;
byte display_intensity;
unsigned long t_now, t_last;
long td_eyes, td_wifi, idle_time, idle_move_delay;

#define TIMER_DELAY_EYES 50
#define TIMER_DELAY_RESTART 1000 * 60 * 10

#define NUM_TIMERS 2
#define TIMER_EYES 0
#define TIMER_RESTART 1

const PROGMEM long timers_max[] = {
  TIMER_DELAY_EYES,
  TIMER_DELAY_RESTART
};
long timers[NUM_TIMERS];

void setup() {
  Serial.begin(115200);
  while (!Serial) { }
  Serial.println("STARTING UP");

  if (!SPIFFS.begin())
  {
    // Serious problem
    Serial.println("SPIFFS Mount failed");
  } else {
    Serial.println("SPIFFS Mount succesfull");
  }

  web_pad = 0;

  setupWifiServer();
  resetTimers();
  prerenderPositions();
  resetDisplay();
  updateDisplay();
}

void resetTimers() {
  t_last = millis();
  for (i = 0; i < NUM_TIMERS; i++) {
    timers[i] = timers_max[i];
  }
  idle_time = 0;
  idle_move_delay = 0;
}

void loop() {
  server.handleClient();
  webSocket.loop();

  unsigned long t_delta;
  t_now = millis();
  t_delta = t_now - t_last;
  t_last = t_now;

  for (i = 0; i < NUM_TIMERS; i++) {
    timers[i] -= t_delta;
    if (timers[i] <= 0) {
      long elapsed = timers_max[i] - timers[i];
      switch (i) {
        case TIMER_EYES: loopEyes(elapsed); break;
        case TIMER_RESTART: loopRestart(elapsed); break;
      }
      timers[i] = timers_max[i];
    }
  }
}

void setupWifiServer() {
  Serial.print("Connecting to wifi...");
  wifiManager.setAPCallback(configModeCallback);
  wifiManager.autoConnect();

  // Connect to WiFi network
  /*
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(ssid);
    WiFi.begin(ssid, password);
  */

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("wifi connected");

  // Start the server
  server.begin();
  Serial.println("Server started");

  // Print the IP address
  Serial.println(WiFi.localIP());

  if ( MDNS.begin ( SERVER_NAME ) ) {
    Serial.println ( "MDNS responder started" );
  }

  server.serveStatic("/", SPIFFS, "/index.html");
  server.serveStatic("/index.html", SPIFFS, "/index.html");
  server.serveStatic("/index.css", SPIFFS, "/index.css");
  server.serveStatic("/index.js", SPIFFS, "/index.js");
  server.serveStatic("/test.txt", SPIFFS, "/test.txt");

  server.on ( "/up", handleUp );
  server.on ( "/down", handleDown );
  server.on ( "/reset", handleReset );
  server.onNotFound ( handleNotFound );
  server.begin();

  Serial.println ( "HTTP server started" );

  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  Serial.println ( "WebSocket server started" );
}

void configModeCallback (WiFiManager *myWiFiManager) {
  Serial.println("Entered config mode");
  Serial.println(WiFi.softAPIP());
  //if you used auto generated SSID, print it
  Serial.println(myWiFiManager->getConfigPortalSSID());
}

#define USE_SERIAL Serial

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      USE_SERIAL.printf("[%u] Disconnected!\n", num);
      break;
    case WStype_CONNECTED:
      {
        IPAddress ip = webSocket.remoteIP(num);
        USE_SERIAL.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);

        // send message to client
        webSocket.sendTXT(num, "Connected");
      }
      break;
    case WStype_TEXT:
      USE_SERIAL.printf("[%u] get Text: %s\n", num, payload);

      if (strcmp("talkon", (const char *)payload) == 0) {
        web_pad |= NES_SELECT;
      }
      if (strcmp("talkoff", (const char *)payload) == 0) {
        web_pad &= ~NES_SELECT;
      }
      if (strcmp("blinkon", (const char *)payload) == 0) {
        web_pad |= NES_B | NES_A;
      }
      if (strcmp("blinkoff", (const char *)payload) == 0) {
        web_pad &= ~(NES_B | NES_A);
      }
      if (strcmp("joypadoff", (const char *)payload) == 0) {
        web_pad &= ~(NES_UP | NES_DOWN | NES_LEFT | NES_RIGHT);
      }
      if (strcmp("joypadupon", (const char *)payload) == 0) {
        web_pad |= NES_UP;
      }
      if (strcmp("joypadupoff", (const char *)payload) == 0) {
        web_pad &= ~NES_UP;
      }
      if (strcmp("joypaddownon", (const char *)payload) == 0) {
        web_pad |= NES_DOWN;
      }
      if (strcmp("joypaddownoff", (const char *)payload) == 0) {
        web_pad &= ~NES_DOWN;
      }
      if (strcmp("joypadlefton", (const char *)payload) == 0) {
        web_pad |= NES_LEFT;
      }
      if (strcmp("joypadleftoff", (const char *)payload) == 0) {
        web_pad &= ~NES_LEFT;
      }
      if (strcmp("joypadrighton", (const char *)payload) == 0) {
        web_pad |= NES_RIGHT;
      }
      if (strcmp("joypadrightoff", (const char *)payload) == 0) {
        web_pad &= ~NES_RIGHT;
      }

      Serial.print("web_pad = ");
      Serial.println(web_pad);

      // send message to client
      // webSocket.sendTXT(num, "message here");

      // send data to all connected clients
      // webSocket.broadcastTXT("message here");
      break;
    case WStype_BIN:
      USE_SERIAL.printf("[%u] get binary length: %u\n", num, length);
      hexdump(payload, length);

      // send message to client
      // webSocket.sendBIN(num, payload, length);
      break;
  }

}

void handleUp () {
  char temp[400];
  if (display_intensity < INTENSITY_MAX) {
    setDisplayIntensity(display_intensity + 1);
  }
  snprintf(temp, 400, "<html><body>Intensity = %02d (up)</body></html>", display_intensity);
  server.send ( 200, "text/html", temp );
}

void handleDown () {
  char temp[400];
  if (display_intensity > INTENSITY_MIN) {
    setDisplayIntensity(display_intensity - 1);
  }
  snprintf(temp, 400, "<html><body>Intensity = %02d (down)</body></html>", display_intensity);
  server.send ( 200, "text/html", temp );
}

void handleReset () {
  server.send ( 200, "text/plain", "RESET" );
  delay(1000);
  ESP.restart();
}

void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += ( server.method() == HTTP_GET ) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";

  for ( uint8_t i = 0; i < server.args(); i++ ) {
    message += " " + server.argName ( i ) + ": " + server.arg ( i ) + "\n";
  }

  server.send ( 404, "text/plain", message );
}

void loopRestart(unsigned long t_delta) {
  ESP.restart();
}

void loopEyes(unsigned long t_delta) {
  // Adjust intensity based on the potentiometer position
  // pot = map(analogRead(POT_PIN), 0, 1023, 0, 15);
  // setDisplayIntensity(pot);
  matrix.setIntensity(display_intensity);

  // Grab gamepad state
  pad = nintendo.buttons();

  if (pad == 0 && web_pad != 0) { pad = web_pad; }

  // If the gamepad is entirely neutral, increment the idle timer.
  // Otherwise, reset it.
  idle_time = (pad != 0) ? 0 : (idle_time + t_delta);

  // If we're past the max idle time, start performing idle animations
  if (idle_time >= IDLE_TIME_MAX) {

    // Constrain the idle timer to max value, so we don't
    // just keep incrementing and overflow the variable
    idle_time = IDLE_TIME_MAX;

    // Tick down the delay timer until the next idle move.
    idle_move_delay -= t_delta;

    if (idle_move_delay <= 0) {
      setRandomIdlePad();
    }

    // Tap the mouth button occasionally while idle
    if (random(0, 100) < IDLE_MOUTH_CHANCE) {
      idle_pad |= NES_SELECT;
    } else {
      idle_pad &= ~NES_SELECT;
    }

    // Set the idle animation pad state as if it were user input
    pad = idle_pad;

  }

  setTargetsFromPad(pad);
  animateToTargets();
  updateDisplay();
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
  target_pos[0][POS_MOUTH] = (pad & NES_SELECT) ? MOUTH_MIN : MOUTH_MAX;
  /*
    if (pad & NES_SELECT) {
    target_pos[EYE_RIGHT][POS_Y] = abs(PUPIL_Y_MAX - target_pos[EYE_RIGHT][POS_Y]);
    }
  */

}

// Move current eye & eyelid positions one step toward target positions
void animateToTargets() {
  for (int eye_idx = 0; eye_idx < NUM_EYES; eye_idx++) {
    for (int k = 0; k < NUM_POS; k++) {
      current = current_pos[eye_idx][k];
      target = target_pos[eye_idx][k];
      if (current != target) {
        current += (current < target) ? step_pos[k] : 0 - step_pos[k];
        current_pos[eye_idx][k] = constrain(current, min_pos[k], max_pos[k]);
      }
    }
  }
}

// Render "eyelid" in display buffer by blacking out rows
void renderEyelid(int idx, int y) {
  for (int i = 0; i < y; i++) {
    eyes[idx][i] = 0;
  }
}

// Ensure the displays are on, intense, and cleared.
void resetDisplay() {
  setDisplayIntensity(1);
  matrix.setRotation(0, 3);
  matrix.setRotation(1, 3);
  matrix.setRotation(2, 3);
  matrix.setRotation(3, 3);
  matrix.setRotation(4, 3);
  matrix.setRotation(5, 3);
  matrix.fillScreen(LOW);
  for (int i = 0; i < NUM_EYES; i++) {
    current_pos[i][POS_X] = target_pos[i][POS_X] = PUPIL_X_CENTER;
    current_pos[i][POS_Y] = target_pos[i][POS_Y] = PUPIL_Y_CENTER;
    current_pos[i][POS_EYELID] = target_pos[i][POS_EYELID] = 0;
    current_pos[i][POS_MOUTH] = target_pos[i][POS_MOUTH] = 0;
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
  for (int i = start_y; i < end_y; i++) {
    positions[x][y][i] ^= pupil;
  }
}

// Fill the set of prerendered positions for all x/y pairs
// Probably a gross over-optimization, but kind of hoping
// doing less math on every eye position will save battery life
void prerenderPositions() {
  for (int x = 0; x <= PUPIL_X_MAX; x++) {
    for (int y = 0; y <= PUPIL_Y_MAX; y++) {
      renderPosition(x, y);
    }
  }
}

// Set display intensity
void setDisplayIntensity(int intensity) {
  display_intensity = intensity;
}

// Set the LED contents from the display buffers
void updateDisplay() {
  for (int i = 0; i < NUM_EYES; i++) {
    copyEyeballPosition(i, current_pos[i][POS_X], current_pos[i][POS_Y]);
    renderEyelid(i, current_pos[i][POS_EYELID]);
  }
  for (int i = 0; i < NUM_EYES; i++) {
    for (int y = 0; y < NUM_ROWS; y++) {
      for (int x = 0; x < NUM_COLS; x++) {
        int row = eyes[i][y];
        matrix.drawPixel(x + (NUM_COLS * i), y, bitRead(row, (7 - x)));
      }
    }
  }

  int pos_mouth = current_pos[0][POS_MOUTH];
  for (int i = 0; i < 4; i++) {
    for (int y = 0; y < NUM_ROWS; y++) {
      int mirror_row = (y < NUM_ROWS / 2) ? y : NUM_ROWS - y - 1;
      int row = (mirror_row < pos_mouth) ? 0 : mouth[i][y];
      for (int x = 0; x < NUM_COLS; x++) {
        matrix.drawPixel(x + (NUM_COLS * (i + 2)), y, bitRead(row, (7 - x)));
      }
    }
  }
  matrix.write();
}
