#pragma once

void GifScreen_setup(int slot);
void GifScreen_render(int slot);
bool GifScreen_key(int index, bool pressed);

// Called when the web UI replaces the file the active screen is playing.
void GifScreen_close();
