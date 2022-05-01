#pragma warning(push)
#pragma warning(disable : 4251 26451 26812 26495 6387 6385)
#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>
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
#pragma warning(disable : 4251 26451 26812 26495 6387 6385)
#include "usi.grpc.pb.h"
#pragma warning(pop)

class UsiProtocolServer final : public usi::UsiProtocol::Service
{
public:
  grpc::Status Communicate(grpc::ServerContext *context,
                           grpc::ServerReaderWriter<usi::GuiMessage, usi::EngineMessage> *stream) override {
    input_thread_ = std::make_unique<std::thread>(std::bind(&UsiProtocolServer::ReadStandardInput, this, stream));

    usi::EngineMessage em;
    while (stream->Read(&em)) {
      switch (em.msg_case()) {
      case usi::EngineMessage::MsgCase::kId: {
        usi::EngineId id = em.id();
        std::cout << "id name " << id.name() << std::endl;
        std::cout << "id author " << id.author() << std::endl;
      } break;
      case usi::EngineMessage::MsgCase::kSm: {
        usi::SingleMessage msg = em.sm();
        switch (msg) {
        case usi::ISREADY: std::cout << "isready" << std ::endl; break;
        default: break;
        }
      } break;
      default: break;
      }
    }
    if (!exit_flag_) {
      // GUIからのメッセージではなく、エンジンとの接続が切れてループを抜けたと思われる
      // getlineで止まっていると思うので、スレッドを捨てる
      input_thread_->detach();
    } else {
      input_thread_->join();
    }

    return grpc::Status::OK;
  }

  // 標準入力から読むこむ
  void ReadStandardInput(grpc::ServerReaderWriter<usi::GuiMessage, usi::EngineMessage> *stream) {
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
        stream->Write(msg);

        exit_flag_ = true;
        break;
      } else if (token == "usi") {
        usi::GuiMessage msg;
        msg.set_sm(usi::SingleMessage::USI);
        stream->Write(msg);
      }
    }
  }

private:
  std::mutex mutex_;
  std::condition_variable cv_;
  std::unique_ptr<std::thread> input_thread_;

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
  server->Wait();
}

int main() {
  RunServer();

  return 0;
}
