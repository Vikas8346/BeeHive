# Beehive
A screen recording tool that can save to disk (MP4 or MPTS) or stream to a server (RTSP).

## Dependencies
`ffmpeg` is the only dependency. It needs to be installed and should be in PATH (beehive will run commands like `ffmpeg ...`).

## Usage

### Recording
Type `make release` to build the executable, then type `./beehive_release` to run it. Type `q` or `quit` into stdin to quit.

### Streaming
First, you need a RTSP server. Easiest one to use out of the box is [mediamtx](https://github.com/bluenviron/mediamtx). Expose it globally with something like `ngrok`. Then, run beehive like above with RTSP output. Finally, the client can use `viewstream.py` to watch the stream (or any other method to watch rtsp streams).

## Output Formats
with my minimal testing, i've found that mpeg4 has lower overhead, but with much worse quality output (color, bitrate, etc). mpegts (using h264) has better quality with more overhead.

## Known Issues
- Most CG functions are deprecated on Mac. This results in incomprehensible video. Can't do anything about this.
- On Windows, other windows can overlay streaming one
- 2 window instances for fullscreen VSCode on OSX

## Performance stuff to test
- producer and consumer threads for creating images and piping them
- CGDisplayStream vs individual CGWindowListCreateImage calls
- kCGWindowImageNominalResolution vs kCGWindowImageBestResolution
