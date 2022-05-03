#pragma warning(push)
#pragma warning(disable : 4251 4267 26451 26812 26495 6387 6385)
#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>
#include <google/protobuf/empty.pb.h>
#pragma warning(pop)

#include <condition_variable>
#include <iostream>
#include <memory>
#include <mutex>
#include <queue>
#include <sstream>
#include <string>
#include <thread>

#pragma warning(push)
#pragma warning(disable : 4251 4267 26451 26812 26495 6387 6385)
#include "usi.grpc.pb.h"
#pragma warning(pop)

class UsiProtocolServer final : public usi::UsiProtocol::Service
{
public:
  grpc::Status RecieveMessage(grpc::ServerContext *context, const ::google::protobuf::Empty *empty,
                              grpc::ServerWriter<usi::GuiMessage> *writer) override {
    std::string cmd, token;
    std::string position;
    while (true) {
      if (!std::getline(std::cin, cmd)) {
        cmd = "quit";
      }

      std::istringstream iss(cmd);

      token.clear();
      iss >> std::skipws >> token;

      if (token == "quit") {
        usi::GuiMessage msg;
        msg.set_sm(usi::SingleMessage::QUIT);
        writer->Write(msg);

        std::unique_lock<std::mutex> lock(mutex_);
        exit_flag_ = true;
        cv_.notify_one();
        break;
      } else if (token == "usi") {
        usi::GuiMessage msg;
        msg.set_sm(usi::SingleMessage::USI);
        writer->Write(msg);
      }
    }

    return grpc::Status::OK;
  }

  grpc::Status SendEngineId(grpc::ServerContext *context, const usi::EngineId *engine_id,
                            ::google::protobuf::Empty *empty) override {
    std::cout << "id name " << engine_id->name() << std::endl;
    std::cout << "id author " << engine_id->author() << std::endl;

    return grpc::Status::OK;
  }

  void Shutdown(std::unique_ptr<grpc::Server> &server) {
    std::unique_lock<std::mutex> lock(mutex_);
    cv_.wait(lock, [this] { return exit_flag_; });
    server->Shutdown();
  }

private:
  std::mutex mutex_;
  std::condition_variable cv_;

  bool exit_flag_ = false;
};

void RunServer() {
  std::string server_address("0.0.0.0:50051");
  UsiProtocolServer service;

  grpc::ServerBuilder builder;
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);
  std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
  std::cout << "Server listening on " << server_address << std::endl;

  std::thread th(std::bind(&UsiProtocolServer::Shutdown, &service, std::ref(server)));
  server->Wait();
  th.join();
}

int main() {
  RunServer();

  return 0;
}
