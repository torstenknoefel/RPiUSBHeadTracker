#!/bin/bash
# Opens the head tracker's own Wi-Fi ("hotspot") when the Pi is on no Wi-Fi
# network, so it can be set up with a phone and no terminal:
#   - no Wi-Fi network saved: the hotspot opens right away
#   - saved networks, but none joined for WAIT_SAVED seconds: the hotspot opens
#   - while nobody uses the hotspot, the saved networks are tried again every
#     RETRY seconds (without saved ones: the list of networks is renewed)
# The hotspot is open (no password) and named HeadTracker-XXXX (end of the
# Wi-Fi address). It has the address 10.42.0.1; headtracker-captive.conf makes
# every name point there, so phones open the setup page by themselves.
# Before the hotspot opens, the nearby networks are written to $RUN/networks
# for the setup page (the Wi-Fi cannot search while it is a hotspot).
#
# Runs as root from headtracker-wifi.service (see install_wifi.sh).

IFACE="${WIFI_IFACE:-wlan0}"
HOTSPOT=headtracker-hotspot
ADDRESS=10.42.0.1
RUN=/run/headtracker-wifi
WAIT_SAVED="${WAIT_SAVED:-90}"
RETRY="${RETRY:-300}"

log() { echo "$*"; }

mkdir -p "$RUN"
chmod 755 "$RUN"

# Waits until NetworkManager knows the Wi-Fi device
for _ in $(seq 60); do
  nmcli -t -f DEVICE device 2>/dev/null | grep -qx "$IFACE" && break
  sleep 2
done

# The hotspot connection in NetworkManager (made once)
if ! nmcli -t -f NAME connection show | grep -qx "$HOTSPOT"; then
  suffix=$(tr -d ':' < "/sys/class/net/$IFACE/address" | tail -c 5 | tr 'a-f' 'A-F')
  log "Making the hotspot connection HeadTracker-$suffix"
  nmcli connection add type wifi ifname "$IFACE" con-name "$HOTSPOT" autoconnect no \
    ssid "HeadTracker-$suffix" 802-11-wireless.mode ap 802-11-wireless.band bg \
    802-11-wireless.channel 6 ipv4.method shared ipv4.addresses "$ADDRESS/24" \
    ipv6.method disabled >/dev/null
fi

# Number of saved Wi-Fi networks the Pi joins by itself (not the hotspot)
saved_networks() {
  local n=0 uuid type auto mode
  while IFS=: read -r uuid type auto; do
    [ "$type" = "802-11-wireless" ] && [ "$auto" = "yes" ] || continue
    mode=$(nmcli -g 802-11-wireless.mode connection show uuid "$uuid" 2>/dev/null)
    [ "$mode" = "ap" ] || n=$((n + 1))
  done < <(nmcli -t -f UUID,TYPE,AUTOCONNECT connection show 2>/dev/null)
  echo $n
}

device_state() { nmcli -g GENERAL.STATE device show "$IFACE" 2>/dev/null | cut -d' ' -f1; }
active_connection() { nmcli -g GENERAL.CONNECTION device show "$IFACE" 2>/dev/null; }

# true if a phone or PC is (or was a moment ago) on the hotspot
someone_on_hotspot() { ip neigh show dev "$IFACE" 2>/dev/null | grep -q -E 'REACHABLE|DELAY|PROBE'; }

scan() {
  nmcli -t -f SSID,SIGNAL,SECURITY device wifi list ifname "$IFACE" --rescan yes \
    > "$RUN/networks.new" 2>/dev/null && mv "$RUN/networks.new" "$RUN/networks"
  chmod 644 "$RUN/networks" 2>/dev/null
}

open_hotspot() {
  scan
  log "Opening the hotspot ($(wc -l < "$RUN/networks" 2>/dev/null || echo 0) networks nearby)"
  nmcli --wait 30 connection up id "$HOTSPOT" >/dev/null || log "The hotspot did not open"
  hotspot_since=$(date +%s)
}

lost_since=$(date +%s)
hotspot_since=$(date +%s)
last_note=""
while true; do
  now=$(date +%s)
  state=$(device_state)
  note=""
  if [ "$(active_connection)" = "$HOTSPOT" ]; then
    lost_since=$now
    if [ $((now - hotspot_since)) -ge "$RETRY" ] && ! someone_on_hotspot; then
      if [ "$(saved_networks)" -gt 0 ]; then
        log "Nobody on the hotspot: trying the saved networks again"
        nmcli connection down id "$HOTSPOT" >/dev/null
        nmcli device connect "$IFACE" >/dev/null 2>&1 &
      else
        log "Nobody on the hotspot: renewing the list of networks"
        nmcli connection down id "$HOTSPOT" >/dev/null
        open_hotspot
      fi
    fi
  elif [ "$state" = "100" ]; then
    lost_since=$now                     # on a network
  elif [ -n "$state" ] && [ "$state" -gt 30 ] && [ "$state" -lt 100 ]; then
    :                                   # joining a network: wait (at most WAIT_SAVED)
  elif [ -z "$state" ] || [ "$state" -lt 30 ]; then
    note="The Wi-Fi is not usable (state ${state:-unknown})"
  fi
  if [ "$(active_connection)" != "$HOTSPOT" ] && [ "$state" != "100" ] \
     && [ -n "$state" ] && [ "$state" -ge 30 ]; then
    if [ "$(saved_networks)" -eq 0 ]; then
      open_hotspot
    elif [ $((now - lost_since)) -ge "$WAIT_SAVED" ]; then
      log "No saved network joined for $WAIT_SAVED seconds"
      open_hotspot
    fi
  fi
  if [ "$note" != "$last_note" ]; then
    [ -n "$note" ] && log "$note"
    last_note=$note
  fi
  sleep 5
done
