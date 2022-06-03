## Async io ffmpeg demo
Async io impl, [FFmpeg](https://github.com/fdimushka/FFmpeg) use [async_runtime](https://github.com/fdimushka/async_runtime) 
library.

## Build instructions
To build you will need:

* [async_runtime](https://github.com/fdimushka/async_runtime) >= 0.1.0
* [ffmpeg](https://github.com/fdimushka/FFmpeg) >= 4.6
* libuv >= 1.44.1

```
mkdir build
cd cmake -DFFMPEG_INSTALL_DIR=/path/to/installed/dir/ffmpeg -DAR_INSTALL_DIR=/path/to/installed/dir/ar ..
make 
./async_video_read
``` 

