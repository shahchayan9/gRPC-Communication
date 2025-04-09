#!/usr/bin/env python3

import grpc
from concurrent import futures
import time
from python_client.generated import data_service_pb2, data_service_pb2_grpc

class DataServiceImpl(data_service_pb2_grpc.DataServiceServicer):
    def QueryData(self, request, context):
        print(f"Received query: {request.query_string}")
        response = data_service_pb2.QueryResponse()
        response.query_id = request.query_id
        response.success = True
        response.message = "Success from Python test server"
        
        # Add a test result
        entry = data_service_pb2.DataEntry()
        entry.key = "test_key"
        entry.string_value = "This is a test value"
        response.results.append(entry)
        
        return response

def serve():
    server = grpc.server(futures.ThreadPoolExecutor(max_workers=10))
    data_service_pb2_grpc.add_DataServiceServicer_to_server(
        DataServiceImpl(), server)
    server.add_insecure_port('127.0.0.1:50060')
    server.start()
    print("Server started on port 50060")
    try:
        while True:
            time.sleep(3600)
    except KeyboardInterrupt:
        server.stop(0)

if __name__ == '__main__':
    serve()
