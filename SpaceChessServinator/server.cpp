#include <string>
#include <iostream>
#include <csignal>
#include <unordered_map>
#include <iostream>
#include <fstream>
#include <queue>
#include <vector>
#include <mutex>
#include <random>
#include <chrono>
#include <thread>

#include <grpcpp/grpcpp.h>
#include "chesscom.grpc.pb.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;

//using chesscom::ChessCom;
//using chesscom::MathRequest;
//using chesscom::MathReply;
//using chesscom::ChessCom;


struct MatchStruct{
    bool newUpdate = false;
    bool askingForDraw = false;
    chesscom::MatchEvent matchEvent = chesscom::MatchEvent::Non;
    std::string whitePlayer;
    std::string blackPlayer;
    std::string matchToken;
    std::vector<std::shared_ptr<chesscom::Move>> moves;
    std::string& getWhitePlayer(){return whitePlayer;}
    std::string& getBlackPlayer(){return blackPlayer;}
    std::string& getMatchToken(){return matchToken;}
    
};

static const int MAX_SLEEP_MS = 1000;
static const int SHUTDOWN_WAIT_MS = MAX_SLEEP_MS*2.5;

std::atomic<int> tokenCounter(1);
std::atomic<bool> orderedShutdown(false);
std::sig_atomic_t signaled = 0;
std::mutex lock;
std::unique_ptr<Server> server;
static std::unordered_map<std::string, std::string> userTokens;
static std::ofstream logFile;
std::queue<std::string> lookingForMatchQueue;
std::map<std::string, std::string> foundMatchReply;
std::map<std::string, std::shared_ptr<MatchStruct>> matches;



void SigintHandler (int param)
{
    signaled = 1;
    std::cout << "Signal interupt. Fuck yeah\n";
    logFile << "signal interut shutdown\n";

    server->Shutdown();
}

std::string createMatch(std::string& player1Token, std::string& player2Token){
    std::string matchToken = "match"+ std::to_string(tokenCounter++);
    std::shared_ptr<MatchStruct> match = std::make_shared<MatchStruct>();
    match->matchToken = matchToken;
    std::random_device rd;
    std::mt19937 mt(rd());
    std::uniform_int_distribution<int> dist(0, 1);
    if(dist(mt) == 1)
    {
        match->whitePlayer = player1Token;
        match->blackPlayer = player2Token;
    }
    else
    {
        match->whitePlayer = player2Token;
        match->blackPlayer = player1Token;
    }
    matches[matchToken] = match;
    std::cout << "  checing match" << std::endl << std::flush;
    auto matPtr = matches[matchToken];
    std::cout << "  white player " <<  matPtr->whitePlayer << std::endl << std::flush;
    return matchToken;
}

class ChessComImplementation final : public chesscom::ChessCom::Service {
    Status Login(ServerContext* context, const chesscom::LoginForm* request, chesscom::LoginResult* response) override 
    {
        response->set_usertoken("usertoken"+std::to_string(tokenCounter++));
        response->set_successfulllogin(true);
        std::cout << "User " << request->username() << " " << response->usertoken() << " logged in" << std::endl << std::flush;
        return Status::OK;
    }

    Status LookForMatch(ServerContext* context, const chesscom::UserIdentity* request, chesscom::LookForMatchResult* response) override 
    {
        bool loop = true;
        std::string matchToken = "";
        std::string userToken = request->usertoken();
        std::cout << userToken << " looking for match"<< std::endl << std::flush;
        {
            std::unique_lock<std::mutex> scopeLock (lock);
            if(!lookingForMatchQueue.empty()){
                std::string opponent = lookingForMatchQueue.front();
                lookingForMatchQueue.pop();
                matchToken = createMatch(userToken, opponent);
                foundMatchReply[opponent] = matchToken;
                loop = false;
            }
            else
            {
                std::cout << userToken << " entering queue"<< std::endl << std::flush;
                lookingForMatchQueue.emplace(userToken);
            }
        }
        while(loop){
            std::this_thread::sleep_for(std::chrono::milliseconds(MAX_SLEEP_MS));
            {
                std::unique_lock<std::mutex> scopeLock (lock);
                std::cout << userToken << " checking foundMatchReply " << std::flush << std::to_string(foundMatchReply.count(userToken)) << std::endl << std::flush;
                if(foundMatchReply.count(userToken) > 0){
                    std::cout << userToken << " found match reply MT: " << foundMatchReply[userToken]<< std::endl << std::flush;
                    matchToken = foundMatchReply[userToken];
                    foundMatchReply.erase(userToken);
                    loop = false;
                }
            }
        }
        {
            std::unique_lock<std::mutex> scopeLock (lock);
            std::cout << userToken << " found match " <<  matchToken << std::endl << std::flush;
            std::cout << "  checing match" << std::endl << std::flush;
            auto matPtr = matches[matchToken];
            std::cout << "  white player " <<  matPtr->whitePlayer << std::endl << std::flush;
        }
        response->set_succes(true);
        response->set_matchtoken(matchToken);
        response->set_iswhiteplayer(matches[matchToken]->whitePlayer == userToken);
        return Status::OK;
    }

    Status Match(ServerContext* context, grpc::ServerReaderWriter< chesscom::MoveResult, chesscom::MovePacket>* stream) override 
    {
        chesscom::MovePacket movePkt;
        chesscom::MoveResult moveResultPkt;
        stream->Read(&movePkt);
        if(movePkt.doingmove()){
            std::cout << "Error: doing move as first " << movePkt.usertoken() << ". Terminating!" << std::endl << std::flush;
            return Status::OK; 
        }
        std::cout << "Opening matchstream for " << movePkt.matchtoken() << " " <<  movePkt.usertoken() << std::endl << std::flush;
        std::string userToken = movePkt.usertoken();
        std::shared_ptr<MatchStruct> matchPtr = matches[movePkt.matchtoken()];
        bool playerWhite = matchPtr->whitePlayer == userToken;
        bool loop = true;
        while (loop)
        {
            bool isPlayersCurrentTurn;
            {
                std::unique_lock<std::mutex> scopeLock (lock);
                isPlayersCurrentTurn = matchPtr->moves.size()%2 == (playerWhite?0:1); 
            }
            std::cout  << movePkt.matchtoken() << " " <<  movePkt.usertoken()<< " IsCurrentTurn "<< std::to_string(isPlayersCurrentTurn) << std::endl << std::flush;
            if(isPlayersCurrentTurn)
            {
                bool isUpdate;
                {
                    std::unique_lock<std::mutex> scopeLock (lock);
                    isUpdate = matchPtr->newUpdate;
                    if(isUpdate){
                        matchPtr->newUpdate = false;
                        moveResultPkt.set_movehappned(true);
                        moveResultPkt.set_opponentaskingfordraw(false);
                        moveResultPkt.set_allocated_move(matchPtr->moves.back().get());
                        stream->Write(moveResultPkt);
                        moveResultPkt.release_move();
                    }
                    std::cout  << movePkt.matchtoken() << " " <<  movePkt.usertoken()<< " SentMoveResult "<< std::to_string(isUpdate) << std::endl << std::flush;
                }
                if(!stream->Read(&movePkt))throw "premeture end of steam";
                if(movePkt.askingfordraw())throw "draw not implemented";
                std::shared_ptr<chesscom::Move> movePtr = std::make_shared<chesscom::Move>();
                movePtr->set_from(movePkt.move().from());
                movePtr->set_to(movePkt.move().to());
                std::cout  << movePkt.matchtoken() << " " <<  movePkt.usertoken()<< " Got move " << movePkt.move().from() << " " << movePkt.move().to() << std::endl << std::flush;
                {
                    std::unique_lock<std::mutex> scopeLock (lock);
                    matchPtr->moves.push_back(movePtr);
                    matchPtr->newUpdate = true;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(MAX_SLEEP_MS));
            }
            else
            {

                std::this_thread::sleep_for(std::chrono::milliseconds(MAX_SLEEP_MS));
            }
        }
        std::cout  << movePkt.matchtoken() << " " <<  movePkt.usertoken()<< " Matchstream ended." << std::endl << std::flush;
        return Status::OK;
    }



};

void Run() {
    std::string address("0.0.0.0:43326");
    ChessComImplementation service;

    ServerBuilder builder;

    builder.AddListeningPort(address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    server = builder.BuildAndStart();
    std::cout << "Chess Server listening on port: " << address << std::endl;

    server->Wait();
    std::cout << "After wait happened";
    
}

int main(int argc, char** argv) {
    //void (*prev_handler)(int);
    //prev_handler = signal(SIGINT, SigintHandler);
    logFile.open ("server.log", std::ios::out | std::ios::trunc);
    logFile << "Writing this to a file.\n";
    Run();
    logFile << "Exiting\n";
    logFile.close();
    return 0;
}
