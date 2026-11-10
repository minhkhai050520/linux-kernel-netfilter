#!/bin/sh
set -eu

CTL="myfw-ctl"
SRC_IP="192.168.1.2"
SRC_PORT="3000"
DST_PORT="4000"
PROTO="tcp"

# Define a helper to get uptime in seconds instead of using bash $SECONDS
get_seconds() {
    # Read system uptime from /proc/uptime and keep only the integer part before the decimal point
    read -r uptime_sec _ < /proc/uptime
    echo "${uptime_sec%.*}"
}

# Run the loop for 20 seconds
start_time=$(get_seconds)
end_time=$((start_time + 20))

while [ "$(get_seconds)" -lt "$end_time" ]; do
    echo "=== Cycle $(date +%H:%M:%S) ==="

    # 1) Add rules from 192.168.1.3 to 192.168.1.102
    dst=3
    while [ "$dst" -le 102 ]; do
        dst_ip="192.168.1.${dst}"

        "$CTL" -fw rule add \
            --src-ip "$SRC_IP" \
            --dst-ip "$dst_ip" \
            --src-port "$SRC_PORT" \
            --dst-port "$DST_PORT" \
            --proto "$PROTO" \
            --action accept
            
        dst=$((dst + 1))
    done

    echo "All add rules completed"

    # 2) Delete the rules that were just added
    dst=3
    while [ "$dst" -le 102 ]; do
        dst_ip="192.168.1.${dst}"

        "$CTL" -fw rule del \
            --src-ip "$SRC_IP" \
            --dst-ip "$dst_ip" \
            --src-port "$SRC_PORT" \
            --dst-port "$DST_PORT" \
            --proto "$PROTO"
            
        dst=$((dst + 1))
    done

    echo "All delete rules completed"
done

echo "Loop finished after 20 seconds"

