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


#include "GSSenderTask.h"
#include "GSNotification.h"

#include "Poco/Notification.h"
#include "Poco/AutoPtr.h"
#include "Poco/Logger.h"
#include "Poco/Thread.h"
#include "Poco/Timespan.h"
#include "Poco/File.h"
#include "Poco/FileStream.h"
#include "Poco/StreamCopier.h"

#include "Poco/Net/StreamSocket.h"
#include "Poco/Net/SocketStream.h"
#include "Poco/Net/SocketAddress.h"

#include "Poco/String.h"
#include "Poco/StringTokenizer.h"
#include <vector>
#include <memory>
#include <atomic>


using namespace Poco;
using namespace Poco::Net;
using namespace Poco::Util;


//
// Send Runnable
//

class SendRunnable : public Poco::Runnable {
public:
	SendRunnable(Logger& logger, const std::string& file, const std::string& printer, bool readonly)
		: _logger(logger), _file(file), _printer(printer), _readonly(readonly) 
	{
	}

	void run() override 
	{
		if (!Poco::File(_file).exists()) 
		{
			_logger.error("File [%s] does not exist.", _file);
			_ok = false;
			return;
		}

		try 
		{
			if (_readonly) 
			{ 
				_ok = true; 
				_logger.information("READONLY: Would send [%s] to [%s] ...", _file, _printer);
				return; 
			}

			_logger.information("Sending [%s] to [%s] ...", _file, _printer);
			Poco::FileInputStream fis(_file);
			Poco::Net::StreamSocket sock;
			sock.connect(Poco::Net::SocketAddress(_printer), Poco::Timespan(5,0));
			sock.setSendTimeout(Poco::Timespan(30,0));
			sock.setReceiveTimeout(Poco::Timespan(30,0));
			Poco::Net::SocketStream ss(sock);
			Poco::StreamCopier::copyStream(fis, ss);
			_logger.information("Sending [%s] to [%s] succesfully completed.", _file, _printer);
			_ok = true;
		} 
		catch (...) 
		{
			_ok = false;
		}
	}

	bool ok() const 
	{ 
		return _ok; 
	}

private:
	Poco::Logger& _logger;
	std::string _file;
	std::string _printer;
	bool _readonly{true};
	std::atomic<bool> _ok{false};
};



GSSenderTask::GSSenderTask(Poco::NotificationQueue& sendQ, Logger& logger, LayeredConfiguration& config) :
	Task("GSSenderTask"),
	_logger(logger),
	_sendQ(sendQ),
	_readonly(config.getBool("readonly", true)),
	_disposal(config.getBool("disposal", false))
{
	if (_disposal)
		_logger.warning("Files will be deleted after successful print.");
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
				_logger.information("Sender got job: %s=[%s], printers=%z",
					job->formatLabel, job->outputPath, job->printers.size());
				

				std::vector<std::unique_ptr<SendRunnable>> runners;
				std::vector<std::unique_ptr<Poco::Thread>> threads;
				runners.reserve(job->printers.size());
				threads.reserve(job->printers.size());

				for (size_t i = 0; i < job->printers.size(); ++i) 
				{
					runners.emplace_back(new SendRunnable(_logger, job->outputPath, job->printers[i], _readonly));
					threads.emplace_back(std::make_unique<Poco::Thread>());
					threads.back()->start(*runners.back());
					_logger.information("Printing Job started to %s", job->printers[i]);
				}
				
				bool allOk = true;
				for (size_t i = 0; i < threads.size(); ++i) 
				{
					threads[i]->join();
					if (!runners[i]->ok()) 
					{
						_logger.error("Failed sending to %s", job->printers[i]);
						allOk = false;
					}
				}

				// upon successfully printing, the files will be deleted 
				// once disposal is true in properties file
				if (allOk && _disposal) 
				{
					try 
					{

						Poco::File(job->outputPath).remove();
						Poco::File(job->inputPath).remove();
						_logger.information("Deleted files [%s] and [%s]",
							job->outputPath, job->inputPath);
					}
					catch (Poco::FileNotFoundException& ex)
					{
						_logger.error("File not found during cleanup: %s", ex.displayText());
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

