#include "data_service.h"
#include <iostream>

namespace mini2 {

// DataServiceClient implementation
DataServiceClient::DataServiceClient(const std::string& target) {
    // Create a channel to the server
    channel_ = grpc::CreateChannel(target, grpc::InsecureChannelCredentials());
    stub_ = dataservice::DataService::NewStub(channel_);
}

DataServiceClient::~DataServiceClient() {
    // Clean up
}

QueryResult DataServiceClient::queryData(const Query& query) {
    // Create request
    dataservice::QueryRequest request;
    request.set_query_id(query.id);
    request.set_query_string(query.query_string);
    
    for (const auto& param : query.parameters) {
        request.add_parameters(param);
    }
    
    // Call the RPC
    dataservice::QueryResponse response;
    grpc::ClientContext context;
    
    grpc::Status status = stub_->QueryData(&context, request, &response);
    
    // Process response
    if (status.ok()) {
        QueryResult result;
        result.query_id = response.query_id();
        result.success = response.success();
        result.message = response.message();
        
        for (const auto& grpc_entry : response.results()) {
            DataEntry entry;
            entry.key = grpc_entry.key();
            entry.timestamp = DataEntry::getCurrentTimestamp();
            
            switch (grpc_entry.value_case()) {
                case dataservice::DataEntry::kStringValue:
                    entry.value = grpc_entry.string_value();
                    break;
                case dataservice::DataEntry::kIntValue:
                    entry.value = grpc_entry.int_value();
                    break;
                case dataservice::DataEntry::kDoubleValue:
                    entry.value = grpc_entry.double_value();
                    break;
                case dataservice::DataEntry::kBoolValue:
                    entry.value = grpc_entry.bool_value();
                    break;
                default:
                    // Unknown value type
                    break;
            }
            
            result.results.push_back(entry);
        }
        
        return result;
    } else {
        return QueryResult::createFailure(query.id, 
            "RPC failed: " + status.error_message());
    }
}

bool DataServiceClient::sendData(const std::string& source, const std::string& destination, 
                                const std::vector<uint8_t>& data) {
    // Create request
    dataservice::DataMessage request;
    request.set_message_id(std::to_string(DataEntry::getCurrentTimestamp()));
    request.set_source(source);
    request.set_destination(destination);
    request.set_data(data.data(), data.size());
    
    // Call the RPC
    dataservice::Empty response;
    grpc::ClientContext context;
    
    grpc::Status status = stub_->SendData(&context, request, &response);
    
    return status.ok();
}

bool DataServiceClient::streamData(const Query& query, 
                                  std::function<void(const std::vector<uint8_t>&, bool)> callback) {
    // Create request
    dataservice::QueryRequest request;
    request.set_query_id(query.id);
    request.set_query_string(query.query_string);
    
    for (const auto& param : query.parameters) {
        request.add_parameters(param);
    }
    
    // Call the streaming RPC
    grpc::ClientContext context;
    auto reader = stub_->StreamData(&context, request);
    
    // Process stream
    dataservice::DataChunk chunk;
    while (reader->Read(&chunk)) {
        std::vector<uint8_t> data(chunk.data().begin(), chunk.data().end());
        callback(data, chunk.is_last());
    }
    
    grpc::Status status = reader->Finish();
    return status.ok();
}

bool DataServiceClient::isConnected() const {
    return channel_->GetState(false) == GRPC_CHANNEL_READY ||
           channel_->GetState(false) == GRPC_CHANNEL_IDLE;
}

// DataServiceServer implementation
DataServiceServer::DataServiceServer(const std::string& process_id, const std::string& address)
    : process_id_(process_id), address_(address), running_(false) {
    service_ = std::make_unique<DataServiceImpl>(process_id);
}

DataServiceServer::~DataServiceServer() {
    stop();
}

bool DataServiceServer::start() {
    if (running_) {
        return true; // Already running
    }
    
    // Build server
    grpc::ServerBuilder builder;
    builder.AddListeningPort(address_, grpc::InsecureServerCredentials());
    builder.RegisterService(service_.get());
    
    // Start server
    server_ = builder.BuildAndStart();
    if (!server_) {
        std::cerr << "Failed to start server at " << address_ << std::endl;
        return false;
    }
    
    // Start server thread
    running_ = true;
    server_thread_ = std::thread([this]() {
        std::cout << "Server started at " << address_ << std::endl;
        server_->Wait();
    });
    
    return true;
}

void DataServiceServer::stop() {
    if (!running_) {
        return;
    }
    
    running_ = false;
    
    // Shutdown server
    if (server_) {
        server_->Shutdown();
    }
    
    // Join server thread
    if (server_thread_.joinable()) {
        server_thread_.join();
    }
    
    std::cout << "Server stopped" << std::endl;
}

bool DataServiceServer::isRunning() const {
    return running_;
}

void DataServiceServer::setQueryHandler(std::function<QueryResult(const Query&)> handler) {
    service_->setQueryHandler(handler);
}

void DataServiceServer::setDataHandler(std::function<void(const std::string&, const std::string&, const std::vector<uint8_t>&)> handler) {
    service_->setDataHandler(handler);
}

// DataServiceImpl implementation
DataServiceImpl::DataServiceImpl(const std::string& process_id)
    : process_id_(process_id) {
}

void DataServiceImpl::setQueryHandler(std::function<QueryResult(const Query&)> handler) {
    query_handler_ = handler;
}

void DataServiceImpl::setDataHandler(std::function<void(const std::string&, const std::string&, const std::vector<uint8_t>&)> handler) {
    data_handler_ = handler;
}

grpc::Status DataServiceImpl::QueryData(grpc::ServerContext* context, 
                                      const dataservice::QueryRequest* request,
                                      dataservice::QueryResponse* response) {
    if (!query_handler_) {
        return grpc::Status(grpc::StatusCode::UNIMPLEMENTED, "Query handler not set");
    }
    
    // Convert request to internal Query
    Query query = convertFromGrpc(*request);
    
    // Process the query
    QueryResult result = query_handler_(query);
    
    // Convert result to gRPC response
    convertToGrpc(result, response);
    
    return grpc::Status::OK;
}

grpc::Status DataServiceImpl::SendData(grpc::ServerContext* context,
                                     const dataservice::DataMessage* request,
                                     dataservice::Empty* response) {
    if (!data_handler_) {
        return grpc::Status(grpc::StatusCode::UNIMPLEMENTED, "Data handler not set");
    }
    
    // Convert data
    std::string source = request->source();
    std::string destination = request->destination();
    std::vector<uint8_t> data(request->data().begin(), request->data().end());
    
    // Process the data
    data_handler_(source, destination, data);
    
    return grpc::Status::OK;
}

grpc::Status DataServiceImpl::StreamData(grpc::ServerContext* context,
                                       const dataservice::QueryRequest* request,
                                       grpc::ServerWriter<dataservice::DataChunk>* writer) {
    if (!query_handler_) {
        return grpc::Status(grpc::StatusCode::UNIMPLEMENTED, "Query handler not set");
    }
    
    // Convert request to internal Query
    Query query = convertFromGrpc(*request);
    
    // Process the query
    QueryResult result = query_handler_(query);
    
    // Stream the results
    for (size_t i = 0; i < result.results.size(); ++i) {
        const auto& entry = result.results[i];
        
        // Create chunk
        dataservice::DataChunk chunk;
        chunk.set_chunk_id(entry.key);
        chunk.set_is_last(i == result.results.size() - 1);
        
        // Serialize entry to chunk data
        // In a real implementation, you would use a proper serialization format
        std::string serialized = entry.key + ":";
        
        if (std::holds_alternative<int>(entry.value)) {
            serialized += "int:" + std::to_string(std::get<int>(entry.value));
        }
        else if (std::holds_alternative<double>(entry.value)) {
            serialized += "double:" + std::to_string(std::get<double>(entry.value));
        }
        else if (std::holds_alternative<bool>(entry.value)) {
            serialized += "bool:" + std::string(std::get<bool>(entry.value) ? "true" : "false");
        }
        else if (std::holds_alternative<std::string>(entry.value)) {
            serialized += "string:" + std::get<std::string>(entry.value);
        }
        else if (std::holds_alternative<std::vector<uint8_t>>(entry.value)) {
            serialized += "binary:";
            // Just indicate it's binary data
        }
        
        chunk.set_data(serialized);
        
        // Write chunk
        if (!writer->Write(chunk)) {
            break;
        }
    }
    
    return grpc::Status::OK;
}

Query DataServiceImpl::convertFromGrpc(const dataservice::QueryRequest& request) {
    Query query;
    query.id = request.query_id();
    query.query_string = request.query_string();
    
    for (const auto& param : request.parameters()) {
        query.parameters.push_back(param);
    }
    
    return query;
}

void DataServiceImpl::convertToGrpc(const QueryResult& result, dataservice::QueryResponse* response) {
    response->set_query_id(result.query_id);
    response->set_success(result.success);
    response->set_message(result.message);
    
    for (const auto& entry : result.results) {
        auto* grpc_entry = response->add_results();
        grpc_entry->set_key(entry.key);
        
        if (std::holds_alternative<int>(entry.value)) {
            grpc_entry->set_int_value(std::get<int>(entry.value));
        }
        else if (std::holds_alternative<double>(entry.value)) {
            grpc_entry->set_double_value(std::get<double>(entry.value));
        }
        else if (std::holds_alternative<bool>(entry.value)) {
            grpc_entry->set_bool_value(std::get<bool>(entry.value));
        }
        else if (std::holds_alternative<CrashData>(entry.value)) {
            // Convert CrashData to a string representation or extract a relevant field
            const CrashData& crash = std::get<CrashData>(entry.value);
            // Include more crash info in a structured format
            std::string crash_info = "Date: " + crash.crash_date + 
                                   ", Time: " + crash.crash_time +
                                   ", Borough: " + crash.borough + 
                                   ", Killed: " + std::to_string(crash.persons_killed);
            grpc_entry->set_string_value(crash_info);
        }
        else if (std::holds_alternative<std::string>(entry.value)) {
            grpc_entry->set_string_value(std::get<std::string>(entry.value));
        }
        // Binary data type not directly supported in the proto
    }
}

} // namespace mini2
