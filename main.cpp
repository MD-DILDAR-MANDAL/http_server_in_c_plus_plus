// -----------------------------------------------------------------------------
// Headers and Namespace Aliases
// -----------------------------------------------------------------------------
#include <iostream>                // For console input/output (std::cout)
#include <string>                  // For std::string and string manipulation
#include <map>                     // Although not explicitly used in main logic, it's often useful
#include <vector>                  // Although not explicitly used in main logic, it's often useful
#include <sstream>                 // For std::stringstream used to build the headers response
#include <cpprest/http_listener.h> // Core header for the HTTP server
#include <cpprest/json.h>          // Header for JSON handling (included but not used here)

// Aliases to make the code cleaner and avoid verbose namespace prefixes
using namespace web;                          // Core C++ REST SDK namespace
using namespace http;                         // HTTP-specific types (e.g., status_codes)
using namespace utility;                      // Utility types (e.g., string conversions, used internally)
using namespace http::experimental::listener; // Namespace for the http_listener class

// -----------------------------------------------------------------------------
// Request Handlers
// -----------------------------------------------------------------------------

/**
 * @brief Handles GET requests to the /hello endpoint.
 * * Simply returns a 200 OK status with a "hello\n" body.
 * * @param request The incoming HTTP request object.
 */
void handle_hello(http_request request)
{
    // U() macro converts a standard string literal to a platform-specific wide string
    // (std::wstring or std::string, depending on OS/build settings), which Casablanca uses.
    request.reply(status_codes::OK, U("hello\n"));
}

/**
 * @brief Handles GET requests to the /headers endpoint.
 * * Iterates through all headers in the incoming request and returns them in the response body.
 * * @param request The incoming HTTP request object.
 */
void handle_headers(http_request request)
{
    std::stringstream response;

    // Iterate over the request headers (which is a map<string_t, string_t>)
    for (const auto &header : request.headers())
    {
        // Append the header name, a colon, the value, and a newline to the stream
        response << header.first << ": " << header.second << "\n";
    }

    // Reply with a 200 OK status and the built string of headers as the body
    request.reply(status_codes::OK, response.str());
}

// -----------------------------------------------------------------------------
// Main Function and Server Setup
// -----------------------------------------------------------------------------

int main()
{
    // 1. HTTP Listener Initialization
    // Creates an http_listener object bound to the URI "http://localhost:8090".
    // The listener is not active yet; it's just configured.
    http_listener listener("http://localhost:8090");

    // 2. Request Routing Setup
    // Configures the listener to handle HTTP GET requests.
    // The second argument is a lambda function that serves as the **request router**.
    // This lambda is executed by an internal worker thread for every incoming GET request.
    listener.support(methods::GET, [](http_request request)
                     {
        // Get the requested path from the URI (e.g., "/hello" or "/headers")
        auto path = request.relative_uri().path();
        
        // --- Routing Logic (Simple path-based routing) ---
        if (path == "/hello") {
            handle_hello(request);
        } else if (path == "/headers") {
            handle_headers(request);
        } else {
            // Default handler for any path not explicitly handled
            request.reply(status_codes::NotFound, U("Not found"));
        } });

    // 3. Server Startup and Main Loop
    try
    {
        // listener.open() starts the asynchronous process of binding to the port.
        // .wait() blocks the main thread until the listener is fully initialized and ready.
        // This is a blocking call critical for server startup.
        listener.open().wait();

        std::cout << "Listening on http://localhost:8090" << std::endl;

        // The main thread enters an **infinite loop**. This keeps the main application
        // process alive and prevents the program from exiting. The actual handling
        // of requests happens asynchronously on the library's internal worker threads.
        while (true)
            ;
    }
    catch (const std::exception &e)
    {
        // Catches any exceptions that occur during initialization (e.g., port already in use)
        std::cout << e.what() << std::endl;
    }

    // Note: The listener is never explicitly closed in this structure because
    // the `while(true)` loop makes the program run indefinitely.
    return 0;
}