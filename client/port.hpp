#ifndef PORT_HPP
#define PORT_HPP


class TPort
{
public:
    TPort(const std::string& string) : string_(string) {}

    std::string const& ToString() const { return string_; }
private:
    std::string string_;
};


#endif // PORT_HPP