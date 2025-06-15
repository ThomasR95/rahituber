#pragma once
#include <iostream>

#include "mongoose.h"

#include <deque>
#include <thread>
#include <chrono>
#include <string>
#include <functional>

inline void ev_handler(struct mg_connection* c, int ev, void* ev_data);

class WebSocket
{
public:

	struct QueueItem {
		std::string stateId;
		int activeState;
	};

	void Start(int port = 8000)
	{
		if (_active)
			return;

		_port = port;

		if (_pollThread != nullptr)
		{
			if (_pollThread->joinable())
				_pollThread->join();

			delete _pollThread;
			_pollThread = nullptr;
		}

		_active = true;

		_pollThread = new std::thread([&] {

			mg_mgr_init(&_eventManager);
			mg_http_listen(&_eventManager, ("http://0.0.0.0:"+std::to_string(_port)).c_str(), ev_handler, this);

			while (_active)
			{
				mg_mgr_poll(&_eventManager, 1000);
				std::this_thread::sleep_for(std::chrono::milliseconds(1000/60));
			}

			mg_mgr_free(&_eventManager);

		});
	}

	void Stop()
	{
		_active = false;
	}

	~WebSocket()
	{
		_active = false;

		if (_pollThread != nullptr)
		{
			if (_pollThread->joinable())
				_pollThread->join();

			delete _pollThread;
			_pollThread = nullptr;
		}
	}

	void AddQueueItem(const QueueItem& qi) 
	{ 
		_msgQueue.push_back(qi); 
	}

	QueueItem QueueFront()
	{
		if (_msgQueue.size() > 0)
			return _msgQueue.front();
		else return {"-1", -1};
	}

	void PopQueueFront() 
	{ 
		if (_msgQueue.size() > 0)
			_msgQueue.pop_front(); 
	}

	void ClearQueue() 
	{
		_msgQueue.clear();
	}

	bool hasQueue()
	{
		return _msgQueue.size() > 0;
	}

	std::function<void(const std::string&)> _logFunction;

private:

	struct mg_mgr _eventManager = {};

	std::thread* _pollThread = nullptr;
	bool _active = false;
	std::deque<QueueItem> _msgQueue;
	int _port = 8000;

};

inline void ev_handler(struct mg_connection* c, int ev, void* ev_data)
{

	WebSocket* webSocket = static_cast<WebSocket*>(c->fn_data);

	if (ev == MG_EV_HTTP_MSG && webSocket != nullptr)
	{
		struct mg_http_message* hm = (struct mg_http_message*)ev_data;

		//std::cout << hm->message.buf << std::endl;

		webSocket->_logFunction("HTTP Message received: " + std::string(hm->message.buf));

		if (mg_match(hm->uri, mg_str("/state"), NULL))
		{
			std::string decodedQuery;
			decodedQuery.resize(hm->query.len);
			mg_url_decode(hm->query.buf, hm->query.len, decodedQuery.data(), hm->query.len, 0);

			auto query = mg_str(decodedQuery.c_str());

			char* stringRet = mg_json_get_str(query, "$[0]");
			std::string stateID = "";

			if (stringRet == NULL)
				stateID = std::to_string(mg_json_get_long(query, "$[0]", -1));
			else
				stateID = stringRet;

			long stateActive = mg_json_get_long(query, "$[1]", -1);

			try
			{
				mg_http_reply(c, 200, "Content-Type: application/json\r\n",
					"{%m:%d, %m:%d}\n", MG_ESC("state"), stateID, MG_ESC("active"), stateActive);

			}
			catch (...)
			{
				webSocket->_logFunction("HTTP reply failed");
			}
			

			std::cout << "Recieved: State " << stateID << " active " << stateActive << std::endl;

			webSocket->_logFunction("State change added to queue: " + stateID + " = " + std::to_string(stateActive));

			webSocket->AddQueueItem({ stateID, stateActive });
		}
		else
		{
			try
			{
				mg_http_reply(c, 400, "", "");
			}
			catch (...)
			{
				webSocket->_logFunction("HTTP reply failed");
			}
		}
	}
}