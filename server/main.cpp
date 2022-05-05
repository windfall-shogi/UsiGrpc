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



void ParsePosition(std::istringstream &iss, Position &position, StateListPtr &states, std::vector<Move> &moves,
                   std::string &sfen_start) {
  std::string sfen, token;
  iss >> token;
  if (token == "startpos") {
    sfen = SFEN_HIRATE;
    iss >> token; // "moves" を消費
  } else {
    // "sfen" から始まっても、始まらなくても良いようにする
    if (token != "sfen") {
      sfen = token + " ";
    }
    while (iss >> token && token != "moves") {
      sfen += token + " ";
    }
  }
  // 開始局面を保存
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
  // やねうら王だとこの後にスレッド関係の処理が少しある
}

void Go(std::istringstream &iss, std::vector<Move> &moves, std::string &sfen_start,
        grpc::ServerWriter<usi::GuiMessage> *writer) {
  usi::Go go;
  usi::Position position;
  position.set_start(sfen_start);
  for (const auto &m : moves) {
    position.add_moves(static_cast<uint32_t>(m));
  }
  go.set_allocated_position(&position);
  usi::Time time;
  bool time_flag = false;

  std::string token;
  while (iss >> token) {
    if (token == "ponder") {
      go.set_ponder(true);
    } else if (token == "mate") {
      go.set_mate(true);
      iss >> token;
      if (token == "infinite") {
        go.set_infinite(true);
      } else {
        go.set_mate_ms(std::stoi(token));
      }
    } else if (token == "btime") {
      iss >> token;
      time.set_btime(std::stoi(token));
      time_flag = true;
    } else if (token == "wtime") {
      iss >> token;
      time.set_wtime(std::stoi(token));
      time_flag = true;
    } else if (token == "byoyomi") {
      iss >> token;
      time.set_byoyomi(std::stoi(token));
      time_flag = true;
    } else if (token == "binc") {
      iss >> token;
      time.set_binc(std::stoi(token));
      time_flag = true;
    } else if (token == "winc") {
      iss >> token;
      time.set_winc(std::stoi(token));
      time_flag = true;
    } else if (token == "infinite") {
      go.set_infinite(true);
    }
  }
  if (time_flag) {
    go.set_allocated_ms(&time);
  }

  usi::GuiMessage msg;
  msg.set_allocated_go(&go);
  writer->Write(msg);
}

void OutputCp(const usi::Info_ScoreCp &score) {
  std::cout << " cp " << score.cp();
  switch (score.bound()) {
  case usi::Info::ScoreCp::LOWER: std::cout << " lowerbound"; break;
  case usi::Info::ScoreCp::UPPER: std::cout << " upperbound"; break;
  default: break;
  }
}
void OutputMate(const usi::Info_ScoreMate &score) {
  std::cout << " mate ";
  switch (score.step_case()) {
  case usi::Info::ScoreMate::kMate: std::cout << score.mate(); break;
  case usi::Info::ScoreMate::kSign: std::cout << (score.sign() ? '+' : '-'); break;
  default: break;
  }
}
void OutputScore(const usi::Info *info) {
  std::cout << " score";
  switch (info->score_case()) {
  case usi::Info::ScoreCase::kCp: OutputCp(info->cp()); break;
  case usi::Info::ScoreCase::kMate: OutputMate(info->mate()); break;
  default: break;
  }
}

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
        SetOption(cmd, writer);
      } else if (token == "position") {
        ParsePosition(iss, position, states, moves, sfen_start);
        // goの時のデータに局面の情報を含める
      } else if (token == "go") {
        Go(iss, moves, sfen_start, writer);
      } else if (token == "ponderhit") {
        states->emplace_back();
        if (ponder_move_ == MOVE_NULL) {
          position.do_null_move(states->back());
        } else {
          position.do_move(ponder_move_, states->back());
        }
        moves.emplace_back(ponder_move_);

        usi::Go go;
        usi::Position position;
        position.set_start(sfen_start);
        for (const auto &m : moves) {
          position.add_moves(static_cast<uint32_t>(m));
        }
        go.set_allocated_position(&position);

        usi::GuiMessage msg;
        msg.set_allocated_go(&go);
        writer->Write(msg);
      } else if (token == "stop") {
          usi::GuiMessage msg;
          msg.set_sm(usi::SingleMessage::STOP);
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
      std::cout << std::endl;
    }

    return grpc::Status::OK;
  }

  grpc::Status SendUsiOk(grpc::ServerContext *context, const ::google::protobuf::Empty *option,
                         ::google::protobuf::Empty *empty) override {
    std::cout << "usiok" << std::endl;
    return grpc::Status::OK;
  }

  grpc::Status SendReadyOk(grpc::ServerContext *context, const ::google::protobuf::Empty *option,
                           ::google::protobuf::Empty *empty) override {
    std::cout << "isready" << std::endl;
    return grpc::Status::OK;
  }

  grpc::Status SendBestMove(grpc::ServerContext *context, const usi::BestMove *best_move,
                            ::google::protobuf::Empty *empty) override {
    std::cout << "bestmove " << USI::move(static_cast<Move>(best_move->move()));
    if (best_move->ponder() != 0) {
      std::cout << " ponder " << USI::move(static_cast<Move>(best_move->ponder()));
    }
    std::cout << std::endl;
    return grpc::Status::OK;
  }

  grpc::Status SendInfo(grpc::ServerContext *context, const usi::Info *info,
                        ::google::protobuf::Empty *empty) override {
    std::cout << "info";
    if (info->has_depth()) {
      std::cout << " depth " << info->depth();
    }
    if (info->has_seldepth()) {
      std::cout << " seldepth " << info->seldepth();
    }
    if (info->has_time()) {
      std::cout << " time " << info->time();
    }
    if (info->has_nodes()) {
      std::cout << " nodes " << info->nodes();
    }
    if (info->has_multipv()) {
      std::cout << " multipv " << info->multipv();
    }
    OutputScore(info);
    if (info->has_curmove()) {
      std::cout << " curmove " << USI::move(info->curmove());
    }
    if (info->has_hashfull()) {
      std::cout << " hashfull " << info->hashfull();
    }
    if (info->has_nps()) {
      std::cout << " nps " << info->nps();
    }
    switch (info->variable_case()) {
    case usi::Info::VariableCase::kPv:
      std::cout << " pv";
      for (const auto m : info->pv().pv()) {
        std::cout << ' ' << USI::move(m);
      }
      break;
    case usi::Info::VariableCase::kStr: std::cout << " string " << info->str(); break;
    default: break;
    }
    std::cout << std::endl;

    return grpc::Status::OK;
  }

  grpc::Status SendCheckmate(grpc::ServerContext *context, const usi::Checkmate *checkmate,
                             ::google::protobuf::Empty *empty) override {
    std::cout << "checkmate";
    switch (checkmate->status()) {
    case usi::Checkmate_Status_NOMATE: std::cout << " nomate"; break;
    case usi::Checkmate_Status_NOTIMPLEMENTED: std::cout << " notimplemented"; break;
    case usi::Checkmate_Status_TIMEOUT: std::cout << " timeout"; break;
    default:
      for (const auto m : checkmate->moves()) {
        std::cout << ' ' << USI::move(m);
      }
      break;
    }
    std::cout << std::endl;

    return grpc::Status::OK;
  }

  void Shutdown(std::unique_ptr<grpc::Server> &server) {
    std::unique_lock<std::mutex> lock(mutex_);
    cv_.wait(lock, [this] { return exit_flag_; });
    server->Shutdown();
  }

private:
  void SetOption(const std::string &cmd, grpc::ServerWriter<usi::GuiMessage> *writer) {
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
      case OptionType::CHECK: o.set_flag(value == "true"); break;
      case OptionType::SPIN: o.set_number(std::stoi(value)); break;
      case OptionType::BUTTON: break;
      default: o.set_str(value); break;
      }
      usi::GuiMessage msg;
      msg.set_allocated_option(&o);

      writer->Write(msg);
    }
  }

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

  Move ponder_move_;
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
