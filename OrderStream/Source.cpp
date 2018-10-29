//==============================================================
// Copyright Bruno Kieba - 2018
// 
// Updated main with main(argc, argv)
// Added XML file to select source feed files
// Added thread synchronization
//==============================================================
#include "pch.h"
#include <iostream>
#include <vector>
#include <fstream>
#include <boost/regex.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/range/irange.hpp>
#include <boost/format.hpp>
#include <boost/thread.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/foreach.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>

using namespace std;
using namespace boost;

#include "OrderStream.hpp"
#include "TradePlot.hpp"

const string szSessionFeed("task1.sessionfeed.");

int main(int argc, char *argv[])
{
	// Check that we have the expected argument in input command. Example command expected is: "OrderStream feed1"
	if (argc < 2) {
		std::cout << "Not enough or invalid arguments, please try again. Usage is prog.exe -in <infile>\n";
		Sleep(2000);
		exit(0);
	}

	// Setup the tree to parse the xml file
	using namespace boost::property_tree::xml_parser;
	using boost::property_tree::ptree;
	ptree pt;

	string szXml(argv[1]);
	assert(szXml.empty() == false);
	read_xml(szXml, pt, trim_whitespace | no_comments);

	// Extract the source feeds to evaluate
	string szFeed = pt.get<string>(szSessionFeed + "sourcefeed");
	int nMaxBookLevels = pt.get<int>(szSessionFeed + "maxBookLevels", 5);
	int nMaxBookDepth  = pt.get<int>(szSessionFeed + "maxBookDepth", 5);

	string szSelCsv = szSessionFeed + szFeed + ".csv";
	string szSelLog = szSessionFeed + szFeed + ".log";
	string szCsvFile = pt.get<string>(szSelCsv, "");
	string szLogFile = pt.get<string>(szSelLog, "");

	assert(szCsvFile.empty() == false);
	assert(szLogFile.empty() == false);

	// Create both source feeds to compare
	OBStreamCSV obsCsv(szCsvFile, nMaxBookLevels, nMaxBookDepth);
	OBStreamLog obsLog(szLogFile, nMaxBookLevels, nMaxBookDepth);

	// Evaluate both files concurrently and wait for both threds to complete
	boost::thread_group ths;
	ths.create_thread(boost::bind(&OBStreamCSV::processFeeds, boost::ref(obsCsv)));
	ths.create_thread(boost::bind(&OBStreamLog::processFeeds, boost::ref(obsLog)));
	ths.join_all();

	try {
		// Check whether exception was caught while processing feeds
		obsCsv.CheckNotifyException();
		obsLog.CheckNotifyException();

		// There was no exception. Plot the result to html and console optionally
		TradePlot tp(szXml, obsCsv, obsLog);
	}
	catch (const TracedException& te) {
		te.coutException();
		cout << "Plot of source feeds difference was not generated." << endl;
	}

	cout << " Thread Id Main:" << boost::this_thread::get_id() << endl;
	// Exit normally
	return (0);
}
