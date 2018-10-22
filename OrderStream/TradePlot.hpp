#pragma once

typedef struct InjectParams {

	string		szInstrCsv;
	string		szInstrLog;

	string		szHtml;
	string		szHeader;
	string		szTitle;
	string		szSubTitle;

	string		szMarkerBegin;
	string		szMarkerEnd;

	int			nOfferDepth;
	string		szAny;

} InjectParams;


// Base class
class TradePlot {

public:
	// Chart plotting interface methds
	TradePlot() = delete;
	explicit TradePlot(const string& szXmlFile, OBStreamCSV& obsCsv, OBStreamLog& obsLog);

private:
	void	plotOrderBook(mapOffers& moBid, mapOffers& moAsk, InjectParams& ijParams);
	void	plotOrderDiff(mapOffers& moCsvBid, mapOffers& moLogBid, mapOffers& moCsvAsk, mapOffers& moLogAsk, InjectParams& ijParams);
	void	plotSpread(vector<int>& csvSpread, vector<int>& logSpread, InjectParams& ijParams);
	void	plotPriceDiff(vecBidAsk& vBidAskCsv, vecBidAsk& vBidAskLog, InjectParams& ijParams);
	void	plotVolatility(InjectParams& ijParams);
	void	plotWall(mapKeyVal& mkvBidCsv, mapKeyVal& mkvAskCsv, mapKeyVal& mkvBidLog, mapKeyVal& mkvAskLog, InjectParams& ijParams);
	void	plotVariation(const string& szBid, const string& szAsk, InjectParams& ijParams);

	void	consoleOut(OBStreamCSV& obsCsv, OBStreamLog& obsLog);
	string	getFormattedStream(const string& szFeedName, const OBRowFeed& obrf, const string& szFormat);
	void	injectHtml(const InjectParams& ijParams, const vstring& vs);

private:
	string	m_szConsoleLog;
	bool	m_bConsoleOut;

	boost::shared_ptr<OrderBook> m_pCsvBook;
	boost::shared_ptr<OrderBook> m_pLogBook;
};
