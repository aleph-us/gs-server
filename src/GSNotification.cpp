//
// GSNotification.cpp
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


using namespace Poco;
using namespace Poco::Util;


PrintNotification::PrintNotification(const std::string& printer, const std::string& file, bool disposable, std::vector<std::string> gsArgs):
	_printer(printer),
	_file(file),
	_disposable(disposable),
	_gsArgs(gsArgs)
{
	int opts = StringTokenizer::TOK_TRIM;
	StringTokenizer st(_printer, "/", opts);
	if (st.count() > 0) _printer = st[0];
	if (_printer.empty())
		throw Poco::InvalidArgumentException("Printer must be specified.");
	if (st.count() > 1)
	{
		if (st[1] == "PCL")
		{
			_type = PRINT_DIRECT_PCL;
			return;
		}
		else if (st[1] == "PDF")
		{
			_type = PRINT_DIRECT_PDF;
			return;
		}
	}
	_type = PRINT_DIRECT_PCL;
}


PrintNotification::~PrintNotification()
{
}

