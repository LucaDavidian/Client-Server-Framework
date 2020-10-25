#include "Server.h"

enum class MyMessages : uint8_t
{
	SERVER_ACCEPT, SERVER_REFUSE, TEXT_MSG,
};

class MyServer : public Server<MyMessages>
{
public:
	MyServer(uint16_t port) : Server(port) {}

	void OnStart() override
	{
		PRINTLN("server running");
	}

	void OnListen() override
	{
		PRINT("server listening @ ");
		PRINT(mHost);
		PRINT(" on port ");
		PRINTLN(std::to_string(mPort));
	}

	bool OnClientConnect(ConnectionPtr connection) override
	{   
		bool accept = true;

		if (accept)
		{
			Message<MyMessages> message(MyMessages::SERVER_ACCEPT);
			message << connection->GetId();

			Send(connection, message);

			return true;
		}
		else
		{
			Message<MyMessages> message(MyMessages::SERVER_REFUSE);
			message << "I don't like you";

			Send(connection, message);

			return false;
		}
	}

	void OnClientAccepted(ConnectionPtr client) override
	{
		PRINT("accepted connection from ");
		PRINT(client->GetHost());
		PRINT(" on port ");
		PRINTLN(client->GetPort());
	}

	void OnClientDisconnect(ConnectionPtr connection)
	{
		PRINT("removed connection: "); 
		PRINTLN(connection->GetId());
	}

	void OnMessage(ConnectionPtr sender, Message<MyMessages> &message)
	{
		switch (message.GetType())
		{
			case MyMessages::TEXT_MSG:
			{
				uint32_t senderId;
				message >> senderId;

				uint32_t recipientId;
				message >> recipientId;

				std::string s;
				message >> s;
				
				message << s;
				message << recipientId;
				message << senderId;

				PRINT("[");
				PRINT(std::to_string(senderId));
				PRINT("] : ");
				PRINTLN(s);

				if (recipientId == 0U)
					break;
				else if (recipientId == (uint32_t)-1)
					SendAll(message);
				else
					Send(recipientId, message);
			}
				break;
		}
	}
};

int main(int argc, char **argv)
{
	MyServer server(60005);
	server.Start();

	while (true)
	{
		if (server.Available())
			server.ProcessMessage();
	}

	return 0;
}