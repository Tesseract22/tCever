#pragma once
#include "HTTPResponse.hpp"
#include "HTTPUnit.hpp"
#include "MultiThreadQueue.hpp"
#include "Task.hpp"

#include <condition_variable>
#include <coroutine>
#include <cstddef>
#include <fstream>
#include <functional>
#include <iostream>
#include <mutex>
#include <netinet/in.h>
#include <ostream>
#include <string>
#include <sys/epoll.h>
#include <thread>
#include <variant>
#include <vector>
// TCP server implementation
#define DEFAULT_PORT 80
class SocketException : std::exception {

  public:
    explicit SocketException();
    SocketException(const char *msg) : msg_(msg) {}
    SocketException(const std::string &msg) : msg_(msg) {}
    virtual ~SocketException() noexcept {}
    virtual const char *what() const noexcept { return msg_.data(); }

  protected:
    std::string msg_;
};
class TCPServer {
  public:
    TCPServer(HTTPUnit &&http, size_t listen_threads = 2,
              size_t parse_threads = 2, std::ostream &log_io = std::cout,
              std::ostream &err_io = std::cerr);
    ~TCPServer();
    void serverStart();
    void serverStop();

    void logRequest(HTTP::HTTPRequest *request, HTTP::HTTPResponse *response);
    void static sigintHandler(int dummy);
    typedef int FileFd;
    struct Response {
        std::string str = "";
        FileFd fd = -1;
    };

  private:
    enum Status {
        Open = 0b1,
        PendingRead = 0b10,
        PendingClose = 0b100,
        Closed = 0,
    };
    class EPoll {
      public:
        EPoll(std::mutex &m_, int master_socket_fd, std::vector<int> &sockt_vec,
              std::vector<std::mutex *> &mutex_vec,
              const std::function<Task<Response>(std::string &)> &callback);
        EPoll(EPoll &&other) = default;
        EPoll(const EPoll &X) = delete;

        Task<void> wait(int dummy_fd);
        void stop();
        int epoll_fd_;

      private:
        void addSocket(int socket_fd_);
        void delSocket(int socket_fd_);
        void modSocket(int socket_fd_, int act);
        int getSocket(int socket_fd_);

        void lockSocket(int socket_fd);
        void unlockSocket(int socket_fd);

        std::mutex &m_;
        int master_socket_fd_;
        std::vector<std::mutex *> &mutex_vec_;
        std::vector<int> &socket_vec_;
        std::function<Task<Response>(std::string &)> handleReqeust_;
        epoll_event temp_event_;
        bool running_ = false;
        // The socket file descriptor is directly used as indexes
        // The value in the vector represent the status of the socket
    };
    // std::function<Task<std::string>(TCPServer *, const std::string &)>
    //     protocol_;
    static std::vector<TCPServer *> servers_;
    int stop_pipe_[2];

    void waitParse(size_t id);
    void waitListen(size_t id);

    Task<TCPServer::Response> handleRequest(std::string &str);
    bool running_;

    std::ostream &log_io_;
    std::ostream &err_io_;

    /**
      @new
    */
    int socket_fd_;
    sockaddr_in addr_;
    int port_;

    // An array of threads listening to requests, we are not responsible for
    // allocating and deallocating this
    std::vector<std::thread> listen_threads_;
    std::vector<std::thread> parse_threads_;

    std::vector<int> sockets_;
    std::vector<std::mutex *> mutexes_;
    std::mutex epoll_m_;
    // Any array of EPoll object. Each one correspond to one listen_threads_;
    std::vector<EPoll> epolls_;

    HTTPUnit http_;

    char resource_path_[100];
};
