#!/bin/bash
LOG_DIR="/var/log/"
LOG_FILE="/var/log/myapp.log"
MAX_SIZE=104857600  # 100MB in bytes
SLEEP_SECONDS=20

while true; do
    if [ -f "$LOG_FILE" ]; then
        current_size=$(stat -c%s "$LOG_FILE")
        if [ $current_size -ge $MAX_SIZE ]; then
            timestamp=$(date +"%Y-%m-%d_%H-%M-%S")
            mv "$LOG_FILE" "${LOG_FILE}_$timestamp"
            echo "Log file rotated: ${LOG_FILE}_$timestamp"
            touch "$LOG_FILE"
            chmod 644 "$LOG_FILE"
            chown root:root "$LOG_FILE"
        fi
    else
        echo "Log file does not exist: $LOG_FILE"
    fi
    sleep $SLEEP_SECONDS
done

# Check total size of all log files and remove the oldest if necessary
total_size=$(du -sb $LOG_DIR | awk '{print $1}')
while [ $total_size -gt $MAX_DIR_SIZE ]; do
    # Find the oldest log file and delete it
    oldest_file=$(ls -1t $LOG_DIR*myapp*.log | tail -n 1)
    if [ -f "$oldest_file" ]; then
        rm -f "$oldest_file"
        echo "Removed oldest log file: $oldest_file"
    fi
    # Recalculate the total size
    total_size=$(du -sb $LOG_DIR | awk '{print $1}')
done
