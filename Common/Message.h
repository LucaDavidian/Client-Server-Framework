#ifndef MESSAGE_H
#define MESSAGE_H

#include "Vector.h"
#include <string>

template <typename T>
class Connection;

template <typename T>
class Message
{
	friend class Connection<T>;
public:

	/*template <typename D>
	friend message<T> &operator<<(message<T> &message, const d &data)
	{
		uint32_t size = message.mHeader.mSize;
		message.mHeader.mSize += sizeof data;

		message.mBody.Resize(message.mHeader.mSize);
		std::memcpy(message.mBody.Data() + size, &data, sizeof data);

		return message;
	}

	template <typename D>
	friend message<T> &operator>>(message<T> const &message, d &data)
	{
		size_t index = message.mBody.Size() - sizeof data;
		memcpy(&data, message.mBody.Data() + index, sizeof data);

		message.mBody.Resize(index);
		message.mHeader.mSize = message.mBody.Size();

		return message;
	}*/

	Message(T type) : mHeader{ type, 0U } {}

	T GetType() const { return mHeader.mType; }

	template <typename D>
	Message<T> &operator<<(D const &data)
	{
		uint32_t oldSize = mHeader.mSize;
		mHeader.mSize += sizeof data;

		mBody.Resize(mHeader.mSize);
		std::memcpy(mBody.Data() + oldSize, &data, sizeof data);

		return *this;
	}

	template <typename D>
	Message<T> &operator>>(D &data) 
	{
		size_t index = mBody.Size() - sizeof data;
		memcpy(&data, mBody.Data() + index, sizeof data);

		mBody.Resize(index);
		mHeader.mSize = mBody.Size();

		return *this;
	}

	Message<T> &operator<<(const std::string &string)
	{
		uint32_t oldSize = mHeader.mSize;

		mHeader.mSize += string.size() + sizeof(uint32_t);

		mBody.Resize(mHeader.mSize);

		for (unsigned int i = oldSize, j = 0U; i < mHeader.mSize - sizeof(uint32_t); i++, j++)
			mBody.Data()[i] = string[j];
		
		mBody.Data()[mHeader.mSize - sizeof(uint32_t)] = string.size();

		return *this;
	}

	Message<T> &operator<<(const char *cString)
	{
		uint32_t oldSize = mHeader.mSize;

		mHeader.mSize += std::strlen(cString) + sizeof(uint32_t);

		mBody.Resize(mHeader.mSize);

		for (unsigned int i = oldSize, j = 0U; i < mHeader.mSize - 4; i++, j++)
			mBody.Data()[i] = cString[j];

		mBody.Data()[mHeader.mSize - 4] = std::strlen(cString);

		return *this;
	}

	Message<T> &operator>>(std::string &data)
	{
		uint32_t length = mBody.Data()[mHeader.mSize - 4];

		for (unsigned int i = mHeader.mSize - (4 + length); i < mHeader.mSize - 4; i++)
			data.push_back(mBody.Data()[i]);

		mHeader.mSize -= length + 4;

		mBody.Resize(mHeader.mSize);

		return *this;
	}

private:
	Message() = default;  // only Connection<T> can call default ctor

	struct Header
	{
		T mType = T();  // T mType{};       // type of message (enum)
		uint32_t mSize = 0U;                // size of message's body
	} mHeader;

	Vector<uint8_t> mBody;                  // message data
};   

template <typename T>
class OwnedMessage : public Message<T>
{
private:
	using ConnectionPtr = std::shared_ptr<Connection<T>>;
public:
	OwnedMessage(ConnectionPtr sender, const Message<T> &message) : mSender(sender), Message<T>(message) {}

	ConnectionPtr GetSender() const { return mSender; }
private:
	ConnectionPtr mSender;
};

#endif  // MESSAGE_H
