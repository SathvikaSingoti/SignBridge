# Models

| File | What it is |
|---|---|
| `signbridge_gestures.task` | Our fine-tuned gesture model (not committed yet). Made with `ml/finetune_gesture_recognizer.ipynb` from the app's photo dataset. When this file is here, the app's **SignBridge fine-tuned ISL model** option switches on. |

Google's pre-trained Gesture Recognizer isn't stored here: the app loads it from Google's model storage
(`storage.googleapis.com/mediapipe-models/gesture_recognizer/...`).
