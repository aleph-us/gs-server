//
// GSHTTPTask.cpp
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


#include "GSHTTPTask.h"
#include "Poco/Net/HTTPServerParams.h"
#include "Poco/Net/HTTPServerRequest.h"
#include "Poco/Net/HTTPServerResponse.h"
#include "Poco/Net/HTTPRequestHandler.h"
#include "Poco/Net/HTTPRequestHandlerFactory.h"
#include "Poco/Net/HTTPMessage.h"
#include "Poco/Util/LayeredConfiguration.h"
#include "Poco/Logger.h"
#include "Poco/String.h"
#include "Poco/Net/HTMLForm.h"
#include "Poco/Net/PartHandler.h"
#include "Poco/File.h"
#include "Poco/Path.h"
#include "Poco/URI.h"
#include <fstream>
#include "Poco/StreamCopier.h"

using namespace Poco;
using namespace Poco::Net;
using namespace Poco::Util;


class GSCmdHandler : public HTTPRequestHandler
{
public:
	GSCmdHandler(Poco::NotificationQueue& nq)
		: _nq(nq) 
	{
	}

	void handleRequest(HTTPServerRequest& req, HTTPServerResponse& resp) override
	{
		try {
			// Checking wether method is POST, if not respond as ERROR
			if (req.getMethod() != HTTPRequest::HTTP_POST) 
			{
				resp.setStatus(HTTPResponse::HTTP_METHOD_NOT_ALLOWED);
				resp.set("Allow", "POST");
				resp.setContentType("text/plain");
				resp.send() << "Method not allowed. Use POST.\n";
				return;
			}

			// Query Parse
			Poco::URI uri(req.getURI());
			Poco::URI::QueryParameters qp = uri.getQueryParameters();

			bool disposable = false; 
			std::string outputFile;                // from sOutputFile=
			std::vector<std::string> printers;     // from print=ip:port/PCL, ip2:port2/PDF
			std::vector<std::string> gsArgs;       // f.e. -q, -dNOPAUSE, -sDEVICE=pxlmono, sOutputFile= ...

			for (auto& kv : qp) 
			{
				const std::string& k = kv.first;
				const std::string& v = kv.second;

				if (Poco::icompare(k, "disp") == 0) 
				{
					if (v == "1")
						disposable = true;
					continue;
				}
				if (Poco::icompare(k, "print") == 0) 
				{
					Poco::StringTokenizer st(v, ",;", Poco::StringTokenizer::TOK_TRIM | Poco::StringTokenizer::TOK_IGNORE_EMPTY);
					for (const auto& s : st) 
						printers.push_back(s);
					continue;
				}
				if (Poco::icompare(k, "sOutputFile") == 0) 
				{
					outputFile = v;
					gsArgs.push_back("-sOutputFile=" + v);
					continue;
				}
				// All the others are GS arg:
				// without values: "-q", "-dNOPAUSE", "-dBATCH", "-dSAFER"
				// with values:  "-sOutputFile=path/file.pdf", "-sDEVICE=pxlmono"
				if (v.empty()) 
					gsArgs.push_back("-" + k);
				else
					gsArgs.push_back("-" + k + "=" + v);
			}

			// 2) (Opcional) receive PDF body and store it on the location -> outputFile
			bool hasBody = (req.getContentLength() != HTTPMessage::UNKNOWN_CONTENT_LENGTH && req.getContentLength() > 0);
			if (hasBody) 
			{
				if (outputFile.empty()) 
				{
					resp.setStatus(HTTPResponse::HTTP_BAD_REQUEST);
					resp.setContentType("text/plain");
					resp.send() << "Missing sOutputFile for body upload\n";
					return;
				}
				Poco::Path out(outputFile);
				Poco::File(out.parent()).createDirectories();
				std::ofstream ofs(out.toString(), std::ios::binary);
				Poco::StreamCopier::copyStream(req.stream(), ofs);
				ofs.close();
			}

			// 4) Enqueue in the print queue
			if (printers.empty()) 
			{
				resp.setStatus(HTTPResponse::HTTP_OK);
				resp.setContentType("text/plain");
				resp.send() << "OK (no printers specified). Args parsed: " << gsArgs.size() << "\n";
				return;
			}

			for (const auto& prn : printers) 
			{
				_nq.enqueueNotification(new PrintNotification(prn, outputFile, disposable, gsArgs));
			}

			// 5) Response to HTTP client
			resp.setStatus(HTTPResponse::HTTP_OK);
			resp.setContentType("text/plain");
			auto& os = resp.send();
			os << "OK enqueued " << printers.size() << " job(s)\n";
			os.flush();
		}
		catch (Poco::Exception& ex) 
		{
			resp.setStatus(HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
			resp.setContentType("text/plain");
			resp.send() << ex.displayText() << "\n";
		}
		catch (std::exception& ex) 
		{
			resp.setStatus(HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
			resp.setContentType("text/plain");
			resp.send() << ex.what() << "\n";
		}
	}

private:
	Poco::NotificationQueue& _nq;
};

class SimpleHandlerFactory : public HTTPRequestHandlerFactory
{
public:
	SimpleHandlerFactory(Poco::NotificationQueue& nq)
		: _nq(nq) 
	{
	}

	HTTPRequestHandler* createRequestHandler(const HTTPServerRequest& req) override
	{
		return new GSCmdHandler(_nq);
	}

private:
	Poco::NotificationQueue& _nq;
};

// ---- GSHTTPTask ----

GSHTTPTask::GSHTTPTask(Configuration& cfg, Poco::NotificationQueue& nq, const std::string& taskName)
	: Poco::Task(taskName)
	, _serverSocket(Poco::Net::SocketAddress(cfg.getString("http.server.address", "0.0.0.0:9980")))
	, _pReqHandlerFactory(new SimpleHandlerFactory(nq))
	, _httpServer(_pReqHandlerFactory, _serverSocket, new HTTPServerParams)
	, _logger(Poco::Logger::get(name()))
{
	_httpServer.start();
	_logger.information("%s created, listening on %s.", name(), _serverSocket.address().toString());
}

GSHTTPTask::~GSHTTPTask()
{
	try 
	{ 
		_httpServer.stop(); 
	}
	catch (...) 
	{ 
		poco_unexpected(); 
	}
}

void GSHTTPTask::runTask()
{
	_logger.information("%s ready.", name());
	_event.wait();
	_logger.warning("%s done.", name());
}

void GSHTTPTask::wakeUp()
{
	stop();
	_event.set();
}

void GSHTTPTask::stop()
{
	if (!_stop)
	{
		_httpServer.stop();
		_stop = true;
		_logger.warning("%s stopping ...", name());
	}
}
