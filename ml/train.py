"""
SignBridge: train and test classifiers in Python on data exported from the web app.

    pip install scikit-learn matplotlib
    python train.py signbridge-data-*.json          (several files are merged)

Glove and camera recordings are trained and tested separately.
Prints 5-fold cross-validated accuracy for KNN (the model the app uses) and a Random Forest,
per-sign accuracy, and saves confusion_matrix_glove.png / confusion_matrix_cam.png for the report.
Simulated (keyboard) recordings are left out unless you pass --include-sim.
Pass --ignore=pinky (or --ignore=pinky,ring) to leave broken fingers out of the glove model.
"""
import json, sys
import numpy as np
from sklearn.ensemble import RandomForestClassifier
from sklearn.model_selection import StratifiedKFold, cross_val_predict
from sklearn.neighbors import KNeighborsClassifier
from sklearn.pipeline import make_pipeline
from sklearn.preprocessing import StandardScaler
from sklearn.metrics import accuracy_score, classification_report, confusion_matrix


def kind_of(s):
    return s.get('kind') or ('cam' if s.get('src') == 'cam' else 'glove')


def cam_features(fr):
    """Same as index.html: 4 time-segment means of all 132 values, plus wrist speed per hand."""
    fr = np.asarray(fr, dtype=float)
    out = [seg.mean(axis=0) for seg in np.array_split(fr, 4)]
    for h in range(2):
        p = h * 66 + 63
        both = (fr[1:, p] > 0) & (fr[:-1, p] > 0)
        d = np.abs(np.diff(fr[:, p + 1:p + 3], axis=0))[both]
        out.append(d.mean(axis=0) * 20 if len(d) else np.zeros(2))
    return np.concatenate(out)


def features(fr):
    """Same features as index.html: 6 time-segment means of bend+accel, finger range, gyro mean/max."""
    fr = np.asarray(fr, dtype=float)
    segs = np.array_split(fr, 6)
    out = [s[:, :8].mean(axis=0) for s in segs]
    out.append(fr[:, :5].max(axis=0) - fr[:, :5].min(axis=0))
    g = np.abs(fr[:, 8:11])
    out.append(np.column_stack([g.mean(axis=0), g.max(axis=0)]).ravel())
    return np.concatenate(out)


def main():
    files = [a for a in sys.argv[1:] if not a.startswith('--')]
    if not files:
        sys.exit(__doc__)
    samples, seen = [], set()
    for f in files:
        with open(f, encoding='utf-8') as fh:
            for s in json.load(fh)['samples']:
                key = (s.get('t'), s['label'], s.get('who'))
                if key not in seen:
                    seen.add(key); samples.append(s)
    if '--include-sim' not in sys.argv:
        sim = sum(s.get('src') == 'sim' for s in samples)
        samples = [s for s in samples if s.get('src') != 'sim']
        if sim:
            print(f'Left out {sim} simulated recordings.')
    for kind in ('glove', 'cam'):
        S = [s for s in samples if kind_of(s) == kind]
        if S:
            print('\n########## ' + ('Camera' if kind == 'cam' else 'Glove') + ' recordings ##########')
            evaluate(S, kind)


FINGERS = ['thumb', 'index', 'middle', 'ring', 'pinky']


def ignored_fingers():
    """--ignore=pinky,ring  leaves those fingers out of the glove features (like unticking "use" in the app)."""
    for a in sys.argv[1:]:
        if a.startswith('--ignore='):
            return [FINGERS.index(f.strip().lower()) for f in a.split('=', 1)[1].split(',') if f.strip()]
    return []


def evaluate(samples, kind):
    feat = cam_features if kind == 'cam' else features
    X = np.array([feat(s['frames']) for s in samples])
    if kind == 'glove' and ignored_fingers():
        drop = [seg * 8 + f for seg in range(6) for f in ignored_fingers()] + [48 + f for f in ignored_fingers()]
        X = np.delete(X, drop, axis=1)
        print('Ignoring fingers:', ', '.join(FINGERS[f] for f in ignored_fingers()))
    y = np.array([s['label'] for s in samples])
    labels, counts = np.unique(y, return_counts=True)
    print(f'{len(y)} recordings, {len(labels)} signs:', dict(zip(labels.tolist(), counts.tolist())))
    folds = min(5, counts.min())
    if folds < 2 or len(labels) < 2:
        print('Need at least 2 signs with 2+ recordings each.'); return
    cv = StratifiedKFold(folds, shuffle=True, random_state=0)

    models = {
        'KNN (k=5, as in the app)': make_pipeline(StandardScaler(), KNeighborsClassifier(5)),
        'Random Forest': RandomForestClassifier(300, random_state=0),
    }
    best = None
    for name, m in models.items():
        pred = cross_val_predict(m, X, y, cv=cv)
        acc = accuracy_score(y, pred)
        print(f'\n=== {name}: {acc * 100:.1f}% ({folds}-fold cross-validation)')
        print(classification_report(y, pred, zero_division=0))
        if best is None or acc > best[1]:
            best = (name, acc, pred)

    try:
        import matplotlib
        matplotlib.use('Agg')
        import matplotlib.pyplot as plt
        from sklearn.metrics import ConfusionMatrixDisplay
        cm = confusion_matrix(y, best[2], labels=labels)
        fig, ax = plt.subplots(figsize=(1 + 0.7 * len(labels), 1 + 0.6 * len(labels)))
        ConfusionMatrixDisplay(cm, display_labels=labels).plot(ax=ax, cmap='Greens', colorbar=False, xticks_rotation=45)
        ax.set_title(f'{best[0]}: {best[1] * 100:.1f}%')
        fig.tight_layout(); fig.savefig(f'confusion_matrix_{kind}.png', dpi=150)
        print(f'Saved confusion_matrix_{kind}.png')
    except ImportError:
        print('Install matplotlib to save the confusion matrix picture.')


if __name__ == '__main__':
    main()
