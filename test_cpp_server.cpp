#include <iostream>
#include <memory>
#include <string>
#include <grpcpp/grpcpp.h>
#include "generated/data_service.grpc.pb.h"

class DataServiceImpl final : public dataservice::DataService::Service {
public:
    grpc::Status QueryData(grpc::ServerContext* context, 
                          const dataservice::QueryRequest* request,
                          dataservice::QueryResponse* response) override {
        std::cout << "Received query: " << request->query_string() << std::endl;
        
        response->set_query_id(request->query_id());
        response->set_success(true);
        response->set_message("Success from C++ test server");
        
        auto* entry = response->add_results();
        entry->set_key("test_key");
        entry->set_string_value("This is a test value from C++");
        
        return grpc::Status::OK;
    }
    
    grpc::Status SendData(grpc::ServerContext* context,
                         const dataservice::DataMessage* request,
                         dataservice::Empty* response) override {
        std::cout << "Received data message" << std::endl;
        return grpc::Status::OK;
    }
    
    grpc::Status StreamData(grpc::ServerContext* context,
                           const dataservice::QueryRequest* request,
                           grpc::ServerWriter<dataservice::DataChunk>* writer) override {
        std::cout << "Received stream request" << std::endl;
        return grpc::Status::OK;
    }
};

void RunServer() {
    std::string server_address("0.0.0.0:50071");
    DataServiceImpl service;
    
    grpc::ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    
    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
    std::cout << "Server listening on " << server_address << std::endl;
    server->Wait();
}

int main(int argc, char** argv) {
    RunServer();
    return 0;
}
