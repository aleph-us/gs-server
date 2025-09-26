
//
// GSNotification.h
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


#ifndef GSNotification_INCLUDED
#define GSNotification_INCLUDED

#include "Poco/Notification.h"
#include "Poco/Util/LayeredConfiguration.h"
#include "Poco/NotificationQueue.h"
#include "Poco/Logger.h"
#include "Poco/String.h"
#include "Poco/StringTokenizer.h"
#include "Poco/Util/Application.h"
#include <vector>
#include <memory>


namespace Poco
{
	class Logger;
	class NotificationQueue;
}


struct Job {
	std::string pdfPath;
	std::string pclPath;
	std::vector<std::string> gsArgs;
	std::vector<std::string> printers;
	std::string jobId; 
};
using JobPtr = std::shared_ptr<Job>;


class JobNotification : public Poco::Notification {
public:
	JobNotification(JobPtr j) : 
		job(std::move(j)) 
	{
	}

	JobPtr job;
};

#endif // GSNotification_INCLUDED
