# StudioFros - Vinyl

**StudioFros - Vinyl** is a free **VST3 plugin and standalone app** that adds the character of vinyl
records to any audio: crackle, dust, surface noise, wow & flutter, warp, warmth, ageing, mono bass and
more. It has a rack-style interface with real-response VU meters. Made by **GuitarFros**.

* Formats: **VST3** + **Standalone** (stereo)
* Platforms: **Linux**, **macOS** (Apple Silicon + Intel) and **Windows**
* Built with [JUCE](https://juce.com) and CMake

## Controls

| Section | Controls |
|---|---|
| Knobs | Input, Warmth/Character, Crackle, Dust, Hiss/Noise, Wow, Flutter, Warp, Age, Tone, Width, Mix, Output |
| Switches | Speed (33 / 45 / 78 RPM), Surface (Clean / Used / Worn), Mono Bass, Rumble, Power, Bypass |
| Meters / LEDs | Input and Output VU meters (0 VU = -18 dBFS RMS), Power, Bypass, Clip, Crackle, Active |
| Presets | 19 factory presets, previous/next arrows, preset list, **Save**, **Save As**, **Delete**, **Init** |

User presets are saved as XML files in:

* Linux: `~/.config/GuitarFros/StudioFros Vinyl/Presets/`
* macOS: `~/Library/GuitarFros/StudioFros Vinyl/Presets/`
* Windows: `%APPDATA%\GuitarFros\StudioFros Vinyl\Presets\`

## Download

Ready-made builds for the three systems are produced by GitHub Actions on every push:
open the **Actions** tab, pick the latest run and download the artifact for your system.
When a version tag (`v0.7.0`, ...) is pushed, the builds are also attached to a **Release**.

### Install

Copy the `StudioFros - Vinyl.vst3` folder to your VST3 folder and rescan plugins in your DAW:

| System | VST3 folder |
|---|---|
| Linux | `~/.vst3/` |
| macOS | `~/Library/Audio/Plug-Ins/VST3/` (or `/Library/Audio/Plug-Ins/VST3/`) |
| Windows | `C:\Program Files\Common Files\VST3\` |

On macOS the downloaded files are not notarized. If the system refuses to load them, run:
`xattr -dr com.apple.quarantine "StudioFros - Vinyl.vst3"` (and the same for the standalone `.app`).

### Publishing a release (maintainers)

The workflow builds all three systems and, when a tag starting with `v` is pushed, attaches the three ZIP files
(`StudioFros-Vinyl-Linux.zip`, `...-macOS.zip`, `...-Windows.zip`) to a GitHub Release:

```bash
git tag v0.7.1
git push origin v0.7.1
```

If the release step is denied, open the repository **Settings > Actions > General > Workflow permissions** and
choose *Read and write permissions*.

## Build from source

You need CMake 3.22+, Git, a C++17 compiler and an internet connection (JUCE is downloaded automatically
the first time you configure).

### Linux (Ubuntu / Debian)

```bash
sudo apt install build-essential cmake git libasound2-dev libjack-jackd2-dev ladspa-sdk \
  libfreetype-dev libfontconfig1-dev libx11-dev libxcomposite-dev libxcursor-dev libxext-dev \
  libxinerama-dev libxrandr-dev libxrender-dev libglu1-mesa-dev mesa-common-dev

cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
```

The VST3 is copied to `~/.vst3` automatically. The standalone is in
`build/StudioFrosVinyl_artefacts/Release/Standalone/`. (`./build.sh` does the same and lists the results.)

### macOS

Install the Xcode command line tools (`xcode-select --install`) and CMake, then:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j
```

The result is a universal binary (arm64 + x86_64). The VST3 is copied to `~/Library/Audio/Plug-Ins/VST3`.

### Windows

Install Visual Studio 2022 (workload *Desktop development with C++*) and CMake, then, from a terminal:

```bat
cmake -B build
cmake --build build --config Release
```

The VST3 is in `build\StudioFrosVinyl_artefacts\Release\VST3\`. By default it is **not** copied
automatically (the system VST3 folder needs administrator rights). To install it automatically, run the
terminal as administrator and add `-DSTUDIOFROS_COPY_AFTER_BUILD=ON` when configuring.

### CMake options

| Option | Default | Meaning |
|---|---|---|
| `STUDIOFROS_COPY_AFTER_BUILD` | ON (Linux, macOS) / OFF (Windows) | Copy the VST3 to the plugin folder after building |
| `VST3_COPY_DIR` | system default | Folder where the VST3 is copied |

## Project layout

```
Assets/       panel and switch pictures (embedded in the plugin at build time)
Source/       plugin code (PluginProcessor = audio, PluginEditor = interface)
tools/        helper scripts
.github/      GitHub Actions workflow (builds Linux, macOS and Windows)
```

The VU calibration (0 VU = -18 dBFS RMS) can be changed with `kVuReferenceDb` in
`Source/PluginProcessor.h`. The factory presets are the `kFactory` table at the top of
`Source/PluginProcessor.cpp`.

### Replacing the artwork

All control positions are measured in pixels on the 1920 x 1280 panel image and live in the `layout`
namespace and in the editor constructor of `Source/PluginEditor.cpp`. If you use a different panel
picture, update those coordinates.

## License

The source code is licensed under the **GNU Affero General Public License v3** (see `LICENSE`).
The artwork in `Assets/` was generated with **Meta AI (Meta Platforms, Inc.)** and is distributed under
Meta's terms for this non-commercial, open-source project; it is **not** covered by the AGPL.
See `LICENSE` for the details and for the required credit.

*VST is a registered trademark of Steinberg Media Technologies GmbH.*

## Credits

* Plugin design, code and artwork direction: **GuitarFros**
* [JUCE](https://juce.com) framework (AGPLv3)
* Artwork generated with **Meta AI** (Meta Platforms, Inc.)
* Lettering on the VU dials rendered with Liberation Sans (SIL OFL 1.1)
