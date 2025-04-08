#!/bin/bash

# Run Process E
CONFIG_FILE="./config/network_config.json"

if [ ! -f "$CONFIG_FILE" ]; then
    echo "Config file not found: $CONFIG_FILE"
    exit 1
fi

# Run process E
./build/src/process_e/process_e $CONFIG_FILE
