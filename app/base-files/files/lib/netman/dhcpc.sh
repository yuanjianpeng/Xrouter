#!/bin/sh

cmd=$1
[ -n "$dns" ] && echo "nameserver $dns" >/run/dnsmasq/resolv.conf
xbus call netman dhcp "$cmd" "$interface" \
	-pid "$NETMAN_PID" -ip "$ip" -mask "$mask" \
	-gw "$router" -dns "$dns" -lease "$lease"

