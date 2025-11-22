#include <iostream>
#include <string>
#include <map>
#include <vector>
#include <cpprest/http_listener.h>
#include <cpprest/json.h>

using namespace web;
using namespace http;
using namespace utility;
using namespace http::experimental::listener;

void handle_hello(http_request request) {
    request.reply(status_codes::OK, U("hello\n"));
}

void handle_headers(http_request request) {
    std::stringstream response;
    for (const auto& header : request.headers()) {
        response << header.first << ": " << header.second << "\n";
    }
    request.reply(status_codes::OK, response.str());
}

int main() {
    http_listener listener("http://localhost:8090");

    listener.support(methods::GET, [](http_request request) {
        auto path = request.relative_uri().path();
        if (path == "/hello") {
            handle_hello(request);
        } else if (path == "/headers") {
            handle_headers(request);
        } else {
            request.reply(status_codes::NotFound, U("Not found"));
        }
    });

    try {
        listener.open().wait();
        std::cout << "Listening on http://localhost:8090" << std::endl;
        while (true);
    } catch (const std::exception & e) {
        std::cout << e.what() << std::endl;
    }

    return 0;
}