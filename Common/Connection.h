#ifndef CONNECTION_H
#define CONNECTION_H

#include <memory>
#include <thread>
#include <condition_variable>
#include "ThreadsafeQueue.h"
#include "Message.h"
#include "debug.h"

template <typename T>
class Connection : public std::enable_shared_from_this<Connection<T>>
{
public:
	enum class Owner { CLIENT, SERVER };
private:
	using std::enable_shared_from_this<Connection>::shared_from_this;
public:
	Connection(Owner owner, uint32_t id, const std::string host, uint16_t port, SOCKET socket, ThreadsafeQueue<OwnedMessage<T>> &inMessageQueue, std::condition_variable &condVar);
	~Connection() { Close(); }

	void Send(const Message<T> &message);  
	void Close();

	std::atomic<bool> mIsOpen;

	std::string const &GetHost() const { return mHost; }
	uint16_t GetPort() const { return mPort; }
	uint32_t GetId() const { return mId; }
private:
	std::string mHost;  // other side's endpoint host
	uint16_t mPort;     // other side's endpoint port

	const Owner mOwner;
	uint32_t mId;

	SOCKET mSocket;  

	std::condition_variable &mCondVar;

	ThreadsafeQueue<Message<T>> mOutMessageQueue;
	ThreadsafeQueue<OwnedMessage<T>> &mInMessageQueue;

	//std::thread mSendThread;
	//std::thread mReceiveThread;
	//void Send_();
	//void Receive_();

	std::thread mRunThread;
	void Run();	
};

template <typename T>
Connection<T>::Connection(Owner owner, uint32_t id, const std::string host, uint16_t port, SOCKET socket, ThreadsafeQueue<OwnedMessage<T>> &inMessageQueue, std::condition_variable &condVar)
	: mOwner(owner), mId(id), mHost(host), mPort(port), mSocket(socket), mInMessageQueue(inMessageQueue), mCondVar(condVar)
{
	unsigned long socketMode = 1U;
	if (ioctlsocket(socket, FIONBIO, &socketMode) != 0)  // set non blocking socket
		Error("error setting socket i/o mode");

	mIsOpen = true;
	//mSendThread = std::thread(&Connection<T>::Send_, this);
	//mReceiveThread = std::thread(&Connection<T>::Receive_, this);
	mRunThread = std::thread(&Connection<T>::Run, this);
}

template <typename T>
void Connection<T>::Send(const Message<T> &message)
{
	mOutMessageQueue.EnQueue(message);
}

template <typename T>
void Connection<T>::Close()
{
	if (mIsOpen)
		mIsOpen = false;

	//if (mSendThread.joinable())
	//	mSendThread.join();

	//if (mReceiveThread.joinable())
	//	mReceiveThread.join();

	if (mRunThread.joinable())
		mRunThread.join();

	closesocket(mSocket);
}

//template <typename T>
//void Connection<T>::Send_()
//{
//	while (mIsOpen)
//	{
//		// get from outgoing queue and send 
//		if (!mOutMessageQueue.Empty())
//		{
//			Message<T> outMessage = mOutMessageQueue.Front();
//			mOutMessageQueue.DeQueue();
//
//			int sendFlags = 0;
//			int totalBytesSent = 0;
//
//			while (totalBytesSent < sizeof(Message<T>::Header))
//			{
//				int bytesSent = send(mSocket, reinterpret_cast<char*>(&outMessage.mHeader) + totalBytesSent, sizeof(Message<T>::Header) - totalBytesSent, sendFlags);
//				totalBytesSent += bytesSent;
//
//				if (bytesSent == SOCKET_ERROR)
//					Error("error sending message");
//			}
//
//			totalBytesSent = 0;
//			while (totalBytesSent < outMessage.mBody.Size())
//			{
//				int bytesSent = send(mSocket, reinterpret_cast<char*>(outMessage.mBody.Data()) + totalBytesSent, outMessage.mBody.Size() - totalBytesSent, sendFlags);
//				totalBytesSent += bytesSent;
//
//				if (bytesSent == SOCKET_ERROR)
//					Error("error sending message");
//			}
//		}
//	}
//}

//template <typename T>
//void Connection<T>::Receive_()
//{
//	thread_local static bool skip = false;  // used in non blocking mode
//
//	while (mIsOpen)
//	{
//		skip = false;
//
//		Message<T> inMessage;
//
//		int recvFlags = 0;
//		int totalBytesReceived = 0;
//
//		while (totalBytesReceived < sizeof(Message<T>::Header))  // receive message header
//		{
//			int bytesReceived = recv(mSocket, reinterpret_cast<char*>(&inMessage.mHeader) + totalBytesReceived, sizeof(Message<T>::Header) - totalBytesReceived, recvFlags);
//
//			if (bytesReceived == SOCKET_ERROR && WSAGetLastError() == WSAEWOULDBLOCK)
//			{
//				skip = true;
//				break;
//			}
//
//			if (bytesReceived == SOCKET_ERROR || bytesReceived == 0)
//				//Error("error receiving message");
//			//else if (bytesReceived == 0)  // other side closed connection
//			{
//				mIsOpen = false;
//				skip = true;
//				mCondVar.notify_one();
//
//				break;
//			}
//
//			totalBytesReceived += bytesReceived;
//		}
//
//		if (skip)
//			continue;
//
//		inMessage.mBody.Resize(inMessage.mHeader.mSize);
//
//		totalBytesReceived = 0;
//		while (totalBytesReceived < inMessage.mBody.Size())  // receive message body
//		{
//			int bytesReceived = recv(mSocket, reinterpret_cast<char*>(inMessage.mBody.Data()) + totalBytesReceived, inMessage.mBody.Size() - totalBytesReceived, recvFlags);
//
//			if (bytesReceived == SOCKET_ERROR && WSAGetLastError() == WSAEWOULDBLOCK)
//				continue;
//
//			if (bytesReceived == SOCKET_ERROR || bytesReceived == 0)
//				//Error("error receiving message");
//			//else if (bytesReceived == 0)  // other side closed connection
//			{
//				mIsOpen = false;
//				skip = true;
//				mCondVar.notify_one();
//
//				break;
//			}
//
//			totalBytesReceived += bytesReceived;
//		}
//
//		if (skip)
//			continue;
//
//		if (mOwner == Owner::SERVER)
//			mInMessageQueue.EnQueue(OwnedMessage<T>(shared_from_this(), inMessage));  // put received message into incoming queue	
//		else
//			mInMessageQueue.EnQueue(OwnedMessage<T>(nullptr, inMessage));
//	}                                                                      
//}

template <typename T>
void Connection<T>::Run()
{
	while (mIsOpen)
	{
		if (!mOutMessageQueue.Empty())   // while there are outgoing messages in queue send 
		{
			Message<T> outMessage = mOutMessageQueue.Front();
			mOutMessageQueue.DeQueue();

			int sendFlags = 0;
			int totalBytesSent = 0;

			while (totalBytesSent < sizeof(Message<T>::Header))
			{
				int bytesSent = send(mSocket, reinterpret_cast<char*>(&outMessage.mHeader) + totalBytesSent, sizeof(Message<T>::Header) - totalBytesSent, sendFlags);
				totalBytesSent += bytesSent;

				if (bytesSent == SOCKET_ERROR)
					Error("error sending message");
			}

			totalBytesSent = 0;
			while (totalBytesSent < outMessage.mBody.Size())
			{
				int bytesSent = send(mSocket, reinterpret_cast<char*>(outMessage.mBody.Data()) + totalBytesSent, outMessage.mBody.Size() - totalBytesSent, sendFlags);
				totalBytesSent += bytesSent;

				if (bytesSent == SOCKET_ERROR)
					Error("error sending message");
			}
		}

		thread_local static bool skip = false;  // used in non blocking mode

		skip = false;

		Message<T> inMessage;

		int recvFlags = 0;
		int totalBytesReceived = 0;

		while (totalBytesReceived < sizeof(Message<T>::Header))  // receive message header
		{
			int bytesReceived = recv(mSocket, reinterpret_cast<char*>(&inMessage.mHeader) + totalBytesReceived, sizeof(Message<T>::Header) - totalBytesReceived, recvFlags);

			if (bytesReceived == SOCKET_ERROR && WSAGetLastError() == WSAEWOULDBLOCK)
			{
				skip = true;
				break;
			}

			if (bytesReceived == SOCKET_ERROR || bytesReceived == 0)
				//Error("error receiving message");
			//else if (bytesReceived == 0)  // other side closed connection
			{
				mIsOpen = false;
				skip = true;
				mCondVar.notify_one();

				break;
			}

			totalBytesReceived += bytesReceived;
		}

		if (skip)
			continue;

		inMessage.mBody.Resize(inMessage.mHeader.mSize);

		totalBytesReceived = 0;
		while (totalBytesReceived < inMessage.mBody.Size())  // receive message body
		{
			int bytesReceived = recv(mSocket, reinterpret_cast<char*>(inMessage.mBody.Data()) + totalBytesReceived, inMessage.mBody.Size() - totalBytesReceived, recvFlags);

			if (bytesReceived == SOCKET_ERROR && WSAGetLastError() == WSAEWOULDBLOCK)
				continue;

			if (bytesReceived == SOCKET_ERROR || bytesReceived == 0)
				//Error("error receiving message");
			//else if (bytesReceived == 0)  // other side closed connection
			{
				mIsOpen = false;
				skip = true;
				mCondVar.notify_one();

				break;
			}

			totalBytesReceived += bytesReceived;
		}

		if (skip)
			continue;

		if (mOwner == Owner::SERVER)
			mInMessageQueue.EnQueue(OwnedMessage<T>(shared_from_this(), inMessage));  // put received message into incoming queue	
		else
			mInMessageQueue.EnQueue(OwnedMessage<T>(nullptr, inMessage));
	}
}

#endif  // CONNECTION_H