# gst-sensors
gst-sensors contains GStreamer plugins designed to stream sensor data, such as
GPS or accelerometer data. It is designed with the goal of enabling sensor data
as a first-class citizen in GStreamer, with buffer types, encoders, muxers, and
other support.

## Build

### Prerequisites
- meson: `pip3 install meson`
- ninja: `pip3 install ninja`
- gst-plugins-base/gstreamer: `apt install gstreamer1.0-plugins-base` or similar
  on other distros

## Instructions

### First time builds

```
mkdir build
cd build
meson ..
ninja
```

### Rebuilding

To rebuild at any time, you just need to rerun the last `ninja` command:

```
ninja
```

You can run this command from any directory you like. For instance, to rebuild
from the top level directory, use the ninja `-C` option to point ninja at the
build directory:

```
ninja -C build
```
