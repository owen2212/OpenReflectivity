#ifndef SCREENSHOT_HPP
#define SCREENSHOT_HPP

#include <string>

// reads the back buffer into a timestamped PNG under out_dir. Call after
// all drawing for the frame but before the buffer swap.
bool save_screenshot_png(int fb_width, int fb_height,
                         const std::string &out_dir = "screenshots",
                         std::string *out_path = nullptr);

// same capture to an explicit path (OPENREFL_AUTOSHOT)
bool save_screenshot_png_to(int fb_width, int fb_height, const std::string &path);

#endif
