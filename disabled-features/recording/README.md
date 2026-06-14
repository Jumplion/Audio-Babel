# Recording Feature (Disabled)

This directory contains the code for the "Record" page that was previously
part of the Audio Babel site. The feature has been disabled and removed from
the live site, but the code is preserved here for potential future use.

## What's here

- `record.html` — the standalone Record page (records microphone audio,
  converts it to WAV, and extracts its audio index).
- `recorder.js` — the `createRecorder` controller used by the page
  (start/stop recording, upload/extract index).
- `record.css` — styles specific to the Record page.
- `wavConversion.js` — `convertWebMToWav`, extracted from
  `docs/js/utils/wavUtils.js` since it was only used by the recorder.

## What changed in the live site

- The "Record" link was removed from `docs/index.html` and
  `docs/components/navbar.html`.
- The recorder wiring (import of `createRecorder`, event listeners for
  `recordStartStop`/`uploadRecording`, and related `setLoading` references)
  was removed from `docs/js/pages/main.js`.
- `convertWebMToWav` was removed from `docs/js/utils/wavUtils.js` (moved to
  `wavConversion.js` above) since nothing else used it.

## How to restore

1. Move `record.html`, `recorder.js`, and `record.css` back into
   `docs/`, `docs/js/pages/`, and `docs/css/` respectively, and fix the
   import/asset paths (they currently point back at `../../docs/...` to
   work from this archived location).
2. Move `convertWebMToWav` from `wavConversion.js` back into
   `docs/js/utils/wavUtils.js` (or keep it as a separate module and update
   `recorder.js`'s import accordingly).
3. Re-add the recorder wiring block to `docs/js/pages/main.js` (import
   `createRecorder`, the `recordStartStop`/`uploadRecording` element lookups
   and `setLoading` controls, and the event listeners) — see git history for
   the removed block.
4. Re-add the "Record" links to `docs/index.html` and
   `docs/components/navbar.html`.
