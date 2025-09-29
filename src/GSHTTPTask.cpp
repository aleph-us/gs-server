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
#include "Poco/NullStream.h"
#include "Poco/Net/SocketAddress.h"
#include "Poco/URI.h"
#include "Poco/Path.h"
#include "Poco/File.h"
#include "Poco/StreamCopier.h"
#include "Poco/StringTokenizer.h"

#include <vector>
#include <fstream> 
#include <algorithm>
#include <cctype>

using namespace Poco;
using namespace Poco::Net;
using namespace Poco::Util;


class GSCmdHandler : public HTTPRequestHandler
{
public:
	GSCmdHandler(Poco::NotificationQueue& ConvQ, const std::string& filesDir)
		: _convQ(ConvQ), _dir(filesDir), _logger(Poco::Logger::get("GSHTTP"))
	{
	}

	void handleRequest(HTTPServerRequest& req, HTTPServerResponse& resp) override
	{
		try {
			// Checking wether method is POST, if not respond as ERROR
			if (req.getMethod() != HTTPRequest::HTTP_POST) 
			{
				sendBadRequest(req, resp, HTTPResponse::HTTP_BAD_REQUEST, "Method not allowed. Use POST.");
				return;
			}

			// Query Parse
			Poco::URI uri(req.getURI());
			Poco::URI::QueryParameters qp = uri.getQueryParameters();

			auto job = std::make_shared<Job>();
			
			Poco::Path inputPath;
			Poco::Path outputPath;
			std::string ext;						// pcl, jpg, png, ...
			std::string device;						// pxlmono, pxlcolor, png16m, jpeg, ...
			std::string baseName;
			std::vector<std::string> printers;		// from print=ip:port, ip2:port, ...
			std::vector<std::string> gsArgs;		// f.e. -q, -dNOPAUSE, -sDEVICE=pxlmono, sOutputFile= ...

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
				if(Poco::icompare(k, "sDEVICE") == 0)
				{
					device = v;
					continue;
				}
				if (Poco::icompare(k, "sOutputFile") == 0) 
				{
					baseName = Poco::Path(v).getFileName();
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

			if(device.empty())
			{
				sendBadRequest(req, resp, HTTPResponse::HTTP_BAD_REQUEST, "Missing device name");
				return;
			}

			if(baseName.empty())
			{
				sendBadRequest(req, resp, HTTPResponse::HTTP_BAD_REQUEST, "Missing file name");
				return;
			}

			// inputPath
			inputPath = Poco::Path(_dir, baseName); 
			inputPath.setExtension("pdf");
			job->inputPath = inputPath.toString();

			// extension determination
			ext = mapDevice(device);
			if(ext == "none")
			{
				sendBadRequest(req, resp, HTTPResponse::HTTP_BAD_REQUEST, "Extenstion not supported");
				return;
			}

			// outputPath
			outputPath = Poco::Path(_dir, baseName); 
			outputPath.setExtension(ext);
			job->outputPath = outputPath.toString();

			// finish gsArgs vector - path parameters required to be at the end
			gsArgs.push_back(std::string("-sDEVICE=") + device);
			gsArgs.push_back(std::string("-sOutputFile=") + outputPath.toString());
			gsArgs.push_back(inputPath.toString());

			std::transform(ext.begin(), ext.end(), ext.begin(), ::toupper);
			job->formatLabel = ext;  // PCL, PDF, JPG, ...

			// 2) (Opcional) receive PDF body and store it on the location -> outputFile
			bool hasBody = (req.getContentLength() != HTTPMessage::UNKNOWN_CONTENT_LENGTH && req.getContentLength() > 0);
			if (!hasBody) 
			{
				sendBadRequest(req, resp, HTTPResponse::HTTP_BAD_REQUEST, "Missing PDF body");
				return;
			}
			Poco::File(inputPath.parent()).createDirectories();
			{
				std::ofstream ofs(inputPath.toString(), std::ios::binary);
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
			sendBadRequest(req, resp, HTTPResponse::HTTP_INTERNAL_SERVER_ERROR, ex.displayText());
		}
		catch (std::exception& ex) 
		{
			sendBadRequest(req, resp, HTTPResponse::HTTP_INTERNAL_SERVER_ERROR, ex.what());
		}
	}

	std::string mapDevice(const std::string d);

private:
	void sendBadRequest(Poco::Net::HTTPServerRequest& req,
						Poco::Net::HTTPServerResponse& resp,
						Poco::Net::HTTPResponse::HTTPStatus st, 
						const std::string& message)
	{
		try 
		{
			const bool chunked = Poco::icompare(req.getTransferEncoding(),"chunked")==0;
			if (chunked || (req.getContentLength()!=Poco::Net::HTTPMessage::UNKNOWN_CONTENT_LENGTH
					&& req.getContentLength()>0)) 
			{
				Poco::NullOutputStream nos;
				Poco::StreamCopier::copyStream(req.stream(), nos); // drain
			}
		} 
		catch (...) 
		{
		}

		resp.setStatus(st);
		resp.setContentType("text/plain");
		auto& os = resp.send();
		os << message << "\n";
		os.flush();
	}
	Poco::NotificationQueue& _convQ;
	std::string _dir;
	Poco::Logger& _logger;
};

inline std::string GSCmdHandler::mapDevice(const std::string d)
{
	// PCL
	if (d == "pxlmono" || d == "pxlcolor" || d == "pcl3" || d == "pclm" || d == "pclm8")
		return "pcl";

	// image
	if (d == "png16m" || d == "png16" || d == "png48" || d == "pngalpha" || d == "pnggray" || d == "pngmono")
		return "png";

	if (d == "jpeg" || d == "jpeggray" || d == "jpegcmyk")
		return "jpg";

	return "none";
}

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
