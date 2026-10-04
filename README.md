# LXQt Resource Monitor

Resource Monitor is a dynamically loaded **LXQt Panel 2.3.2** plugin for Lubuntu 26.04. It displays selected system and I/O meters together in one panel item, styled after LXQt's CPU Monitor.

The commands below assume you have already cloned this repository and are running them from its `lxqt-resource-monitor/` folder.

## Appearance and defaults

- Optional meters for CPU, RAM, SWAP, local disk I/O, local network I/O and Internet I/O. CPU, RAM and SWAP are selected by default.
- Bottom-up vertical columns by default; top-down changes which edge represents zero. Each one-pixel-wide column is one EMA sample. New columns enter on the right and older columns roll left; column height represents the EMA. The newest column is brighter to make the rolling direction visible. At the default 19 px per resource, the graph shows the latest 19 one-second EMA samples. The overlaid number remains the latest live value.
- CPU is green, RAM blue, SWAP amber, disk purple, local network teal and Internet red. The meters have no borders, leaving their full width for the resource display and value text.
- Rounded usage values are shown over the bars without a `%` postfix; tooltips retain percentages.
- Default width is **57 px for three selected resources**, giving each bar 19 px. The widget grows or shrinks with the number of selected resources, keeping each bar the same width. The setting is configurable live from 48 to 300 px for three resources.
- The widget update interval defaults to one second and is configurable from 0.5 seconds; EMA sampling and repainting run once per second regardless of that setting.
- The tooltip shows percentages, used/total RAM and SWAP values, and read/write rates for enabled I/O meters.
- Network traffic is classified by its remote IP address: private/local IP traffic appears under Local net; public IP traffic appears under Internet. The optional `resourcemonitor-netcap` helper reads IP headers only and does not save addresses or packet contents. Network sampling requires this helper to be installed as a root-owned executable with `CAP_NET_RAW`; the helper drops that capability after opening its packet socket.
- If swap is not configured, its meter is empty and the tooltip says `SWAP: not configured`.

RAM usage uses libstatgrab's `used / total` values. On Linux, libstatgrab treats cached file memory as reclaimable rather than used memory.

## Run the CTests

The CTests cover resource formatting, CPU sample validity, live libstatgrab sampling, EMA history ordering and pixel-column mapping, and IPv4/IPv6 traffic classification including malformed packets and address-range boundaries. The CPU regression test includes the libstatgrab 0.92 behavior where a non-null aggregate CPU sample reports zero entries. On Ubuntu/Lubuntu:

```bash
sudo apt install build-essential cmake ninja-build qt6-base-dev libstatgrab-dev
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

## Build the LXQt Panel plugin

The plugin uses private headers from LXQt Panel 2.3.2. The build script downloads that source and compiles only the resource monitor with a small standalone CMake project; it does not configure or build LXQt Panel or its other plugins. Run these commands from the `lxqt-resource-monitor/` folder.

Install the build dependencies directly. This does not require source-package (`deb-src`) entries:

```bash
sudo apt update
sudo apt install build-essential cmake ninja-build git dpkg-dev qt6-base-dev libkf6windowsystem-dev liblxqt2-dev libstatgrab-dev
```

Then build and package the plugin with one command:

```bash
./packaging/build.sh
```

The packages are written to `build/packages/`; intermediate build files are in `build/lxqt-panel-2.3.2-resource-monitor/`. On x86-64, the plugin and helper are compiled for the x86-64 baseline with SSSE3 enabled and SSE4.1, SSE4.2, and AVX disabled. A newer build host therefore cannot silently produce binaries that require those newer instructions. Other architectures use their toolchain's default target.

## Build a binary Debian package

You can compile on one computer and install binary `.deb` files on another. The target computer does not need `deb-src`, a compiler, or the LXQt Panel source tree. The `lxqt-resource-monitor` package contains the panel plugin. The separate `lxqt-resource-monitor-netcap` package is optional and is needed only for Local net and Internet traffic meters.

Install the base plugin package:

```bash
sudo apt install ./build/packages/lxqt-resource-monitor_1.0.8-1_$(dpkg --print-architecture).deb
```

For IP-based Local net and Internet meters, also install the optional helper package:

```bash
sudo apt install ./build/packages/lxqt-resource-monitor-netcap_1.0.8-1_$(dpkg --print-architecture).deb
```

The build script records the build computer's exact `lxqt-panel` package version and detects shared-library dependencies. APT fetches missing runtime packages during installation. The packages are architecture and Ubuntu-release specific, and require the same `lxqt-panel` package version as the build computer. After installation, restart LXQt Panel and add **Resource Monitor** in **Panel Settings → Widgets**. To remove both packages, run `sudo apt remove lxqt-resource-monitor lxqt-resource-monitor-netcap`.

## Install for all users

The Debian packages handle shared-library dependencies and helper capabilities automatically. For a manual plugin-only install, use the build output path below. This leaves the packaged `lxqt-panel` executable and existing plugins unchanged:

```bash
MULTIARCH="$(dpkg-architecture -qDEB_HOST_MULTIARCH)"
PLUGIN_BUILD="build/lxqt-panel-2.3.2-resource-monitor/plugin-resourcemonitor"
sudo install -Dm755 "$PLUGIN_BUILD/libresourcemonitor.so" "/usr/lib/${MULTIARCH}/lxqt-panel/libresourcemonitor.so"
sudo install -Dm644 "$PLUGIN_BUILD/resourcemonitor.desktop" /usr/share/lxqt/lxqt-panel/resourcemonitor.desktop
```

For Local net and Internet meters only, also install the capture helper and grant its network capability:

```bash
sudo install -Dm755 "$PLUGIN_BUILD/resourcemonitor-netcap" /usr/bin/resourcemonitor-netcap
sudo chown root:root /usr/bin/resourcemonitor-netcap
sudo setcap cap_net_raw=ep /usr/bin/resourcemonitor-netcap
```

Restart the LXQt Panel or sign out and back in to make it rescan plugins. Then open **Panel Settings → Widgets**, choose **Add**, and select **Resource Monitor**. Its settings dialog controls enabled resources, value text, update interval, EMA column height direction, and width; changes apply live.

To remove the all-users installation:

```bash
MULTIARCH="$(dpkg-architecture -qDEB_HOST_MULTIARCH)"
sudo rm -f "/usr/lib/${MULTIARCH}/lxqt-panel/libresourcemonitor.so"
sudo rm -f /usr/share/lxqt/lxqt-panel/resourcemonitor.desktop
sudo rm -f /usr/bin/resourcemonitor-netcap
```

## First visual check

Try the default **57 px** width with CPU, RAM and SWAP selected. The three resource meters are each 19 pixels wide, with the latest EMA column at the right and older columns moving left. The overlaid number shows the current live value. Enable the optional disk and network meters from the settings dialog. Network meters require the optional capture helper with `CAP_NET_RAW`.

## License

The plugin source is distributed under the GNU Lesser General Public License, version 2.1 or (at your option) any later version. See [LICENSE](LICENSE).
