#pragma once

typedef set<long>					priceSet, sizeSet;
typedef map<long, int>				mapPrice, mapSize;
typedef pair<long, long>			pairPriceSize;
typedef pair<long, long>			pairBidAsk;

typedef pair<long, int>				pairSizeRow;
typedef pair<int, long>				pairRowSize;
typedef pair<long, pairSizeRow>		pairPriceSizeRow;

typedef vector<pairBidAsk>			vecBidAsk;
typedef vector<string>				vstring;
typedef vector<pairSizeRow>			vSizeRow;

typedef map<int, long>				mapRowSize;
typedef map<long, int>				mapSizeRow;

typedef map<long, long>				mapKeyVal;
typedef map<long, vSizeRow>			mapOffers;

typedef struct BidAskSizeOffer {

	mapKeyVal	mapBidPrice;
	mapKeyVal	mapAskPrice;
	mapKeyVal	mapBidSize;
	mapKeyVal	mapAskSize;
	
} BidAskLWall;

typedef struct PriceOffers {

	mapOffers	bidOffers;	// multimap<price, map<size,row>>
	mapOffers	askOffers;	// multimap<price, map<size,row>>

} PriceOffers;

struct OrderBook
{
	string			szSourceFeed;
	string			szInstrument;
	int				nBookDepth;

	priceSet		sBidPrice;			// Dictinct best bid price
	priceSet		sAskPrice;			// Distinct best ask price

	priceSet		sBidSize;			// Distinct best bid size
	priceSet		sAskSize;			// Distinct best ask size

	vecBidAsk		vBidAsk;			// Best bid ask feeds for the entire session
	vector<int>		vSpread;			// Spread on each feed for the entire session

	PriceOffers		priceOffers;		// Bid and Ask offer book
	BidAskSizeOffer	lastOffer;			// bid and ask levels from the last feed sorted by price and quantity

	string			szBidVariation;		// Bid price percentage variation
	string			szAskVariation;		// Ask price percentage variation
};
