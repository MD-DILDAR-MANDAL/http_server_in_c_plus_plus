/*g++ -std=c++17 main.cpp -o server*/
#include <boost/asio.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <thread>
#include <string>
#include <memory>
#include <iostream>

using tcp = boost::asio::ip::tcp;
namespace http = boost::beast::http;

// ---------------------------
// ROUTER
// ---------------------------
std::string handle_request(const http::request<http::string_body> &req)
{
    std::string target = std::string(req.target());

    if (target == "/hello")
        return "hello\n";

    if (target == "/headers")
    {
        std::string headers;
        for (auto &h : req.base())
        {
            headers += std::string(h.name_string()) + ": " + std::string(h.value()) + "\n";
        }
        return headers;
    }
    return "Not Found";
}

// ---------------------------
// PER-SESSION CLASS
// ---------------------------
class session : public std::enable_shared_from_this<session>
{
    tcp::socket socket_;
    boost::beast::flat_buffer buffer_;
    http::request<http::string_body> req_;
    http::response<http::string_body> res_;

public:
    explicit session(tcp::socket socket)
        : socket_(std::move(socket)) {}

    void run()
    {
        do_read();
    }

private:
    void do_read()
    {
        auto self = shared_from_this();

        http::async_read(socket_, buffer_, req_,
                         [self](boost::beast::error_code ec, std::size_t)
                         {
                             if (!ec)
                                 self->do_write();
                         });
    }

    void do_write()
    {
        auto self = shared_from_this();

        std::string body = handle_request(req_);

        res_.version(req_.version());
        res_.keep_alive(false);

        if (body == "Not Found")
        {
            res_.result(http::status::not_found);
        }
        else
        {
            res_.result(http::status::ok);
        }

        res_.set(http::field::server, "Boost.Beast Server");
        res_.body() = body;
        res_.prepare_payload();

        http::async_write(socket_, res_,
                          [self](boost::beast::error_code ec, std::size_t)
                          {
                              self->socket_.shutdown(tcp::socket::shutdown_send, ec);
                          });
    }
};

//--------------------
// LISTENER CLASS
//--------------------
class listener : public std::enable_shared_from_this<listener>
{
    boost::asio::io_context &ioc_; // keep reference to original io_context
    tcp::acceptor acceptor_;
    tcp::socket socket_;

public:
    listener(boost::asio::io_context &ioc, tcp::endpoint endpoint)
        : ioc_(ioc), acceptor_(ioc), socket_(ioc)
    {
        boost::beast::error_code ec;
        acceptor_.open(endpoint.protocol(), ec);
        if (ec)
            throw std::runtime_error(ec.message());

        acceptor_.set_option(boost::asio::socket_base::reuse_address(true), ec);
        if (ec)
            throw std::runtime_error(ec.message());

        acceptor_.bind(endpoint, ec);
        if (ec)
            throw std::runtime_error(ec.message());

        acceptor_.listen(boost::asio::socket_base::max_listen_connections, ec);
        if (ec)
            throw std::runtime_error(ec.message());
    }
    void run()
    {
        do_accept();
    }

private:
    void do_accept()
    {
        // used to create a std::shared_ptr to the current object from within a
        // class that publicly inherits from std::enable_shared_from_this.
        // This is necessary when the object's lifetime must be extended beyond
        // the current scope, particularly in asynchronous operations where a
        // lambda or callback may outlive the original scope that created it.
        auto self = shared_from_this();

        self->socket_ = tcp::socket(ioc_);

        acceptor_.async_accept(self->socket_, [self](boost::beast::error_code ec)
                               {
            if(!ec){
                std::make_shared<session>(std::move(self->socket_))->run();
            }else {
                std::cerr << "accept error: " << ec.message() << "\n";
            }
            self->do_accept(); });
    }
};

int main()
{
    const int PORT = 8090;
    const int THREADS = std::thread::hardware_concurrency();

    try
    {
        // io_context object with a specified number of threads
        boost::asio::io_context ioc;

        // creating a TCP endpoint for IPv4 using the "any" IP address on a specified port
        auto endp = tcp::endpoint(tcp::v4(), PORT);

        // create a shared instance of a listener class, which is
        // typically designed to handle incoming network connections using Boost.
        // Beast and boost::asio. The listener class is usually derived from
        // std::enable_shared_from_this to allow safe use of shared_from_this()
        // within asynchronous operations. The constructor initializes the
        // acceptor_ with the provided io_context (ioc) and
        // tcp::endpoint (endpoint), opens the acceptor, sets the reuse_address
        // option, binds it to the endpoint, and starts listening for incoming
        // connections. The run() method initiates the asynchronous acceptance of
        // new connections by calling acceptor_.async_accept, which triggers the
        // on_accept callback when a connection arrives.
        std::make_shared<listener>(ioc, endp)
            ->run();

        // A common pattern used when implementing a thread pool in C++ to
        // pre-allocate space for a specified number of threads.
        // This approach helps avoid repeated memory allocations and
        // reallocations as threads are added to the vector, improving
        //  performance during the pool's initialization phase. The reserve
        //  function ensures that the underlying storage of the vector has enough
        //  capacity to hold the requested number of threads, which is particularly
        //  useful when the number of threads is known in advance, such as when
        //  setting up a fixed-size thread pool
        std::vector<std::thread> pool;
        pool.reserve(THREADS);

        std::cout << "Server running on http://localhost:" << PORT << "\n";
        std::cout << "Threads: " << THREADS << "\n";

        // In practice, after reserving space, threads are typically created using emplace_back to construct them in place within the vector, passing a lambda or function object that defines the thread's behavior, such as polling a work queue for tasks.
        for (int i = 0; i < THREADS; i++)
            pool.emplace_back([&ioc]
                              { ioc.run(); });

        // waiting for all worker threads to finish.
        // blocks the current thread of execution until the thread it is called
        // on has finished its task.
        // This ensures that the calling thread waits for the target thread to
        // complete its execution before proceeding to the next statement in the
        // code.
        // It is used to synchronize threads, allowing the main thread or another
        //  thread to wait for the completion of a spawned thread
        // before continuing, which is essential for ensuring that all work is
        // completed before the program exits.
        // The join() function can only be called once on a thread; attempting
        // to call it a second time results in undefined behavior, typically
        // terminating the program
        for (auto &t : pool)
        {
            if (t.joinable())
            {
                t.join();
            }
        }
    }
    catch (std::exception &e)
    {
        std::cerr << "Fatal Error: " << e.what() << "\n";
    }
}