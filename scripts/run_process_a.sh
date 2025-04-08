#!/bin/bash

# Run Process A
CONFIG_FILE="./config/network_config.json"

if [ ! -f "$CONFIG_FILE" ]; then
    echo "Config file not found: $CONFIG_FILE"
    exit 1
fi

# Run process A
./build/src/process_a/process_a $CONFIG_FILE
