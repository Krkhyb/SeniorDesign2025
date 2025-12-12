echo "Detecting Raspberry Pi IP Addresses..."

interfaces=$(ls /sys/class/net | grep -v lo)
found=false
for iface in $interfaces; do
    ipAddr=$(ip -4 addr show "$iface" | grep -oP '(?<=inet\s)\d+(\.\d+){3}')
    if [[ -n "ipAddr" ]]; then
	echo "Interface: $iface"
	echo "IP Address: $ipAddr"
	echo
	found=true
    fi
done

if ["$found" = false ]; then
    echo "No active IPv4 network connections detected."
fi
