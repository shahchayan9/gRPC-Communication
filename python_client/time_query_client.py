#!/usr/bin/env python3

import sys
import os
import json
import time
import grpc
import argparse

# Add the current directory to the path
current_dir = os.path.dirname(os.path.abspath(__file__))
sys.path.append(current_dir)
sys.path.append(os.path.join(current_dir, 'generated'))

# Import directly from the generated files in the same directory
try:
    import generated.data_service_pb2 as data_service_pb2
    import generated.data_service_pb2_grpc as data_service_pb2_grpc
except ImportError:
    try:
        from python_client.generated import data_service_pb2
        from python_client.generated import data_service_pb2_grpc
    except ImportError:
        print("Error: Unable to import the generated Protocol Buffer files.")
        print("Please run the script from the project root directory.")
        sys.exit(1)

def query_crashes_by_time(server_address, crash_time):
    """
    Query crashes that occurred at a specific time.
    
    Args:
        server_address: Server address (host:port)
        crash_time: Time to search for (e.g., "8:00")
    
    Returns:
        Dictionary with query results
    """
    # Create channel and stub
    channel = grpc.insecure_channel(server_address)
    stub = data_service_pb2_grpc.DataServiceStub(channel)
    
    # Create request
    request = data_service_pb2.QueryRequest()
    request.query_id = str(int(time.time() * 1000))  # Use timestamp as ID
    request.query_string = "get_by_time"
    request.parameters.append(crash_time)
    
    # Make the call
    try:
        start_time = time.time()
        response = stub.QueryData(request)
        end_time = time.time()
        
        # Process response
        results = []
        for entry in response.results:
            result = {'key': entry.key}
            
            value_type = entry.WhichOneof('value')
            if value_type == 'string_value':
                result['value'] = entry.string_value
                result['type'] = 'string'
            elif value_type == 'int_value':
                result['value'] = entry.int_value
                result['type'] = 'int'
            elif value_type == 'double_value':
                result['value'] = entry.double_value
                result['type'] = 'double'
            elif value_type == 'bool_value':
                result['value'] = entry.bool_value
                result['type'] = 'bool'
            
            results.append(result)
        
        # Create response object
        return {
            'query_id': response.query_id,
            'success': response.success,
            'message': response.message,
            'execution_time': f"{(end_time - start_time):.3f} seconds",
            'result_count': len(results),
            'results': results[:20]  # Only show first 20 results
        }
        
    except grpc.RpcError as e:
        print(f"RPC error: {e.code()}: {e.details()}")
        return {
            'success': False,
            'message': f"RPC error: {e.code()}: {e.details()}",
            'results': []
        }

def main():
    parser = argparse.ArgumentParser(description='Query Crashes by Time')
    parser.add_argument('--server', type=str, default='localhost:50051',
                      help='Server address (host:port)')
    parser.add_argument('--time', type=str, required=True,
                      help='Crash time to search for (e.g., "8:00")')
    
    args = parser.parse_args()
    
    # Execute query
    result = query_crashes_by_time(args.server, args.time)
    
    # Print result
    print(json.dumps(result, indent=2))

if __name__ == '__main__':
    main()