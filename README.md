# SignBridge

**Indian Sign Language to speech, in real time, in the browser.** SignBridge reads right-hand ISL signs, from a sensor glove or a webcam, and speaks them aloud. The hearing person replies by voice, and their words appear as large text. Sign recognition runs inside the browser; no video leaves the computer.

> IoT & Robotics Foundation Lab (CSE25309) project · ESP32 · Bluetooth LE · MediaPipe · transfer learning · Web app

**▶ Try it live: https://sathvikasingoti.github.io/SignBridge/** (Chrome; allow the camera)

![Circuit](docs/images/diagrams/circuit-schematic.png)

## What it does

- **Two inputs, one app.**
  - **Sensor glove:** 4 flex sensors (thumb, index, middle, ring) and an MPU6050 on an ESP32, sent over Bluetooth LE at 50 Hz.
  - **Webcam:** Google's MediaPipe tracks 21 points on the hand.
- **Three recognition models:**
  1. **Google's pre-trained Gesture Recognizer.** Works with no training; each gesture is mapped to a phrase.
  2. **SignBridge fine-tuned ISL model.** The same Google model, fine-tuned on our own right-hand ISL signs (transfer learning). The notebook is ready; the model hasn't been trained yet.
  3. **k-NN on your recordings.** Learns a new sign from a handful of examples, and works for the glove too.
- **One-handed signs:** help, yes, no, thank you, pain, water, stop, eat, come, sleep, okay, plus "rest" (no sign).
- **Speaks only when sure:** a confidence threshold plus 4 matching predictions in a row before it speaks anything.
- **Two-way conversation:** text-to-speech for the signer, speech-to-text (English or Hindi) for the reply.
- **Built-in dataset tools:**
  - a guided recording session;
  - a photo dataset export for fine-tuning;
  - a leave-one-out accuracy test with a confusion matrix.
- **Right-hand-only tracking:** a second hand in view is ignored.

## How it works

```mermaid
flowchart LR
  G[Glove: 4 flex + MPU6050] -- ESP32 · BLE 50 Hz --> A
  C[Webcam] -- MediaPipe hand tracking --> A
  A[SignBridge app] --> M{Model}
  M -->|pre-trained| P[Google Gesture Recognizer]
  M -->|fine-tuned| F[SignBridge ISL model]
  M -->|few-shot| K[k-NN]
  P & F & K --> S[Confidence + stability check] --> T[Speech]
  R[Reply by voice] --> STT[Speech-to-text] --> A
```

## Quick start

**Use the app:**
- **Locally:** double-click `start-signbridge.bat`, then open http://localhost:8766 in Chrome or Edge.
- **Online:** open https://sathvikasingoti.github.io/SignBridge/ in Chrome or Edge. It redeploys on every push to `main`.

Connect the glove (or press **Use camera**), then **Start recognising**. With the camera, thumbs up says "Yes", open palm says "Stop", and so on. You can change what each gesture says in the Talk tab.

**Fine-tune on your own signs:**
1. **Record:** Train tab → *Guided dataset session* with the camera. Record each sign 30+ times, plus "rest".
2. **Export:** Data → *Download photo dataset (.zip)*.
3. **Train:** open [`ml/finetune_gesture_recognizer.ipynb`](ml/finetune_gesture_recognizer.ipynb) in Google Colab, upload the zip, and run all cells.
4. **Use it:** put the downloaded `signbridge_gestures.task` in `app/models/` and choose *SignBridge fine-tuned ISL model*.

**Evaluate in Python:**

```bash
pip install -r ml/requirements.txt
python ml/train.py signbridge-data-*.json        # k-NN vs Random Forest, grouped CV, confusion matrices
```

**Flash the glove:** open `firmware/signbridge_glove/signbridge_glove.ino` in Arduino IDE or arduino-cli. Use the board *ESP32 Dev Module* and upload at 115200 baud. Wiring is in [the build guide](docs/build-guide.html).

## Hardware

| Part | Qty | Pin |
|---|---|---|
| ESP32 DevKit V1 (CH340) | 1 | – |
| Spectra Symbol 2.2" flex sensor | 4 | D32 thumb, D33 index, D34 middle, D35 ring |
| 10 kΩ resistor (33–47 kΩ gives a bigger swing) | 4 | pin → GND |
| MPU6050 (GY-521) | 1 | SDA D21, SCL D22 |

Each finger is a voltage divider read by the ESP32's ADC1 pins (ADC2 is shared with the radio). The glove has no pinky sensor.

The firmware takes a trimmed mean of 16 reads per sample and smooths adaptively. That holds a resting finger steady but lets real bends through quickly. The app's *Re-zero* button corrects flex-sensor creep.

## Repository

| Folder | Contents |
|---|---|
| `app/` | The web app (single page, no build step) and `models/` |
| `firmware/` | ESP32 glove firmware, Tinkercad practice sketch |
| `ml/` | `train.py` (k-NN / Random Forest evaluation), Colab fine-tuning notebook |
| `data/` | How the dataset is made; raw exports stay out of git |
| `docs/` | Build guide, circuit diagrams, project notes |
| `report/` | Project report |
| `tools/` | Serial-port testing scripts for the glove |

## Results

Accuracy figures will be added here after the dataset is recorded. They'll come from the app's leave-one-recording-out test and from `train.py`'s cross-validation grouped by recording.

## Limitations

- **Single, isolated signs** from the right hand; no continuous sentences, no two-handed signs.
- **The glove can't sense where the hand is** relative to the body, only finger bend, tilt and motion.
- **Camera accuracy** depends on lighting and distance.
- **Google's pre-trained model** knows 7 everyday gestures, not ISL. The fine-tuned model is meant to recognise ISL.
- **Flex sensors** drift and differ from unit to unit; calibrate before use.

## License

MIT. Hand tracking and the base gesture model are Google's MediaPipe (Apache 2.0).
