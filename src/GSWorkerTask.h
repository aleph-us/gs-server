//
// GSWorkerTask.h
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


#include "Poco/Task.h"
#include "Poco/Notification.h"
#include "Poco/Util/LayeredConfiguration.h"
#include "GSNotification.h"

namespace Poco
{
	class Logger;
	class NotificationQueue;
}


class GSWorkerTask : public Poco::Task
{
public:
	GSWorkerTask(Poco::NotificationQueue& convQ, Poco::NotificationQueue& sendQ, Poco::Logger& logger, Poco::Util::LayeredConfiguration& config);
	~GSWorkerTask();

	void runTask();

private:
	bool convertPCL(const std::vector<std::string>& gsArgs);

	Poco::NotificationQueue& _convQ;
	Poco::NotificationQueue& _sendQ;
	Poco::Logger& _logger;
	Poco::Util::LayeredConfiguration& _config;
};


