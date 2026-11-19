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
		bool getStatus = false;
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

	void sendStatus(std::vector<QueueItem> status)
	{

		std::string jsonReply = "";
		
		if (status.size() == 1)
		{
			jsonReply = std::to_string(status[0].activeState);
		}
		else
		{
			jsonReply = "{";
			for (int s = 0; s < status.size(); s++)
			{
				jsonReply += "\"" + status[s].stateId + "\":" + std::to_string(status[s].activeState);
				if (s < status.size() - 1)
				{
					jsonReply += ", ";
				}
			}
			jsonReply += "}\n";
		} 

		if (_eventManager.conns != NULL)
		{
			mg_connection* conn = &_eventManager.conns[0];
			while (conn != NULL)
			{
				try
				{
					mg_http_reply(conn, 200, "Content-Type: application/json\r\n",
						jsonReply.c_str());

					if (_logFunction)
						_logFunction("Replied : Status\n" + jsonReply);
				}
				catch (...)
				{
					if (_logFunction)
						_logFunction("HTTP reply failed");
				}

				conn = conn->next;
			}
		}
		
	}

	std::function<void(const std::string&)> _logFunction;

	std::function<void(const std::string&)> _getStateFnc;

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

			webSocket->AddQueueItem({ stateID, int(stateActive) });
		}
		else if (mg_match(hm->uri, mg_str("/status"), NULL))
		{

			std::string decodedQuery;
			decodedQuery.resize(hm->query.len);
			mg_url_decode(hm->query.buf, hm->query.len, decodedQuery.data(), hm->query.len, 0);

			auto query = mg_str(decodedQuery.c_str());

			std::string stateID = "";

			if (query.len > 0)
			{
				char* stringRet = mg_json_get_str(query, "$[0]");

				if (stringRet == NULL)
					stateID = std::to_string(mg_json_get_long(query, "$[0]", -1));
				else
					stateID = stringRet;
			}

			if (stateID.length() > 0)
			{
				webSocket->AddQueueItem({ stateID, 0, true });
				webSocket->_logFunction( "Recieved: Status " + stateID);
			}
			else
			{
				webSocket->AddQueueItem({ "", 0, true});
				webSocket->_logFunction("Recieved: Status (all) ");
			}
			
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