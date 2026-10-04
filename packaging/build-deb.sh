#!/bin/sh
set -eu
umask 022

usage()
{
    echo "Usage: $0 PLUGIN_BUILD_DIR [VERSION] [OUTPUT_DIR]" >&2
    exit 2
}

[ "$#" -ge 1 ] || usage

project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
plugin_build_dir=$(CDPATH= cd -- "$1" && pwd)
package_version=${2:-1.0.8-1}
output_dir=${3:-"$project_dir/build/packages"}

dpkg --validate-version "$package_version"

for artifact in libresourcemonitor.so resourcemonitor.desktop resourcemonitor-netcap; do
    if [ ! -f "$plugin_build_dir/$artifact" ]; then
        echo "Missing build artifact: $plugin_build_dir/$artifact" >&2
        exit 1
    fi
done

if ! command -v dpkg-shlibdeps >/dev/null 2>&1; then
    echo "dpkg-shlibdeps is required; install dpkg-dev on the build machine." >&2
    exit 1
fi

panel_version=$(dpkg-query -W -f='${Version}' lxqt-panel 2>/dev/null) || {
    echo "Install the target lxqt-panel package on the build machine first." >&2
    exit 1
}
architecture=$(dpkg --print-architecture)
multiarch=$(dpkg-architecture -qDEB_HOST_MULTIARCH)

mkdir -p "$output_dir"
output_dir=$(CDPATH= cd -- "$output_dir" && pwd)
package_root=$(mktemp -d "$output_dir/.lxqt-resource-monitor.XXXXXX")
netcap_root=$(mktemp -d "$output_dir/.lxqt-resource-monitor-netcap.XXXXXX")
cleanup()
{
    rm -rf "$package_root" "$netcap_root"
}
trap cleanup EXIT HUP INT TERM

plugin_dir="$package_root/usr/lib/$multiarch/lxqt-panel"
doc_dir="$package_root/usr/share/doc/lxqt-resource-monitor"
netcap_doc_dir="$netcap_root/usr/share/doc/lxqt-resource-monitor-netcap"
mkdir -p "$plugin_dir" "$doc_dir" "$package_root/usr/share/lxqt/lxqt-panel" \
    "$package_root/debian" "$package_root/DEBIAN" \
    "$netcap_root/usr/bin" "$netcap_doc_dir" "$netcap_root/debian" "$netcap_root/DEBIAN"

install -m 755 "$plugin_build_dir/libresourcemonitor.so" "$plugin_dir/"
install -m 644 "$plugin_build_dir/resourcemonitor.desktop" \
    "$package_root/usr/share/lxqt/lxqt-panel/"
install -m 644 "$project_dir/README.md" "$doc_dir/README.md"
install -m 644 "$project_dir/LICENSE" "$doc_dir/LICENSE"
install -m 644 "$project_dir/LICENSE" "$doc_dir/copyright"
install -m 755 "$plugin_build_dir/resourcemonitor-netcap" "$netcap_root/usr/bin/"
install -m 644 "$project_dir/LICENSE" "$netcap_doc_dir/copyright"

cat > "$package_root/debian/control" <<'EOF'
Source: lxqt-resource-monitor
Section: utils
Priority: optional
Maintainer: Local package builder <builder@localhost>
Standards-Version: 4.7.2

Package: lxqt-resource-monitor
Architecture: any
Description: optional system and I/O meters for the LXQt Panel
 A compact panel plugin for CPU, memory, swap, disk, and network activity.
EOF
cat > "$netcap_root/debian/control" <<'EOF'
Source: lxqt-resource-monitor
Section: utils
Priority: optional
Maintainer: Local package builder <builder@localhost>
Standards-Version: 4.7.2

Package: lxqt-resource-monitor-netcap
Architecture: any
Description: optional network capture helper for LXQt Resource Monitor
 Counts traffic by destination IP address for the Local net and Internet meters.
EOF

get_shlib_dependencies()
{
    root=$1
    binary=$2
    shlib_output=$(cd "$root" && dpkg-shlibdeps -O -e "$binary")
    case "$shlib_output" in
        shlibs:Depends=*) printf '%s' "${shlib_output#shlibs:Depends=}" ;;
        *)
            echo "Could not determine shared-library dependencies: $shlib_output" >&2
            return 1
            ;;
    esac
}

plugin_dependencies=$(get_shlib_dependencies \
    "$package_root" "usr/lib/$multiarch/lxqt-panel/libresourcemonitor.so")
netcap_dependencies=$(get_shlib_dependencies "$netcap_root" usr/bin/resourcemonitor-netcap)
rm -rf "$package_root/debian" "$netcap_root/debian"

cat > "$package_root/DEBIAN/control" <<EOF
Package: lxqt-resource-monitor
Version: $package_version
Section: utils
Priority: optional
Architecture: $architecture
Depends: lxqt-panel (= $panel_version), $plugin_dependencies
Suggests: lxqt-resource-monitor-netcap (= $package_version)
Maintainer: Local package builder <builder@localhost>
Description: optional system and I/O meters for the LXQt Panel
 A compact panel plugin for CPU, memory, swap, disk, and network activity.
EOF

cat > "$netcap_root/DEBIAN/control" <<EOF
Package: lxqt-resource-monitor-netcap
Version: $package_version
Section: utils
Priority: optional
Architecture: $architecture
Depends: lxqt-resource-monitor (= $package_version), libcap2-bin, $netcap_dependencies
Replaces: lxqt-resource-monitor (<< $package_version)
Maintainer: Local package builder <builder@localhost>
Description: optional network capture helper for LXQt Resource Monitor
 Counts traffic by destination IP address for the Local net and Internet meters.
EOF

cat > "$netcap_root/DEBIAN/postinst" <<'EOF'
#!/bin/sh
set -e
setcap cap_net_raw=ep /usr/bin/resourcemonitor-netcap
EOF
chmod 755 "$netcap_root/DEBIAN/postinst"

find "$package_root" "$netcap_root" -type d -exec chmod 755 {} +
plugin_package="$output_dir/lxqt-resource-monitor_${package_version}_${architecture}.deb"
netcap_package="$output_dir/lxqt-resource-monitor-netcap_${package_version}_${architecture}.deb"
dpkg-deb --build --root-owner-group "$package_root" "$plugin_package"
dpkg-deb --build --root-owner-group "$netcap_root" "$netcap_package"
echo "Created $plugin_package"
echo "Created $netcap_package"
