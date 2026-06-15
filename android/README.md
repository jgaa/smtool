# SMtool Android App

This is the Android companion app for SMtool, designed for fast and frictionless idea capture via voice and text.

## Credits & Acknowledgements

The voice transcription capabilities in this application are built upon the excellent work of the [NotelyVoice](https://github.com/Notely-Voice/NotelyVoice) project. 

Specifically, this project adapts:
- The embedded `whisper.cpp` native integration and JNI wrappers.
- The `StreamingAudioChunker` for efficient, memory-safe processing of large audio files.
- The core transcription orchestration patterns.

We are grateful to the NotelyVoice contributors for providing a robust foundation for offline, privacy-focused speech-to-text on Android.
