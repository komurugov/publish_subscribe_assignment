# publish_subscribe_assignment
Server and client implementing a publish-subscribe protocol on TCP.
The solution contains Asio library as a submodule, so it's necessary to clone the submodule for building the project.
The solution and project files were created in Microsoft Visual Studio Community 2022.
The projects were built in the same IDE under Windows 11 64-bit and tested only under the same OS, but theoretically should be cross-platform for Windows and Linux.

# Folders and files
- asio - the submodule for work with TCP.
- common/message.hpp - functions for creating and parsing messages being sent over TCP (used both by server and client).
- server/server.cpp - server.
- client/client.cpp - client.
- client/TPort.hpp - abstraction for pointing out a port in the client application.
- client/TUserCommand.hpp - parsing strings into class hierarchy of user commands.
