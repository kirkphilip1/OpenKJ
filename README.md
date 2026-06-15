[![Language grade: C/C++](https://img.shields.io/lgtm/grade/cpp/g/OpenKJ/OpenKJ.svg?logo=lgtm&logoWidth=18)](https://lgtm.com/projects/g/OpenKJ/OpenKJ/context:cpp)
[![Copr build status](https://copr.fedorainfracloud.org/coprs/openkj/OpenKJ-unstable/package/openkjtools/status_image/last_build.png)](https://copr.fedorainfracloud.org/coprs/openkj/OpenKJ-unstable/package/openkjtools/)
[![Windows Build](https://github.com/OpenKJ/OpenKJ/actions/workflows/windows-test.yml/badge.svg)](https://github.com/OpenKJ/OpenKJ/actions/workflows/windows-test.yml)
[![Test building on macOS](https://github.com/OpenKJ/OpenKJ/actions/workflows/macos-test.yml/badge.svg)](https://github.com/OpenKJ/OpenKJ/actions/workflows/macos-test.yml)

**Downloads**  
If you are looking for installers for Windows or macOS, please visit the Downloads section at https://openkj.org

Linux users can grab OpenKJ stable versions from flathub: https://flathub.org/apps/details/org.openkj.OpenKJ

If you would like to install Linux versions of the unstable builds, please refer to the OpenKJ documentation wiki.

Documentation can be found at https://docs.openkj.org

If you need help with OpenKJ, you can reach out to support@openkj.org via email.

OpenKJ
======

Cross-platform open source karaoke show hosting software.

OpenKJ is a fully featured karaoke hosting program.
A few features:
* Save/track/load regular singers
* Key changer
* Tempo control
* EQ
* End of track silence detection (after last CDG draw command)
* Rotation ticker on the CDG display
* Option to use a custom background or display a rotating slide show on the CDG output dialog while idle
* Fades break music in and out automatically when karaoke tracks start/end
* Remote request server integration allowing singers to look up and submit songs via the web or mobile apps
* Automatic performance recording
* Autoplay karaoke mode
* Lots of other little things

It currently handles media+g zip files (zip files containing an mp3, wav, or ogg file and a cdg file) and paired mp3 and cdg files. It also can play non-cdg based video files (mkv, mp4, mpg, avi) for both break music and karaoke.

Database entries for the songs are based on the file naming scheme. Custom patterns can also be defined in the program using regular expressions.

---

## Building OpenKJ

OpenKJ is built using CMake and requires a C++20 compliant compiler, Qt6, and LibVLC. 
*Note: `spdlog`, `libzip`, and `taglib` are automatically fetched and built statically using CPM (CMake Package Manager) during configuration.*

### Requirements
- **Qt 6** (Core, Gui, Sql, Network, NetworkAuth, Widgets, Concurrent, Svg, PrintSupport, Multimedia, WebSockets)
- **LibVLC** (3.0+)
- **CMake** (3.14+)
- **Ninja** or **Make**

---

### **macOS**

1. **Install Dependencies** (using Homebrew):
   ```bash
   brew install cmake ninja pkgconf cppunit utf8cpp qt
   brew install --cask vlc
   ```

2. **Configure & Build**:
   ```bash
   # Get the Qt installation prefix from Homebrew
   QT_PREFIX=$(brew --prefix qt)

   # Configure the build directory
   cmake -DSPDLOG_USE_BUNDLED=true -DCMAKE_BUILD_TYPE=Release -DBUILDONLY=True -DCMAKE_PREFIX_PATH=$QT_PREFIX -B build -G Ninja

   # Compile the app
   cmake --build build
   ```
   The built `openkj.app` bundle will be located inside the `build/` directory.

---

### **Linux**

#### Ubuntu / Debian
1. **Install Dependencies**:
   ```bash
   sudo apt update
   sudo apt install build-essential cmake ninja-build pkg-config \
                    qt6-base-dev qt6-svg-dev qt6-multimedia-dev \
                    qt6-networkauth-dev qt6-websockets-dev libvlc-dev
   ```

#### Fedora
1. **Install Dependencies** (Note: `vlc-devel` requires the **RPM Fusion Free** repository to be enabled):
   ```bash
   sudo dnf install cmake ninja-build gcc-c++ pkgconfig \
                    qt6-qtbase-devel qt6-qtsvg-devel qt6-qtmultimedia-devel \
                    qt6-qtnetworkauth-devel qt6-qtwebsockets-devel vlc-devel
   ```

#### Build Commands
```bash
# Configure the build directory
cmake -DSPDLOG_USE_BUNDLED=true -DCMAKE_BUILD_TYPE=Release -B build -G Ninja

# Compile the application
cmake --build build
```

---

### **Windows**

1. **Install Dependencies**:
   - **Visual Studio 2022**: Install with the "Desktop development with C++" workload (MSVC compiler).
   - **Qt 6**: Install via the Qt Online Installer. Select the **MSVC 2022 64-bit** compiler component, along with modules for **SVG**, **Network Auth**, **Multimedia**, and **WebSockets**.
   - **CMake** and **Ninja** (available via Visual Studio installer, or standalone).
   - **LibVLC SDK**: Download the 64-bit VLC zip file (VLC 3.x) from VideoLAN and extract it to a directory, e.g., `C:/vlc-sdk`.

2. **Configure & Build** (Run inside the **x64 Native Tools Command Prompt for VS 2022**):
   ```cmd
   # Configure the build directory (pointing to the extracted LibVLC SDK)
   cmake -B build -DCMAKE_BUILD_TYPE=Release -DLIBVLC_SDK_PATH="C:/vlc-sdk" -G Ninja

   # Compile the application
   cmake --build build
   ```
