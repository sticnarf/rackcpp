#include <iostream>
#include "client.hpp"
#include "errors.hpp"
#include "../utils/logger.hpp"

Client::Client(HttpServer *server)
        : server(server), parser(this), queued(0) {
    tcp = new uv_tcp_t;
    tcp->data = this;
}

struct PushBufferHandler {
    Client *client;
    const char *buf;
    size_t len;

    PushBufferHandler(Client *client, const char *buf, size_t len) : client(client), buf(buf), len(len) {}
};

void Client::pushBuffer(uv_work_t *work) {
    auto handler = static_cast<PushBufferHandler *>(work->data);
    auto client = handler->client;
    client->parser.pushBuf(handler->buf, handler->len);
}

void Client::pushBufferCallback(uv_work_t *work, int status) {
    if (status < 0)
        Logger::getInstance().error("PushBuffer error: {}", uv_strerror(status));

    auto handler = static_cast<PushBufferHandler *>(work->data);
    delete[] handler->buf;
    delete handler;
}

void Client::pushBuf(const char *buf, size_t len) {
    parser.pushBuf(buf, len);
//    auto *pushWork = new uv_work_t;
//    pushWork->data = new PushBufferHandler(this, buf, len);
//    uv_queue_work(uv_default_loop(), pushWork, pushBuffer, pushBufferCallback);
}

Client::~Client() {
    delete tcp;
}

static std::shared_ptr<Response> buildErrorResponse(const HttpError &e) {
    // TODO Now use HTTP/1.1 arbitrarily
    auto resp = std::make_shared<Response>(HttpVersion::HTTP_1_1);

    resp->setStatusCode(e.getCode());
    resp->body = e.getReason();

    return resp;
}

void Client::processRequest() {
    if (currRequest && currResponse && currMiddleware) {
        auto polyResponse = std::dynamic_pointer_cast<Response>(currResponse);
        currMiddleware = currMiddleware->call(currRequest, polyResponse);
        server->writeChunks(AsyncChunkedResponseHandler(currRequest, currResponse),
                            reinterpret_cast<uv_stream_t *>(tcp));

        if (currResponse->finished) {
            currMiddleware = nullptr;
            currRequest = nullptr;
            currResponse = nullptr;
            return;
        }
    }

    if (!parser.hasCompleteRequest())
        return;

    RequestPtr req = parser.yieldRequest();
    if (req->isBad()) {
        auto badRequest = std::dynamic_pointer_cast<BadRequest>(req);
        auto e = badRequest->getError();
        Logger::getInstance().error("Error code {}: {}", static_cast<int>(e.getCode()), e.getReason());
        auto errorResp = buildErrorResponse(e);
        server->writeResponse(reinterpret_cast<uv_stream_t *>(tcp), errorResp);
        return;
    }

    try {
        auto resp = std::make_shared<Response>(req->getHttpVersion());
        currMiddleware = server->middleware->call(req, resp);
        server->writeResponse(reinterpret_cast<uv_stream_t *>(tcp), resp);

        if (resp->isChunked()) {
            currRequest = req;
            currResponse = std::dynamic_pointer_cast<ChunkedResponse>(resp);
        }
    } catch (HttpError &e) {
        auto errorResp = buildErrorResponse(e);
        server->writeResponse(reinterpret_cast<uv_stream_t *>(tcp), errorResp);
    }
}

void Client::startProcessing(uv_work_t *work) {
    auto client = static_cast<Client *>(work->data);
    while (!client->closed) {
        std::unique_lock<std::mutex> awaitLock(client->awaitMutex);
        client->awaitCv.wait(awaitLock,
                             [&] {
                                 bool hasRequest = ((client->currRequest &&
                                                     client->currResponse &&
                                                     client->currMiddleware) ||
                                                    client->parser.hasCompleteRequest());
                                 return (client->queued < 8 && hasRequest)
                                        || client->closed;
                             });

        client->processRequest();
    }
    client->queued--;
}

void Client::startProcessingCallback(uv_work_t *work, int status) {
    if (status < 0)
        Logger::getInstance().error("StartProcessing error: {}", uv_strerror(status));

    delete work;
}

void Parser::process() {
    try {
        currentParser = currentParser->process();
    } catch (HttpError &e) {
        auto badRequest = std::make_shared<BadRequest>(e);
        completeRequests.push(badRequest);
        client->awaitCv.notify_one();
        buffer = std::make_shared<Buffer>();
        currentParser = std::make_shared<StartLineParser>(std::make_shared<Request>(), buffer);
    }

    if (currentParser->isFinished()) {
        completeRequests.push(currentParser->getRequest());
        client->awaitCv.notify_one();
        currentParser = std::make_shared<StartLineParser>(std::make_shared<Request>(), buffer);
    }
}

void Client::closeConnection() {
    closed = true;
    awaitCv.notify_all();

//    if (queued < 0)
//        uv_close(reinterpret_cast<uv_handle_t *>(this->tcp), nullptr);
    auto work = new uv_work_t;
    work->data = this;
    uv_queue_work(uv_default_loop(), work, realCloseConnection, realCloseConnectionCallback);
}

void Client::realCloseConnection(uv_work_t *work) {
    auto client = static_cast<Client *>(work->data);
    if (client->queued < 0) {
        uv_close(reinterpret_cast<uv_handle_t *>(client->tcp), closeCallback);
    } else {
        auto work2 = new uv_work_t;
        work2->data = client;
        uv_queue_work(uv_default_loop(), work2, realCloseConnection, realCloseConnectionCallback);
    }
}

void Client::realCloseConnectionCallback(uv_work_t *work, int status) {
    if (status < 0) {
        Logger::getInstance().error("realCloseConnection error: {}", uv_strerror(status));
        // error!
    }

    delete work;
}

void Client::closeCallback(uv_handle_t *handle) {
    delete static_cast<Client *>(handle->data);
}
