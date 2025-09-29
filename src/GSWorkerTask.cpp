//
// GSWorkerTask.cpp
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


#include "GSWorkerTask.h"
#include "GSNotification.h"

#include "Poco/Notification.h"
#include "Poco/NotificationQueue.h"
#include "Poco/Logger.h"

#include <vector>
#include <string>

#include "iapi.h"
#include "ierrors.h"


using namespace Poco;
using namespace Poco::Util;



GSWorkerTask::GSWorkerTask(Poco::NotificationQueue& convQ, Poco::NotificationQueue& sendQ, 
		Poco::Logger& logger, Poco::Util::LayeredConfiguration& config) :
	Task("GSWorkerTask"),
	_convQ(convQ),
	_sendQ(sendQ),
	_logger(logger),
	_config(config)
{
}

GSWorkerTask::~GSWorkerTask()
{
}


void GSWorkerTask::runTask()
{
	while (!isCancelled())
	{
		try
		{
			if (Poco::Notification::Ptr nf = _convQ.waitDequeueNotification(1000)) 
			{
				Poco::AutoPtr<JobNotification> jn  = nf.cast<JobNotification>();
				if (jn) 
				{
					auto job = jn->job;

					const bool ok = convert(job->gsArgs);
					if (ok) 
					{
						_logger.information("PDF->PCL done: %s", job->pclPath);
						if(!job->printers.empty())
						{
							_sendQ.enqueueNotification(new JobNotification(job));
						}
						else
							_logger.warning("No listed printer, conversion only");
						
					} 
					else 
					{
						_logger.error("PDF->PCL failed for job %s", job->jobId);
					}
				}
				else 
					_logger.warning("Unexpected notification type in convQ");
			}
		}
		catch (Poco::Exception& ex)
		{
			_logger.error(ex.displayText());
		}
		catch (std::exception& ex)
		{
			_logger.error(ex.what());
		}
	}
}

bool GSWorkerTask::convert(const std::vector<std::string>& gsArgs)
{
	void* minst = NULL;
	int code, code1;
		
	std::vector<const char*> argv;
	argv.reserve(gsArgs.size() + 2);
	argv.push_back(""); 

	for (auto& s : gsArgs)
		argv.push_back(s.c_str());

	int gsargc = static_cast<int>(argv.size());

	code = gsapi_new_instance(&minst, NULL);
	if (code < 0)
	{
		_logger.error("gs_new_instance error=%d, quitting", code);
		return false;
	}
	else if (_logger.trace())
		_logger.trace("Created gs instance.");

	code = gsapi_set_arg_encoding(minst, GS_ARG_ENCODING_UTF8);
	if (code == 0)
	{
		code = gsapi_init_with_args(minst, gsargc, const_cast<char**>(argv.data()));
		if (code == 0 || (code == gs_error_Quit))
			_logger.trace("Conversion processed.");
		else 
			_logger.error("gsapi_init_with_args error=%d", code);
	}
	else
		_logger.error("gsapi_set_arg_encoding error=%d", code);

	code1 = gsapi_exit(minst);
	if ((code == 0) || (code == gs_error_Quit))
		code = code1;

	gsapi_delete_instance(minst);
	if (_logger.trace())
		_logger.trace("Deleted gs instance.");

	if ((code != 0) && (code != gs_error_Quit))
	{
		_logger.error("gs error=%d, quitting", code);
		return false;
	}
	else if (_logger.trace())
		_logger.trace("Conversion succesfully completed.");

	return true;
}

