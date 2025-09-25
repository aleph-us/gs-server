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


class PrintRunnable;

class GSWorkerTask : public Poco::Task
{
public:
	GSWorkerTask(Poco::NotificationQueue& printNQ, Poco::Logger& logger, Poco::Util::LayeredConfiguration& config);
	~GSWorkerTask();

	void runTask();

private:
	void runPrint(PrintRunnable& print);

	Poco::Logger& _logger;
	Poco::NotificationQueue& _printNQ;
	std::string _printApp;
	Poco::Util::LayeredConfiguration& _config;
};


