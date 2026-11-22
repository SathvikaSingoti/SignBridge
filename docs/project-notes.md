# SignBridge: project notes for the report

Everything you need to write the report: what the project does, how it is built, how it was tested, the results so far, and the problems we solved. Each section maps to a usual report heading. Diagrams are in `docs/images/diagrams/`; the repository README lists where all the code is.

---

## 1. Abstract (draft)

SignBridge is a low-cost assistive system that lets a person who uses Indian Sign Language (ISL) communicate with someone who doesn't know sign language. It has two kinds of input:

- **A sensor glove:** four flex sensors (thumb, index, middle, ring) and an MPU6050 motion sensor on an ESP32, sent to a laptop over Bluetooth Low Energy.
- **A webcam:** hand tracking with Google MediaPipe.

A small machine-learning model (k-nearest neighbours) runs inside a web page in the browser. It is trained on recordings the user makes, recognises a vocabulary of emergency signs, and speaks the matching phrase aloud. The hearing person replies by voice, and their speech is shown as large text. Everything runs on the device, and no video leaves it.

## 2. Problem and objectives

- Deaf and hard-of-hearing people often can't communicate quickly with hospital staff, police or the public, especially in emergencies.
- **Objective 1:** recognise 11 one-handed signs (help, yes, no, thank you, pain, water, stop, eat, come, sleep, okay) and speak them.
- **Objective 2:** show the other person's spoken reply as text, so the conversation works both ways.
- **Objective 3:** keep it cheap (an ESP32 and four flex sensors) with sign recognition running locally in the browser.
- **Objective 4:** avoid wrong outputs. The system stays silent unless it is confident.

## 3. System overview

```
 Glove (4 flex + MPU6050) --ESP32--> Bluetooth LE --\
                                                     >-- Web app: features -> KNN model -> confidence check -> speech
 Webcam -------------> MediaPipe hand tracking ---/                                   <- speech-to-text reply
```

- The ESP32 only **streams raw sensor data**. All recognition happens in the app.
- Glove and camera recordings are stored and trained separately, because they are different kinds of data.

## 4. Hardware

### 4.1 Components

| Component | Qty | Purpose |
|---|---|---|
| ESP32 DevKit V1 (30-pin, CH340 USB chip) | 1 | Reads sensors, sends data over Bluetooth LE |
| Spectra Symbol 2.2" flex sensor | 5 | One per finger. About 25 kΩ straight, rising when bent |
| 10 kΩ resistor | 5 | Bottom half of each voltage divider (47 kΩ recommended, see §8) |
| MPU6050 (GY-521 / HW-123 board) | 1 | 3-axis accelerometer and gyroscope for hand tilt and motion |
| Breadboard, jumper wires, extension wires | – | Prototype wiring |
| Push button (optional) | 1 | Starts a recording from the glove |

### 4.2 Circuit

Each finger is a **voltage divider**: 3V3 → flex sensor → analog pin → 10 kΩ → GND.
When the finger bends, the sensor's resistance goes up, so the voltage at the pin goes down:

V_pin = 3.3 V × R_fixed / (R_fixed + R_flex)

With R_flex ≈ 25–35 kΩ straight and R_fixed = 10 kΩ, the pin sits at about 0.7–0.9 V. That reads about 700–900 on the ESP32's 12-bit ADC (0–4095), which matches what we measured.

| Finger | ESP32 pin | GPIO |
|---|---|---|
| Thumb | D32 | 32 |
| Index | D33 | 33 |
| Middle | D34 | 34 |
| Ring | D35 | 35 |

The glove has no pinky sensor: the first one failed after soldering, so the design uses four fingers.

| MPU6050 pin | Connected to |
|---|---|
| VCC | 3V3 |
| GND | GND |
| SDA | D21 |
| SCL | D22 |

**Design choice:** only **ADC1** pins (GPIO 32–39) are used for the sensors. The ESP32's ADC2 pins stop working while Bluetooth or Wi-Fi is on.

Diagrams: `diagrams/circuit-schematic.svg`, `diagrams/breadboard-layout.svg`, `diagrams/esp32-pins.svg`.

## 5. Firmware (ESP32)

File: `firmware/signbridge_glove/signbridge_glove.ino`. Built with Arduino CLI 1.6.0 and the ESP32 Arduino core 3.3.12. It uses 85% of flash.

- **Sampling:**
  - 50 times a second.
  - Each flex reading is the **average of 16 ADC reads**, then smoothed (new = 0.6 × old + 0.4 × reading).
  - This cut the resting noise from about ±60–170 to about ±13 (a range of about 26).
- **ADC:** 12-bit resolution, 11 dB attenuation, so it covers the full 0–3.3 V range.
- **MPU6050:**
  - The code scans I2C at start-up and wakes the chip.
  - Settings: 44 Hz low-pass filter, accelerometer ±2 g, gyroscope ±1000 °/s.
  - If the chip isn't found, the glove still works and sends zeros for motion.
- **Bluetooth LE:**
  - Device name "SignBridge Glove", using the Nordic UART Service (`6e400001-…`). Packets go out as notifications on TX `6e400003-…`.
  - Each packet is 17 bytes:

| Bytes | Content |
|---|---|
| 0–7 | 4 flex values (thumb, index, middle, ring), uint16 little-endian (0–4095) |
| 8–9 | Reserved, always 0 |
| 10–12 | Accelerometer x, y, z as int8, 1 unit = 1/32 g |
| 13–15 | Gyroscope x, y, z as int8, 1 unit = 8 °/s |
| 16 | Flags: bit 0 = button held, bit 1 = MPU6050 found |

- **Status LED:** the on-board blue LED (GPIO 2) blinks while waiting for the app and stays on when connected.
- **Serial output:** five lines a second with all values, used for testing (see `tools/live.ps1`).

## 6. Software (web app)

File: `index.html`, a single page that runs in Chrome or Edge.

- **Inputs:**
  - **Glove:** connects over Web Bluetooth.
  - **Camera:** MediaPipe HandLandmarker (tasks-vision 0.10.14) tracks up to 2 hands and finds 21 points on each, running on the device's GPU.
  - **Keyboard simulation:** for developing without hardware. It is clearly labelled with a red banner, and its recordings are tagged as simulated and left out of accuracy figures.
- **Tabs:** Talk, Train, Glove setup.
- **Default vocabulary:** rest (never spoken), help, yes, no, thank you, pain, water, stop, eat, come, sleep, okay. All are one-handed. Each sign has a spoken phrase, e.g. "water" → "I need water".

### 6.1 Features

- **Glove:**
  - Each recording is 60 frames (1.2 s).
  - The values per frame are 4 finger bends (scaled 0–1 using a calibration of open hand and fist), 3 accelerometer and 3 gyroscope values.
  - Features: the means of 6 time segments for the bends and the accelerometer, each finger's range, and the mean and maximum of the gyroscope.
  - Weights: flex 1.0, accelerometer 0.6, gyroscope 0.6.
- **Camera:**
  - Each recording is 30 frames (1.2 s).
  - Each hand gives 21 points measured relative to the wrist and scaled by palm size, so the result doesn't depend on distance from the camera, plus a visible flag and the wrist position.
  - That makes 132 values per frame for 2 hands.
  - Features: the means of 4 time segments, plus wrist speed.

### 6.2 Classifier

- **k-nearest neighbours** on standardised features.
  - k adapts to the data: k = (smallest recordings-per-sign) − 1, clamped between 1 and 5.
  - KNN was chosen because it trains instantly, works with few examples, and runs easily in a browser.
- **Confidence:** the share of the k neighbours that agree.
  - A prediction is "unknown" if it is too far from all training data: more than 3 × the typical nearest-neighbour distance.
- **Speaking rule:** a sign is spoken only when all of these hold:
  - confidence ≥ the threshold (80% by default);
  - the same label 4 times in a row;
  - at least 1.2 s since the last phrase.
  The rule re-arms after a different sign or rest. This prevents repeated or accidental speech.
- **Speech:** text-to-speech in English (India). The reply uses Chrome speech recognition in English (India) or Hindi.
- **Storage:** recordings are kept in the browser's IndexedDB and can be exported and imported as JSON.

### 6.3 Evaluation tools

- **In the app:** a leave-one-out accuracy test, with overall %, per-sign % and a confusion matrix.
- **`train.py` (Python, scikit-learn):** loads exported JSON and runs **5-fold stratified cross-validation** for KNN and a Random Forest (300 trees).
  - Glove and camera recordings are evaluated separately.
  - It prints precision, recall and F1, and saves `confusion_matrix_glove.png` and `confusion_matrix_cam.png`.

## 7. Testing and results so far

### 7.1 Software (verified)

- **Camera pipeline:** tested with real hand photos (thumbs up, pointing up, victory).
  - Hand tracking loaded in about 2 s.
  - Recording, the accuracy test and live recognition worked, and the right phrase was spoken.
  - A recording with no hand visible is refused.
- **Glove pipeline:** tested end to end with the keyboard simulation (labelled as simulated).
- **`train.py`:** checked on generated test data.

### 7.2 Hardware (measured on 16 Nov 2026)

| Test | Result |
|---|---|
| Firmware compiles and uploads (115200 baud, COM7) | ✅ Verified, hash checked |
| Thumb, index, middle, ring connected | ✅ Read about 710 / 805 / 885 / 830 when straight |
| Pinky sensor | ❌ Unreliable even after re-soldering; removed from the design |
| Thumb wire pulled out of 3V3 → reading drops to 0 | ✅ Proves the sensor is in the circuit |
| Noise at rest after the 16-read averaging | ✅ About ±13 (range about 26) |
| Bend response on the glove (22 Nov) | ✅ Open hand → fist: thumb 170, index 382, middle 226, ring 330 |
| Noise at rest with trimmed mean + adaptive smoothing (22 Nov) | ✅ About ±7 |
| MPU6050 detected | ✅ Found at 0x68 after re-soldering its headers; readings follow hand tilt and rotation (it read "none" earlier, on 16 Nov) |
| Bluetooth connection to the app | ✅ 49–50 packets per second |

### 7.3 Accuracy

**No accuracy figures yet.** Fill this in after recording:
1. In the app, record each sign as 4-second holds (at least 2 per sign), plus "rest".
2. Train tab → **Run test**, and screenshot the confusion matrix.
3. Train tab → **Export**, then run `python train.py signbridge-data-*.json` for grouped cross-validation results and a confusion-matrix picture.

Report the numbers you get, and say how many people recorded and under what conditions (lighting, distance).

## 8. Problems faced and how we solved them

| Problem | Cause | Fix |
|---|---|---|
| AirBeat prototype: MPU not found, D33 drifting | 3V3 jumper on the wrong ESP32 pin; D33 wire one column off; breadboard rail split in the middle | Rewired from scratch with a step-by-step guide; bridged the split rails |
| Firmware wouldn't build on Windows | The build tool couldn't find `cmd.exe` on the shell's PATH | Added the Windows system folder to PATH |
| All four fingers jumped from about 850 to about 3000 at once | The 3V3 and GND feeds to the rails had been swapped | Swapped back; the readings returned to normal. The MPU was reverse-powered during this |
| Pinky read 0 (then briefly 1118), then kept flickering | Damaged sensor or solder joint on the thin extension wires | Re-soldered twice; still unreliable, so the pinky was removed and the glove uses four fingers |
| Readings noisy (±60–170) | ESP32 ADC noise | Average 16 reads per sample in firmware → about ±13 |
| Small bend response | Bending only the stiff end, or the wrong direction; 10 kΩ isn't the ideal match for a 25 kΩ sensor | Bend the whole dark strip; use 47 kΩ (≈ √(25k × 100k)) for about twice the swing; Spectra warns that soldering on the tabs can damage the sensor |
| Camera blocked on a phone over http | Browsers only allow camera and Bluetooth on https or localhost | Run on the laptop (localhost) or host on GitHub Pages (https) |

## 9. Limitations

- Static and short dynamic signs only. It recognises one sign at a time, not full sentences or grammar.
- Camera accuracy depends on lighting, background and distance. The model has to be trained per user and setting.
- The glove can't see hand shape directly, only finger bend (and tilt and motion when the MPU works).
- iPhones can use the camera mode but can't connect to the glove, because iOS has no Web Bluetooth.
- Speech-to-text uses Chrome's online speech service.

## 10. Future work

- Replace the MPU6050 and add hand-motion features back.
- Use 47 kΩ resistors and mount the sensors in sleeves on the glove.
- Combine glove and camera data in one model.
- Record a larger dataset from more signers; try a small neural network (e.g. an LSTM) for dynamic signs.
- Battery power and a compact PCB for the glove.

## 11. Files

See the repository README for the folder layout.

## 12. Honesty notes for the report

- Label the Tinkercad screenshot as an **Arduino Uno practice circuit**. Tinkercad has no ESP32 or MPU6050.
- Don't report the keyboard-simulation results as glove accuracy. The app warns about this.
- Report only accuracy numbers you measured, together with how they were measured.
