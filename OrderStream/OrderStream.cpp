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

#include "OrderBook.hpp"
#include "OrderStream.hpp"
#include "TradePlot.hpp"


struct CompareRowSize {
	bool operator()(const pairRowSize& a, const pairRowSize& b) const { return a.second != b.second; }
};

OBStream::OBStream(const string& szFile, const int& nMaxBookLevels, const int& nMaxBookDepth) : m_szFile(szFile), m_nMaxBookLevels(nMaxBookLevels), m_nMaxBookDepth(nMaxBookDepth) {

	m_pOrderBook = boost::make_shared<OrderBook>();
	m_pOrderBook->szSourceFeed = m_szFile;
}

void OBStream::buildOfferLast() {

	// Sort the bid prices in a map
	vector<OBRowFeed>::reverse_iterator iter = m_vobrf.rbegin();
	
	for (auto& i : iter->vecBidLevels) {
		m_pOrderBook->lastOffer.mapBidPrice.insert(make_pair(i.first,  i.second));
		m_pOrderBook->lastOffer.mapBidSize.insert(make_pair(i.second, i.first));
	}

	for (auto& i : iter->vecAskLevels) {
		m_pOrderBook->lastOffer.mapAskPrice.insert(make_pair(i.first, i.second));
		m_pOrderBook->lastOffer.mapAskSize.insert(make_pair(i.second, i.first));
	}
}

void OBStream::buildOffers() {

	// Build the distinct set of bid and ask and their associated quantity offers
	for (auto& kBid : m_pOrderBook->sBidPrice) {

		auto itr1 = mapBidFeed.lower_bound(kBid);
		auto itr2 = mapBidFeed.upper_bound(kBid);

		// A map container will help sort the quantity of this price and give their row
		mapRowSize mrsBid;

		// Sort by size this distinct bid price, the row is kept to keep sequence
		while (itr1 != itr2) {

			// itr1 = map<price,  <quantity,    row>         >
			//            first   <second.first second.second>
			mrsBid.insert(make_pair(itr1->second.second, itr1->second.first));
			++itr1;
		}

		// Add the range of sizes for this key along with their row
		addMapSizeOnRowChange(kBid, mrsBid, m_pOrderBook->priceOffers.bidOffers);
	}

	// Repeat the same procedure for the Ask offers - (repeat code to be reviewed..) 
	for (auto& kAsk : m_pOrderBook->sAskPrice) {

		auto itr1 = mapAskFeed.lower_bound(kAsk);
		auto itr2 = mapAskFeed.upper_bound(kAsk);

		// A map container will help sort the quantity of this price and give their row
		mapRowSize mrsAsk;

		// Sort by size the size for this distinct ask price
		while (itr1 != itr2) {

			// itr1 = map<price,  <quantity,    row>         >
			//            first   <second.first second.second>
			mrsAsk.insert(make_pair(itr1->second.second, itr1->second.first));
			++itr1;
		}
		// Add the range of sizes for this key along with their row
		addMapSizeOnRowChange(kAsk, mrsAsk, m_pOrderBook->priceOffers.askOffers);
	}
}

void OBStream::addMapSizeOnRowChange(const long& kPrice, mapRowSize& mrs, mapOffers& mo) {

	// Remove all the size elements that are duplicates of those preceding them in a map of <row,size> pairs
	// and reverse the values in the reorganized pairs in a new vector of <size, row> elements.

	// A lambda function is used to reverse the values in all the pairs, and another to erase the duplicates.

	vector<pairSizeRow> vpsr;
	std::for_each(mrs.begin(), mrs.end(), [&vpsr](const pairRowSize& prs) { vpsr.push_back(make_pair(prs.second, prs.first));});
	vpsr.erase(std::unique(vpsr.begin(), vpsr.end(), [](const pairSizeRow& pprev, const pairSizeRow& pnext) {return pprev.first == pnext.first; }), vpsr.end());
	mo.insert(make_pair(kPrice, vpsr));
}

void OBStream::addPriceSizeLevels(vector<pairPriceSize>& vp, const string& szLevel, const regex& re) {

	for (sregex_iterator i = sregex_iterator(szLevel.begin(), szLevel.end(), re); i != sregex_iterator(); ++i)
	{
		smatch m = *i;
		vp.push_back(make_pair(boost::lexical_cast<long>(m.str(1)), boost::lexical_cast<long>(m.str(2))));

		if (vp.size() == m_nMaxBookLevels)
			break;
	}
}

void OBStream::setVariation() {

	// Set precision to two decimal points
	stringstream ssBid, ssAsk;
	ssBid << fixed << setprecision(2);
	ssAsk << fixed << setprecision(2);

	// Calculate bid variation whether positive or negative
	long lFirst = m_vBids.at(0);
	long lLast  = m_vBids.at(m_vBids.size()-1);
	double dDiff = boost::lexical_cast<double>(abs(lLast - lFirst));
	double dVariation = dDiff / lLast * 100;

	// Convert bid percentage to string for easy display
	if (lLast < lFirst)
		ssBid << "-";
	ssBid << dVariation;
	m_pOrderBook->szBidVariation = ssBid.str();

	// Calculate ask variation
	lFirst = m_vAsks.at(0);
	lLast  = m_vAsks.at(m_vBids.size() - 1);
	dDiff = boost::lexical_cast<double>(abs(lLast - lFirst));
	dVariation = dDiff/ lLast * 100;

	// Convert ask percentages to string for easy display
	if (lLast < lFirst)
		ssAsk << "-";
	ssAsk << dVariation;
	m_pOrderBook->szAskVariation = ssAsk.str();
}

void OBStream::buildOrderBook() {

	if (m_vobrf.size() == 0)
		return;

	// Use first record as source to indicate the instrument of the feeds
	m_pOrderBook->szSourceFeed	= m_szFile;
	m_pOrderBook->szInstrument	= m_vobrf.at(0).szInstrument;
	m_pOrderBook->nBookDepth	= m_nMaxBookDepth;

	for (vector<OBRowFeed>::iterator it = m_vobrf.begin(); it != m_vobrf.end(); ++it) {
	
		// Transform iterator into index
		int iRow = boost::distance(m_vobrf.begin(), it);

		// Make sure the market is not stuck on null bid or ask
		//if (it->pairBidPriceSize.first > 0 && it->pairAskPriceSize.first > 0 && it->pairBidPriceSize.second > 0 && it->pairAskPriceSize.second > 0) {
		if (it->pairBidPriceSize.first > 0 && it->pairAskPriceSize.first > 0) {

			m_vBids.push_back(it->pairBidPriceSize.first);
			m_vAsks.push_back(it->pairAskPriceSize.first);

			// Keep the distinct bid, ask, and spread from  all the feeds
			m_pOrderBook->sBidPrice.insert(it->pairBidPriceSize.first);
			m_pOrderBook->sBidSize.insert(it->pairBidPriceSize.second);

			m_pOrderBook->sAskPrice.insert(it->pairAskPriceSize.first);
			m_pOrderBook->sAskSize.insert(it->pairAskPriceSize.second);

			// Record the spread on best bid and ask
			m_pOrderBook->vSpread.push_back(it->pairAskPriceSize.first - it->pairBidPriceSize.first);

			// Record the best bid and ask
			m_pOrderBook->vBidAsk.push_back(make_pair(it->pairBidPriceSize.first, it->pairAskPriceSize.first));

			// Record the bid offers on this feed
			for (vector<pairPriceSize>::iterator itb = it->vecBidLevels.begin(); itb != it->vecBidLevels.end(); ++itb) {
				//m_pOrderBook->priceOffers.bidOffers.insert(make_pair(itb->first, make_pair(itb->second, iRow)));
				mapBidFeed.insert(make_pair(itb->first, make_pair(itb->second, iRow)));
			}

			// Record the ask offers on this feed
			for (vector<pairPriceSize>::iterator ita = it->vecAskLevels.begin(); ita != it->vecAskLevels.end(); ++ita) {
				//m_pOrderBook->priceOffers.askOffers.insert(make_pair(ita->first, make_pair(ita->second, iRow)));
				mapAskFeed.insert(make_pair(ita->first, make_pair(ita->second, iRow)));
			}
		}
	}
	buildOffers();

	// Plot the last feed offers
	buildOfferLast();

	// Calculate variation
	setVariation();
}

using boost::lexical_cast;
using boost::bad_lexical_cast;

void OBStreamCSV::processFeeds()
{
	regex reQuotedFields("\"(.*?)\"");
	regex rePriceQty("Price:\\s+([0-9]+)\\s+Quantity:\\s+([0-9]+)");

	ifstream file(getSourceFile());
	string line;

	// Skip header line
	getline(file, line);

	// Read and parse all feeds
	while (getline(file, line)) {

		vector<string> words;

		for (sregex_iterator i = sregex_iterator(line.begin(), line.end(), reQuotedFields); i != sregex_iterator(); ++i)
		{
			smatch m = *i;
			string s = m.str(1);
			words.push_back(s);
		}

		OBRowFeed obrf;
		obrf.szInstrument = words.at(OBStreamCSV::CSVFEED_INSTRUMENT);
		obrf.datetime = words.at(OBStreamCSV::CSVFEED_DATETIME);

		// Take only first character of the status
		obrf.szFeedStat = words.at(OBStreamCSV::CSVFEED_STATUS);

		obrf.pairAskPriceSize = make_pair(boost::lexical_cast<long>(words.at(OBStreamCSV::CSVFEED_ASK_PRICE)), boost::lexical_cast<long>(words.at(OBStreamCSV::CSVFEED_ASK_SIZE)));
		obrf.pairBidPriceSize = make_pair(boost::lexical_cast<long>(words.at(OBStreamCSV::CSVFEED_BID_PRICE)), boost::lexical_cast<long>(words.at(OBStreamCSV::CSVFEED_BID_SIZE)));

		addPriceSizeLevels(obrf.vecBidLevels, words.at(OBStreamCSV::CSVFEED_BID_LEVELS), rePriceQty);
		addPriceSizeLevels(obrf.vecAskLevels, words.at(OBStreamCSV::CSVFEED_ASK_LEVELS), rePriceQty);

		// Log parsed feed stream
		m_vobrf.push_back(obrf);
	}

	// Close the file
	file.close();

	// Build the order book. Nothing will get built if the file wasn't open successfully of there was no lines processed.
	buildOrderBook();
}

void OBStreamLog::processFeeds()
{
	regex reDate("[0-9]+-[0-9:]+.[0-9]+");
	regex reEllipsis("\\{(.*?)\\}");
	regex rePriceQty("([0-9]*),([0-9]*)");

	ifstream file(getSourceFile());
	string line;

	// Read and parse all row feeds
	while (getline(file, line)) {

		OBRowFeed obrf;
		smatch m;

		if (regex_search(line, m, reDate) == true) {
			obrf.datetime = m.str();
		}

		vector<string> fields;

		for (sregex_iterator i = sregex_iterator(line.begin(), line.end(), reEllipsis); i != sregex_iterator(); ++i)
		{
			m = *i;
			string s = m.str(1);
			fields.push_back(s);
		}

		obrf.szInstrument = fields.at(OBStreamLog::LOGFEED_INSTRUMENT);
		obrf.szFeedStat = fields.at(OBStreamLog::LOGFEED_STATUS);
		// skip the data quality

		if (regex_search(fields.at(OBStreamLog::LOGFEED_BID_PRICESIZE), m, rePriceQty) == true) {
			obrf.pairBidPriceSize = make_pair(boost::lexical_cast<long>(m.str(1)), boost::lexical_cast<long>(m.str(2)));
		}

		if (regex_search(fields.at(OBStreamLog::LOGFEED_ASK_PRICESIZE), m, rePriceQty) == true) {
			obrf.pairAskPriceSize = make_pair(boost::lexical_cast<long>(m.str(1)), boost::lexical_cast<long>(m.str(2)));
		}

		addPriceSizeLevels(obrf.vecBidLevels, fields.at(OBStreamLog::LOGFEED_BID_BOOK), rePriceQty);
		addPriceSizeLevels(obrf.vecAskLevels, fields.at(OBStreamLog::LOGFEED_ASK_BOOK), rePriceQty);

		m_vobrf.push_back(obrf);
	}

	// Close the file
	file.close();

	// Build the order book. Nothing will get built if the file wasn't open successfully of there was no lines processed.
	buildOrderBook();
}
