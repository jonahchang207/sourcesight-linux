#pragma once

#include <string>

// Composited full-screen capture: game + transparent overlay together.
//
// Why this exists: glReadPixels / an FBO readback would only contain the
// overlay's own transparent framebuffer. The game is a *separate* window /
// surface below us. The only image that contains both is the compositor's
// final output, so this utility captures at that level:
//
//   Linux (X11/XWayland) : XGetImage() on the root window, or `grim` when
//                          running under Wayland/Hyprland where the X root
//                          does not see native Wayland surfaces.
//   Video                : `wf-recorder` (PipeWire, preferred on Wayland) or
//                          `ffmpeg -f x11grab` fallback. Both record the
//                          composited desktop, so overlay ESP + game appear.
//
// Typical usage:
//   ScreenCapture::CaptureScreenshot();            // ./captures/shot_*.ppm|png
//   ScreenCapture::RecordEntireScreen();           // start ./captures/*.mp4
//   ... play ...
//   ScreenCapture::StopRecording();
//
// No new link dependencies: screenshots use Xlib (already linked) and write
// dependency-free PPM; video shells out to an external recorder binary.
class ScreenCapture {
public:
    ScreenCapture() = delete;

    // Grab one composited frame (overlay + game) to disk.
    // Empty path -> auto-generated under ./captures/.
    // Returns true on success. On X11-fallback the file is PPM (P6); via
    // `grim` it is PNG as produced by grim itself.
    static bool CaptureScreenshot(const std::string& path = "");

    // Start recording the entire composited screen (overlay + game) to a
    // video file. Empty path -> auto-generated ./captures/*.mp4.
    // Returns false if already recording or no recorder backend is found.
    // This is the "record the entire screen" entry point.
    static bool StartRecording(const std::string& path = "", int fps = 60);

    // Alias with the literal requested name; identical to StartRecording().
    static bool RecordEntireScreen(const std::string& path = "", int fps = 60);

    // Gracefully stop the active recording (SIGINT -> finalize MP4).
    // No-op when not recording.
    static void StopRecording();

    static bool IsRecording();
    static std::string ActiveRecordingPath();

    static std::string DefaultScreenshotPath();
    static std::string DefaultRecordingPath();
};
