#include <iostream>

#include "screencapture.cpp"

using std::cerr;


int main()
{
    ScreenCapture capturer(true, false);

    // Select a screen to stream from
    bool success = capturer.display_active_windows();

    if (!success) {
        cerr << "ERROR: couldn't select window to stream from.\n";
        return 1;
    }

    // Start capturing
    success = capturer.capture();

    if (!success) {
        cerr << "ERROR: failed to capture screen.\n";
        return 1;
    }

    return 0;
}
