#include <iostream>
#include <string>
#include <vector>
#include <atomic>
#include <chrono>
#include <thread>

#ifdef __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#include <CoreServices/CoreServices.h>
#include <CoreGraphics/CoreGraphics.h>
#include <ImageIO/ImageIO.h>
#include <ApplicationServices/ApplicationServices.h> // SetFrontProcess, GetProcessForPID
#elif _WIN32
#include <stdio.h> // _popen, _pclose
#include <windows.h>
#include <wingdi.h>
#endif

using std::cout, std::cin, std::cerr; // IO
using std::string, std::to_string, std::vector, std::pair; // Containers
using std::atomic, std::thread; // Multithreading
using std::chrono::high_resolution_clock, std::chrono::milliseconds; // Timing


constexpr auto RTP_OUTPUT = "-sdp_file output/video_stream -f rtp rtp://0.0.0.0:5050";
constexpr auto RTSP_OUTPUT = "-rtsp_transport tcp -f rtsp rtsp://0.0.0.0:8554/stream";
constexpr auto MPEGTS_OUTPUT = "-vcodec libx264 -preset ultrafast output/video.ts";
constexpr auto MPEG4_OUTPUT = "-vcodec mpeg4 output/video.mp4"; // Default output is mp4

constexpr uint16_t FPS = 30;
constexpr size_t MAX_FRAMES = 7200; // Max frames in output video.
const auto FRAME_DURATION_MS = 1000 / FPS;

#ifdef _WIN32 // This function must be global to pass as a function ptr to EnumWindows.
// https://stackoverflow.com/questions/10246444/how-can-i-get-enumwindows-to-list-all-windows
BOOL CALLBACK enum_window_callback(HWND hwnd, LPARAM active_window_handles_void_ptr)
{
    int length = GetWindowTextLength(hwnd) + 1;
    char * buffer = new char[length];
    GetWindowText(hwnd, buffer, length);
    string window_title(buffer);
    delete[] buffer;

    // If the window is not visible, or is minimized, or has no title, or is system app, don't include it.
    if (!IsWindowVisible(hwnd) || IsIconic(hwnd) || window_title.empty() ||
        window_title == "Program Manager" || window_title == "Setup" ||
        window_title == "Microsoft Text Input Application")
        return TRUE;

    vector<HWND> * active_window_handles_ptr = reinterpret_cast<vector<HWND>*>(active_window_handles_void_ptr);

    cout << active_window_handles_ptr->size() + 1 << ":  " << window_title << "      ";
    active_window_handles_ptr->push_back(hwnd);

    return TRUE;
}
#endif

struct ScreenCapture {
    // ---- CROSS PLATFORM CODE ---- //
    atomic<bool> server_up_flag;

    unsigned int window_height;
    unsigned int window_width;

    bool stream;
    bool continue_on_fail;

    ScreenCapture(bool stream_in, bool continue_on_fail_in): server_up_flag{true}, window_height{0}, window_width{0},
                                                            stream{stream_in}, continue_on_fail(continue_on_fail_in) {}

    /*
     * user_select_window_idx makes the user pick an window from [1, max_window_count]
     * and subtracts 1 (for 0-based indexing)
     *
     * Returns: index from [0, max_window_count)
     */
    size_t user_select_window_idx(size_t max_window_count)
    {
        size_t window_idx = max_window_count+1;

        while (window_idx < 1 || window_idx > max_window_count) { // While invalid input
            cout << "Enter window index to stream from [1 - " << max_window_count << "]: ";
            if (cin >> window_idx) continue;
            // Bad input. Clear error and input buffer
            cin.clear();
            string dmy;
            cin >> dmy;
        }

        return window_idx-1; // Convert to 0-based index
    }

    /*
     * Allows for easy program termination without SIGINT.
     */
    void wait_for_quit_input()
    {
        string input;
        while (server_up_flag) {
            cin >> input;
            if (input == "q" || input == "quit") server_up_flag = false;
        }
    }

    /*
     * Cross platform wrapper for OS-specific display_active_windows() functions.
     */
    bool display_active_windows()
    {
    #ifdef __APPLE__
        return osx_display_active_windows();
    #elif _WIN32
        return win_display_active_windows();
    #endif

        return true;
    }

    /*
     * capture starts capturing video from selected window using ffmpeg.
     * it runs until 'q' or 'quit' is typed.
     */
    bool capture()
    {
        string dimensions = to_string(window_width) + 'x' + to_string(window_height);
        string ffmpeg_cmd =
            "ffmpeg -hide_banner -y -f rawvideo -video_size " + dimensions +
            " -pix_fmt bgra -re -i - -r " + to_string(FPS) + ' ' + RTSP_OUTPUT;
    #ifdef _WIN32
        FILE* streampipe = _popen(ffmpeg_cmd.c_str(), "wb");
    #else
        FILE* streampipe = popen(ffmpeg_cmd.c_str(), "w");
    #endif

        thread quit_listener(&ScreenCapture::wait_for_quit_input, this);

        size_t frame_count = 0;
        bool success;
        high_resolution_clock::time_point start_time, end_time;
        long long remaining_time_ms;

        while (server_up_flag) {
            start_time = high_resolution_clock::now();

            // ---- image piping ---- //
        #ifdef __APPLE__
            success = osx_pipe_image(streampipe);
        #elif _WIN32
            success = win_pipe_image(streampipe);
        #endif

            if (!success) {
                cerr << "ERROR: Couldn't pipe image. Continuing.\n";
                if (!continue_on_fail) {
                    server_up_flag = false;
                    break;
                }
            }

            if (!stream) {
                ++frame_count;
                if (frame_count == MAX_FRAMES) {
                    cout << "Frame count exceeded MAX_FRAMES value of "
                         << MAX_FRAMES << ". Type any key then hit ENTER to exit.\n";
                    server_up_flag = false;

                    break;
                }
            }

            // ---- end image piping ---- //

            end_time = high_resolution_clock::now();

            // convert elapsed time to milliseconds and subtract from max amount
            remaining_time_ms = FRAME_DURATION_MS - (end_time - start_time) / milliseconds(1);

            // If the image piping was too slow, don't sleep and just continue
            if (remaining_time_ms <= 0) 
                continue;

            // Otherwise, sleep a bit to prevent unnecessary overhead
            // 1/2 is just an arbitrary number I picked, I don't want to oversleep
            std::this_thread::sleep_for(milliseconds( remaining_time_ms * 1/2 ));
        }

        quit_listener.join();
    #ifdef _WIN32
        _pclose(streampipe);
    #else
        pclose(streampipe);
    #endif

        return true;
    }
    // ---- END CROSS PLATFORM CODE ---- //

#ifdef __APPLE__ // ---- OSX ONLY ---- //
    CGWindowID selected_window_id;

    /*
     * Structure to hold information about active OSX windows.
     */
    struct ActiveWindowData {
        CFNumberRef id;
        CFDictionaryRef bounds;
        CFNumberRef pid;
    };

    /*
     * osx_pipe_image captures a single image on OSX using the CoreGraphics.h library
     * and pipes it to the `streampipe` argument.
     * 
     * Returns: boolean indicating whether or not the image could be created
     */
    bool osx_pipe_image(FILE* streampipe)
    {
        CGImageRef img = CGWindowListCreateImage(
            CGRectNull, // CGRectNull means capture minimum size needed in order to capture window
            kCGWindowListExcludeDesktopElements | kCGWindowListOptionIncludingWindow,
            selected_window_id,
            kCGWindowImageBestResolution | kCGWindowImageBoundsIgnoreFraming
        );
        if (!img) return false;

        size_t width = CGImageGetWidth(img);
        size_t height = CGImageGetHeight(img);

        if (!width || !height || CGImageGetBitsPerPixel(img) != 32 || CGImageGetBitsPerComponent(img) != 8) {
            CGImageRelease(img);
            return false;
        }

        CFDataRef img_data = CGDataProviderCopyData(CGImageGetDataProvider(img));
        const UInt8 * img_data_ptr = CFDataGetBytePtr(img_data);

        size_t data_length = static_cast<size_t>(CFDataGetLength(img_data));

        fwrite(img_data_ptr, 1, data_length, streampipe);
        fflush(streampipe);

        CGImageRelease(img);
        CFRelease(img_data);

        return true;
    }

    /*
     * osx_display_active_windows displays a list of user windows that can be streamed from on OSX,
     * using the CoreGraphics.h library.
     *
     * Returns: bool indicating whether or not this function succeeded.
     */
    bool osx_display_active_windows()
    {
        CFArrayRef window_infolist_ref = CGWindowListCopyWindowInfo(
            kCGWindowListOptionOnScreenOnly |
            kCGWindowListExcludeDesktopElements |
            kCGWindowListOptionIncludingWindow,
            kCGNullWindowID
        );
        CFIndex window_count = CFArrayGetCount(window_infolist_ref);

        // Window IDs that can actually be streamed from and their bounds, as a dict
        vector<ActiveWindowData> active_windows;

        if (window_infolist_ref == NULL) {
            cerr << "ERROR: couldn't get window list.\n";
            return false;
        }

        // Iterate through window info list and print the name if it exists.
        for (CFIndex idx = 0; idx < window_count; ++idx) {
            CFDictionaryRef window_info_ref = (CFDictionaryRef) CFArrayGetValueAtIndex(window_infolist_ref, idx);
            // If the window doesn't have a name, skip
            if (!CFDictionaryContainsKey(window_info_ref, kCGWindowOwnerName)) continue;

            int window_layer;
            CFNumberRef window_layer_ref = (CFNumberRef) CFDictionaryGetValue(window_info_ref, kCGWindowLayer);
            CFNumberGetValue(window_layer_ref, kCFNumberIntType, &window_layer);
            if (window_layer > 1) continue; // Skip windows that are not at the top layer.

            CFStringRef window_name_ref = (CFStringRef) CFDictionaryGetValue(window_info_ref, kCGWindowOwnerName);
            // This window can be streamed from.
            active_windows.push_back({
                    (CFNumberRef) CFDictionaryGetValue(window_info_ref, kCGWindowNumber),
                    (CFDictionaryRef) CFDictionaryGetValue(window_info_ref, kCGWindowBounds),
                    (CFNumberRef) CFDictionaryGetValue(window_info_ref, kCGWindowOwnerPID)
            });
            const char * window_name = CFStringGetCStringPtr(window_name_ref, kCFStringEncodingUTF8);
            cout << active_windows.size() << ". "
                << (window_name != nullptr ? window_name : "Undefined window, sorry!") << "      ";
        }
        cout << '\n';

        // Get ID of selected window.
        size_t selected_window_idx = user_select_window_idx(active_windows.size());
        ActiveWindowData window_data = active_windows[selected_window_idx];
        CFNumberGetValue(window_data.id, kCGWindowIDCFNumberType, &selected_window_id);

        // Bring window to foreground
        int pid;
        ProcessSerialNumber psn;

        CFNumberGetValue(window_data.pid, kCFNumberIntType, &pid);
        GetProcessForPID(pid, &psn);

        SetFrontProcess(&psn);

        // Set bounds of ScreenCapture struct
        CGRect window_bounds;
        CGRectMakeWithDictionaryRepresentation(window_data.bounds, &window_bounds);

        window_height = static_cast<unsigned int>(CGRectGetHeight(window_bounds)*2);
        window_width = static_cast<unsigned int>(CGRectGetWidth(window_bounds)*2);

        // Clean up memory
        CFRelease(window_infolist_ref);

        return true;
    }
#endif // ---- END OSX ONLY ---- //

#ifdef _WIN32 // ---- Windows ONLY ---- //
    HWND selected_window_handle;
    LONG window_left;
    LONG window_top;

    /*
     * win_pipe_image captures a single image on Windows using the wingdi.h library
     * and pipes it to the `streampipe` argument.
     * 
     * Returns: boolean indicating whether or not the image could be created
     */
    bool win_pipe_image(FILE* streampipe)
    {
        bool success = true;

        // https://www.daniweb.com/programming/software-development/threads/119804/screenshot-maybe-win32api#post593378
        HDC window_device_context = GetDC(HWND_DESKTOP);
        HDC target_device_context = CreateCompatibleDC(window_device_context);
        HBITMAP hbitmap = CreateCompatibleBitmap(
            window_device_context,
            window_width,
            window_height
        );
        // Select window bitmap into target DC
        SelectObject(target_device_context, hbitmap);
        // Copy bits from window DC to target DC
        BitBlt(
            target_device_context, 0, 0, window_width, window_height,
            window_device_context, window_left, window_top, SRCCOPY
        );

        // This struct is needed as a config for getting raw bits from a bitmap
        LPBITMAPINFO lp_bitmap_info = (LPBITMAPINFO) (new char[sizeof(BITMAPINFOHEADER) + 256 * sizeof(RGBQUAD)]);
        ZeroMemory(&lp_bitmap_info->bmiHeader, sizeof(BITMAPINFOHEADER));
        lp_bitmap_info->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);

        // Populate lp_bitmap_info
        GetDIBits(target_device_context, hbitmap, 0, window_height, NULL, lp_bitmap_info, DIB_RGB_COLORS); 
        char * image_buffer = new char[lp_bitmap_info->bmiHeader.biSizeImage];
        // DIBs are stored upside down in memory. Make height negative to flip image
        lp_bitmap_info->bmiHeader.biHeight *= -1;
        // Fill buffer
        GetDIBits(target_device_context, hbitmap, 0, window_height, image_buffer, lp_bitmap_info, DIB_RGB_COLORS);

        if (success)
            // Pipe image
            fwrite(image_buffer, 1, lp_bitmap_info->bmiHeader.biSizeImage, streampipe);

        // Memory cleanup
        ReleaseDC(NULL, window_device_context);
        ReleaseDC(NULL, target_device_context);
        DeleteObject(hbitmap);
        DeleteObject(lp_bitmap_info);
        delete[] image_buffer;

        return success;
    }

    /*
     * win_display_active_windows displays a list of user windows that can be streamed from on Windows,
     * using the Windows.h library.
     *
     * Returns: bool indicating whether or not this function succeeded.
     */
    bool win_display_active_windows()
    {
        vector<HWND> active_window_handles;

        EnumDesktopWindows(NULL, enum_window_callback, reinterpret_cast<LPARAM>(&active_window_handles));
        cout << '\n';

        size_t selected_window_idx = user_select_window_idx(active_window_handles.size());
        selected_window_handle = active_window_handles[selected_window_idx];

        // Bring window to foreground
        SetForegroundWindow(selected_window_handle);

        RECT window_bounds;
        GetWindowRect(selected_window_handle, &window_bounds);
        window_height = window_bounds.bottom - window_bounds.top;
        window_width = window_bounds.right - window_bounds.left;
        window_left = window_bounds.left;
        window_top = window_bounds.top;

        return true;
    }

#endif // ---- Windows ONLY ---- //

};
