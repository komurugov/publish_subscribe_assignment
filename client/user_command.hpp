#ifndef USER_COMMAND_HPP
#define USER_COMMAND_HPP


#include <regex>


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
    TPort const& Port() const { return port_; }
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

std::unique_ptr<TUserCommand> StringToUserCommand(std::string const& string)
{
    bool constexpr DEBUG = false;   // "true" enables commands abbreviations
    std::regex re;
    std::smatch match;

    re = DEBUG ? "^co$" : "^CONNECT ([0-9]+) (.+)$";
    if (std::regex_match(string, match, re))
    {
        TPort port{ DEBUG ? "1999" : std::string{ match[1] } };
        std::string clientName{ DEBUG ? "default_client" : std::string{ match[2] } };
        return std::make_unique<TUserCommandConnect>(port, clientName);
    }

    re = DEBUG ? "^di$" : "^DISCONNECT$";
    if (std::regex_match(string, match, re))
    {
        return std::make_unique<TUserCommandDisconnect>();
    }

    re = DEBUG ? "^su ([^ ]+)$" : "^SUBSCRIBE ([^ ]+)$";    // topic name shouldn't contain spaces to be able to distinct it from data in the "publish" command
    if (std::regex_match(string, match, re))
    {
        std::string topic{ match[1] };
        return std::make_unique<TUserCommandSubscribe>(topic);
    }

    re = DEBUG ? "^un ([^ ]+)$" : "^UNSUBSCRIBE ([^ ]+)$";  // topic name shouldn't contain spaces to be able to distinct it from data in the "publish" command
    if (std::regex_match(string, match, re))
    {
        std::string topic{ match[1] };
        return std::make_unique<TUserCommandUnsubscribe>(topic);
    }

    re = DEBUG ? "^pu ([^ ]+) (.+)" : "^PUBLISH ([^ ]+) (.+)";
    if (std::regex_match(string, match, re))
    {
        std::string topic{ match[1] };
        std::string data{ match[2] };
        return std::make_unique<TUserCommandPublish>(topic, data);
    }

    throw std::logic_error("Cannot parse this command!");
}

#endif // USER_COMMAND_HPP
