#pragma once

#include "TracedException.hpp"
#include "OrderBook.hpp"

struct OBRowFeed
{
	string						szInstrument;
	string						datetime;
	string						szFeedStat;

	pairPriceSize				pairBidPriceSize;
	pairPriceSize				pairAskPriceSize;

	vector<pairPriceSize>		vecBidLevels;
	vector<pairPriceSize>		vecAskLevels;
};

class OBStream
{
private:
	int		m_id;
	string	m_szFile;
	int		m_nMaxBookLevels;
	int		m_nMaxBookDepth;

	// Keeps valid bid ask feeds in order
	vector<long>	m_vBids;
	vector<long>	m_vAsks;

	// Map of <price, <row,size>>
	multimap<long, pairSizeRow>	m_mapBidFeed;
	multimap<long, pairSizeRow>	m_mapAskFeed;

protected:
	vector<OBRowFeed>				m_vobrf;
	boost::shared_ptr<OrderBook>	m_pOrderBook;

	// Trace any exception that might occur
	ErrorExceptionInfo m_eei;

public:
	OBStream(const string& szFile, const int& nMaxBookLevels, const int& nMaxBookDepth);

	const string& getSourceFile() const					{ return m_szFile; }

	const OBRowFeed& getRowFeedAt(int i)				{ return m_vobrf.at(i); }
	int getNumRows() const								{ return m_vobrf.size(); }

	operator boost::shared_ptr<OrderBook>()				{ return m_pOrderBook; }
	boost::shared_ptr<OrderBook> getOrderBook()			{ return m_pOrderBook; }
	const bool IsCaughtException() const				{ return !m_eei.szDesc.empty(); }
	void setExceptionInfo(const TracedException& te)	{ m_eei = te.getExceptionInfo(); }

	void buildOrderBook();
	void addPriceSizeLevels(vector<pairPriceSize>& vp, const string& szLevel, const regex& re);

	
	virtual void processFeeds() = 0;
	virtual const string getObjectName() const = 0;
	virtual void CheckNotifyException() const;

private:
	//void buildWall(const priceSet& ps, const mapLevels& ml, mapBook& mPrice, mapBook& mSize);
	void addMapSizeOnRowChange(const long& kPrice, mapRowSize& mrs, mapOffers& mo);
	void buildOffers();
	void buildOfferLast();
	void setVariation();	// Set bid and ask price variations

	static constexpr auto SZ_OBSTREAM_EXCEPTION	= "OBStream Exception";
};

class OBStreamCSV : public OBStream {
public:
	OBStreamCSV() = delete;
	OBStreamCSV(const string& szFile, int& nMaxBookLevels, int& nMaxBookDepth) : OBStream(szFile, nMaxBookLevels, nMaxBookDepth) {}

	void processFeeds();
	const string getObjectName() const { return "OBStreamCSV"; }

	enum CSVFEED_ROW_ID {	
		CSVFEED_INSTRUMENT=0, 
		CSVFEED_DATETIME, 
		CSVFEED_FLAGS,
		CSVFEED_VOLUMEACC,
		CSVFEED_STATUS,
		CSVFEED_LATEST_PRICE,
		CSVFEED_LATEST_SIZE,
		CSVFEED_ASK_PRICE=7, 
		CSVFEED_ASK_SIZE, 
		CSVFEED_BID_PRICE, 
		CSVFEED_BID_SIZE, 
		CSVFEED_BID_LEVELS, 
		CSVFEED_ASK_LEVELS };

private:
	static constexpr auto SZ_OBSTREAMCSV_EXCEPTION = "OBStreamCSV Exception";
};

class OBStreamLog : public OBStream {
public:
	OBStreamLog() = delete;
	OBStreamLog(const string& szFile, int& nMaxBookLevels, int& nMaxBookDepth) : OBStream(szFile, nMaxBookLevels, nMaxBookDepth) {}

	void processFeeds();
	const string getObjectName() const { return "OBStreamLog"; }

	enum LOGFEED_ROW_ID {
		LOGFEED_INSTRUMENT = 0,
		LOGFEED_STATUS,
		LOGFEED_DATA_QUALITY,
		LOGFEED_BID_PRICESIZE,
		LOGFEED_ASK_PRICESIZE,
		LOGFEED_BID_BOOK,
		LOGFEED_ASK_BOOK
	};

private:
	static constexpr auto SZ_OBSTREAMLOG_EXCEPTION = "OBStreamLog Exception";
};



