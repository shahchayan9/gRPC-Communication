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

class DataClient:
    """
    Client for interacting with the data service.
    """
    
    def __init__(self, server_address: str):
        """
        Initialize the client with the server address.
        
        Args:
            server_address: Address of the server (host:port)
        """
        self.channel = grpc.insecure_channel(server_address)
        self.stub = data_service_pb2_grpc.DataServiceStub(self.channel)
    
    def query_data(self, query_string: str, parameters: List[str] = None) -> Dict[str, Any]:
        """
        Send a query to the server.
        
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
            response = self.stub.QueryData(request)
            
            # Convert to dictionary
            result = {
                'query_id': response.query_id,
                'success': response.success,
                'message': response.message,
                'results': []
            }
            
            # Process results
            for entry in response.results:
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
                
                result['results'].append(data_entry)
            
            return result
            
        except grpc.RpcError as e:
            print(f"RPC error: {e.code()}: {e.details()}")
            return {
                'query_id': request.query_id,
                'success': False,
                'message': f"RPC error: {e.code()}: {e.details()}",
                'results': []
            }
    
    def send_data(self, source: str, destination: str, data: bytes) -> bool:
        """
        Send data to a specific destination.
        
        Args:
            source: Source identifier
            destination: Destination identifier
            data: Data to send
            
        Returns:
            True if successful, False otherwise
        """
        # Create request
        request = data_service_pb2.DataMessage()
        request.message_id = str(int(time.time() * 1000))  # Use timestamp as ID
        request.source = source
        request.destination = destination
        request.data = data
        
        # Make the call
        try:
            self.stub.SendData(request)
            return True
        except grpc.RpcError as e:
            print(f"RPC error: {e.code()}: {e.details()}")
            return False
    
    def stream_data(self, query_string: str, parameters: List[str] = None) -> List[Dict[str, Any]]:
        """
        Stream data from the server.
        
        Args:
            query_string: The query to execute
            parameters: Optional parameters for the query
            
        Returns:
            List of data chunks
        """
        # Create request
        request = data_service_pb2.QueryRequest()
        request.query_id = str(int(time.time() * 1000))  # Use timestamp as ID
        request.query_string = query_string
        
        if parameters:
            for param in parameters:
                request.parameters.append(param)
        
        # Make the call
        chunks = []
        try:
            for response in self.stub.StreamData(request):
                chunk = {
                    'chunk_id': response.chunk_id,
                    'data': response.data,
                    'is_last': response.is_last
                }
                chunks.append(chunk)
            
            return chunks
            
        except grpc.RpcError as e:
            print(f"RPC error: {e.code()}: {e.details()}")
            return []

def main():
    """
    Main entry point for the client.
    """
    parser = argparse.ArgumentParser(description='Data Service Client')
    parser.add_argument('--server', type=str, default='localhost:50051',
                        help='Server address (host:port)')
    parser.add_argument('--query', type=str, help='Query to execute')
    parser.add_argument('--params', type=str, nargs='*', help='Query parameters')
    parser.add_argument('--send', action='store_true', help='Send data')
    parser.add_argument('--source', type=str, help='Source for data')
    parser.add_argument('--dest', type=str, help='Destination for data')
    parser.add_argument('--data', type=str, help='Data to send')
    parser.add_argument('--stream', action='store_true', help='Stream data')
    
    args = parser.parse_args()
    
    # Create client
    client = DataClient(args.server)
    
    if args.query:
        # Execute query
        result = client.query_data(args.query, args.params)
        print(json.dumps(result, indent=2))
    
    elif args.send and args.source and args.dest and args.data:
        # Send data
        success = client.send_data(args.source, args.dest, args.data.encode('utf-8'))
        print(f"Send result: {'Success' if success else 'Failed'}")
    
    elif args.stream and args.query:
        # Stream data
        chunks = client.stream_data(args.query, args.params)
        for chunk in chunks:
            print(f"Chunk: {chunk['chunk_id']}, last: {chunk['is_last']}")
            print(f"Data: {chunk['data']}")
    
    else:
        parser.print_help()

if __name__ == '__main__':
    main()
