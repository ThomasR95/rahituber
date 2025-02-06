#pragma once
#include <iostream>

#include "mongoose.h"

#include <deque>
#include <thread>
#include <chrono>

inline void ev_handler(struct mg_connection* c, int ev, void* ev_data);

class WebSocket
{
public:

	struct QueueItem {
		int stateIdx;
		int activeState;
	};

	void Start(int port = 8000)
	{
		if (_active)
			return;

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
			mg_http_listen(&_eventManager, ("http://0.0.0.0:"+std::to_string(port)).c_str(), ev_handler, this);

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
		else return {-1, -1};
	}

	void PopQueueFront() 
	{ 
		_msgQueue.pop_front(); 
	}

	void ClearQueue() 
	{
		_msgQueue.clear();
	}

private:

	struct mg_mgr _eventManager;

	std::thread* _pollThread = nullptr;
	bool _active = false;
	std::deque<QueueItem> _msgQueue;

};

inline void ev_handler(struct mg_connection* c, int ev, void* ev_data)
{

	WebSocket* webSocket = static_cast<WebSocket*>(c->fn_data);

	if (ev == MG_EV_HTTP_MSG && webSocket != nullptr)
	{
		struct mg_http_message* hm = (struct mg_http_message*)ev_data;

		//std::cout << hm->message.buf << std::endl;

		if (mg_match(hm->uri, mg_str("/state"), NULL))
		{
			long stateID = mg_json_get_long(hm->query, "$[0]", -1);
			long stateActive = mg_json_get_long(hm->query, "$[1]", -1);
			mg_http_reply(c, 200, "Content-Type: application/json\r\n",
				"{%m:%d, %m:%d}\n", MG_ESC("state"), stateID, MG_ESC("active"), stateActive);

			std::cout << "Recieved: State " << stateID << " active " << stateActive << std::endl;

			webSocket->AddQueueItem({ stateID, stateActive });
		}
		else
		{
			mg_http_reply(c, 400, "", "");
		}
	}
}