#include <cstdlib>
#include <deque>
#include <iostream>
#include <list>
#include <memory>
#include <set>
#include <utility>
#include "../asio/asio/include/asio.hpp"
#include "../common/message.hpp"

using asio::ip::tcp;

using std::cout;
using std::cerr;

//----------------------------------------------------------------------

typedef std::deque<TMessageWithSizePrefix> message_queue;

//----------------------------------------------------------------------

class participant
{
public:
  virtual ~participant() {}
  virtual void deliver(const std::string& msg, std::string const& topic) = 0;
};

typedef std::shared_ptr<participant> participant_ptr;

//----------------------------------------------------------------------

class room
{
public:
  void join(participant_ptr participant)
  {
    participants_.insert(participant);
    cout << "A client connected." << std::endl;
  }

  void leave(participant_ptr participant)
  {
    participants_.erase(participant);
    cout << "A client disconnected." << std::endl;
  }

  void deliver(const std::string& msg, std::string const& topic)
  {
    for (auto participant: participants_)
      participant->deliver(msg, topic);
  }

private:
  std::set<participant_ptr> participants_;
};

//----------------------------------------------------------------------

class session
  : public participant,
    public std::enable_shared_from_this<session>
{
public:
  session(tcp::socket socket, room& room)
    : socket_(std::move(socket)),
      room_(room)
  {
  }

  void start()
  {
    room_.join(shared_from_this());
    do_read_header();
  }

  void deliver(const std::string& data, std::string const& topic)
  {
    if (std::find(topics_.begin(), topics_.end(), topic) == topics_.end())
          return;
    bool write_in_progress = !write_msgs_.empty();
    TMessageWithSizePrefix msg;
    ServerMessageProcessor{}.Serialize(topic, data, msg);
    write_msgs_.push_back(msg);
    if (!write_in_progress)
    {
      do_write();
    }
    cout << "The server is sending data \"" << data << "\" with the topic \"" << topic << "\" to a client." << std::endl;
  }

private:
  void ProcessMessageFromClient(TMessageWithSizePrefix const& msg)
  {
      std::unique_ptr<ClientMessageProcessor> processor{ CreateClientMessageProcessor(msg) };
      if (processor.get() == nullptr)
      {
          return;
      }

      if (auto parser = dynamic_cast<ClientMessageSubscribeProcessor*>(processor.get()))
      {
          std::string topic{ parser->Topic(msg) };
          cout << "A client tries to subscribe to the topic \"" << topic << "\"." << std::endl;
          topics_.insert(topic);
      }
      else if (auto parser = dynamic_cast<ClientMessageUnsubscribeProcessor*>(processor.get()))
      {
          std::string topic{ parser->Topic(msg) };
          cout << "A client tries to unsubscribe from the topic \"" << topic << "\"." << std::endl;
          topics_.erase(topic);
      }
      else if (auto parser = dynamic_cast<ClientMessagePublishProcessor*>(processor.get()))
      {
          std::string topic{ parser->Topic(msg) };
          std::string data{ parser->Data(msg) };
          cout << "A client sent data \"" << data << "\" with topic \"" << topic << "\"." << std::endl;
          room_.deliver(data, topic);
      }
  }

  void do_read_header()
  {
    auto self(shared_from_this());
    asio::async_read(socket_,
        asio::buffer(read_msg_.data(), TMessageWithSizePrefix::header_length),
        [this, self](std::error_code ec, std::size_t /*length*/)
        {
          if (!ec && read_msg_.decode_header())
          {
            do_read_body();
          }
          else
          {
            room_.leave(shared_from_this());
          }
        });
  }

  void do_read_body()
  {
    auto self(shared_from_this());
    asio::async_read(socket_,
        asio::buffer(read_msg_.body(), read_msg_.body_length()),
        [this, self](std::error_code ec, std::size_t /*length*/)
        {
          if (!ec)
          {
            ProcessMessageFromClient(read_msg_);
            do_read_header();
          }
          else
          {
            room_.leave(shared_from_this());
          }
        });
  }

  void do_write()
  {
    auto self(shared_from_this());
    asio::async_write(socket_,
        asio::buffer(write_msgs_.front().data(),
          write_msgs_.front().length()),
        [this, self](std::error_code ec, std::size_t /*length*/)
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
            room_.leave(shared_from_this());
          }
        });
  }

  tcp::socket socket_;
  room& room_;
  TMessageWithSizePrefix read_msg_;
  message_queue write_msgs_;
  std::set<std::string> topics_;
};

//----------------------------------------------------------------------

class server
{
public:
  server(asio::io_context& io_context,
      const tcp::endpoint& endpoint)
    : acceptor_(io_context, endpoint)
  {
    do_accept();
  }

private:
  void do_accept()
  {
    acceptor_.async_accept(
        [this](std::error_code ec, tcp::socket socket)
        {
          if (!ec)
          {
            std::make_shared<session>(std::move(socket), room_)->start();
          }
          do_accept();
        });
  }

  tcp::acceptor acceptor_;
  room room_;
};

//----------------------------------------------------------------------

int main(int argc, char* argv[])
{
  try
  {
    if (argc != 2)
    {
      cerr << "Usage: server <port>" << std::endl;
      return 1;
    }

    asio::io_context io_context;

    tcp::endpoint endpoint{ tcp::v4(), static_cast<asio::ip::port_type>(std::atoi(argv[1])) };

    server server{ io_context, endpoint };

    io_context.run();
  }
  catch (std::exception& e)
  {
    std::cerr << "Exception: " << e.what() << "\n";
  }

  return 0;
}
