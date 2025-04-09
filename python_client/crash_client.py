#!/usr/bin/env python3

import sys
import json
import time
import grpc
import argparse
from typing import List, Dict, Any, Optional

# Import generated Protocol Buffer code
from python_client.generated import data_service_pb2
from python_client.generated import data_service_pb2_grpc

class CrashDataClient:
    """
    Client for querying crash data across the distributed system.
    """
    
    def __init__(self, server_address: str):
        """
        Initialize the client with the server address.
        
        Args:
            server_address: Address of the server (host:port)
        """
        self.channel = grpc.insecure_channel(server_address)
        self.stub = data_service_pb2_grpc.DataServiceStub(self.channel)
    
    def get_all_crashes(self):
        """
        Get all crash data across all boroughs.
        
        Returns:
            Dictionary with query results
        """
        return self._execute_query("get_all")
    
    def get_by_borough(self, borough: str):
        """
        Get crash data for a specific borough.
        
        Args:
            borough: Borough name (BROOKLYN, QUEENS, BRONX, STATEN ISLAND)
            
        Returns:
            Dictionary with query results
        """
        return self._execute_query("get_by_borough", [borough])
    
    def get_by_street(self, street: str):
        """
        Get crash data for incidents on a specific street.
        
        Args:
            street: Street name or part of street name
            
        Returns:
            Dictionary with query results
        """
        return self._execute_query("get_by_street", [street])
    
    def get_by_date_range(self, start_date: str, end_date: str):
        """
        Get crash data for incidents within a date range.
        
        Args:
            start_date: Start date in MM/DD/YYYY format
            end_date: End date in MM/DD/YYYY format
            
        Returns:
            Dictionary with query results
        """
        return self._execute_query("get_by_date_range", [start_date, end_date])
    
    def get_crashes_with_injuries(self, min_injuries: int = 1):
        """
        Get crash data for incidents with at least the specified number of injuries.
        
        Args:
            min_injuries: Minimum number of injuries (default: 1)
            
        Returns:
            Dictionary with query results
        """
        return self._execute_query("get_crashes_with_injuries", [str(min_injuries)])
    
    def get_crashes_with_fatalities(self, min_fatalities: int = 1):
        """
        Get crash data for incidents with at least the specified number of fatalities.
        
        Args:
            min_fatalities: Minimum number of fatalities (default: 1)
            
        Returns:
            Dictionary with query results
        """
        return self._execute_query("get_crashes_with_fatalities", [str(min_fatalities)])
    
    def _execute_query(self, query_string: str, parameters: List[str] = None):
        """
        Execute a query and return the results.
        
        Args:
            query_string: The query to execute
            parameters: Optional parameters for the query
            
        Returns:
            Dictionary with query results
        """
        # Create request
        request = data_service_pb2.QueryRequest()
        request.query_id = str(int(time.time() * 1000))  # Use timestamp as ID
        request.query_string = query_string
        
        if parameters:
            for param in parameters:
                request.parameters.append(param)
        
        # Make the call
        try:
            start_time = time.time()
            response = self.stub.QueryData(request)
            end_time = time.time()
            
            # Format results for display
            crashes = []
            
            for entry in response.results:
                # For crash data, we're just getting a placeholder string
                # In a real system, you'd deserialize the actual crash data
                if entry.WhichOneof('value') == 'string_value' and entry.string_value.startswith('CrashData:'):
                    # This is just a placeholder, use the key to identify the crash
                    crashes.append({
                        'id': entry.key,
                        'borough': entry.key.split('_')[0].upper() if '_' in entry.key else 'UNKNOWN',
                        'type': 'Crash Data (details not shown in summary)'
                    })
                else:
                    # Regular data
                    data_entry = {'key': entry.key}
                    
                    # Determine value type
                    value_type = entry.WhichOneof('value')
                    if value_type == 'string_value':
                        data_entry['value'] = entry.string_value
                        data_entry['type'] = 'string'
                    elif value_type == 'int_value':
                        data_entry['value'] = entry.int_value
                        data_entry['type'] = 'int'
                    elif value_type == 'double_value':
                        data_entry['value'] = entry.double_value
                        data_entry['type'] = 'double'
                    elif value_type == 'bool_value':
                        data_entry['value'] = entry.bool_value
                        data_entry['type'] = 'bool'
                    
                    crashes.append(data_entry)
            
            return {
                'query_id': response.query_id,
                'success': response.success,
                'message': response.message,
                'execution_time': f"{(end_time - start_time):.3f} seconds",
                'crash_count': len(crashes),
                'crashes': crashes[:10]  # Show only first 10 for brevity
            }
            
        except grpc.RpcError as e:
            print(f"RPC error: {e.code()}: {e.details()}")
            return {
                'query_id': request.query_id,
                'success': False,
                'message': f"RPC error: {e.code()}: {e.details()}",
                'crashes': []
            }

def main():
    """
    Main entry point for the crash data client.
    """
    parser = argparse.ArgumentParser(description='Crash Data Query System')
    parser.add_argument('--server', type=str, default='localhost:50051',
                        help='Server address (host:port)')
    parser.add_argument('--all', action='store_true', help='Get all crashes')
    parser.add_argument('--borough', type=str, help='Get crashes by borough')
    parser.add_argument('--street', type=str, help='Get crashes by street')
    parser.add_argument('--dates', type=str, nargs=2, metavar=('START_DATE', 'END_DATE'),
                        help='Get crashes in date range (MM/DD/YYYY format)')
    parser.add_argument('--injuries', type=int, default=1, 
                        help='Get crashes with at least N injuries')
    parser.add_argument('--fatalities', type=int, default=1,
                        help='Get crashes with at least N fatalities')
    
    args = parser.parse_args()
    
    # Create client
    client = CrashDataClient(args.server)
    
    # Execute requested query
    result = None
    
    if args.all:
        result = client.get_all_crashes()
    elif args.borough:
        result = client.get_by_borough(args.borough)
    elif args.street:
        result = client.get_by_street(args.street)
    elif args.dates:
        result = client.get_by_date_range(args.dates[0], args.dates[1])
    elif args.injuries:
        result = client.get_crashes_with_injuries(args.injuries)
    elif args.fatalities:
        result = client.get_crashes_with_fatalities(args.fatalities)
    else:
        parser.print_help()
        return
    
    # Print results
    print(json.dumps(result, indent=2))

if __name__ == '__main__':
    main()
