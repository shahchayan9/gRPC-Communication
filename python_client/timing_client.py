#!/usr/bin/env python3

import sys
import os
import json
import time
import grpc
import argparse
import re
from prettytable import PrettyTable

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

def parse_timing_data(timing_data):
    """Parse the timing data into a structured format."""
    timing_info = {}
    current_process = None
    
    # Process each line of timing data
    for line in timing_data.split('\n'):
        line = line.strip()
        
        # Check for process identifier
        process_match = re.match(r'\s*\[Process\s+([A-E])\]\s*', line)
        if process_match:
            current_process = process_match.group(1)
            timing_info[current_process] = {}
            continue
            
        # Check for timing entry
        timing_match = re.match(r'\s*([A-Za-z_]+)\s*:\s*([0-9.]+)\s*seconds.*', line)
        if timing_match and current_process:
            operation = timing_match.group(1)
            time_value = float(timing_match.group(2))
            timing_info[current_process][operation] = time_value
            
    return timing_info

def query_with_timing(server_address, query_type, params=None):
    """Query the system and display detailed timing information."""
    # Create channel and stub
    channel = grpc.insecure_channel(server_address)
    stub = data_service_pb2_grpc.DataServiceStub(channel)
    
    # Create request
    request = data_service_pb2.QueryRequest()
    request.query_id = str(int(time.time() * 1000))  # Use timestamp as ID
    request.query_string = query_type
    
    if params:
        for param in params:
            request.parameters.append(param)
    
    # Start client-side timing
    start_time = time.time()
    
    # Make the call
    try:
        response = stub.QueryData(request)
        end_time = time.time()
        
        # Calculate total time
        total_time = end_time - start_time
        
        # Process the results
        results = []
        for entry in response.results:
            result = {'key': entry.key}
            
            value_type = entry.WhichOneof('value')
            if value_type == 'string_value':
                result['value'] = entry.string_value
                result['type'] = 'string'
                
                # Try to parse CrashData
                if "Date:" in entry.string_value and "Time:" in entry.string_value:
                    result['type'] = 'crash_data'
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
        
        # Parse timing data if available
        timing_info = None
        if hasattr(response, 'timing_data') and response.timing_data:
            timing_info = parse_timing_data(response.timing_data)
        
        return {
            'query_id': response.query_id,
            'success': response.success,
            'message': response.message,
            'client_time': total_time,
            'result_count': len(results),
            'timing_info': timing_info,
            'results': results[:5]  # Show only first 5 results for brevity
        }
        
    except grpc.RpcError as e:
        print(f"RPC error: {e.code()}: {e.details()}")
        return {
            'success': False,
            'message': f"RPC error: {e.code()}: {e.details()}",
            'results': []
        }

def display_timing_results(result):
    """Display the timing results in a nicely formatted table."""
    print(f"\n===== Query Results =====")
    print(f"Query ID: {result['query_id']}")
    print(f"Success: {result['success']}")
    print(f"Message: {result['message']}")
    print(f"Total Results: {result['result_count']}")
    print(f"Client-side Total Time: {result['client_time']:.6f} seconds")
    
    # Display sample results
    if result['results']:
        print("\nSample Results:")
        for i, item in enumerate(result['results']):
            print(f"  {i+1}. {item['key']}: {item.get('value', 'N/A')} ({item['type']})")
    
    # Display timing information
    if result.get('timing_info'):
        print("\n===== Timing Metrics =====")
        
        # Create a table for timing data
        table = PrettyTable()
        table.field_names = ["Process", "Operation", "Time (seconds)"]
        
        # Get all unique operations across processes
        all_operations = set()
        for process, operations in result['timing_info'].items():
            all_operations.update(operations.keys())
        
        # Sort operations for consistent display
        sorted_operations = sorted(all_operations)
        
        # Add data to table
        for process in sorted(result['timing_info'].keys()):
            operations = result['timing_info'][process]
            
            for operation in sorted_operations:
                if operation in operations:
                    table.add_row([process, operation, f"{operations[operation]:.6f}"])
                else:
                    table.add_row([process, operation, "N/A"])
            
            # Add separator between processes
            if process != sorted(result['timing_info'].keys())[-1]:
                table.add_row(["-" * 7, "-" * 20, "-" * 12])
        
        # Set table formatting
        table.align = "l"
        print(table)
        
        # Summary statistics
        print("\nTiming Summary:")
        print(f"  Client-side Total Time: {result['client_time']:.6f} seconds")
        
        # Calculate server-side processing time (if available)
        total_processing_times = []
        for process, operations in result['timing_info'].items():
            if 'Total_Processing' in operations:
                total_processing_times.append(operations['Total_Processing'])
        
        if total_processing_times:
            max_processing_time = max(total_processing_times)
            print(f"  Server-side Max Processing Time: {max_processing_time:.6f} seconds")
            print(f"  Network Overhead: {result['client_time'] - max_processing_time:.6f} seconds")

def main():
    parser = argparse.ArgumentParser(description='Query with Detailed Timing')
    parser.add_argument('--server', type=str, default='localhost:50051',
                       help='Server address (host:port)')
    parser.add_argument('--query', type=str, required=True,
                       help='Query type (get_all, get_by_borough, get_by_time, etc.)')
    parser.add_argument('--params', type=str, nargs='*', default=[],
                       help='Query parameters')
    
    args = parser.parse_args()
    
    # Install PrettyTable if not already present
    try:
        from prettytable import PrettyTable
    except ImportError:
        print("PrettyTable not installed. Installing...")
        import subprocess
        try:
            subprocess.check_call([sys.executable, "-m", "pip", "install", "prettytable"])
            from prettytable import PrettyTable
            print("PrettyTable installed successfully.")
        except Exception as e:
            print(f"Failed to install PrettyTable: {e}")
            print("Continuing without pretty formatting...")
    
    # Execute query with timing
    result = query_with_timing(args.server, args.query, args.params)
    
    # Display results
    display_timing_results(result)

if __name__ == '__main__':
    main()
