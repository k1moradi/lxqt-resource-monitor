# LXQt Resource Monitor

Resource Monitor is a dynamically loaded **LXQt Panel 2.3.2** plugin for Lubuntu 26.04. It places CPU, physical memory and swap meters together in one panel item, styled after LXQt's CPU Monitor.

## Appearance and defaults

- Three adjacent meters, ordered CPU, RAM, SWAP.
- Bottom-up vertical bars by default; top-down, left-to-right and right-to-left are configurable.
- CPU is green, RAM blue and SWAP amber. The colored outlines remain visible at 0%, so the meters are identifiable without labels.
- Percentages are shown by default.
- Default total width is **78 px**, configurable live from 48 to 300 px.
- Sampling interval defaults to one second and is configurable from 0.5 seconds.
- The tooltip shows percentages and used/total RAM and SWAP values.
- If swap is not configured, its meter is empty and the tooltip says `SWAP: not configured`.

RAM usage uses libstatgrab's `used / total` values. On Linux, libstatgrab treats cached file memory as reclaimable rather than used memory.

## Run the CTests

The tests cover the percentage calculation and byte formatting helpers used by the plugin. On Ubuntu/Lubuntu:

```bash
sudo apt install build-essential cmake qt6-base-dev
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

## Build the LXQt Panel plugin

The plugin uses the same private panel interfaces as the bundled LXQt plugins, so build it in an LXQt Panel 2.3.2 source tree. The included patch adds the plugin to the upstream CMake build and install lists.

Install the build dependencies using Ubuntu's package metadata:

```bash
sudo apt update
sudo apt install build-essential cmake ninja-build git devscripts libstatgrab-dev
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

The resulting files are `build/plugin-resourcemonitor/libresourcemonitor.so` and `build/plugin-resourcemonitor/resourcemonitor.desktop`.

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
```

Restart the LXQt Panel or sign out and back in to make it rescan plugins. Then open **Panel Settings → Widgets**, choose **Add**, and select **Resource Monitor**. Its settings dialog controls percentage text, update interval, bar orientation and total width; changes apply live.

To remove the all-users installation:

```bash
MULTIARCH="$(dpkg-architecture -qDEB_HOST_MULTIARCH)"
sudo rm -f "/usr/lib/${MULTIARCH}/lxqt-panel/libresourcemonitor.so"
sudo rm -f /usr/share/lxqt/lxqt-panel/resourcemonitor.desktop
```

## First visual check

Try the default **78 px** width and check whether `100%` fits comfortably at your panel height. The width can be changed immediately in the settings dialog. Feedback on the preferred default width and how the three colors look with the active theme can guide a follow-up adjustment.

## License

The plugin source is distributed under the GNU Lesser General Public License, version 2.1 or (at your option) any later version. See [LICENSE](LICENSE).
