#include <cstdlib>
#include <deque>
#include <iostream>
#include <thread>
#include "../asio/asio/include/asio.hpp"
#include "../common/message.hpp"
#include "port.hpp"
#include "user_command.hpp"

using asio::ip::tcp;

using std::cin;
using std::cout;

typedef std::deque<message> message_queue;

void ProcessMessageFromServer(message const& msg)
{
    ServerMessage parser;
    cout << "[Message] Topic: " << parser.Topic(msg)
         << " Data: " << parser.Data(msg) << std::endl;
}

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
            ProcessMessageFromServer(read_msg_);
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
                message msg;
                ClientMessageSubscribe{}.Serialize(cmd->Topic(), msg);
                c.write(msg);
            }
            else if (auto cmd = dynamic_cast<TUserCommandUnsubscribe*>(userCommand.get()))
            {
                message msg;
                ClientMessageUnsubscribe{}.Serialize(cmd->Topic(), msg);
                c.write(msg);
            }
            else if (auto cmd = dynamic_cast<TUserCommandPublish*>(userCommand.get()))
            {
                message msg;
                ClientMessagePublish{}.Serialize(cmd->Topic(), cmd->Data(), msg);
                c.write(msg);
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
