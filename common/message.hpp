#ifndef MESSAGE_HPP
#define MESSAGE_HPP


#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>


class TMessageWithSizePrefix
{
public:
  static constexpr std::size_t header_length = 4;
  static constexpr std::size_t max_body_length = 512;

  TMessageWithSizePrefix()
    : body_length_(0)
  {
  }

  const char* data() const
  {
    return data_;
  }

  char* data()
  {
    return data_;
  }

  std::size_t length() const
  {
    return header_length + body_length_;
  }

  const char* body() const
  {
    return data_ + header_length;
  }

  char* body()
  {
    return data_ + header_length;
  }

  std::size_t body_length() const
  {
    return body_length_;
  }

  void body_length(std::size_t new_length)
  {
    body_length_ = new_length;
    if (body_length_ > max_body_length)
      body_length_ = max_body_length;
  }

  bool decode_header()
  {
    char header[header_length + 1] = "";
    strncat_s(header, header_length + 1, data_, header_length);
    body_length_ = std::atoi(header);
    if (body_length_ > max_body_length)
    {
      body_length_ = 0;
      return false;
    }
    return true;
  }

  void encode_header()
  {
    char header[header_length + 1] = "";
    sprintf_s(header, header_length + 1, "%4d", static_cast<int>(body_length_));
    std::memcpy(data_, header, header_length);
  }

protected:
  char data_[header_length + max_body_length];
  std::size_t body_length_;
};


class ClientMessageProcessor
{
public:
    virtual ~ClientMessageProcessor() {}
};

class ClientMessageSubscribeProcessor : public ClientMessageProcessor
{
public:
    static constexpr char Signature()
    {
        return 's';
    }
    void Serialize(std::string const& topic, TMessageWithSizePrefix& msg) const
    {
        size_t length = 1 + topic.length();
        if (length > TMessageWithSizePrefix::max_body_length)
            throw "Too long topic!";
        msg.body()[0] = Signature();
        snprintf(msg.body() + 1, TMessageWithSizePrefix::max_body_length, "%s", topic.c_str());
        msg.body_length(length);
        msg.encode_header();
    }
    std::string Topic(TMessageWithSizePrefix const& msg) const
    {
        return std::string(msg.body() + 1, msg.body_length() - 1);
    }
};

class ClientMessageUnsubscribeProcessor : public ClientMessageProcessor
{
public:
    static constexpr char Signature()
    {
        return 'u';
    }
    void Serialize(std::string const& topic, TMessageWithSizePrefix& msg) const
    {
        size_t length = 1 + topic.length();
        if (length > TMessageWithSizePrefix::max_body_length)
            throw "Too long topic!";
        msg.body()[0] = Signature();
        snprintf(msg.body() + 1, TMessageWithSizePrefix::max_body_length, "%s", topic.c_str());
        msg.body_length(length);
        msg.encode_header();
    }
    std::string Topic(TMessageWithSizePrefix const& msg) const
    {
        return std::string(msg.body() + 1, msg.body_length() - 1);
    }
};

class ClientMessagePublishProcessor : public ClientMessageProcessor
{
public:
    static constexpr char Signature()
    {
        return 'p';
    }
    void Serialize(std::string const& topic, std::string const& data, TMessageWithSizePrefix& msg) const
    {
        size_t length = 1 + topic.length() + 1 + data.length();
        if (length > TMessageWithSizePrefix::max_body_length)
            throw "Too long topic and data!";
        msg.body()[0] = Signature();
        snprintf(msg.body() + 1, TMessageWithSizePrefix::max_body_length, "%s %s", topic.c_str(), data.c_str());
        msg.body_length(length);
        msg.encode_header();
    }
    std::string Data(TMessageWithSizePrefix const& msg) const
    {
        char const* pos = strchr(msg.body() + 1, ' ');
        if (!pos)
            return std::string();
        return std::string(pos + 1, msg.body_length() - (pos - msg.body() + 1));
    }
    std::string Topic(TMessageWithSizePrefix const& msg) const
    {
        char const* pos = strchr(msg.body() + 1, ' ');
        if (!pos)
            return std::string();
        return std::string(msg.body() + 1, pos - (msg.body() + 1));
    }
};

std::unique_ptr<ClientMessageProcessor> CreateClientMessageProcessor(TMessageWithSizePrefix const& msg)
{
    switch (msg.body()[0])
    {
    case ClientMessageSubscribeProcessor::Signature():   return std::make_unique<ClientMessageSubscribeProcessor>();
    case ClientMessageUnsubscribeProcessor::Signature(): return std::make_unique<ClientMessageUnsubscribeProcessor>();
    case ClientMessagePublishProcessor::Signature():     return std::make_unique<ClientMessagePublishProcessor>();
    }
    return std::make_unique<ClientMessageProcessor>();
}


class ServerMessageProcessor
{
public:
    void Serialize(std::string const& topic, std::string const& data, TMessageWithSizePrefix& msg) const
    {
        size_t length = topic.length() + 1 + data.length();
        if (length > TMessageWithSizePrefix::max_body_length)
            throw "Too long topic and data!";
        snprintf(msg.body(), TMessageWithSizePrefix::max_body_length, "%s %s", topic.c_str(), data.c_str());
        msg.body_length(length);
        msg.encode_header();
    }
    std::string Data(TMessageWithSizePrefix const& msg) const
    {
        char const* pos = strchr(msg.body(), ' ');
        if (!pos)
            return std::string();
        return std::string(pos + 1, msg.body_length() - (pos - msg.body() + 1));
    }
    std::string Topic(TMessageWithSizePrefix const& msg) const
    {
        char const* pos = strchr(msg.body(), ' ');
        if (!pos)
            return std::string();
        return std::string(msg.body(), pos - msg.body());
    }
};


#endif // MESSAGE_HPP
