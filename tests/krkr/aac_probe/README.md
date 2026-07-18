# KRKR AAC compatibility probe

This fixture verifies the compatibility path used by games that store an
AAC-in-MP4 file under an `.ogg` name. Copy a real M4A/AAC sample to
`probe_audio.ogg`, launch the project with the ARM64 KRKR core, and require an
`AAC_PLAY_OK` exception after the playback position advances for 1.5 seconds.

The audio sample is deliberately not stored in the repository.
