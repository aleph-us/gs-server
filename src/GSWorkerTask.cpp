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


using namespace Poco;
using namespace Poco::Net;
using namespace Poco::Util;


class PrintRunnable : public Runnable
{
public:
	int returnCode() const
	{
		return _rc;
	}

	virtual Process::PID processID() const
	{
		return 0;
	}

	bool isDone() const
	{
		return _done;
	}

protected:
	int _rc = 0;
	bool _done = false;
	Logger& _logger = Logger::get("PrintRunnable");
};


class PrintDirect : public PrintRunnable
{
public:
	PrintDirect() = delete;

	PrintDirect(const std::string& printer, const std::string& file, PrintNotification::Type type,  std::vector<std::string> gsArgs) :
		_printer(printer),
		_file(file),
		_type(type),
		_gsArgs(gsArgs),
		_readonly(Application::instance().config().getBool("readonly", false))
	{
	}

	void run()
	{
		try
		{
			bool success = false;
			if (_type == PrintNotification::PRINT_DIRECT_PCL)
			{
				success = sendPCL();
			}
			else if (_type == PrintNotification::PRINT_DIRECT_PDF)
			{
				success = sendFile(_file, _printer);
			}
			if (!success)
				_logger.error("Failed to send file [%s] to [%s]", _file, _printer);
		}
		catch (Poco::Exception& ex)
		{
			_logger.error(ex.displayText());
		}
		catch (std::exception& ex)
		{
			_logger.error(ex.what());
		}
		_done = true;
	}

private:
	bool sendFile(const std::string& file, const std::string& printer)
	{
		try
		{
			if (!_readonly)
			{
				_logger.information("Sending [%s] to [%s] ...", file, printer);
				FileInputStream fis(file);
				StreamSocket sock;
				Timespan tout = 5000000;
				sock.connect(SocketAddress(printer), tout);
				SocketStream str(sock);
				StreamCopier::copyStream(fis, str);
				if (_logger.trace())
					_logger.trace("Sending [%s] to [%s] succesfully completed.", file, printer);
			}
			else
				_logger.information("READONLY: Would send [%s] to [%s] ...", file, printer);
		}
		catch (Exception& exc)
		{
			_logger.error(exc.displayText());
			return false;
		}
		return true;
	}

	bool sendPCL()
	{
		Path p(_file);
		p.setExtension("pcl");
		const std::string PCL_FILE = p.toString();

		const std::string PCL_FILE_CREATE = "-sOutputFile=" + PCL_FILE;

		//_gsArgs
		void* minst = NULL;
		int code, code1;
		const char* gsargv[8];
		int gsargc;
		gsargv[0] = "";
		gsargv[1] = "-q";
		gsargv[2] = "-dNOPAUSE";
		gsargv[3] = "-dBATCH";
		gsargv[4] = "-dSAFER";
		gsargv[5] = "-sDEVICE=pxlmono";
		gsargv[6] = PCL_FILE_CREATE.c_str();
		gsargv[7] = _file.c_str();
		gsargc = 8;

		_logger.information("Converting [%s] to [%s] ...", _file, PCL_FILE);
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
			code = gsapi_init_with_args(minst, gsargc, const_cast<char**>(gsargv));
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

		return sendFile(PCL_FILE, _printer);
	}

	std::string _printer;
	std::string _file;
	PrintNotification::Type _type;
	std::vector<std::string> _gsArgs;
	bool _readonly;
};


GSWorkerTask::GSWorkerTask(NotificationQueue& printNQ, Logger& logger, LayeredConfiguration& config) :
	Task("ChargeSheetPrintTask"),
	_logger(logger),
	_printNQ(printNQ),
	_printApp(config.getString("printApp", "")),
	_config(config)
{
}

GSWorkerTask::~GSWorkerTask()
{
}


void GSWorkerTask::runTask()
{
	bool printing = false;
	while (!isCancelled())
	{
		printing = false;
		try
		{
			if (!_printNQ.empty())
			{
				printing = true;
				Poco::Notification::Ptr pNF = _printNQ.dequeueNotification();
				if (pNF)
				{
					auto pPrint = pNF.cast<PrintNotification>();
					std::string printer(1, '"');
					printer.append(/*_config.getString(*/pPrint->printer()/*, "")*/).append(1, '"');
					std::string file(1, '"');
					file.append(pPrint->file()).append(1, '"');
					_logger.information("Print request:\nPrinter: [%s] => [%s]\nFile: [%s]",
						pPrint->printer(), printer, file);
					if (!printer.empty())
					{
						if ((pPrint->type() == PrintNotification::PRINT_DIRECT_PDF) || (pPrint->type() == PrintNotification::PRINT_DIRECT_PCL))
						{
							PrintDirect printDirect(pPrint->printer(), pPrint->file(), pPrint->type(), pPrint->gsArgs());
							runPrint(printDirect);
						}
						else
						{
							_logger.debug("Incorrect type, should be PRINT_DIRECT_PDF or PRINT_DIRECT_PCL");
							continue;
						}

						if (pPrint->isDisposable()) // disposable file (blank sheet), delete
						{
							Path p(pPrint->file());
							File pdf(p);
							if (pdf.exists())
							{
								pdf.remove();
								_logger.debug("Deleted [%s].", p.toString());
							}
							p.setExtension("xml");
							File xml(p);
							if (xml.exists())
							{
								xml.remove();
								_logger.debug("Deleted [%s].", p.toString());
							}
						}
					}
					else _logger.error("Printer not specified.");
					_logger.information("Print request succesfully completed.");
				}
			}
			//else _logger.trace("No print data.");
			if (!printing) Thread::sleep(1000);
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

void GSWorkerTask::runPrint(PrintRunnable& print)
{
	Poco::Thread t;
	t.start(print);
	Stopwatch sw; sw.start();
	while (true)
	{
		if (print.isDone())
		{
			_logger.information("Printing process completed succesfully.");
			t.join();
			break;
		}
		if (sw.elapsedSeconds() >= 60)
		{
			Process::PID pid = print.processID();
			if (pid > 0)
			{
				_logger.error("Printing process PID=%d timed out, return code=%d",
					static_cast<int>(pid), print.returnCode());
				Process::kill(pid);
			}
			t.join();
			break;
		}
	}
}
