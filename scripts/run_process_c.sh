#!/bin/bash

# Run Process C
CONFIG_FILE="./config/network_config.json"

if [ ! -f "$CONFIG_FILE" ]; then
    echo "Config file not found: $CONFIG_FILE"
    exit 1
fi

# Run process C
./build/src/process_c/process_c $CONFIG_FILE
