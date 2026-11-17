# Dataset

Recordings come from the SignBridge app (Train tab → Data):

- **Export data (.json):** every recording (camera landmarks and glove sensor values) with its sign and who recorded it. Used by `ml/train.py`.
- **Download photo dataset (.zip):** 3 photos per camera recording, one folder per sign (`none` = resting hand). Used by `ml/finetune_gesture_recognizer.ipynb`.

Put files you want to keep in `data/raw/`. That folder is git-ignored, because photos and names are personal data.
Only commit an anonymised export (e.g. `data/signbridge-landmarks.json` with the `who` field removed) once the whole team agrees.
