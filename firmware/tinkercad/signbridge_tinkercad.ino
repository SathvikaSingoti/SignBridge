// SignBridge - Tinkercad practice circuit (Arduino Uno)
// Tinkercad has no ESP32 and no MPU6050, so this is the 4-finger flex part only, on an Uno.
// It is the same divider circuit as the real glove: 5V -> flex sensor -> pin, 10k from pin -> GND.
//
//   Flex sensors: thumb A0, index A1, middle A2, ring A3
//   LEDs (each through a 220 ohm resistor to GND): thumb 3, index 5, middle 6, ring 9
//
// Start the simulation with every sensor straight. Then drag each sensor's slider:
// its LED gets brighter as the finger bends, and the Serial Monitor shows the bend in %
// and which hand shape the 4 fingers make. The hand-shape table is a simple demo of
// finger patterns, not the trained sign model the real app uses.

const int NF = 4;                                // thumb, index, middle, ring (no pinky)
const int FLEX_PINS[NF] = {A0, A1, A2, A3};
const int LED_PINS[NF]  = {3, 5, 6, 9};           // PWM pins so brightness can follow the bend
const char* FINGERS[NF] = {"thumb", "index", "middle", "ring"};

const int BENT_ON = 60;    // a finger counts as bent above 60%
const int BENT_OFF = 40;   // and as straight again below 40%

// Hand shapes: 1 = bent, 0 = straight, in the order thumb index middle ring.
struct Shape { const char* bits; const char* name; };
const Shape SHAPES[] = {
  {"0000", "open hand"},
  {"1111", "fist"},
  {"1011", "pointing (index up)"},
  {"0111", "thumbs up"},
  {"1001", "victory (index + middle up)"},
  {"0011", "L shape (thumb + index out)"},
  {"1000", "four fingers up (thumb in)"},
};
const int NUM_SHAPES = sizeof(SHAPES) / sizeof(SHAPES[0]);

int rest[NF];          // reading with the finger straight, taken at start-up
int maxChange[NF];     // biggest change seen so far = fully bent
float smooth[NF];
bool bent[NF];
unsigned long lastPrint = 0;

void setup() {
  Serial.begin(9600);
  for (int i = 0; i < NF; i++) pinMode(LED_PINS[i], OUTPUT);
  delay(200);
  for (int i = 0; i < NF; i++) {
    rest[i] = analogRead(FLEX_PINS[i]);
    smooth[i] = rest[i];
    maxChange[i] = 30;
    bent[i] = false;
  }
  Serial.println("SignBridge Tinkercad ready. All sensors must be straight at start-up.");
  Serial.println("Bend each sensor fully once so it learns its range.");
}

void loop() {
  int pct[NF];
  char bits[NF + 1];
  for (int i = 0; i < NF; i++) {
    smooth[i] = smooth[i] * 0.7 + analogRead(FLEX_PINS[i]) * 0.3;
    int change = abs((int)smooth[i] - rest[i]);          // works whichever way the reading moves
    if (change > maxChange[i]) maxChange[i] = change;
    pct[i] = constrain(map(change, 0, maxChange[i], 0, 100), 0, 100);
    if (!bent[i] && pct[i] > BENT_ON) bent[i] = true;
    if (bent[i] && pct[i] < BENT_OFF) bent[i] = false;
    analogWrite(LED_PINS[i], map(pct[i], 0, 100, 0, 255));
    bits[i] = bent[i] ? '1' : '0';
  }
  bits[NF] = '\0';

  if (millis() - lastPrint > 300) {
    lastPrint = millis();
    for (int i = 0; i < NF; i++) {
      Serial.print(FINGERS[i]); Serial.print(' ');
      Serial.print(pct[i]); Serial.print("%  ");
    }
    Serial.print("| ");
    const char* name = "no match";
    for (int s = 0; s < NUM_SHAPES; s++) {
      if (strcmp(bits, SHAPES[s].bits) == 0) { name = SHAPES[s].name; break; }
    }
    Serial.println(name);
  }
  delay(20);
}
