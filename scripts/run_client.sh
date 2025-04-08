#!/bin/bash

# Run Python client
QUERY=${1:-"get_all"}
SERVER=${2:-"localhost:50051"}

python3 ./python_client/client.py --server $SERVER --query $QUERY
