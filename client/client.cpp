#include <cstdlib>
#include <deque>
#include <iostream>
#include <thread>
#include "../asio/asio/include/asio.hpp"
#include "../common/message.hpp"

using asio::ip::tcp;

using std::cin;
using std::cout;

typedef std::deque<message> message_queue;

class TPort
{
public:
    TPort(const std::string& string) : string_(string) {}

    std::string const& ToString() const { return string_; }
private:
    std::string string_;
};

class client
{
public:
  client()
    : resolver_(io_context_),
      socket_(io_context_)
  {
  }

  std::size_t run()
  {
      return io_context_.run();
  }

  void do_connect(const TPort port)
  {
      auto endpoints = resolver_.resolve("127.0.0.1", port.ToString());
      asio::async_connect(socket_, endpoints,
          [this](std::error_code ec, tcp::endpoint)
          {
              if (!ec)
              {
                  do_read_header();
              }
          });
  }

  void write(const message& msg)
  {
    asio::post(io_context_,
        [this, msg]()
        {
          bool write_in_progress = !write_msgs_.empty();
          write_msgs_.push_back(msg);
          if (!write_in_progress)
          {
            do_write();
          }
        });
  }

  void close()
  {
    asio::post(io_context_, [this]() { socket_.close(); });
  }

private:
  void do_read_header()
  {
    asio::async_read(socket_,
        asio::buffer(read_msg_.data(), message::header_length),
        [this](std::error_code ec, std::size_t /*length*/)
        {
          if (!ec && read_msg_.decode_header())
          {
            do_read_body();
          }
          else
          {
            socket_.close();
          }
        });
  }

  void do_read_body()
  {
    asio::async_read(socket_,
        asio::buffer(read_msg_.body(), read_msg_.body_length()),
        [this](std::error_code ec, std::size_t /*length*/)
        {
          if (!ec)
          {
            std::cout.write(read_msg_.body(), read_msg_.body_length());
            std::cout << "\n";
            do_read_header();
          }
          else
          {
            socket_.close();
          }
        });
  }

  void do_write()
  {
    asio::async_write(socket_,
        asio::buffer(write_msgs_.front().data(),
          write_msgs_.front().length()),
        [this](std::error_code ec, std::size_t /*length*/)
        {
          if (!ec)
          {
            write_msgs_.pop_front();
            if (!write_msgs_.empty())
            {
              do_write();
            }
          }
          else
          {
            socket_.close();
          }
        });
  }

private:
  asio::io_context io_context_;
  tcp::resolver resolver_;
  tcp::socket socket_;
  message read_msg_;
  message_queue write_msgs_;
};

class TUserCommand
{
public:
    virtual ~TUserCommand() {}
};

class TUserCommandConnect : public TUserCommand
{
public:
    TUserCommandConnect(TPort const& port) : port_{ port } {}
    TPort const& GetPort() const { return port_; }
private:
    TPort port_;
};

class TUserCommandDisconnect : public TUserCommand
{
};

class TUserCommandPublish : public TUserCommand
{
};

TUserCommand* StringToUserCommand(std::string const& string)
{
    if (string.find("co") == 0)
        return new TUserCommandConnect(TPort{ "" });
    else if (string.find("di") == 0)
        return new TUserCommandDisconnect;
    else
        return new TUserCommandPublish;
}

int main(int argc, char* argv[])
{
  try
  {
    client c;

    std::thread t([&c](){ while (true) c.run(); }); // we need a loop because io_context.run immediately returns when it's out of work

    while (true)
    {
        std::string inputString;
        std::getline(cin, inputString);
        std::unique_ptr<TUserCommand> userCommand{ StringToUserCommand(inputString) };
        if (typeid(*userCommand) == typeid(TUserCommandConnect))
        {
            c.do_connect(TPort("111"));
        }
        else if (typeid(*userCommand) == typeid(TUserCommandDisconnect))
        {
            c.close();
        }
        else
        {
            message msg;
            msg.body_length(inputString.size());
            std::memcpy(msg.body(), inputString.c_str(), msg.body_length());
            msg.encode_header();
            c.write(msg);
        }
    }

    c.close();
    t.join();
  }
  catch (std::exception& e)
  {
    std::cerr << "Exception: " << e.what() << "\n";
  }

  return 0;
}
