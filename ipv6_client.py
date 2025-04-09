#!/usr/bin/env python3

import grpc
import time
import argparse
from python_client.generated import data_service_pb2, data_service_pb2_grpc

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--server', default='[::1]:50051')
    parser.add_argument('--query', default='get_all')
    args = parser.parse_args()
    
    # Create channel with IPv6 options
    options = [('grpc.enable_http_proxy', 0)]
    channel = grpc.insecure_channel(args.server, options=options)
    stub = data_service_pb2_grpc.DataServiceStub(channel)
    
    request = data_service_pb2.QueryRequest()
    request.query_id = str(int(time.time() * 1000))
    request.query_string = args.query
    
    try:
        response = stub.QueryData(request)
        print(f"Success: {response.success}")
        print(f"Message: {response.message}")
        print(f"Results: {len(response.results)} entries")
        
        for entry in response.results:
            print(f"Entry key: {entry.key}")
    except Exception as e:
        print(f"Error: {e}")

if __name__ == "__main__":
    main()
