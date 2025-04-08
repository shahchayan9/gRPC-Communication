#!/bin/bash

# Run Process D
CONFIG_FILE="./config/network_config.json"

if [ ! -f "$CONFIG_FILE" ]; then
    echo "Config file not found: $CONFIG_FILE"
    exit 1
fi

# Run process D
./build/src/process_d/process_d $CONFIG_FILE
