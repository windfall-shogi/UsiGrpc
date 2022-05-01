#pragma warning(push)
#pragma warning(disable : 4251 26451 26812 26495 6387 6385)
#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>
#pragma warning(pop)

#include <iostream>
#include <memory>
#include <string>

#pragma warning(push)
#pragma warning(disable : 4251 26451 26812 26495 6387 6385)
#include "usi.grpc.pb.h"
#pragma warning(pop)

enum class Type { READ = 1, WRITE = 2, CONNECT = 3, DONE = 4, FINISH = 5 };

class AsyncUsiProtocolServer
{
public:
  AsyncUsiProtocolServer() {
    // In general avoid setting up the server in the main thread (specifically,
    // in a constructor-like function such as this). We ignore this in the
    // context of an example.
    std::string server_address("0.0.0.0:50051");

    grpc::ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service_);
    cq_ = builder.AddCompletionQueue();
    server_ = builder.BuildAndStart();

    // This initiates a single stream for a single client. To allow multiple
    // clients in different threads to connect, simply 'request' from the
    // different threads. Each stream is independent but can use the same
    // completion queue/context objects.
    stream_ = std::make_unique<grpc::ServerAsyncReaderWriter<usi::GuiMessage, usi::EngineMessage>>(&context_);
    service_.RequestCommunicate(&context_, stream_.get(), cq_.get(), cq_.get(),
                                reinterpret_cast<void *>(Type::CONNECT));

    // This is important as the server should know when the client is done.
    context_.AsyncNotifyWhenDone(reinterpret_cast<void *>(Type::DONE));

    grpc_thread_ = std::make_unique<std::thread>((std::bind(&AsyncUsiProtocolServer::GrpcThread, this)));
    std::cout << "Server listening on " << server_address << std::endl;
  }

  ~AsyncUsiProtocolServer() {
    std::cout << "Shutting down server...." << std::endl;
    server_->Shutdown();
    // Always shutdown the completion queue after the server.
    cq_->Shutdown();
    grpc_thread_->join();
  }

  void SetResponse(const std::string &response) {
    if (response == "quit" && IsRunning()) {
      stream_->Finish(grpc::Status::CANCELLED, reinterpret_cast<void *>(Type::FINISH));
    }
    response_str_ = response;
  }
  bool IsRunning() const { return is_running_; }

  void GrpcThread() {
    while (true) {
      void *got_tag = nullptr;
      bool ok = false;
      if (!cq_->Next(&got_tag, &ok)) {
        std::cerr << "Server stream closed. Quitting" << std::endl;
        break;
      }

      // assert(ok);

      if (ok) {
        std::cout << std::endl << "**** Processing completion queue tag " << got_tag << std::endl;
        switch (static_cast<Type>(reinterpret_cast<size_t>(got_tag))) {
        case Type::READ:
          std::cout << "Read a new message." << std::endl;
          // AsyncHelloSendResponse();
          break;
        case Type::WRITE:
          std::cout << "Sending message (async)." << std::endl;
          // AsyncWaitForHelloRequest();
          break;
        case Type::CONNECT:
          std::cout << "Client connected." << std::endl;
          // AsyncWaitForHelloRequest();
          break;
        case Type::DONE:
          std::cout << "Server disconnecting." << std::endl;
          is_running_ = false;
          break;
        case Type::FINISH: std::cout << "Server quitting." << std::endl; break;
        default: std::cerr << "Unexpected tag " << got_tag << std::endl; assert(false);
        }
      }
    }
  }

private:
  usi::EngineMessage request_;
  std::string response_str_ = "Default server response";
  grpc::ServerContext context_;
  usi::UsiProtocol::AsyncService service_;
  std::unique_ptr<grpc::ServerCompletionQueue> cq_;
  std::unique_ptr<grpc::Server> server_;
  std::unique_ptr<grpc::ServerAsyncReaderWriter<usi::GuiMessage, usi::EngineMessage>> stream_;
  std::unique_ptr<std::thread> grpc_thread_;
  bool is_running_ = true;
};

int main() {
  AsyncUsiProtocolServer server;
  return 0;
}
