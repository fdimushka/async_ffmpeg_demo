cmake_minimum_required(VERSION 3.20)
project(async_video_read)

set(CMAKE_CXX_STANDARD 17)
option(FFMPEG_INSTALL_DIR /usr/local/)
option(AR_INSTALL_DIR /usr/local/)

include_directories(${FFMPEG_INSTALL_DIR}/include)
include_directories(${AR_INSTALL_DIR}/include)

link_directories(${FFMPEG_INSTALL_DIR}/lib
                 ${AR_INSTALL_DIR}/lib64)

add_executable(async_video_read main.cpp)

target_link_libraries(async_video_read pthread avcodec avformat avcodec swscale swresample avutil avfilter ar uv m lzma z bz2)
