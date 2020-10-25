#ifndef SERVER_H
#define SERVER_H

#include <WinSock2.h>
#include <WS2tcpip.h>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <string>
#include "Vector.h"
#include "Connection.h"
#include "ThreadsafeQueue.h"
#include "Message.h"
#include "debug.h"

template <typename T>
class Server
{
protected:
	using ConnectionPtr = std::shared_ptr<Connection<T>>;  // type alias for a shared pointer to a connection object
public:
	Server(uint16_t port);
	~Server();

	void Start();
	void Stop();
	void Send(ConnectionPtr connection, const Message<T> &message) const;
	void Send(uint32_t connectionId, const Message<T> &message) const;
	void SendAll(const Message<T> &message, ConnectionPtr ignore = nullptr) const;
	void Disconnect(ConnectionPtr connection);

	bool Available() const { return !mInMessageQueue.Empty(); }
	void ProcessMessage();
protected:
	virtual void OnStart() = 0;
	virtual void OnListen() = 0;
	virtual bool OnClientConnect(ConnectionPtr connection) = 0;
	virtual void OnClientAccepted(ConnectionPtr connection) = 0;
	virtual void OnClientDisconnect(ConnectionPtr connection) = 0;
	virtual void OnMessage(ConnectionPtr sender, Message<T> &message) = 0;

	std::string mHost;
	uint16_t mPort;
private:
	mutable std::mutex mMutex;        // guards the list of connections (listen thread / removal thread)
	std::condition_variable mCondVar;

	Vector<ConnectionPtr> mConnections;
	ThreadsafeQueue<OwnedMessage<T>> mInMessageQueue;
	
	SOCKET mListenSocket;

	std::thread mListenThread;
	void Listen();

	std::thread mRemoveConnectionsThread;
	void RemoveConnections();

	static const uint8_t sMaxNumConnections = 10;
	bool mIsRunning;
};

template <typename T>
Server<T>::Server(uint16_t port) :mListenSocket(INVALID_SOCKET), mIsRunning(false)
{
	WSAData wsa;
	WSAStartup(MAKEWORD(2, 2), &wsa);

	addrinfo hints, *address;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	//hints.ai_flags = AI_PASSIVE;

	if (getaddrinfo(nullptr, std::to_string(port).c_str(), &hints, &address) != 0)
		Error("cannot get server address");

	char stringBuf[INET6_ADDRSTRLEN];
	if (address->ai_family == AF_INET)
		inet_ntop(AF_INET, &reinterpret_cast<sockaddr_in *>(address->ai_addr)->sin_addr, stringBuf, sizeof stringBuf);
	else  // address->ai_family == AF_INET6
		inet_ntop(AF_INET6, &reinterpret_cast<sockaddr_in6 *>(address->ai_addr)->sin6_addr, stringBuf, sizeof stringBuf);

	mHost = stringBuf;

	mPort = ntohs(address->ai_family == AF_INET ? reinterpret_cast<sockaddr_in *>(address->ai_addr)->sin_port : reinterpret_cast<sockaddr_in6 *>(address->ai_addr)->sin6_port);

	if ((mListenSocket = socket(address->ai_family, address->ai_socktype, address->ai_protocol)) == INVALID_SOCKET)
		Error("cannot create server socket");

	if (bind(mListenSocket, address->ai_addr, address->ai_addrlen) != 0)  
		Error("cannot bind server socket");
}

template <typename T>
Server<T>::~Server()
{
	mIsRunning = false;

	if (mRemoveConnectionsThread.joinable())
		mRemoveConnectionsThread.join();

	if (mListenThread.joinable())
		mListenThread.join();  // TODO: accept non-blocking
		 
	closesocket(mListenSocket);

	WSACleanup();
}

template <typename T>
void Server<T>::Start()
{
	if (mIsRunning)  // TODO: error check
		return;

	mIsRunning = true;

	OnStart();

	mListenThread = std::thread(&Server::Listen, this);                        // thread that listens for and accepts new connections
	mRemoveConnectionsThread = std::thread(&Server::RemoveConnections, this);  // thread that waits for clients to disconnect
}
template <typename T>
void Server<T>::Stop()
{
	mIsRunning = false;

	if (mListenThread.joinable())  // TODO: non-blocking listening socket 
		mListenThread.join();

	for (std::shared_ptr<Connection<T>> &connection : mConnections)  // lock mutex
		connection->Close();

	if (mRemoveConnectionsThread.joinable())
		mRemoveConnectionsThread.join();
}

template <typename T>
void Server<T>::Listen()
{
	if (listen(mListenSocket, sMaxNumConnections) != 0)
		Error("listen error");

	OnListen();

	static uint32_t connectionId = 1000U;

	while (mIsRunning)
	{
		sockaddr_storage clientAddress;
		int clientAddressLength = sizeof(sockaddr_storage);
		SOCKET clientSocket = accept(mListenSocket, reinterpret_cast<sockaddr*>(&clientAddress), &clientAddressLength);    // accept connections (blocking)

		if (clientSocket == INVALID_SOCKET)
			Error("cannot create client socket");

		char clientHost[INET6_ADDRSTRLEN];
		if (clientAddress.ss_family == AF_INET)
			inet_ntop(AF_INET, &reinterpret_cast<sockaddr_in *>(&clientAddress)->sin_addr, clientHost, sizeof clientHost);
		else  // clientAddress.ss_family == AF_INET6
			inet_ntop(AF_INET6, &reinterpret_cast<sockaddr_in6 *>(&clientAddress)->sin6_addr, clientHost, sizeof clientHost);

		uint16_t clientPort = clientAddress.ss_family == AF_INET ? ntohs(reinterpret_cast<sockaddr_in *>(&clientAddress)->sin_port) : ntohs(reinterpret_cast<sockaddr_in6 *>(&clientAddress)->sin6_port);

		std::lock_guard<std::mutex> guard(mMutex);

		ConnectionPtr newConnection(new Connection<T>(Connection<T>::Owner::SERVER, connectionId++, clientHost, clientPort, clientSocket, mInMessageQueue, mCondVar));

		if (OnClientConnect(newConnection))            // callback called on new connections (TODO: refusal doesn't work, connect and then disconnect?)
		{
			mConnections.InsertLast(newConnection);    // if connection is accepted 
			OnClientAccepted(mConnections.Last());
		}
	}
}

template <typename T>
void Server<T>::Send(ConnectionPtr connection, const Message<T> &message) const
{
	if (connection->mIsOpen)
		connection->Send(message);
}

template <typename T>
void Server<T>::Send(uint32_t connectionId, const Message<T> &message) const
{
	std::lock_guard<std::mutex> guard(mMutex);

	for (const ConnectionPtr &connection : mConnections)
	{
		if (connection->GetId() == connectionId)
			connection->Send(message);
	}
}

template <typename T>
void Server<T>::SendAll(const Message<T> &message, ConnectionPtr ignore) const
{
	std::lock_guard<std::mutex> guard(mMutex);

	for (const ConnectionPtr &connection : mConnections)
	{
		if (connection != ignore && connection->mIsOpen)
			connection->Send(message);
	}
}

template <typename T>
void Server<T>::ProcessMessage()
{
	OwnedMessage<T> message(mInMessageQueue.Front());
	mInMessageQueue.DeQueue();

	OnMessage(message.GetSender(), message);
}

template <typename T>
void Server<T>::RemoveConnections()
{
	while (mIsRunning)
	{
		std::unique_lock<std::mutex> guard(mMutex);
		mCondVar.wait(guard);   // this thread waits for a connection to notify that a client has disconnected

		typename Vector<ConnectionPtr>::Iterator it = mConnections.Begin();   // poll which client disconnected from server and remove connection
		while (it != mConnections.End())
			if (!(*it)->mIsOpen)
			{
				OnClientDisconnect(*it);
				it = mConnections.Remove(it);  // calls Connection's destructor which closes the connection
			}
			else
				++it;
	}
}

template <typename T>
void Server<T>::Disconnect(ConnectionPtr connection)
{
	// connection.Disconnect();

	// remove from list of connections
}

#endif  // SERVER_H
