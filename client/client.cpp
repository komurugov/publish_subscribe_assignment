#include <cstdlib>
#include <deque>
#include <iostream>
#include <regex>
#include <thread>
#include "../asio/asio/include/asio.hpp"
#include "../common/message.hpp"

using asio::ip::tcp;

using std::cin;
using std::cout;

typedef std::deque<message> message_queue;

bool constexpr DEBUG = false;

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
            cout << "[Message] Topic: " << read_msg_.ServerToClientTopic() << " Data: " << read_msg_.ServerToClientData() << std::endl;
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
    TUserCommandConnect(TPort const& port, std::string const& clientName)
        : port_{ port }, clientName_{ clientName }
    {
    }
    TPort const& GetPort() const { return port_; }
private:
    TPort port_;
    std::string clientName_;
};

class TUserCommandDisconnect : public TUserCommand
{
};

class TUserCommandSubscribe : public TUserCommand
{
public:
    TUserCommandSubscribe(std::string const& topic)
        : topic_(topic)
    {
    }
    std::string const& Topic() { return topic_; }
private:
    std::string topic_;
};

class TUserCommandUnsubscribe : public TUserCommand
{
public:
    TUserCommandUnsubscribe(std::string const& topic)
        : topic_(topic)
    {
    }
    std::string const& Topic() { return topic_; }
private:
    std::string topic_;
};

class TUserCommandPublish : public TUserCommand
{
public:
    TUserCommandPublish(std::string const& topic, std::string const& data)
        : topic_(topic),
          data_(data)
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
    std::regex re;
    std::smatch match;
    
    re = DEBUG ? "^co$" : "^CONNECT ([0-9]+) (.+)$";
    if (std::regex_match(string, match, re))
    {
        TPort port{ DEBUG ? "1999" : std::string{ match[1] } };
        std::string clientName{ DEBUG ? "default_client" : std::string{ match[2] } };
        return new TUserCommandConnect(port, clientName);
    }

    re = DEBUG ? "^di$" : "^DISCONNECT$";
    if (std::regex_match(string, match, re))
    {
        return new TUserCommandDisconnect;
    }

    re = DEBUG ? "^su ([^ ]+)$" : "^SUBSCRIBE ([^ ]+)$";    // topic name shouldn't contain spaces to be able to distinct it from data in the "publish" command
    if (std::regex_match(string, match, re))
    {
        std::string topic{ match[1] };
        return new TUserCommandSubscribe(topic);
    }

    re = DEBUG ? "^un ([^ ]+)$" : "^UNSUBSCRIBE ([^ ]+)$";  // topic name shouldn't contain spaces to be able to distinct it from data in the "publish" command
    if (std::regex_match(string, match, re))
    {
        std::string topic{ match[1] };
        return new TUserCommandUnsubscribe(topic);
    }

    re = DEBUG ? "^pu ([^ ]+) (.+)" : "^PUBLISH ([^ ]+) (.+)";
    if (std::regex_match(string, match, re))
    {
        std::string topic{ match[1] };
        std::string data{ match[2] };
        return new TUserCommandPublish(topic, data);
    }

    throw std::logic_error("Cannot parse this command!");
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
        try
        {
            std::unique_ptr<TUserCommand> userCommand{ StringToUserCommand(inputString) };

            if (auto cmd = dynamic_cast<TUserCommandConnect*>(userCommand.get()))
            {
                c.do_connect(cmd->GetPort());
            }
            else if (auto cmd = dynamic_cast<TUserCommandDisconnect*>(userCommand.get()))
            {
                c.close();
            }
            else if (auto cmd = dynamic_cast<TUserCommandSubscribe*>(userCommand.get()))
            {
                c.write(ClientMessageSubscribe(cmd->Topic()));
            }
            else if (auto cmd = dynamic_cast<TUserCommandUnsubscribe*>(userCommand.get()))
            {
                c.write(ClientMessageUnsubscribe(cmd->Topic()));
            }
            else if (auto cmd = dynamic_cast<TUserCommandPublish*>(userCommand.get()))
            {
                c.write(ClientMessagePublish(cmd->Topic(), cmd->Data()));
            }
        }
        catch (std::exception& e)
        {
            std::cerr << e.what() << std::endl;
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
