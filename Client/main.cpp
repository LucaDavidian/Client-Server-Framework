#include "Client.h"
#include <thread>
#include <iostream>

enum class MyMessages: uint8_t
{
	SERVER_ACCEPT, SERVER_REFUSE, TEXT_MSG,
};

class MyClient : public Client<MyMessages>
{
public:
	~MyClient()
	{
		if (mConsoleThread.joinable())
			mConsoleThread.join();
	}

	void OnConnect(const std::string host, uint16_t port) override
	{
		PRINT("connected to server running @ ");
		PRINT(host);
		PRINT(" on port ");
		PRINTLN(port);
	}

	void OnDisconnect() override
	{

	}

	void OnConnectionLost() override
	{
		PRINTLN("lost connection with server");
	}

	void OnMessage(Message<MyMessages> &message) override
	{
		switch (message.GetType())
		{
			case MyMessages::SERVER_ACCEPT:
			{
				uint32_t id;
				message >> mId;

				Message<MyMessages> cmsg(MyMessages::TEXT_MSG);
				cmsg << "hello from client ";
				cmsg << 0U;
				cmsg << mId;

				Send(cmsg);

				mConsoleThread = std::thread(&MyClient::ConsoleThread, this);
			}
			break;

			case MyMessages::SERVER_REFUSE:
			{
				std::string reason;
				message >> reason;

				PRINT("server refused connection: ");  PRINTLN(reason);
				
				Disconnect();
			}
			break;

			case MyMessages::TEXT_MSG:
			{
				uint32_t senderId;
				message >> senderId;

				uint32_t recipientId;
				message >> recipientId;

				std::string s;
				message >> s;

				PRINT("["); PRINT(std::to_string(senderId)); PRINT("] : "); PRINTLN(s);
			}
			break;
		}
	}
private:
	std::thread mConsoleThread;

	void ConsoleThread()
	{
		while (IsConnected())
		{
			std::string s;

			std::getline(std::cin, s);

			if (!std::cin)
				continue;

			if (s == "exit")
			{
				Disconnect();
				break;
			}

			Message<MyMessages> cmsg(MyMessages::TEXT_MSG);
			cmsg << s;

			if (s.find("all:", 0, 4) != std::string::npos)
				cmsg << (uint32_t)-1;
			else if (s.find("to:", 0, 3) != std::string::npos)
			{
				size_t index = 3;
				cmsg << (uint32_t)std::stoul(&s[3], &index);
			}
			else
				cmsg << 0U;

			cmsg << mId;

			Send(cmsg);
		}
	}
};

int main(int argc, char **argv)
{
	MyClient client;
	client.Connect("localhost", 60005);

	while (client.IsConnected())
	{
		if (client.Available())
			client.ProcessMessage();
	}

	return 0;
}