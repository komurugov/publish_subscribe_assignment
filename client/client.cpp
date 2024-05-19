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

class TUserCommandSubscribe : public TUserCommand
{
public:
    TUserCommandSubscribe(char const* topicPtr, size_t const topicLen)
        : topic_(topicPtr, topicLen)
    {
    }
    std::string const& Topic() { return topic_; }
private:
    std::string topic_;
};

class TUserCommandPublish : public TUserCommand
{
public:
    TUserCommandPublish(char const* topicPtr, size_t const topicLen, char const* dataPtr, size_t dataLen)
        : topic_(topicPtr, topicLen),
          data_(dataPtr, dataLen)
    {
    }
    std::string const& Topic() { return topic_; }
    std::string const& Data() { return data_; }
private:
    std::string topic_;
    std::string data_;
};

TUserCommand* StringToUserCommand(std::string const& string)
{
    if (string.find("co") == 0)
        return new TUserCommandConnect(TPort{ "" });
    else if (string.find("di") == 0)
        return new TUserCommandDisconnect;
    else if (string.find("su") == 0)
        return new TUserCommandSubscribe(string.c_str() + 3, string.length() - 3);
    else
    {
        char const* space = strchr(string.c_str(), ' ');
        return new TUserCommandPublish(string.c_str(), space - string.c_str(), space + 1, string.length() - (space - string.c_str() + 1));
    }
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
        else if (typeid(*userCommand) == typeid(TUserCommandSubscribe))
        {
            auto cmd = dynamic_cast<TUserCommandSubscribe*>(userCommand.get());
            c.write(ClientMessageSubscribe(cmd->Topic()));
        }
        else if (typeid(*userCommand) == typeid(TUserCommandPublish))
        {
            auto cmd = dynamic_cast<TUserCommandPublish*>(userCommand.get());
            c.write(ClientMessagePublish(cmd->Topic(), cmd->Data()));
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
