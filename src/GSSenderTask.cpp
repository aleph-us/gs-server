//
// GSSenderTask.cpp
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


#include "GSNotification.h"
#include "Poco/NotificationQueue.h"
#include "Poco/Logger.h"
#include "Poco/Thread.h"
#include "Poco/Process.h"
#include "Poco/Path.h"
#include "Poco/File.h"
#include "Poco/Stopwatch.h"
#include "Poco/StringTokenizer.h"
#include "Poco/String.h"
#include "Poco/FileStream.h"
#include "Poco/StreamCopier.h"
#include "Poco/Net/StreamSocket.h"
#include "Poco/Net/SocketStream.h"
#include "Poco/Net/SocketAddress.h"
#include "Poco/Util/Application.h"
#include "iapi.h"
#include "ierrors.h"
#include <sstream>
#include "GSSenderTask.h"


using namespace Poco;
using namespace Poco::Net;
using namespace Poco::Util;


GSSenderTask::GSSenderTask(Poco::NotificationQueue& sendQ, Logger& logger, LayeredConfiguration& config) :
	Task("GSSenderTask"),
	_logger(logger),
	_sendQ(sendQ),
	_readonly(config.getBool("readonly", true)),
	_disposal(config.getBool("disposal", false))
{
}

GSSenderTask::~GSSenderTask()
{
}


void GSSenderTask::runTask()
{
	while (!isCancelled())
	{
		try
		{
			if (Notification::Ptr nf = _sendQ.waitDequeueNotification(1000))
			{
				AutoPtr<JobNotification> jn = nf.cast<JobNotification>();
				if (!jn) 
				{
					_logger.warning("Unexpected notification type in sendQ");
					continue;
				}

				JobPtr job = jn->job;	
				_logger.information("Sender got job: PCL=[%s], printers=%zu",
					job->pclPath, job->printers.size());
				
				bool allOk = true;
				for (const auto& prn : job->printers) 
				{
					bool ok = sendFile(job->pclPath, prn);
					_logger.information(" -> Printer: %s", prn);
					if (!ok) 
					{
						allOk = false;
						_logger.error("Failed to send [%s] to printer [%s]",
							job->pclPath, prn);
					}
				}
				if (allOk && _disposal) 
				{
					try 
					{
						Poco::File(job->pclPath).remove();
						Poco::File(job->pdfPath).remove();
						_logger.information("Deleted files [%s] and [%s]",
							job->pclPath, job->pdfPath);
					}
					catch (Poco::Exception& ex) 
					{
						_logger.error("Cleanup failed: %s", ex.displayText());
					}
				}
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


bool GSSenderTask::sendFile(const std::string& pclfile, const std::string& printer)
{
	try
	{
		if (!_readonly)
		{
			_logger.information("Sending [%s] to [%s] ...", pclfile, printer);
			FileInputStream fis(pclfile);
			StreamSocket sock;
			Timespan tout = 5000000;
			sock.connect(SocketAddress(printer), tout);
			SocketStream str(sock);
			StreamCopier::copyStream(fis, str);
			if (_logger.trace())
				_logger.trace("Sending [%s] to [%s] succesfully completed.", pclfile, printer);
		}
		else
			_logger.information("READONLY: Would send [%s] to [%s] ...", pclfile, printer);
	}
	catch (Exception& exc)
	{
		_logger.error(exc.displayText());
		return false;
	}
	return true;
}

