#ifndef MESSAGE_HPP
#define MESSAGE_HPP

#include <cstdio>
#include <cstdlib>
#include <cstring>

enum class ClientMessageType
{
    Unknown,
    Subscribe,
    Unsubscribe,
    Publish
};

class message
{
public:
  static constexpr std::size_t header_length = 4;
  static constexpr std::size_t max_body_length = 512;

  message()
    : body_length_(0)
  {
  }

  message(std::string const& str)
      : body_length_(str.length())
  {
      memcpy(data_ + header_length, str.c_str(), body_length_);
      encode_header();
  }

  message(char const* str, size_t len)
      : body_length_(len)
  {
      memcpy(data_ + header_length, str, body_length_);
      encode_header();
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

  ClientMessageType Type()
  {
      switch (data_[header_length])
      {
      case 's': return ClientMessageType::Subscribe;
      case 'u': return ClientMessageType::Unsubscribe;
      case 'p': return ClientMessageType::Publish;
      default: return ClientMessageType::Unknown;
      }
  }

  message PublishData()
  {
      char* pos = strchr(data_ + header_length + 1, ' ');
      if (!pos)
          return message();
      return message(pos + 1, body_length_ - (pos - (data_ + header_length) + 1));
  }

  std::string PublishTopic()
  {
      char* pos = strchr(data_ + header_length + 1, ' ');
      if (!pos)
          return std::string();
      return std::string(data_ + header_length + 1, pos - (data_ + header_length + 1));
  }

  std::string SubscribeTopic()
  {
      return std::string(data_ + header_length + 1, body_length_ - 1);
  }

  std::string UnsubscribeTopic()
  {
      return std::string(data_ + header_length + 1, body_length_ - 1);
  }

protected:
  char data_[header_length + max_body_length];
  std::size_t body_length_;
};

class ClientMessagePublish : public message
{
public:
    ClientMessagePublish(std::string const& topic, std::string const& data)
    {
        size_t length = 1 + topic.length() + 1 + data.length();
        if (length > max_body_length)
            throw "Too long topic and data!";
        data_[header_length] = 'p';
        snprintf(data_ + header_length + 1, max_body_length, "%s %s", topic.c_str(), data.c_str());
        body_length_ = length;
        encode_header();
    }
};

class ClientMessageUnsubscribe : public message
{
public:
    ClientMessageUnsubscribe(std::string const& topic)
    {
        size_t length = 1 + topic.length();
        if (length > max_body_length)
            throw "Too long topic!";
        data_[header_length] = 'u';
        snprintf(data_ + header_length + 1, max_body_length, "%s", topic.c_str());
        body_length_ = length;
        encode_header();
    }
};

class ClientMessageSubscribe : public message
{
public:
    ClientMessageSubscribe(std::string const& topic)
    {
        size_t length = 1 + topic.length();
        if (length > max_body_length)
            throw "Too long topic!";
        data_[header_length] = 's';
        snprintf(data_ + header_length + 1, max_body_length, "%s", topic.c_str());
        body_length_ = length;
        encode_header();
    }
};


#endif // MESSAGE_HPP
