# Hammer Launcher

A lightweight, modern C++ launcher designed for Source engine mappers. It automatically detects installed Valve games across all Steam libraries and provides a streamlined interface to launch Hammer, Hammer++, or Source 2 Workshop Tools.

## Features

* **Automatic Game Detection:** Scans all local drives and Steam libraries for installed Valve games.
* **Smart Filtering:** Only displays games that actually include the Hammer editor or Source 2 tools.
* **Steam Integration:** Parses local Steam configs to display the currently active profile (Avatar, Nickname, ID).
* **Cache-based Assets:** Extracts game banners and icons directly from the local Steam `librarycache`, meaning no internet connection is required to render the UI.
* **Hammer++ Support:** Detects installed Hammer++ executables and provides a built-in installation guide (including compatibility tables and target directories).
* **Source 2 Support:** Automatically detects Source 2 games (CS2, HLA, Dota 2) and launches Workshop Tools via Steam URL protocol.

## Technologies Used

* **Language:** C++17
* **GUI Framework:** [wxWidgets](https://www.wxwidgets.org/)
* **Windows APIs:** WinHTTP (for downloading missing assets), WIC (Windows Imaging Component for WebP/PNG/JPG decoding), Registry (for Steam path detection).

## How to Build

To compile this project from source, you will need:
1. **Visual Studio 2022** (with "Desktop development with C++" workload).
2. **wxWidgets** (compiled for MSVC 2022 x64).

### Build Steps

* **1. Clone the repository:**
git clone https://github.com/Floppaponel/HammerLauncher.git

* **2. Open HammerLauncher.sln in Visual Studio 2022.**
* **3. Ensure the solution platform is set to x64 and configuration to Release.**
* **4. Link wxWidgets to the project:**
   - Open Project Properties -> C/C++ -> General.
   - In Additional Include Directories, add the path to your wxWidgets include folder and include/msvc.
   - Open Linker -> General.
   - In Additional Library Directories, add the path to your wxWidgets lib/vc14x_x64_dll (or vc14x_x64_lib if using static linking).
* **5. Ensure the resource.rc file is included in the project (it compiles the app.ico into the .exe).**
* **6. Build the solution (Ctrl+Shift+B).**

### Notes
- If you are compiling the Release version, ensure you have copied the required .dll files from wxWidgets/lib/vc14x_x64_dll into the output folder next to your .exe.
- The application requires app.ico and arrow.png to be present in the executable's directory for the UI to render correctly.
- The logic.cpp file relies on winhttp.lib and windowscodecs.lib (both are included in the standard Windows SDK).
## Usage
1. Download the latest .zip archive from the Releases page.
2. Extract the archive to any folder on your PC.
3. Run HammerLauncher.exe.
## License
This project is licensed under the MIT License. See the LICENSE file for details.
