# Circuit Grid on Android

Landscape, touch-first build, laid out for 2400x1080 (scales to other sizes).

## Controls
Left bar: Wire, Switch, Lamp, AND, OR, NOT, XOR, Erase, Pan.
Right bar: Rotate (shows placement direction), Play/Pause, Step, Faster, Slower
(yellow pips show speed), Save, Load (flash green/red), Recenter.

On the canvas, tap to place a cell. Tapping a switch toggles it, and tapping a gate with
the same gate tool rotates it. Drag to paint, and use two fingers to pan and pinch-zoom.
The circuit autosaves when the app goes to the background and reloads on start.

## Build
1. In Android Studio's SDK Manager install **NDK 25.2.9519653**, CMake and Ninja.
   (Another NDK works if you change `ndkVersion` in `app/build.gradle` to match.)
2. Build SFML 2.6.1 **Release** for each ABI. It installs into the NDK at
   `sources/third_party/sfml`:

       export NDK=$ANDROID_HOME/ndk/25.2.9519653
       git clone -b 2.6.1 https://github.com/SFML/SFML.git && cd SFML
       for abi in arm64-v8a armeabi-v7a; do
         cmake -S . -B build-$abi -G Ninja \
           -DCMAKE_SYSTEM_NAME=Android -DCMAKE_SYSTEM_VERSION=21 \
           -DCMAKE_ANDROID_ARCH_ABI=$abi -DCMAKE_ANDROID_NDK=$NDK \
           -DCMAKE_ANDROID_STL_TYPE=c++_shared -DCMAKE_BUILD_TYPE=Release
         cmake --build build-$abi && cmake --install build-$abi
       done

   If you build SFML in Debug instead, library names gain a `-d` suffix: update
   `Android.mk` and the `sfml-activity` entry in `AndroidManifest.xml` to match.
3. Open the `android/` folder in Android Studio (it will create the Gradle wrapper), plug in
   the phone with USB debugging on, and press Run.

The desktop build is unchanged. `MobileGame` also accepts mouse input on desktop
(left button = one finger, wheel = zoom) for quick testing without a phone.
