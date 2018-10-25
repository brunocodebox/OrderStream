#pragma once

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
	multimap<long, pairSizeRow>	mapBidFeed;
	multimap<long, pairSizeRow>	mapAskFeed;

protected:
	vector<OBRowFeed>				m_vobrf;
	boost::shared_ptr<OrderBook>	m_pOrderBook;

public:
	OBStream(const string& szFile, const int& nMaxBookLevels, const int& nMaxBookDepth);

	const string& getSourceFile() const			{ return m_szFile; }

	const OBRowFeed& getRowFeedAt(int i)		{ return m_vobrf.at(i); }
	int getNumRows() const						{ return m_vobrf.size(); }

	operator boost::shared_ptr<OrderBook>()		{ return m_pOrderBook; }
	boost::shared_ptr<OrderBook> getOrderBook()	{ return m_pOrderBook; }

	virtual void processFeeds() = 0;

	void buildOrderBook();
	void addPriceSizeLevels(vector<pairPriceSize>& vp, const string& szLevel, const regex& re);
	//void buildIndexSizeChange(mapRowSize& mrs, vector<int>& vs);

private:
	//void buildWall(const priceSet& ps, const mapLevels& ml, mapBook& mPrice, mapBook& mSize);
	void addMapSizeOnRowChange(const long& kPrice, mapRowSize& mrs, mapOffers& mo);
	void buildOffers();
	void buildOfferLast();
	void setVariation();	// Set bid and ask price variations
};

class OBStreamCSV : public OBStream {
public:
	OBStreamCSV() = delete;
	OBStreamCSV(const string& szFile, int& nMaxBookLevels, int& nMaxBookDepth) : OBStream(szFile, nMaxBookLevels, nMaxBookDepth) {}

	void processFeeds();

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
};

class OBStreamLog : public OBStream {
public:
	OBStreamLog() = delete;
	OBStreamLog(const string& szFile, int& nMaxBookLevels, int& nMaxBookDepth) : OBStream(szFile, nMaxBookLevels, nMaxBookDepth) {}

	void processFeeds();

	enum LOGFEED_ROW_ID {
		LOGFEED_INSTRUMENT = 0,
		LOGFEED_STATUS,
		LOGFEED_DATA_QUALITY,
		LOGFEED_BID_PRICESIZE,
		LOGFEED_ASK_PRICESIZE,
		LOGFEED_BID_BOOK,
		LOGFEED_ASK_BOOK
	};
};



