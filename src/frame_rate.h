// stuntman - frame rate. See frame_rate.cpp.

#pragma once

// The game's own frame rate (swaps per second), averaged over the last
// half second. 0 until the game presents its first frames.
float GuestFps();

// Makes the emulated vblank match frame_limiter (free-running with it, paced
// at the video mode's refresh without). Called at startup and on every swap,
// so changes from the menu apply at once.
void SyncFramePacing();
