# LXQt Resource Monitor

Resource Monitor is a dynamically loaded **LXQt Panel 2.3.2** plugin for Lubuntu 26.04. It displays selected system and I/O meters together in one panel item, styled after LXQt's CPU Monitor.

## Appearance and defaults

- Optional meters for CPU, RAM, SWAP, local disk I/O, local network I/O and Internet I/O. CPU, RAM and SWAP are selected by default.
- Bottom-up vertical bars by default; top-down, left-to-right and right-to-left are configurable.
- CPU is green, RAM blue, SWAP amber, disk purple, local network teal and Internet red. The colored outlines remain visible at 0%, so the meters are identifiable without labels.
- Rounded usage values are shown over the bars without a `%` postfix; tooltips retain percentages.
- Default width is **57 px for three selected resources**, giving each bar 19 px. The widget grows or shrinks with the number of selected resources, keeping each bar the same width. The setting is configurable live from 48 to 300 px for three resources.
- Sampling interval defaults to one second and is configurable from 0.5 seconds.
- The tooltip shows percentages, used/total RAM and SWAP values, and read/write rates for enabled I/O meters.
- Network traffic is classified by its remote IP address: private/local IP traffic appears under Local net; public IP traffic appears under Internet. The optional `resourcemonitor-netcap` helper reads IP headers only and does not save addresses or packet contents. Network sampling requires this helper to be installed as a root-owned executable with `CAP_NET_RAW`; the helper drops that capability after opening its packet socket.
- If swap is not configured, its meter is empty and the tooltip says `SWAP: not configured`.

RAM usage uses libstatgrab's `used / total` values. On Linux, libstatgrab treats cached file memory as reclaimable rather than used memory.

## Run the CTests

The CTests cover resource formatting, the CPU sample validity rule, and a live libstatgrab CPU query. The regression test includes the libstatgrab 0.92 behavior where a non-null aggregate CPU sample reports zero entries. On Ubuntu/Lubuntu:

```bash
sudo apt install build-essential cmake ninja-build qt6-base-dev libstatgrab-dev
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

## Build the LXQt Panel plugin

The plugin uses the same private panel interfaces as the bundled LXQt plugins, so build it in an LXQt Panel 2.3.2 source tree. The included patch adds the plugin to the upstream CMake build and install lists.

Install the build dependencies using Ubuntu's package metadata:

```bash
sudo apt update
sudo apt install build-essential cmake ninja-build git devscripts libstatgrab-dev libcap2-bin
sudo apt build-dep lxqt-panel
```

`apt build-dep` requires Ubuntu `deb-src` entries. Enable those entries and run `sudo apt update` if the command reports that source repositories are disabled.

Apply the patch to a clean upstream checkout:

```bash
git clone --depth 1 --branch 2.3.2 https://github.com/lxqt/lxqt-panel.git
cd lxqt-panel
git apply /path/to/lxqt-panel-2.3.2-resourcemonitor.patch
```

Configure and build just the new plugin:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build --target resourcemonitor
```

The resulting files include `build/plugin-resourcemonitor/libresourcemonitor.so`, `build/plugin-resourcemonitor/resourcemonitor.desktop` and `build/plugin-resourcemonitor/resourcemonitor-netcap`.

## Install for all users

Install the plugin module and desktop metadata into LXQt's plugin paths. This leaves the packaged `lxqt-panel` executable and existing plugins unchanged:

```bash
MULTIARCH="$(dpkg-architecture -qDEB_HOST_MULTIARCH)"
sudo install -Dm755 \
    build/plugin-resourcemonitor/libresourcemonitor.so \
    "/usr/lib/${MULTIARCH}/lxqt-panel/libresourcemonitor.so"
sudo install -Dm644 \
    build/plugin-resourcemonitor/resourcemonitor.desktop \
    /usr/share/lxqt/lxqt-panel/resourcemonitor.desktop
sudo install -Dm755 \
    build/plugin-resourcemonitor/resourcemonitor-netcap \
    /usr/bin/resourcemonitor-netcap
sudo chown root:root /usr/bin/resourcemonitor-netcap
sudo setcap cap_net_raw=ep /usr/bin/resourcemonitor-netcap
getcap /usr/bin/resourcemonitor-netcap
```

Restart the LXQt Panel or sign out and back in to make it rescan plugins. Then open **Panel Settings → Widgets**, choose **Add**, and select **Resource Monitor**. Its settings dialog controls enabled resources, value text, update interval, bar orientation and width; changes apply live.

To remove the all-users installation:

```bash
MULTIARCH="$(dpkg-architecture -qDEB_HOST_MULTIARCH)"
sudo rm -f "/usr/lib/${MULTIARCH}/lxqt-panel/libresourcemonitor.so"
sudo rm -f /usr/share/lxqt/lxqt-panel/resourcemonitor.desktop
sudo rm -f /usr/bin/resourcemonitor-netcap
```

## First visual check

Try the default **57 px** width with CPU, RAM and SWAP selected, and check whether the three-digit `100` fits comfortably at your panel height. Enable the optional disk and network meters from the settings dialog. Network meters remain unavailable until the capture helper is installed with `CAP_NET_RAW` as described above.

## License

The plugin source is distributed under the GNU Lesser General Public License, version 2.1 or (at your option) any later version. See [LICENSE](LICENSE).
