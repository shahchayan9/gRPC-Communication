#pragma once

#include <memory>
#include <string>
#include <thread>
#include <atomic>
#include <grpcpp/grpcpp.h>
#include "data/data_structures.h"
#include "shared_memory/shared_memory.h"

// Include the generated gRPC code
// Note: This will be generated by protoc when building
#include "data_service.grpc.pb.h"

namespace mini2 {

// Forward declaration
class DataServiceImpl;

// gRPC client for DataService
class DataServiceClient {
public:
    DataServiceClient(const std::string& target);
    ~DataServiceClient();
    
    // Query data from the service
    QueryResult queryData(const Query& query);
    
    // Send data to the service (one-way)
    bool sendData(const std::string& source, const std::string& destination, const std::vector<uint8_t>& data);
    
    // Stream data from the service
    bool streamData(const Query& query, std::function<void(const std::vector<uint8_t>&, bool)> callback);
    
    // Check if the client is connected
    bool isConnected() const;

private:
    std::unique_ptr<dataservice::DataService::Stub> stub_;
    std::shared_ptr<grpc::Channel> channel_;
};

// gRPC server for DataService
class DataServiceServer {
public:
    DataServiceServer(const std::string& process_id, const std::string& address);
    ~DataServiceServer();
    
    // Start the server
    bool start();
    
    // Stop the server
    void stop();
    
    // Check if the server is running
    bool isRunning() const;
    
    // Set the query handler
    void setQueryHandler(std::function<QueryResult(const Query&)> handler);
    
    // Set the data message handler
    void setDataHandler(std::function<void(const std::string&, const std::string&, const std::vector<uint8_t>&)> handler);
    
private:
    std::string process_id_;
    std::string address_;
    std::unique_ptr<grpc::Server> server_;
    std::unique_ptr<DataServiceImpl> service_;
    std::thread server_thread_;
    std::atomic<bool> running_;
};

// Implementation of the gRPC service
class DataServiceImpl final : public dataservice::DataService::Service {
public:
    DataServiceImpl(const std::string& process_id);
    
    // Set handlers
    void setQueryHandler(std::function<QueryResult(const Query&)> handler);
    void setDataHandler(std::function<void(const std::string&, const std::string&, const std::vector<uint8_t>&)> handler);
    
    // gRPC service methods
    grpc::Status QueryData(grpc::ServerContext* context, 
                          const dataservice::QueryRequest* request,
                          dataservice::QueryResponse* response) override;
    
    grpc::Status SendData(grpc::ServerContext* context,
                         const dataservice::DataMessage* request,
                         dataservice::Empty* response) override;
    
    grpc::Status StreamData(grpc::ServerContext* context,
                           const dataservice::QueryRequest* request,
                           grpc::ServerWriter<dataservice::DataChunk>* writer) override;
    
private:
    std::string process_id_;
    std::function<QueryResult(const Query&)> query_handler_;
    std::function<void(const std::string&, const std::string&, const std::vector<uint8_t>&)> data_handler_;
    
    // Convert between gRPC and internal types
    Query convertFromGrpc(const dataservice::QueryRequest& request);
    void convertToGrpc(const QueryResult& result, dataservice::QueryResponse* response);
};

} // namespace mini2
