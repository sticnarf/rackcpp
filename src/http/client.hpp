#ifndef HANE_CLIENT_HPP
#define HANE_CLIENT_HPP

#include "./parser/parser.hpp"
#include "./http.hpp"

class Client {
private:
    HttpServer *server;
//    uv_idle_t idler;
    uv_tcp_t *tcp;
    uv_write_t write;
    uv_buf_t buf;
    Parser parser;
public:
    explicit Client(HttpServer *server);

    void pushBuf(const char *buf, size_t len);

    void processRequest();

    ~Client();

    friend class HttpServer;

    friend void onNewConnection(uv_stream_t *serverTcp, int status);
    friend void readCallback(uv_stream_t *clientTcp, ssize_t nread, const uv_buf_t *buf);
};

#endif