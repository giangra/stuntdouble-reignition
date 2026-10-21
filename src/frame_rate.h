// stuntman - frame rate. See frame_rate.cpp.

#pragma once

// The game's own frame rate (swaps per second), averaged over the last
// half second. 0 until the game presents its first frames.
float GuestFps();
