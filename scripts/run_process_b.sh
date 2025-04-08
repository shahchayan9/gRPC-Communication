#!/bin/bash

# Run Process B
CONFIG_FILE="./config/network_config.json"

if [ ! -f "$CONFIG_FILE" ]; then
    echo "Config file not found: $CONFIG_FILE"
    exit 1
fi

# Run process B
./build/src/process_b/process_b $CONFIG_FILE
