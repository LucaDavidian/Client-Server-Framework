#ifndef CLIENT_H
#define CLIENT_H

#include <WinSock2.h>
#include <WS2tcpip.h>
#include <string>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include "ThreadsafeQueue.h"
#include "Message.h"
#include "Connection.h"
#include "debug.h"

template <typename T>
class Client
{
public:
	Client();
	~Client();

	void Connect(const std::string &host, uint16_t port);  
	void Disconnect();
	void Send(const Message<T> &message);

	bool Available() const { return !mInMessageQueue.Empty(); }
	void ProcessMessage();

	bool IsConnected() const { std::lock_guard<std::mutex> guard(mMutex);  return mConnection != nullptr; }
protected:
	virtual void OnConnect(const std::string host, uint16_t port) = 0;
	virtual void OnDisconnect() = 0;
	virtual void OnConnectionLost() = 0;
	virtual void OnMessage(Message<T> &message) = 0;

	uint32_t mId;
private:
	mutable std::mutex mMutex;
	std::condition_variable mCondVar;

	std::unique_ptr<Connection<T>> mConnection;     
	ThreadsafeQueue<OwnedMessage<T>> mInMessageQueue;

	std::thread mCheckConnectionLostThread;
	void CheckConnectionLostThread()   
	{
		std::unique_lock<std::mutex> lock(mMutex);
		mCondVar.wait(lock, [&] { return mConnection == nullptr ? true : !mConnection->mIsOpen.load(); }); 

		if (mConnection)   // server closed connection (notify from connection) otherwise connection is null and client closed connection (notify from Disconnect or from destructor)
		{
			mConnection.reset();   // destroys the connection (calls Connection<T>::Close()) and sets pointer to null
			OnConnectionLost();
		}
	}
};

template <typename T>
Client<T>::Client()
{
	WSAData wsa;
	WSAStartup(MAKEWORD(2, 2), &wsa);
}

template <typename T>
Client<T>::~Client() 
{	
	Disconnect();
	WSACleanup();
}

template <typename T>
void Client<T>::Connect(std::string const &host, uint16_t port)
{
	addrinfo hints, *serverAddress;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	//hints.ai_flags = AI_PASSIVE;

	if (getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &serverAddress) != 0)
		Error("cannot get connection address");

	SOCKET connectionSocket;
	if ((connectionSocket = socket(serverAddress->ai_family, serverAddress->ai_socktype, serverAddress->ai_protocol)) == INVALID_SOCKET)
		Error("cannot create connection socket");

	if (connect(connectionSocket, serverAddress->ai_addr, serverAddress->ai_addrlen) != 0)
		Error("connection error");

	char serverHost[INET6_ADDRSTRLEN];
	if (serverAddress->ai_family == AF_INET)
		inet_ntop(AF_INET, &reinterpret_cast<sockaddr_in*>(serverAddress->ai_addr)->sin_addr, serverHost, sizeof serverHost);
	else  // serverAddress->ai_family == AF_INET6
		inet_ntop(AF_INET6, &reinterpret_cast<sockaddr_in6*>(serverAddress->ai_addr)->sin6_addr, serverHost, sizeof serverHost);

	uint16_t serverPort = serverAddress->ai_family == AF_INET ? ntohs(reinterpret_cast<sockaddr_in*>(serverAddress->ai_addr)->sin_port) : ntohs(reinterpret_cast<sockaddr_in6*>(serverAddress->ai_addr)->sin6_port);

	mConnection = std::make_unique<Connection<T>>(Connection<T>::Owner::CLIENT, 0U, serverHost, serverPort, connectionSocket, mInMessageQueue, mCondVar);
	mCheckConnectionLostThread = std::thread(&Client::CheckConnectionLostThread, this);    // started after connection is created (notify always after wait)
	OnConnect(mConnection->GetHost(), mConnection->GetPort());
}

template <typename T>
void Client<T>::Disconnect()
{
	if (mConnection)  // if a connection pointer is valid Disconnect has been called from client code or destructor, else the CheckConnectionLostThread called it
	{
		{
			std::lock_guard<std::mutex> lock(mMutex);
			mConnection.reset();      // destroys the connection (calls Connection<T>::Close()) and sets pointer to null
		}

		mCondVar.notify_one();       // notify the thread waiting for server side 
	}

	if (mCheckConnectionLostThread.joinable())
		mCheckConnectionLostThread.join();
}

template <typename T>
void Client<T>::Send(const Message<T> &message)
{
	if (mConnection)
		mConnection->Send(message);
}

template <typename T>
void Client<T>::ProcessMessage()
{
	OwnedMessage<T> message = mInMessageQueue.Front();
	mInMessageQueue.DeQueue();

	OnMessage(message);
}

#endif 