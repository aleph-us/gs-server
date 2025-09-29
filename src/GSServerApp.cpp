//
//GSServerApp.cpp
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// Copyright (C) 2021-2025 Aleph ONE Software Engineering LLC
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
#include "Poco/AutoPtr.h"
#include "Poco/Timespan.h"
#include "Poco/AsyncNotificationCenter.h"
#include "Poco/LoggingFactory.h"
#include "Poco/LoggingRegistry.h"
#include "Poco/Data/SQLChannel.h"
#include "Poco/Util/Option.h"
#include "Poco/Util/OptionSet.h"
#include "Poco/Util/HelpFormatter.h"
#include "Poco/TaskManager.h"
#include "Poco/Data/ODBC/Connector.h"
#include "Poco/Util/ServerApplication.h"
#include <iostream>
#include "GSHTTPTask.h"
#include "GSWorkerTask.h"
#include "GSSenderTask.h"


using namespace Poco;
using namespace Poco::Util;
using namespace Poco::Data;


class GSServerApp: public Poco::Util::ServerApplication
	/// Main application class.
	/// Can run as console application, or background service.
{
public:
	GSServerApp()
	{
	}


	~GSServerApp()
	{
	}

protected:
	void initialize(Application& self)
	{
		Poco::Data::ODBC::Connector::registerConnector();
		SQLChannel::registerChannel();

		if (!_configLoaded) loadConfiguration();

		ServerApplication::initialize(self);

		if (!_helpRequested)
		{
			logger().information(R"(

		____   __ GS_______
      / ___/| / / ___/ ___/
     (___ ) |/ / /__(___ )
    /____/|___/____/____/ TM
                ex devs

    Copyright Ⓒ 2021-2025 Aleph ONE Software Engineering LLC.
	Licensed under AGPL-3.0-or-later.
)");
		logger().information("System information: %s (%s) on %s, %u CPU core(s).\n",
			Poco::Environment::osDisplayName(),
			Poco::Environment::osVersion(),
			Poco::Environment::osArchitecture(),
			Poco::Environment::processorCount());
		}
	}


	void uninitialize()
	{
		if (!_helpRequested)
		{
			logger().information("shutting down");
			ServerApplication::uninitialize();
			Poco::Data::ODBC::Connector::unregisterConnector();
		}
	}


	void defineOptions(OptionSet& options)
	{
		ServerApplication::defineOptions(options);

		options.addOption(
			Option("help", "h", "display help information on command line arguments")
			.required(false)
			.repeatable(false)
			.callback(OptionCallback<GSServerApp>(this, &GSServerApp::handleHelp)));

			options.addOption(
				Option("config-file", "c", "Load configuration data from a file.")
					.required(false)
					.repeatable(true)
					.argument("file")
					.callback(OptionCallback<GSServerApp>(this, &GSServerApp::handleConfig)));
	}


	void handleHelp(const std::string& name, const std::string& value)
	{
		_helpRequested = true;
		displayHelp();
		stopOptionsProcessing();
	}


	void handleConfig(const std::string& name, const std::string& value)
	{
		Path dir(value);
		dir.makeParent().makeDirectory().makeAbsolute();
		loadConfiguration(value);
		config().setString("application.configDir", dir.toString());
		_configLoaded = true;
	}


	void displayHelp()
	{
		HelpFormatter helpFormatter(options());
		helpFormatter.setCommand(commandName());
		helpFormatter.setUsage("OPTIONS");
		helpFormatter.setHeader("GS service.");
		helpFormatter.format(std::cout);
	}


	int main(const ArgVec& args)
	{
		if (!_helpRequested)
		{
			NotificationQueue convQ;
			NotificationQueue sendQ;
			TaskManager tm;

			GSHTTPTask* pGSHTTP = nullptr;
			GSWorkerTask* pWorkerTask = nullptr;
			GSSenderTask* pSenderTask = nullptr;


			try
			{
				pGSHTTP = new GSHTTPTask(config(), convQ);
				tm.start(pGSHTTP);

				pWorkerTask = new GSWorkerTask(convQ, sendQ, logger(), config());
				tm.start(pWorkerTask);

				pSenderTask = new GSSenderTask(sendQ, logger(), config());
				tm.start(pSenderTask);

				std::string svcName = config().getString("service.name");
				logger().information("Service %s running ...", svcName);
				waitForTerminationRequest();
				logger().information("Service %s terminated.", svcName);
			}
			catch (Poco::Exception& ex)
			{
				logger().error(ex.displayText());
			}
			catch (std::exception& ex)
			{
				logger().error(ex.what());
			}
			catch (...)
			{
				logger().error("Unknown error occurred during startup. Exiting.");
			}

			tm.cancelAll();

			if (pGSHTTP)
			{
				pGSHTTP->wakeUp();
				pGSHTTP->stop();
			} 

			tm.joinAll();
		}

		return Application::EXIT_OK;
	}

private:
	bool _helpRequested = false;
	bool _configLoaded = false;
	Poco::AutoPtr<Poco::Data::SQLChannel> _pChannel;
};


POCO_SERVER_MAIN(GSServerApp)
