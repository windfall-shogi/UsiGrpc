#pragma warning(push)
#pragma warning(disable : 4251 4267 26451 26812 26495 6387 6385)
#include <google/protobuf/empty.pb.h>
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
#include <regex>

#pragma warning(push)
#pragma warning(disable : 4251 4267 26451 26812 26495 6387 6385)
#include "usi.grpc.pb.h"
#pragma warning(pop)

#include "position.h"
#include "thread.h"
#include "usi.h"

class UsiProtocolServer final : public usi::UsiProtocol::Service
{
public:
  grpc::Status RecieveMessage(grpc::ServerContext *context, const ::google::protobuf::Empty *empty,
                              grpc::ServerWriter<usi::GuiMessage> *writer) override {
    std::string cmd, token;
    std::string sfen_start;

    Position position;
    StateListPtr states = std::make_unique<StateList>(1);
    std::vector<Move> moves;

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
      } else if (token == "setoption") {
        const std::regex re("setoption (?:name )? (\\.+)(?: value )?(\\.+)");
        std::smatch m;
        if (std::regex_search(cmd, m, re)) {
          std::string name = m.str(1);
          std::string value = m.size() == 3 ? m.str(2) : "";

          const auto &ot = option_types_.at(name);
          usi::OptionValue o;
          o.set_index(ot.index);
          o.set_name(name);
          switch (ot.type) {
          case OptionType::CHECK: 
              o.set_flag(value == "true");
              break;
          case OptionType::SPIN: o.set_number(std::stoi(value)); break;
          case OptionType::BUTTON: break;
          default: o.set_str(value);
              break;
          }
          usi::GuiMessage msg;
          msg.set_allocated_option(&o);

          writer->Write(msg);
        }
      } else if (token == "position") {
        std::string sfen;
        iss >> token;
        if (token == "startpos") {
          sfen = SFEN_HIRATE;
          iss >> token; // "moves" ������
        } else {
          // "sfen" ����n�܂��Ă��A�n�܂�Ȃ��Ă��ǂ��悤�ɂ���
          if (token != "sfen") {
            sfen = token + " ";
          }
          while (iss >> token && token != "moves") {
            sfen += token + " ";
          }
        }
        // �J�n�ǖʂ�ۑ�
        sfen_start = sfen;

        position.set(sfen, &states->back(), Threads.main());
        moves.clear();
        Move m;
        while (iss >> token && (m = USI::to_move(position, token)) != MOVE_NONE) {
          states->emplace_back();
          if (m == MOVE_NULL) {
            position.do_null_move(states->back());
          } else {
            position.do_move(m, states->back());
          }
          moves.emplace_back(m);
        }
        // ��˂��牤���Ƃ��̌�ɃX���b�h�֌W�̏�������������

        // go�̎��̃f�[�^�ɋǖʂ̏����܂߂�
      } else if (token == "go") {
      } else if (token == "ponderhit") {
      } else if (token == "stop") {
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

  grpc::Status SendOption(grpc::ServerContext *context, const usi::EngineOption *engine_option,
                          ::google::protobuf::Empty *empty) override {
    const int size = engine_option->options_size();
    for (int i = 0; i < size; ++i) {
      const auto &option = engine_option->options(i);
      std::cout << "option name " << option.name() << " type ";
      switch (option.option_case()) {
      case usi::Option::kCheck: std::cout << "check default " << (option.check().value() ? "true" : "false"); break;
      case usi::Option::kSpin:
        std::cout << "spin default " << option.spin().value() << " min " << option.spin().min_value() << " max "
                  << option.spin().max_value();
        break;
      case usi::Option::kCombo:
        std::cout << "combo default " << (option.combo().value() == "" ? "<empty>" : option.combo().value());
        for (const auto &s : option.combo().candidates()) {
          std::cout << " var " << s;
        }
        break;
      case usi::Option::kButton: std::cout << "button"; break;
      case usi::Option::kStr:
        std::cout << "string default " << (option.str().value() == "" ? "<empty>" : option.str().value());
        break;
      case usi::Option::kFilename:
        std::cout << "filename default " << (option.filename().value() == "" ? "<empty>" : option.filename().value());
        break;
      default: break;
      }
    }
    std::cout << std::endl;

    return grpc::Status::OK;
  }

  grpc::Status SendReady(grpc::ServerContext *context, const ::google::protobuf::Empty *option,
                         ::google::protobuf::Empty *empty) override {
    std::cout << "isready" << std::endl;
    return grpc::Status::OK;
  }

  grpc::Status SendBestMove(grpc::ServerContext *context, const usi::BestMove *best_move,
                            ::google::protobuf::Empty *empty) override {
    return grpc::Status::OK;
  }

  grpc::Status SendInfo(grpc::ServerContext *context, const usi::Info *info,
                        ::google::protobuf::Empty *empty) override {
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

  enum class OptionType {
      CHECK, SPIN, COMBO, BUTTON, STRING, FILENAME
  };
  struct Option {
    int index;
    OptionType type;
  };
  std::unordered_map<std::string, Option> option_types_;
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
