//==============================================================
// Copyright Bruno Kieba - 2018
// 
// Plot Updated main with main(argc, argv)
// Added XML file to select source feed files
// Added thread synchronization
//==============================================================
#include "pch.h"
#include <iostream>
#include <map>
#include <vector>
#include <set>
#include <fstream>
#include <boost/regex.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/foreach.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/range/adaptors.hpp>
#include <boost/range/adaptor/reversed.hpp>
#include <boost/range/irange.hpp>
#include <boost/range/numeric.hpp>
#include <boost/multiprecision/cpp_dec_float.hpp>
#include <boost/format.hpp>
#include <boost/shared_ptr.hpp>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>

using namespace std;
using namespace boost;

#include "OrderBook.hpp"
#include "OrderStream.hpp"
#include "TradePlot.hpp"

using boost::lexical_cast;
using boost::bad_lexical_cast;

const string szTradePlot("task1.tradeplot.");

TradePlot::TradePlot(const string& szXml, OBStreamCSV& obsCsv, OBStreamLog& obsLog) {

	// Mke sure there is data to work with
	m_pCsvBook = obsCsv.getOrderBook();
	m_pLogBook = obsLog.getOrderBook();

	assert(m_pCsvBook);
	assert(m_pLogBook);

	// Setup the tree to parse the xml file
	using namespace boost::property_tree::xml_parser;
	using boost::property_tree::ptree;
	ptree pt;
	read_xml(szXml, pt, trim_whitespace | no_comments);

	m_bConsoleOut	= pt.get<bool>(szTradePlot   + "console.output", false);
	m_szConsoleLog	= pt.get<string>(szTradePlot + "console.diff", "feeddiff.log");

	// Console out if needed
	if (m_bConsoleOut) {
		consoleOut(obsCsv, obsLog);
	}

	InjectParams ijParams;
	ijParams.szInstrCsv = m_pCsvBook->szInstrument;
	ijParams.szInstrLog = m_pLogBook->szInstrument;
	ijParams.szHtml = pt.get<string>(szTradePlot + "file", "");

	// Plot the bid ask percentage variation from the CSV feed
	ijParams.szHeader = "";
	ijParams.szAny = "CSV";
	ijParams.szMarkerBegin = pt.get<string>(szTradePlot + "markers.begin_csv_variation", "begin_h2_p_csv");
	ijParams.szMarkerEnd = pt.get<string>(szTradePlot + "markers.markers.end_csv_variation", "end_h2_p_csv");
	plotVariation(m_pCsvBook->szBidVariation, m_pCsvBook->szAskVariation, ijParams);

	// Plot the bid ask percentage variation from the LOG feed
	ijParams.szAny = "LOG";
	ijParams.szMarkerBegin = pt.get<string>(szTradePlot + "markers.begin_log_variation", "begin_h2_p_log");
	ijParams.szMarkerEnd = pt.get<string>(szTradePlot + "markers.markers.end_log_variation", "end_h2_p_log");
	plotVariation(m_pLogBook->szBidVariation, m_pLogBook->szAskVariation, ijParams);

	// Plot the CSV book order of bid ask offers as a stacked bar chart
	ijParams.szHeader = "";
	ijParams.nOfferDepth	= m_pCsvBook->nBookDepth;
	ijParams.szMarkerBegin	= pt.get<string>(szTradePlot + "markers.begin_stackbar_csv_data_array", "begin stackbar csv data array");
	ijParams.szMarkerEnd	= pt.get<string>(szTradePlot + "markers.end_stackbar_csv_data_array", "end stackbar csv order data array");
	plotOrderBook(m_pCsvBook->priceOffers.bidOffers, m_pCsvBook->priceOffers.askOffers, ijParams);

	// Plot the LOG book order of bid ask offers as a stacked bar chart
	ijParams.szHeader = "";
	ijParams.nOfferDepth = m_pCsvBook->nBookDepth;
	ijParams.szMarkerBegin = pt.get<string>(szTradePlot + "markers.begin_stackbar_log_data_array", "begin stackbar log data array");
	ijParams.szMarkerEnd = pt.get<string>(szTradePlot + "markers.end_stackbar_log_data_array", "end stackbar log data array");
	plotOrderBook(m_pLogBook->priceOffers.bidOffers, m_pLogBook->priceOffers.askOffers, ijParams);

	// Plot the order difference
	ijParams.szHeader = "";
	ijParams.szMarkerBegin = pt.get<string>(szTradePlot + "markers.begin_order_diff_data_array", "begin order diff data array");
	ijParams.szMarkerEnd = pt.get<string>(szTradePlot + "markers.end_order_diff_data_array", "end order diff data array");
	plotOrderDiff(m_pCsvBook->priceOffers.bidOffers, m_pLogBook->priceOffers.bidOffers, m_pCsvBook->priceOffers.askOffers, m_pLogBook->priceOffers.askOffers, ijParams);

	// Plot the spread
	ijParams.szHeader = "";
	ijParams.szMarkerBegin = pt.get<string>(szTradePlot + "markers.begin_spread_data_array", "begin spread data array");
	ijParams.szMarkerEnd = pt.get<string>(szTradePlot + "markers.end_spread_data_array", "end spread data array");
	plotSpread(m_pCsvBook->vSpread, m_pLogBook->vSpread, ijParams);

	// Plot the price variation
	ijParams.szHeader = "Bid Ask Price Percentage";
	ijParams.szMarkerBegin = pt.get<string>(szTradePlot + "markers.begin_pie_diff_data_array", "begin pie diff data array");
	ijParams.szMarkerEnd = pt.get<string>(szTradePlot + "markers.end_pie_diff_data_array", "end pie diff data array");
	plotPriceDiff(m_pCsvBook->vBidAsk, m_pLogBook->vBidAsk, ijParams);

	// Plot the last order feed
	ijParams.szHeader = "Size";
	ijParams.szMarkerBegin	= pt.get<string>(szTradePlot + "markers.begin_bar_size_data_array", "begin bar size data array");
	ijParams.szMarkerEnd	= pt.get<string>(szTradePlot + "markers.end_bar_size_data_array", "end bar size data array");
	plotWall(m_pCsvBook->lastOffer.mapBidSize, m_pCsvBook->lastOffer.mapAskSize, m_pLogBook->lastOffer.mapBidSize, m_pLogBook->lastOffer.mapAskSize, ijParams);
}

void TradePlot::plotVariation(const string& szBid, const string& szAsk, InjectParams& ijParams) {

	vector<string> vArray(1);
	stringstream ssCsv, ssLog;
	ssCsv << "\t<h2 id = '" << ijParams.szMarkerBegin << "'>";
	ssCsv << ijParams.szAny << " Feed Variation for " << ijParams.szInstrCsv << ": ";
	ssCsv << "Bid = " << szBid << "%  Ask = " << szAsk << "%";
	vArray.push_back(ssCsv.str());
	injectHtml(ijParams, vArray);
}

void TradePlot::plotOrderBook(mapOffers& moBid, mapOffers& moAsk, InjectParams& ijParams) {

	// Create the bid and ask columns templates
	string szTemplate("\t\t\t['a',b,c,d,e,f,g,h,i,j,k],");
	string szLab("a");
	string szBid1("b"), szBid2("c"), szBid3("d"), szBid4("e"), szBid5("f");
	string szAsk1("g"), szAsk2("h"), szAsk3("i"), szAsk4("j"), szAsk5("k");

	int maxBidRows = moBid.size();
	int maxAskRows = moAsk.size();

	int maxRows = maxBidRows + maxAskRows;
	assert(maxRows > 0);

	vector<string> vArray(maxRows, szTemplate);
	vArray.at(maxRows - 1).pop_back();

	// Build all the bid columns
	for (mapOffers::reverse_iterator rit = moBid.rbegin(); rit != moBid.rend(); ++rit) {

		// Keep index of the price offer
		int iPair = std::distance(moBid.rbegin(), rit);
		
		// Inject the price in the first column
		vArray.at(iPair).replace(vArray.at(iPair).find_first_of('a'), 1, boost::lexical_cast<string>(rit->first));

		char chBidCol = 'b';

		// Inject the sizes in the bid columns within maximum depth
		for (vSizeRow::iterator sit = rit->second.begin(); sit != rit->second.end(); ++sit) {

			int iSize = std::distance(rit->second.begin(), sit);
			if (iSize >= ijParams.nOfferDepth)
				break;
			vArray.at(iPair).replace(vArray.at(iPair).find_first_of(chBidCol), 1, boost::lexical_cast<string>(sit->first));

			// Move to next bid column
			++chBidCol;
		}
	}

	// Build all the ask columns
	for (mapOffers::iterator it = moAsk.begin(); it != moAsk.end(); ++it) {

		// Keep index of the price offer
		int iPair = std::distance(moAsk.begin(), it) + maxBidRows;

		// Inject the price in the first column
		vArray.at(iPair).replace(vArray.at(iPair).find_first_of('a'), 1, boost::lexical_cast<string>(it->first));

		char chBidCol = 'g';

		// Inject the sizes in the bid columns within maximum depth
		for (vSizeRow::iterator sit = it->second.begin(); sit != it->second.end(); ++sit) {

			int iSize = std::distance(it->second.begin(), sit);
			if (iSize >= ijParams.nOfferDepth)
				break;
			vArray.at(iPair).replace(vArray.at(iPair).find_first_of(chBidCol), 1, boost::lexical_cast<string>(sit->first));

			// Move to next ask column
			++chBidCol;
		}
	}

	// Initialize all left over template characters
	boost::regex re("[a-k]");
	for (auto& data : vArray) {
		data.assign(boost::regex_replace(data, re, "0"));
	}
	injectHtml(ijParams, vArray);
}

void TradePlot::plotOrderDiff(mapOffers& moCsvBid, mapOffers& moLogBid, mapOffers& moCsvAsk, mapOffers& moLogAsk, InjectParams& ijParams) {

	string szTemplate("\t\t\t\t[a,b,c],");
	string szLab("a"), szBidDiff("b"), szAskDiff("c");

	// Buffer to calculate hthe difference of quantity in order books
	vector<long> sumSizes;
	map<long,int> mBidDiff, mAskDiff;
	map<long, long> mCsvBidPriceSizes, mLogBidPriceSizes;
	map<long, long> mCsvAskPriceSizes, mLogAskPriceSizes;;

	// Sum up the total sizes in the csv bid offers
	for (mapOffers::iterator it = moCsvBid.begin(); it != moCsvBid.end(); ++it) {

		long lSum = 0;
		// Calculate the size in the csv map for this key
		for (vSizeRow::iterator sit = it->second.begin(); sit != it->second.end(); ++sit) {
			lSum += sit->first;
		}
		mCsvBidPriceSizes.insert(make_pair(it->first, lSum));
	}

	// Sum up the total sizes in the log bid offers
	for (mapOffers::iterator it = moLogBid.begin(); it != moLogBid.end(); ++it) {

		long lSum = 0;
		// Calculate the size in the map for this key
		for (vSizeRow::iterator sit = it->second.begin(); sit != it->second.end(); ++sit) {
			lSum += sit->first;
		}
		mLogBidPriceSizes.insert(make_pair(it->first, lSum));
	}

	// Sum up the total sizes in the csv ask offers
	for (mapOffers::iterator it = moCsvAsk.begin(); it != moCsvAsk.end(); ++it) {

		long lSum = 0;
		// Calculate the size in the csv map for this key
		for (vSizeRow::iterator sit = it->second.begin(); sit != it->second.end(); ++sit) {
			lSum += sit->first;
		}
		mCsvAskPriceSizes.insert(make_pair(it->first, lSum));
	}

	// Sum up the total sizes in the log bid offers
	for (mapOffers::iterator it = moLogAsk.begin(); it != moLogAsk.end(); ++it) {

		long lSum = 0;
		// Calculate the size in the map for this key
		for (vSizeRow::iterator sit = it->second.begin(); sit != it->second.end(); ++sit) {
			lSum += sit->first;
		}
		mLogAskPriceSizes.insert(make_pair(it->first, lSum));
	}

	// Calculate the difference betweem the orders
	vector<int> nBidDiff, nAskDiff;

	// Build map of bid differences
	for (map<long, long>::iterator it = mCsvBidPriceSizes.begin(); it != mCsvBidPriceSizes.end(); ++it) {

		map<long, long>::iterator kit = mLogBidPriceSizes.find(it->first);

		if (kit != mLogBidPriceSizes.end()) {

			long lDiff = abs(it->second - kit->second);
			mBidDiff.insert(make_pair(it->first, lDiff));
		}
	}

	// Build map of ask differences
	for (map<long, long>::iterator it = mCsvAskPriceSizes.begin(); it != mCsvAskPriceSizes.end(); ++it) {

		map<long, long>::iterator kit = mLogAskPriceSizes.find(it->first);

		if (kit != mLogAskPriceSizes.end()) {

			long lDiff = abs(it->second - kit->second);
			mAskDiff.insert(make_pair(it->first, lDiff));
		}
	}

	int maxBid  = mBidDiff.size();
	int maxPlot = mBidDiff.size() + mAskDiff.size();
	assert(maxPlot > 0);

	vector<string> vArray(maxPlot, szTemplate);
	vArray.at(maxPlot - 1).pop_back();

	for (map<long,int>::reverse_iterator it = mBidDiff.rbegin(); it != mBidDiff.rend(); ++it) {
		int iPair = std::distance(mBidDiff.rbegin(), it);
		vArray.at(iPair).replace(vArray.at(iPair).find_first_of('a'), 1, boost::lexical_cast<string>(it->first));
		vArray.at(iPair).replace(vArray.at(iPair).find_first_of('b'), 1, boost::lexical_cast<string>(it->second));
	}

	for (map<long, int>::iterator it = mAskDiff.begin(); it != mAskDiff.end(); ++it) {
		int iPair = std::distance(mAskDiff.begin(), it) + maxBid;
		vArray.at(iPair).replace(vArray.at(iPair).find_first_of('a'), 1, boost::lexical_cast<string>(it->first));
		vArray.at(iPair).replace(vArray.at(iPair).find_first_of('c'), 1, boost::lexical_cast<string>(it->second));
	}

	// Initialize all left over template characters
	boost::regex re("[b-c]");
	for (auto& data : vArray) {
		data.assign(boost::regex_replace(data, re, "0"));
	}

	injectHtml(ijParams, vArray);
}

void TradePlot::plotSpread(vector<int>& csvSpread, vector<int>& logSpread, InjectParams& ijParams) {

	string szTemplate("\t\t\t[a,b,c],");
	string szLab("a"), szBid("b"), szAsk("c");

	// Get the maximum count of mapped bids and ask inclusive of both sources
	int maxSpreads = max(csvSpread.size(), logSpread.size());

	assert(maxSpreads > 0);

	vector<string> vArray(maxSpreads, szTemplate);

	// Remove lsst comma in last vector element
	vArray.at(maxSpreads - 1).pop_back();

	for (int i : boost::irange(0, maxSpreads)) {
		vArray.at(i).replace(vArray.at(i).find_first_of('a'), 1, boost::lexical_cast<string>(i));
	}

	for (vector<int>::iterator it = csvSpread.begin(); it != csvSpread.end(); ++it) {
		int iPair = std::distance(csvSpread.begin(), it);
		vArray.at(iPair).replace(vArray.at(iPair).find_first_of('b'), 1, boost::lexical_cast<string>(*it));
	}

	for (vector<int>::iterator it = logSpread.begin(); it != logSpread.end(); ++it) {
		int iPair = std::distance(logSpread.begin(), it);
		vArray.at(iPair).replace(vArray.at(iPair).find_first_of('c'), 1, boost::lexical_cast<string>(*it));
	}

	// Initialize all left over spreads with last value
	int nSpread = csvSpread.at(csvSpread.size() - 1);
	boost::regex reb("b");
	for (auto& data : vArray) {
		data.assign(boost::regex_replace(data, reb, boost::lexical_cast<string>(nSpread)));
	}

	nSpread = logSpread.at(logSpread.size() - 1);
	boost::regex rec("c");
	for (auto& data : vArray) {
		data.assign(boost::regex_replace(data, rec, boost::lexical_cast<string>(nSpread)));
	}
	injectHtml(ijParams, vArray);
}

void TradePlot::plotPriceDiff(vecBidAsk& vBidAskCsv, vecBidAsk& vBidAskLog, InjectParams& ijParams) {

	// Build the header
	ijParams.szHeader = "\t\t\t['Price', 'Percentage'],";

	// Copy the bid ask prices as pair and skip bid ask considered invalid (either bid or ask at zero)
	vector<long>csvAsk;
	vector<long>csvBid;
	vector<long>logBid;
	vector<long>logAsk;

	for (vecBidAsk::iterator vit = vBidAskCsv.begin(); vit != vBidAskCsv.end(); ++vit) {
		
		if (vit->first > 0 && vit->second > 0) {
			csvBid.push_back(vit->first);
			csvAsk.push_back(vit->second);
		}
	}

	for (vecBidAsk::iterator vit = vBidAskLog.begin(); vit != vBidAskLog.end(); ++vit) {
		if (vit->first > 0 && vit->second > 0) {
			logBid.push_back(vit->first);
			logAsk.push_back(vit->second);
		}
	}

	// Pre-initialize the strings with the instruments
	vector<string> vLabels;
	vLabels.push_back("\t\t\t['[Bid]" + ijParams.szInstrCsv + "'");
	vLabels.push_back("\t\t\t['[Ask]" + ijParams.szInstrCsv + "'");
	vLabels.push_back("\t\t\t['[Bid]" + ijParams.szInstrLog + "'");
	vLabels.push_back("\t\t\t['[Ask]" + ijParams.szInstrLog + "'");

	// Calculate the percentage variation with formula: last quote - first quote / last quote
	vector<double> vPercent;
	vPercent.push_back(abs(boost::lexical_cast<double>(csvBid.front() - csvBid.back())) / csvBid.back());
	vPercent.push_back(abs(boost::lexical_cast<double>(csvAsk.front() - csvAsk.back())) / csvAsk.back());
	vPercent.push_back(abs(boost::lexical_cast<double>(logBid.front() - logBid.back())) / logBid.back());
	vPercent.push_back(abs(boost::lexical_cast<double>(logAsk.front() - logAsk.back())) / logAsk.back());

	// Set precision to two decimal points
	stringstream ss;
	ss << fixed << setprecision(2);
	
	// Get the total number of data arrays
	int nNumData = vLabels.size();

	// Intialise the strings to inject to html
	vector<string> vArray;
	for (int i : boost::irange(0, nNumData)) {
		ss << vLabels.at(i) << "," << vPercent.at(i) << "],";
		vArray.push_back(ss.str());
		ss.clear();
		ss.str(string());
	}
	vArray.at(vArray.size() - 1).pop_back();

	//--------------------------------------------------------------------------------
	// Inject bid price difference bwtween the two instruments
	//--------------------------------------------------------------------------------
	// Swap 'diff' marker identifier with 'bid'
	boost::regex reMarkerDiff("diff");
	ijParams.szMarkerBegin.assign(boost::regex_replace(ijParams.szMarkerBegin, reMarkerDiff, "bid"));
	ijParams.szMarkerEnd.assign(boost::regex_replace(ijParams.szMarkerEnd, reMarkerDiff, "bid"));
	vector<string> vArrayBid;
	vArrayBid.push_back(vArray.at(0));
	vArrayBid.push_back(vArray.at(2));
	//Strip the 'Bid' identifier
	boost::regex reBid("\\[Bid\\]");
	vArrayBid.at(0).assign(boost::regex_replace(vArrayBid.at(0), reBid, ""));
	vArrayBid.at(1).assign(boost::regex_replace(vArrayBid.at(1), reBid, ""));
	vArrayBid.at(vArrayBid.size() - 1).pop_back();
	injectHtml(ijParams, vArrayBid);
	//--------------------------------------------------------------------------------
	// Inject ask price difference bwtween the two instruments
	//--------------------------------------------------------------------------------
	// Swap 'diff' marker identifier with 'ask'
	boost::regex reMarkerBid("bid");
	ijParams.szMarkerBegin.assign(boost::regex_replace(ijParams.szMarkerBegin, reMarkerBid, "ask"));
	ijParams.szMarkerEnd.assign(boost::regex_replace(ijParams.szMarkerEnd, reMarkerBid, "ask"));
	vector<string> vArrayAsk;
	vArrayAsk.push_back(vArray.at(1));
	vArrayAsk.push_back(vArray.at(3));
	//Strip the 'Ask' identifier
	boost::regex reAsk("\\[Ask\\]");
	vArrayAsk.at(0).assign(boost::regex_replace(vArrayAsk.at(0), reAsk, ""));
	vArrayAsk.at(1).assign(boost::regex_replace(vArrayAsk.at(1), reAsk, ""));
	injectHtml(ijParams, vArrayAsk);
	//--------------------------------------------------------------------------------
	// Calculate the price variations
	//long csvBidSum = boost::accumulate(csvBid, 0);
	//long csvAsjSum = boost::accumulate(csvAsk, 0);
	//long logBidSum = boost::accumulate(logBid, 0);
	//long logAskSum = boost::accumulate(logAsk, 0);
}

void TradePlot::plotVolatility(InjectParams& ijParams) {

}

void TradePlot::injectHtml(const InjectParams& ijParams, const vstring& vs)
{
	// Now inject the built columns in the html
	vstring vHtml;

	ifstream file(ijParams.szHtml);
	string line;

	// First read and parse all the html lines
	while (getline(file, line)) {
		vHtml.push_back(line);
	}

	// We are done
	file.close();

	// Inject the lines between the begin and section
	ofstream ofs(ijParams.szHtml);
	regex reBegin(ijParams.szMarkerBegin);
	regex reEnd(ijParams.szMarkerEnd);
	smatch m;

	bool bSkipNextLines = false;

	// Merge trade bars
	for (auto& line : vHtml)
	{
		if (regex_search(line, m, reEnd) == true) {
			bSkipNextLines = false;
		}

		if (bSkipNextLines == true)
			continue;

		ofs << line << endl;

		if (regex_search(line, m, reBegin) == true) {

			// Inject header if any
			if (ijParams.szHeader.empty() == false) 
				ofs << ijParams.szHeader << endl;

			// Inject data array
			for (auto& it : vs) {
				ofs << it << endl;
			}

			// Skip the lines until the end marker line
			bSkipNextLines = true;
		}
	}
	ofs.close();
}

void TradePlot::plotWall(mapKeyVal& mkvBidCsv, mapKeyVal& mkvAskCsv, mapKeyVal& mkvBidLog, mapKeyVal& mkvAskLog, InjectParams& ijParams) {

	// Build chart header line
	ijParams.szHeader = "\t\t\t['" + ijParams.szHeader + " Orders', '" \
		+ "[Bid]" + ijParams.szInstrCsv + "', '[Bid]"  + ijParams.szInstrLog + "', '"  \
		+ "[Ask]" + ijParams.szInstrCsv + "', '[Ask]"  + ijParams.szInstrLog + "'],";

	string szTemplate("\t\t\t['a',b,c,d,e],");
	string szLab("a"), szBid1("b"), szBid2("c"), szAsk1("d"), szAsk2("e");

	// Get the maximum count of mapped bids and ask inclusive
	int maxBidRows = max(mkvBidCsv.size(), mkvBidLog.size());
	int minBidRows = min(mkvBidCsv.size(), mkvBidLog.size());
	int maxAskRows = max(mkvAskCsv.size(), mkvAskLog.size());
	int minAskRows = min(mkvAskCsv.size(), mkvAskLog.size());

	int maxRows = maxBidRows + maxAskRows;
	assert(maxRows > 0);

	vector<string> vArray(maxRows, szTemplate);
	vArray.at(maxRows - 1).pop_back();

	for (mapKeyVal::reverse_iterator rit = mkvBidCsv.rbegin(); rit != mkvBidCsv.rend(); ++rit) {
		int iPair = std::distance(mkvBidCsv.rbegin(), rit);
		vArray.at(iPair).replace(vArray.at(iPair).find_first_of('a'), 1, boost::lexical_cast<string>(rit->second));
		vArray.at(iPair).replace(vArray.at(iPair).find_first_of('b'), 1, boost::lexical_cast<string>(rit->first));
	}

	for (mapKeyVal::reverse_iterator rit = mkvBidLog.rbegin(); rit != mkvBidLog.rend(); ++rit) {
		int iPair = std::distance(mkvBidLog.rbegin(), rit);
		vArray.at(iPair).replace(vArray.at(iPair).find_first_of('c'), 1, boost::lexical_cast<string>(rit->first));
	}

	// Replace 1 column of ask
	for (mapKeyVal::iterator it = mkvAskCsv.begin(); it != mkvAskCsv.end(); ++it) {
		int iPair = std::distance(mkvAskCsv.begin(), it) + maxBidRows;
		vArray.at(iPair).replace(vArray.at(iPair).find_first_of('a'), 1, boost::lexical_cast<string>(it->second));
		vArray.at(iPair).replace(vArray.at(iPair).find_first_of('d'), 1, boost::lexical_cast<string>(it->first));
	}

	for (mapKeyVal::iterator it = mkvAskLog.begin(); it != mkvAskLog.end(); ++it) {
		int iPair = std::distance(mkvAskLog.begin(), it) + maxBidRows;
		vArray.at(iPair).replace(vArray.at(iPair).find_first_of('e'), 1, boost::lexical_cast<string>(it->first));
	}

	// Initialize all left over template characters
	boost::regex re("[a-e]");
	for (auto& data : vArray) {
		data.assign(boost::regex_replace(data, re, "0"));
	}

	injectHtml(ijParams, vArray);
}


string TradePlot::getFormattedStream(const string& szFeedName, const OBRowFeed& obrf, const string& szFormat)
{
	stringstream ss, ssleft, ssright, ssbid, ssask, ssBidQty, ssAskQty, ssBookBid, ssBookAsk;

	vector<pairPriceSize> vBid = obrf.vecBidLevels;
	vector<pairPriceSize> vAsk = obrf.vecAskLevels;

	ssBidQty << boost::lexical_cast<string>(obrf.pairBidPriceSize.first) << ',';
	ssBidQty << boost::lexical_cast<string>(obrf.pairBidPriceSize.second);
	ssAskQty << boost::lexical_cast<string>(obrf.pairAskPriceSize.first) << ',';
	ssAskQty << boost::lexical_cast<string>(obrf.pairAskPriceSize.second);

	//pairPriceSize lp;

	for (vector<pairPriceSize>::iterator it = vBid.begin(); it != vBid.end(); ++it) {

		pairPriceSize lp = *it;

		if (it != vBid.begin()) {
			ssBookBid << ';';
		}

		ssBookBid << boost::lexical_cast<string>(lp.first) << ',';
		ssBookBid << boost::lexical_cast<string>(lp.second);
	}

	for (vector<pairPriceSize>::iterator it = vAsk.begin(); it != vAsk.end(); ++it) {

		pairPriceSize lp = *it;

		if (it != vAsk.begin()) {
			ssBookAsk << ';';
		}

		ssBookAsk << boost::lexical_cast<string>(lp.first) << ',';
		ssBookAsk << boost::lexical_cast<string>(lp.second);
	}

	// Build the stream of Bid and Ask order books
	ss << boost::format(szFormat) \
		% szFeedName % obrf.szInstrument %obrf.datetime %obrf.szFeedStat % ssBidQty.str() % ssAskQty.str() % ssBookBid.str() % ssBookAsk.str();

	return ss.str();
}

void TradePlot::consoleOut(OBStreamCSV& obsCsv, OBStreamLog& obsLog) {

	// Layout the line feeds from each stream sources one below another for comparison
	// The rows are interleaved between files one showing on top of another with an extra CR/LF for clarity.

	int nMinRows = min(obsCsv.getNumRows(), obsLog.getNumRows());

	ofstream ofsDiff(m_szConsoleLog);

	const string szFormatHead = "%1%   %|12t| %2% %|28t|%3% %|52t|%4% %|66t|%5%      %|83t|%6%     %|101t|%7%         %|160t|%8%\n";
	const string szFormatLine = "[%1%] %|12t| %2% %|28t|%3% %|52t|%4% %|66t|Bid{%5%} %|83t|Ask{%6%}%|101t|BookBid{%7%}%|160t|BookAsk{%8%}\n";

	stringstream sshead;
	sshead << boost::format(szFormatHead) % "Source" % "Instrument" % "DateTime" % "Stat" % "Bid" % "Ask" % "BookBid" % "BookAsk";
	ofsDiff << sshead.str();
	cout << sshead.str();

	string szLine1, szLine2;

	// Loop through all the feed records
	for (int i : boost::irange(0, nMinRows)) {

		szLine1 = getFormattedStream(obsCsv.getSourceFile(), obsCsv.getRowFeedAt(i), szFormatLine);
		szLine2 = getFormattedStream(obsLog.getSourceFile(), obsLog.getRowFeedAt(i), szFormatLine);

		// Separate each line with an extra blank line
		ofsDiff << szLine1;
		ofsDiff << szLine2 << endl;

		// Write to console for quick view
		cout << szLine1;
		cout << szLine2 << endl;
	}

	// Release the output file
	ofsDiff.close();
}
