
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


namespace Poco
{
	class Logger;
	class NotificationQueue;
}

class PrintNotification : public Poco::Notification
	/// Notification  for printing. This notification carries
	/// PDF file and printer names, as well as the name of the
	/// external program used to send the PDF file to printer
{
public:
	enum Type
	{
		PRINT_EXT_PDF,
		PRINT_DIRECT_PDF,
		PRINT_DIRECT_PCL
	};

	PrintNotification() = delete;
		/// Deleted default constructor.

	PrintNotification(const std::string& printers, const std::string& file, bool disposable, std::vector<std::string> gsArgs);
		/// Creates the PrintNotification.

	~PrintNotification();
		/// Destroys the PrintNotification.

	Type type() const;
		/// Returns the printing type.

	const std::string& printer() const;
		/// Returns the printer name.

	const std::string& file() const;
		/// Returns the file name.

	bool isDisposable() const;
		/// Returns true if file is disposable, false otherwise.
		/// Disposable files are deleted after printing.
		/// An example of a disposable file is the blank charge sheet file.
		/// charge sheets containing data are retained for recording purposes.

    const std::vector<std::string>& gsArgs() const;

private:
	Type _type;
	std::string _printer;
	std::string _file;
	bool        _disposable = false;
	std::vector<std::string> _gsArgs;
};

inline PrintNotification::Type PrintNotification::type() const
{
	return _type;
}

inline bool PrintNotification::isDisposable() const
{
	return _disposable;
}

inline const std::vector<std::string>& PrintNotification::gsArgs() const
{
	return _gsArgs;
}

inline const std::string& PrintNotification::printer() const
{
	return _printer;
}

inline const std::string& PrintNotification::file() const
{
	return _file;
}

#endif
