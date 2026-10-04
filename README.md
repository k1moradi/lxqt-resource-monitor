# LXQt Resource Monitor

Resource Monitor is a dynamically loaded **LXQt Panel 2.3.2** plugin for Lubuntu 26.04. It displays selected system and I/O meters together in one panel item, styled after LXQt's CPU Monitor.

The commands below assume you have already cloned this repository and are running them from its `lxqt-resource-monitor/` folder.

## Appearance and defaults

- Optional meters for CPU, RAM, SWAP, local disk I/O, local network I/O and Internet I/O. CPU, RAM and SWAP are selected by default.
- Each 19 px meter is a borderless strip of load-threshold columns. The low-to-high direction runs left-to-right by default and can be reversed.
- At the default 19 px meter width, each pixel column tracks its own EMA. Columns matching the current load stay bright; columns from recently higher loads fade with a five-second time constant. Sampling runs once per second, and the number remains the latest live value.
- CPU is green, RAM blue, SWAP amber, disk purple, local network teal and Internet red. The meters have no borders, leaving their full width for the resource display and value text.
- Rounded usage values are shown over the bars without a `%` postfix; tooltips retain percentages.
- Default width is **57 px for three selected resources**, giving each bar 19 px. The widget grows or shrinks with the number of selected resources, keeping each bar the same width. The setting is configurable live from 48 to 300 px for three resources.
- The widget update interval defaults to one second and is configurable from 0.5 seconds; EMA sampling and repainting run once per second regardless of that setting.
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

The plugin uses the same private panel interfaces as the bundled LXQt plugins, so build it against an LXQt Panel 2.3.2 source tree. The included patch adds the plugin to the upstream CMake build and install lists. Run all commands below from the `lxqt-resource-monitor/` folder.

Install the build dependencies using Ubuntu's package metadata:

```bash
sudo apt update
sudo apt install build-essential cmake ninja-build git devscripts dpkg-dev libstatgrab-dev libcap2-bin
sudo apt build-dep lxqt-panel
```

If `apt build-dep` prints `You must put some 'deb-src' URIs in your sources.list`, enable source-package entries on the **build computer**. The default Lubuntu 26.04 setup uses `/etc/apt/sources.list.d/ubuntu.sources`: edit each Ubuntu archive and security stanza, changing `Types: deb` to `Types: deb deb-src`, then run `sudo apt update` and retry `sudo apt build-dep lxqt-panel`. Keep each stanza's existing URI, suite, components and signing key. For older one-line `.list` files, add a matching `deb-src` line for each Ubuntu `deb` line. APT uses `deb-src` entries to retrieve source package metadata for `build-dep` ([APT sources.list manual](https://manpages.ubuntu.com/manpages/resolute/man5/sources.list.5.html)).

Apply the patch to a clean upstream checkout:

```bash
git clone --depth 1 --branch 2.3.2 https://github.com/lxqt/lxqt-panel.git lxqt-panel-2.3.2
git -C lxqt-panel-2.3.2 apply "$PWD/lxqt-panel-2.3.2-resourcemonitor.patch"
```

Configure and build just the new plugin:

```bash
cmake -S lxqt-panel-2.3.2 -B lxqt-panel-2.3.2/build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build lxqt-panel-2.3.2/build --target resourcemonitor resourcemonitor-netcap
```

The resulting files are in `lxqt-panel-2.3.2/build/plugin-resourcemonitor/`.

## Build a binary Debian package

You can compile on one computer and install a binary `.deb` on another. The target computer does not need `deb-src`, a compiler, or the LXQt Panel source tree. Build the plugin as above, then, from this repository, run:

```bash
./packaging/build-deb.sh \
    lxqt-panel-2.3.2/build/plugin-resourcemonitor \
    1.0.5-1 \
    build/packages
```

The script packages the plugin, desktop entry and capture helper; it also records the build computer's exact `lxqt-panel` package version and detects shared-library dependencies. It creates `build/packages/lxqt-resource-monitor_1.0.5-1_<architecture>.deb`. Copy that `.deb` to the other computer and install it with APT so missing runtime dependencies are fetched:

```bash
sudo apt install ./build/packages/lxqt-resource-monitor_1.0.5-1_$(dpkg --print-architecture).deb
```

The `.deb` is architecture and Ubuntu-release specific. Build and install it on computers with the same CPU architecture, Ubuntu/Lubuntu release and exact `lxqt-panel` package version; the plugin uses LXQt Panel's private plugin interface. The package's post-install step gives only `resourcemonitor-netcap` the `CAP_NET_RAW` capability needed for IP-based local-versus-public traffic counts. No source-package repositories are needed on the target computer. After installation, restart LXQt Panel and add **Resource Monitor** in **Panel Settings → Widgets**. To remove the package, run `sudo apt remove lxqt-resource-monitor`.

You can also download the prebuilt `.deb` for the supported amd64 system from the [latest GitHub release](https://github.com/k1moradi/lxqt-resource-monitor/releases/latest). Install the downloaded package with:

```bash
sudo apt install ./lxqt-resource-monitor_1.0.5-1_amd64.deb
```

## Install for all users

Install the plugin module and desktop metadata into LXQt's plugin paths. This leaves the packaged `lxqt-panel` executable and existing plugins unchanged:

```bash
MULTIARCH="$(dpkg-architecture -qDEB_HOST_MULTIARCH)"
sudo install -Dm755 \
    lxqt-panel-2.3.2/build/plugin-resourcemonitor/libresourcemonitor.so \
    "/usr/lib/${MULTIARCH}/lxqt-panel/libresourcemonitor.so"
sudo install -Dm644 \
    lxqt-panel-2.3.2/build/plugin-resourcemonitor/resourcemonitor.desktop \
    /usr/share/lxqt/lxqt-panel/resourcemonitor.desktop
sudo install -Dm755 \
    lxqt-panel-2.3.2/build/plugin-resourcemonitor/resourcemonitor-netcap \
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
