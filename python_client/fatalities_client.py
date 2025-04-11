#!/usr/bin/env python3

import sys
import os
import json
import time
import grpc
import argparse

# Set up paths to find the generated protocol buffer modules
current_dir = os.path.dirname(os.path.abspath(__file__))
project_root = os.path.dirname(current_dir)
generated_dir = os.path.join(current_dir, 'generated')

sys.path.insert(0, project_root)
sys.path.insert(0, current_dir)
sys.path.insert(0, generated_dir)

# Try multiple import approaches to handle different directory structures
try:
    # Direct import from generated directory
    sys.path.insert(0, os.path.join(current_dir, 'generated'))
    from generated import data_service_pb2
    from generated import data_service_pb2_grpc
    print("Imported protocol buffers from local generated directory")
except ImportError:
    try:
        # Import with python_client prefix
        from python_client.generated import data_service_pb2
        from python_client.generated import data_service_pb2_grpc
        print("Imported protocol buffers with python_client prefix")
    except ImportError:
        try:
            # Try absolute imports
            import data_service_pb2
            import data_service_pb2_grpc
            print("Imported protocol buffers directly")
        except ImportError:
            print("Error: Unable to import the Protocol Buffer files.")
            print("Current directory:", os.getcwd())
            print("Python path:", sys.path)
            print("Looking for files in:", generated_dir)
            
            # Check if the files exist
            pb2_path = os.path.join(generated_dir, 'data_service_pb2.py')
            grpc_path = os.path.join(generated_dir, 'data_service_pb2_grpc.py')
            
            if os.path.exists(pb2_path):
                print(f"data_service_pb2.py exists at {pb2_path}")
            else:
                print(f"data_service_pb2.py NOT FOUND at {pb2_path}")
                
            if os.path.exists(grpc_path):
                print(f"data_service_pb2_grpc.py exists at {grpc_path}")
            else:
                print(f"data_service_pb2_grpc.py NOT FOUND at {grpc_path}")
                
            print("\nPlease regenerate the protocol buffer files with:")
            print("./scripts/generate_python_proto_fixed.sh")
            sys.exit(1)

def query_crashes_with_fatalities(server_address, min_fatalities):
    """
    Query crashes with at least the specified number of fatalities.
    
    Args:
        server_address: Server address (host:port)
        min_fatalities: Minimum number of fatalities
    
    Returns:
        Dictionary with query results
    """
    # Create channel and stub
    channel = grpc.insecure_channel(server_address)
    stub = data_service_pb2_grpc.DataServiceStub(channel)
    
    # Create request
    request = data_service_pb2.QueryRequest()
    request.query_id = str(int(time.time() * 1000))  # Use timestamp as ID
    request.query_string = "get_crashes_with_fatalities"
    request.parameters.append(str(min_fatalities))
    
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
                
                # Try to parse CrashData from string if it contains "Date:", "Time:", etc.
                if "Date:" in entry.string_value and "Killed:" in entry.string_value:
                    result['type'] = 'crash_data'
                    # Extract the fatality count if present
                    killed_part = entry.string_value.split("Killed:")[1].strip() if "Killed:" in entry.string_value else "Unknown"
                    result['fatalities'] = killed_part
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
    parser = argparse.ArgumentParser(description='Query Crashes with Fatalities')
    parser.add_argument('--server', type=str, default='localhost:50051',
                      help='Server address (host:port)')
    parser.add_argument('--fatalities', type=int, required=True,
                      help='Minimum number of fatalities')
    
    args = parser.parse_args()
    
    # Execute query
    result = query_crashes_with_fatalities(args.server, args.fatalities)
    
    # Print result
    print(json.dumps(result, indent=2))
    
    # Also print a summary of actual crash data entries
    crash_data_entries = [r for r in result.get('results', []) if r.get('type') == 'crash_data']
    print(f"\nFound {len(crash_data_entries)} actual crash records (out of {result.get('result_count', 0)} total entries)")
    
    # Show number of entries with each fatality count
    if crash_data_entries:
        fatality_counts = {}
        for entry in crash_data_entries:
            fatality_count = entry.get('fatalities', '0')
            fatality_counts[fatality_count] = fatality_counts.get(fatality_count, 0) + 1
        
        print("\nFatality counts:")
        for count, num_entries in sorted(fatality_counts.items()):
            print(f"  {count} fatalities: {num_entries} entries")

if __name__ == '__main__':
    main()
