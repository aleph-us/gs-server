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
#include "GSNotification.h"


#include "Poco/NotificationQueue.h"
#include "Poco/Net/HTTPServerParams.h"
#include "Poco/Net/HTTPServerRequest.h"
#include "Poco/Net/HTTPServerResponse.h"
#include "Poco/Net/HTTPRequestHandler.h"
#include "Poco/Net/HTTPMessage.h"
#include "Poco/Net/SocketAddress.h"
#include "Poco/URI.h"
#include "Poco/Path.h"
#include "Poco/File.h"
#include "Poco/StreamCopier.h"
#include "Poco/StringTokenizer.h"

#include <vector>
#include <fstream> 

using namespace Poco;
using namespace Poco::Net;
using namespace Poco::Util;


class GSCmdHandler : public HTTPRequestHandler
{
public:
	GSCmdHandler(Poco::NotificationQueue& ConvQ, const std::string& filesDir)
		: _convQ(ConvQ), _dir(filesDir)
	{
	}

	void handleRequest(HTTPServerRequest& req, HTTPServerResponse& resp) override
	{
		try {
			// Checking wether method is POST, if not respond as ERROR
			if (req.getMethod() != HTTPRequest::HTTP_POST) 
			{
				sendBadRequest(resp, "Method not allowed. Use POST.");
				return;
			}

			// Query Parse
			Poco::URI uri(req.getURI());
			Poco::URI::QueryParameters qp = uri.getQueryParameters();

			auto job = std::make_shared<Job>();
			std::string baseName;
			Poco::Path pdfPath;
			Poco::Path pclPath;
			std::vector<std::string> printers;     // from print=ip:port, ip2:port
			std::vector<std::string> gsArgs;       // f.e. -q, -dNOPAUSE, -sDEVICE=pxlmono, sOutputFile= ...

			for (auto& kv : qp) 
			{
				const std::string& k = kv.first;
				const std::string& v = kv.second;

				if (Poco::icompare(k, "print") == 0) 
				{
					Poco::StringTokenizer st(v, ",;", Poco::StringTokenizer::TOK_TRIM | Poco::StringTokenizer::TOK_IGNORE_EMPTY);
					for (const auto& s : st) 
						printers.push_back(s);
					continue;
				}
				if (Poco::icompare(k, "sOutputFile") == 0) 
				{
					baseName = v;
					if(baseName.empty())
					{
						sendBadRequest(resp, "Missing file name");
						return;
					}
					pdfPath = Poco::Path(_dir, baseName); 
					pdfPath.setExtension("pdf");
					job->pdfPath = pdfPath.toString();

					pclPath = Poco::Path(_dir, baseName); 
					pclPath.setExtension("pcl");
					job->pclPath = pclPath.toString();

					gsArgs.push_back(std::string("-sOutputFile=") + pclPath.toString());
					gsArgs.push_back(pdfPath.toString());
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

			if (gsArgs.empty()) 
			{
				sendBadRequest(resp, "Missing Ghostscript arguments");
				return;
			}

			// 2) (Opcional) receive PDF body and store it on the location -> outputFile
			bool hasBody = (req.getContentLength() != HTTPMessage::UNKNOWN_CONTENT_LENGTH && req.getContentLength() > 0);
			if (!hasBody) 
			{
				sendBadRequest(resp, "Missing PDF body");
				return;
			}
			Poco::File(pdfPath.parent()).createDirectories();
			{
				std::ofstream ofs(pdfPath.toString(), std::ios::binary);
				Poco::StreamCopier::copyStream(req.stream(), ofs);
			}

			// 4) Enqueue in the print queue
			job->gsArgs = std::move(gsArgs);
			job->printers = std::move(printers);
			_convQ.enqueueNotification(new JobNotification(job));

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
	void sendBadRequest(HTTPServerResponse& resp, const std::string& message)
	{
		resp.setStatus(HTTPResponse::HTTP_BAD_REQUEST);
		resp.setContentType("text/plain");
		resp.send() << message << "\n";
	}
	Poco::NotificationQueue& _convQ;
	std::string _dir;
};

class SimpleHandlerFactory : public HTTPRequestHandlerFactory
{

public:
	using Configuration = Poco::Util::LayeredConfiguration;
	
	SimpleHandlerFactory(Poco::NotificationQueue& convQ, Configuration& cfg)
		: _convQ(convQ), _filesDir(cfg.getString("filesDir"))
	{
	}

	HTTPRequestHandler* createRequestHandler(const HTTPServerRequest& req) override
	{
		return new GSCmdHandler(_convQ, _filesDir);
	}

private:
	Poco::NotificationQueue& _convQ;
	std::string _filesDir;
};

// ---- GSHTTPTask ----

GSHTTPTask::GSHTTPTask(Configuration& cfg, Poco::NotificationQueue& convQ, const std::string& taskName)
	: Poco::Task(taskName)
	, _serverSocket(Poco::Net::SocketAddress(cfg.getString("http.server.address", "0.0.0.0:9980")))
	, _pReqHandlerFactory(new SimpleHandlerFactory(convQ, cfg))
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
