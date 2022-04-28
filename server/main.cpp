#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>

#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include "usi.grpc.pb.h"

class UsiProtocolImpl final : public usi::UsiProtocol::Service {
  grpc::Status communicate(grpc::ServerContext* context, grpc::ServerReaderWriter<usi::GuiMessage, usi::EngineMessage>* stream) override {
    std::cout << "start" << std::endl;

    usi::EngineMessage engine;
    while (stream->Read(&engine)) {

    }

    std::cout << "finish" << std::endl;
    return grpc::Status::OK;
  }
};

void RunServer() {
  std::string server_address("0.0.0.0:50051");
  UsiProtocolImpl service;

  grpc::ServerBuilder builder;
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);
  std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
  std::cout << "Server listening on " << server_address << std::endl;
  server->Wait();
}

int main() {
  RunServer();

  return 0;
}
