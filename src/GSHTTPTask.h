//
// GSHTTPTask.h
//
//
// Copyright (C) 2025 Aleph ONE Software Engineering LLC
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
//


#ifndef GSHTTPTask_INCLUDED
#define GSHTTPTask_INCLUDED

#include "Poco/Task.h"
#include "Poco/Logger.h"
#include "Poco/Event.h"
#include "Poco/SharedPtr.h"
#include "Poco/Util/LayeredConfiguration.h"
#include "Poco/Net/ServerSocket.h"
#include "Poco/Net/HTTPServer.h"
#include "Poco/Net/HTTPRequestHandlerFactory.h"
#include "Poco/NotificationQueue.h"
#include <atomic>

class GSHTTPTask : public Poco::Task
{
public:
	using Configuration = Poco::Util::LayeredConfiguration;

	GSHTTPTask() = delete;
	GSHTTPTask(const GSHTTPTask&) = delete;
	GSHTTPTask(GSHTTPTask&&) = delete;
	GSHTTPTask& operator=(const GSHTTPTask&) = delete;
	GSHTTPTask& operator=(GSHTTPTask&&) = delete;

	explicit GSHTTPTask(Configuration& cfg,  Poco::NotificationQueue& nq, const std::string& taskName = "GSHTTPTask");

	virtual ~GSHTTPTask();

	void runTask() override;
	void wakeUp();
	void stop();

private:
	std::atomic<bool> _stop{false};
	Poco::Net::ServerSocket _serverSocket;
	Poco::SharedPtr<Poco::Net::HTTPRequestHandlerFactory> _pReqHandlerFactory;
	Poco::Net::HTTPServer _httpServer;
	Poco::Event _event;
	Poco::Logger& _logger;
};

#endif // GSHTTPTask_INCLUDED
