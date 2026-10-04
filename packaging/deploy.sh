#!/bin/sh
set -eu

usage()
{
    cat <<'EOF'
Usage: ./packaging/deploy.sh [--dry-run] [--with-netcap] [--no-restart] [PLUGIN_DEB [NETCAP_DEB]]

With no package path, installs the newest package for this computer from
build/packages/. Use --dry-run to validate the package without installing it.
Pass a second .deb or use --with-netcap to install the optional Local net and
Internet traffic helper too.

Run this as your logged-in desktop user, without sudo. The script uses sudo
only for APT and can restart your LXQt Panel so it rescans the installed plugin.
EOF
}

fail()
{
    echo "Error: $*" >&2
    exit 1
}

with_netcap=0
no_restart=0
dry_run=0
while [ "$#" -gt 0 ]; do
    case "$1" in
        --help|-h)
            usage
            exit 0
            ;;
        --with-netcap)
            with_netcap=1
            shift
            ;;
        --no-restart)
            no_restart=1
            shift
            ;;
        --dry-run)
            dry_run=1
            shift
            ;;
        --)
            shift
            break
            ;;
        -* )
            usage >&2
            exit 2
            ;;
        *)
            break
            ;;
    esac
done

[ "$#" -le 2 ] || { usage >&2; exit 2; }
[ "$(id -u)" -ne 0 ] || fail "Run this from your desktop account without sudo; elevation is requested only for APT."

for command_name in dpkg dpkg-deb dpkg-query dpkg-architecture apt-get ldd; do
    command -v "$command_name" >/dev/null 2>&1 || fail "Required command not found: $command_name"
done

project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
package_dir="$project_dir/build/packages"
system_arch=$(dpkg --print-architecture)
multiarch=$(dpkg-architecture -qDEB_HOST_MULTIARCH)

latest_plugin_deb()
{
    newest=
    newest_version=
    for candidate in "$package_dir"/lxqt-resource-monitor_*_"$system_arch".deb; do
        [ -f "$candidate" ] || continue
        candidate_name=$(dpkg-deb -f "$candidate" Package)
        candidate_arch=$(dpkg-deb -f "$candidate" Architecture)
        [ "$candidate_name" = lxqt-resource-monitor ] || continue
        [ "$candidate_arch" = "$system_arch" ] || continue
        candidate_version=$(dpkg-deb -f "$candidate" Version)
        if [ -z "$newest_version" ] \
           || dpkg --compare-versions "$candidate_version" gt "$newest_version"; then
            newest=$candidate
            newest_version=$candidate_version
        fi
    done
    [ -n "$newest" ] || return 1
    printf '%s\n' "$newest"
}

absolute_file()
{
    case "$1" in
        /*) printf '%s\n' "$1" ;;
        *) printf '%s/%s\n' "$PWD" "$1" ;;
    esac
}

if [ "$#" -ge 1 ]; then
    plugin_deb=$(absolute_file "$1")
else
    plugin_deb=$(latest_plugin_deb) || fail "No lxqt-resource-monitor .deb for $system_arch found in $package_dir. Build it first with ./packaging/build.sh."
fi
[ -f "$plugin_deb" ] || fail "Plugin package not found: $plugin_deb"

plugin_name=$(dpkg-deb -f "$plugin_deb" Package)
plugin_version=$(dpkg-deb -f "$plugin_deb" Version)
plugin_arch=$(dpkg-deb -f "$plugin_deb" Architecture)
[ "$plugin_name" = lxqt-resource-monitor ] || fail "$plugin_deb is package '$plugin_name', expected lxqt-resource-monitor."
[ "$plugin_arch" = "$system_arch" ] || fail "Package architecture is $plugin_arch; this computer is $system_arch."

netcap_deb=
if [ "$#" -ge 2 ]; then
    netcap_deb=$(absolute_file "$2")
elif [ "$with_netcap" -eq 1 ]; then
    netcap_deb="$package_dir/lxqt-resource-monitor-netcap_${plugin_version}_${system_arch}.deb"
fi

if [ -n "$netcap_deb" ]; then
    [ -f "$netcap_deb" ] || fail "Network helper package not found: $netcap_deb"
    netcap_name=$(dpkg-deb -f "$netcap_deb" Package)
    netcap_version=$(dpkg-deb -f "$netcap_deb" Version)
    netcap_arch=$(dpkg-deb -f "$netcap_deb" Architecture)
    [ "$netcap_name" = lxqt-resource-monitor-netcap ] || fail "$netcap_deb is package '$netcap_name', expected lxqt-resource-monitor-netcap."
    [ "$netcap_version" = "$plugin_version" ] || fail "The plugin is version $plugin_version but the network helper is $netcap_version. Use matching packages."
    [ "$netcap_arch" = "$system_arch" ] || fail "Network helper architecture is $netcap_arch; this computer is $system_arch."
fi

panel_status=$(dpkg-query -W -f='${Status}' lxqt-panel 2>/dev/null || true)
[ "$panel_status" = 'install ok installed' ] || fail "LXQt Panel is not installed. Install this widget in an LXQt desktop session."
panel_version=$(dpkg-query -W -f='${Version}' lxqt-panel)
required_panel_version=$(dpkg-deb -f "$plugin_deb" Depends \
    | sed -n 's/.*lxqt-panel (= \([^)]*\)).*/\1/p')
if [ -n "$required_panel_version" ] && [ "$panel_version" != "$required_panel_version" ]; then
    fail "This package was built for lxqt-panel $required_panel_version, but this computer has $panel_version. Rebuild the package against this computer's LXQt Panel version; do not force-install it."
fi
installed_monitor_version=$(dpkg-query -W -f='${Version}' lxqt-resource-monitor 2>/dev/null || true)
if [ -n "$installed_monitor_version" ] \
   && dpkg --compare-versions "$installed_monitor_version" gt "$plugin_version"; then
    fail "Version $installed_monitor_version is already installed, newer than the package $plugin_version. Rebuild the current source before deploying."
fi

stage_dir=$(mktemp -d)
trap 'rm -rf "$stage_dir"' 0
trap 'exit 1' HUP INT TERM
dpkg-deb -x "$plugin_deb" "$stage_dir"
staged_library="$stage_dir/usr/lib/$multiarch/lxqt-panel/libresourcemonitor.so"
staged_desktop="$stage_dir/usr/share/lxqt/lxqt-panel/resourcemonitor.desktop"
[ -r "$staged_library" ] || fail "The package does not contain its plugin library in the LXQt Panel plugin directory for $multiarch."
[ -r "$staged_desktop" ] || fail "The package does not contain the LXQt Panel plugin descriptor."
grep -Fqx 'ServiceTypes=LXQtPanel/Plugin' "$staged_desktop" \
    || fail "The packaged descriptor does not register as an LXQt Panel plugin. Rebuild the package from this repository."
grep -Eq '^Name=.+$' "$staged_desktop" \
    || fail "The packaged descriptor has no visible plugin name. Rebuild the package from this repository."
if grep -Eq '^(Hidden|NoDisplay)=true$' "$staged_desktop"; then
    fail "The packaged descriptor marks Resource Monitor hidden. Rebuild the package from this repository."
fi
if [ -n "$netcap_deb" ]; then
    dpkg-deb -x "$netcap_deb" "$stage_dir"
    [ -x "$stage_dir/usr/bin/resourcemonitor-netcap" ] \
        || fail "The network helper package does not contain /usr/bin/resourcemonitor-netcap."
fi

echo "Plugin package:  $plugin_deb ($plugin_version, $plugin_arch)"
echo "LXQt Panel:      $panel_version"
if [ -n "$netcap_deb" ]; then
    echo "Network helper:  $netcap_deb"
else
    echo "Network helper:  not selected (optional)"
fi
echo ""
if [ "$dry_run" -eq 1 ]; then
    echo "Dry run passed: package architecture, LXQt Panel version, plugin path, and desktop registration are valid."
    exit 0
fi

echo "APT will reinstall the package, repair missing package files, and install any declared dependencies."

set -- apt-get install --reinstall "$plugin_deb"
[ -z "$netcap_deb" ] || set -- "$@" "$netcap_deb"
command -v sudo >/dev/null 2>&1 || fail "sudo is required to install packages."
sudo "$@" || fail "APT could not install the packages. If repository indexes are stale, run 'sudo apt update' and try again."

installed_status=$(dpkg-query -W -f='${Status}' lxqt-resource-monitor 2>/dev/null || true)
installed_version=$(dpkg-query -W -f='${Version}' lxqt-resource-monitor 2>/dev/null || true)
[ "$installed_status" = 'install ok installed' ] || fail "APT finished, but lxqt-resource-monitor is not registered as installed."
[ "$installed_version" = "$plugin_version" ] || fail "Installed version is $installed_version; expected $plugin_version."
if [ -n "$netcap_deb" ]; then
    helper_status=$(dpkg-query -W -f='${Status}' lxqt-resource-monitor-netcap 2>/dev/null || true)
    helper_version=$(dpkg-query -W -f='${Version}' lxqt-resource-monitor-netcap 2>/dev/null || true)
    [ "$helper_status" = 'install ok installed' ] \
        || fail "APT finished, but lxqt-resource-monitor-netcap is not registered as installed."
    [ "$helper_version" = "$plugin_version" ] \
        || fail "Installed network helper version is $helper_version; expected $plugin_version."
fi

plugin_library="/usr/lib/$multiarch/lxqt-panel/libresourcemonitor.so"
plugin_desktop=/usr/share/lxqt/lxqt-panel/resourcemonitor.desktop
[ -r "$plugin_library" ] || fail "Installed plugin library is missing: $plugin_library"
[ -r "$plugin_desktop" ] || fail "Installed LXQt plugin descriptor is missing: $plugin_desktop"
grep -Fqx 'ServiceTypes=LXQtPanel/Plugin' "$plugin_desktop" \
    || fail "$plugin_desktop does not register as an LXQt Panel plugin. Rebuild the package from this repository."
grep -Eq '^Name=.+$' "$plugin_desktop" \
    || fail "$plugin_desktop has no visible plugin name. Rebuild the package from this repository."
if grep -Eq '^(Hidden|NoDisplay)=true$' "$plugin_desktop"; then
    fail "$plugin_desktop marks Resource Monitor hidden. Rebuild the package from this repository."
fi

dependency_report=$(ldd "$plugin_library" 2>&1 || true)
if printf '%s\n' "$dependency_report" | grep -q 'not found'; then
    printf '%s\n' "$dependency_report" >&2
    fail "The plugin has unresolved shared-library dependencies. Check that the package is for this Ubuntu release and architecture."
fi

if [ -n "$netcap_deb" ]; then
    helper=/usr/bin/resourcemonitor-netcap
    [ -x "$helper" ] || fail "The network capture helper is missing or not executable: $helper"
    command -v getcap >/dev/null 2>&1 || fail "getcap is missing; reinstall lxqt-resource-monitor-netcap to restore libcap2-bin."
    if ! getcap "$helper" | grep -Fq 'cap_net_raw=ep'; then
        echo "Repairing the network helper's CAP_NET_RAW capability..."
        sudo setcap cap_net_raw=ep "$helper" \
            || fail "Could not set CAP_NET_RAW on $helper. Local net and Internet meters will not work."
    fi
    getcap "$helper" | grep -Fq 'cap_net_raw=ep' \
        || fail "The network helper still lacks CAP_NET_RAW: $helper"
fi

echo ""
echo "Package verification passed: the plugin library, discoverable desktop entry, and linked libraries are present."

user_data_home=${XDG_DATA_HOME:-"${HOME:-}/.local/share"}
user_desktop="$user_data_home/lxqt/lxqt-panel/resourcemonitor.desktop"
if [ -f "$user_desktop" ]; then
    echo "Note: a user-level descriptor also exists at $user_desktop; it may take precedence over the system copy."
fi

restart_panel()
{
    [ "$no_restart" -eq 0 ] || {
        echo "Panel restart skipped (--no-restart). Sign out and back in to refresh LXQt Panel's plugin list."
        return 0
    }

    if [ ! -t 0 ]; then
        echo "No interactive terminal: panel restart skipped. Sign out and back in to refresh the plugin list."
        return 0
    fi

    printf 'Restart LXQt Panel now to refresh its cached widget list? The taskbar may disappear briefly. [Y/n] '
    IFS= read -r answer || answer=
    case "$answer" in
        n|N|no|NO|No)
            echo "Panel restart skipped. Sign out and back in, or rerun this script, before opening Panel Settings."
            return 0
            ;;
    esac

    command -v pgrep >/dev/null 2>&1 || {
        echo "Cannot safely locate the current panel process (pgrep is unavailable). Sign out and back in to refresh it."
        return 0
    }
    [ -n "${DISPLAY:-}${WAYLAND_DISPLAY:-}" ] || {
        echo "No graphical-session display was detected. Sign out and back in to refresh LXQt Panel."
        return 0
    }

    current_session_panel_pids()
    {
        for pid in $(pgrep -u "$(id -u)" -x lxqt-panel 2>/dev/null || true); do
            proc_env="/proc/$pid/environ"
            [ -r "$proc_env" ] || continue
            matches_display=0
            if [ -n "${DISPLAY:-}" ] \
               && tr '\000' '\n' < "$proc_env" 2>/dev/null | grep -Fqx "DISPLAY=$DISPLAY"; then
                matches_display=1
            fi
            if [ -n "${WAYLAND_DISPLAY:-}" ] \
               && tr '\000' '\n' < "$proc_env" 2>/dev/null | grep -Fqx "WAYLAND_DISPLAY=$WAYLAND_DISPLAY"; then
                matches_display=1
            fi
            [ "$matches_display" -eq 1 ] && printf '%s\n' "$pid"
        done
        return 0
    }

    original_pids=$(current_session_panel_pids)
    [ -n "$original_pids" ] || {
        echo "No LXQt Panel process for this graphical session was found. Sign out and back in to refresh it."
        return 0
    }

    panel_command=$(command -v lxqt-panel 2>/dev/null || true)
    [ -n "$panel_command" ] || {
        echo "lxqt-panel was not found in PATH, so I left the running panel untouched. Sign out and back in to refresh it."
        return 0
    }

    for pid in $original_pids; do
        kill -TERM "$pid" 2>/dev/null || true
    done

    attempt=0
    while [ "$attempt" -lt 10 ]; do
        current_pids=$(current_session_panel_pids)
        if [ -z "$current_pids" ]; then
            break
        fi
        for pid in $current_pids; do
            case " $original_pids " in
                *" $pid "*) ;;
                *) echo "LXQt's session manager restarted the panel."; return 0 ;;
            esac
        done
        sleep 1
        attempt=$((attempt + 1))
    done

    current_pids=$(current_session_panel_pids)
    [ -z "$current_pids" ] || {
        echo "The current panel did not stop cleanly; I did not start a second copy. Sign out and back in to refresh it."
        return 0
    }

    log_dir=${XDG_CACHE_HOME:-"${HOME:-}/.cache"}
    mkdir -p "$log_dir"
    panel_log="$log_dir/lxqt-resource-monitor-panel-restart.log"
    nohup "$panel_command" </dev/null >>"$panel_log" 2>&1 &
    new_panel_pid=$!
    sleep 2
    if kill -0 "$new_panel_pid" 2>/dev/null; then
        echo "LXQt Panel restarted. Log: $panel_log"
    else
        echo "LXQt Panel did not stay running. Check $panel_log or sign out and back in." >&2
    fi
}

restart_panel

echo ""
echo "Open Panel Settings → Widgets → Add and select Resource Monitor."
if [ -n "$netcap_deb" ]; then
    echo "The optional network helper is installed and has CAP_NET_RAW for Local net and Internet meters."
fi
