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
package_version=${2:-1.0.0-1}
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
package_root="$output_dir/package-root"
plugin_dir="$package_root/usr/lib/$multiarch/lxqt-panel"
doc_dir="$package_root/usr/share/doc/lxqt-resource-monitor"

mkdir -p "$plugin_dir" "$doc_dir" "$package_root/usr/share/lxqt/lxqt-panel" \
    "$package_root/usr/bin" "$package_root/DEBIAN" "$package_root/debian"
install -m 755 "$plugin_build_dir/libresourcemonitor.so" "$plugin_dir/"
install -m 644 "$plugin_build_dir/resourcemonitor.desktop" \
    "$package_root/usr/share/lxqt/lxqt-panel/"
install -m 755 "$plugin_build_dir/resourcemonitor-netcap" "$package_root/usr/bin/"
install -m 644 "$project_dir/README.md" "$doc_dir/README.md"
install -m 644 "$project_dir/LICENSE" "$doc_dir/LICENSE"
install -m 644 "$project_dir/LICENSE" "$doc_dir/copyright"

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

shlib_output=$(
    cd "$package_root"
    dpkg-shlibdeps -O \
        -e "usr/lib/$multiarch/lxqt-panel/libresourcemonitor.so" \
        -e usr/bin/resourcemonitor-netcap
)
case "$shlib_output" in
    shlibs:Depends=*) shlib_dependencies=${shlib_output#shlibs:Depends=} ;;
    *)
        echo "Could not determine shared-library dependencies: $shlib_output" >&2
        exit 1
        ;;
esac

unlink "$package_root/debian/control"
rmdir "$package_root/debian"

cat > "$package_root/DEBIAN/control" <<EOF
Package: lxqt-resource-monitor
Version: $package_version
Section: utils
Priority: optional
Architecture: $architecture
Depends: lxqt-panel (= $panel_version), libcap2-bin, $shlib_dependencies
Maintainer: Local package builder <builder@localhost>
Description: optional system and I/O meters for the LXQt Panel
 A compact panel plugin for CPU, memory, swap, disk, and network activity.
EOF

cat > "$package_root/DEBIAN/postinst" <<'EOF'
#!/bin/sh
set -e
setcap cap_net_raw=ep /usr/bin/resourcemonitor-netcap
EOF
chmod 755 "$package_root/DEBIAN/postinst"

package_file="$output_dir/lxqt-resource-monitor_${package_version}_${architecture}.deb"
find "$package_root" -type d -exec chmod 755 {} +
dpkg-deb --build --root-owner-group "$package_root" "$package_file"
echo "Created $package_file"
