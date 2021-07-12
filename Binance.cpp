#include "Binance.h"
#include "ApiService.h"
#include "Utils.h"

ApiService apiService(binanceSpotTestnet);
TechnicalAnalysis technicalAnalysis;
Strategy strategy;

void BotData::newOco(const std::vector<Order> &orders)
{
    //TODO
    const Order firstOrder = orders[0];
    std::vector<std::pair<std::string, std::string>> params;
    //mandatory
    params.push_back(std::make_pair("symbol", firstOrder.symbol));
}

void BotData::newOrder(Order const &order)
{
    std::vector<std::pair<std::string, std::string>> params;
    //mandatory
    params.push_back(std::make_pair("symbol", order.symbol));
    params.push_back(std::make_pair("side", order.side));
    params.push_back(std::make_pair("type", order.type));
    if (order.type.find("LIMIT") != std::string::npos)
        params.push_back(std::make_pair("timeInForce", "GTC"));
    params.push_back(std::make_pair("timestamp", std::to_string(order.timestamp)));
    params.push_back(std::make_pair("quantity", std::to_string(order.quantity)));
    //for limit orders
    if (order.price > 0)
        params.push_back(std::make_pair("price", std::to_string(order.price)));
    //for stop orders
    if (order.stopPrice > 0)
        params.push_back(std::make_pair("stopPrice", std::to_string(order.stopPrice)));

    //for SIGNED endpoints
    std::string message, signature;
    apiService.getQueryString(params, message);
    bot.HMACsha256(message, bot.secretKey, signature);
    params.push_back(std::make_pair("signature", signature));

    apiService.getQueryString(params, message);
    std::cout << message << std::endl;

    apiService.request(methods::POST, "/order", params)
        .then([](http_response response)
              { response.extract_string()
                    .then([](std::string res)
                          { std::cout << res << std::endl; })
                    .wait(); })
        .wait();
}

void BotData::getPriceAction(std::string const &symbol, std::string const &interval, long startTime, long endTime,
                             int limit, void callback(HistoricalData &))
{
    std::vector<std::pair<std::string, std::string>> params;
    params.push_back(std::make_pair("symbol", symbol));
    params.push_back(std::make_pair("interval", interval));

    if (startTime != -1)
    {
        params.push_back(std::make_pair("startTime", std::to_string(startTime)));
    }

    if (endTime != -1)
    {
        params.push_back(std::make_pair("endTime", std::to_string(endTime)));
    }

    params.push_back(std::make_pair("limit", std::to_string(limit)));

    apiService.request(methods::GET, "/klines", params)
        .then([&](http_response response)
              {
                  response.extract_json()
                      .then([&](json::value jsonData)
                            {
                                HistoricalData candlestick;
                                for (int i = 0; i < limit; i++)
                                {
                                    candlestick.openTime.push_back(jsonData[i][0].as_number().to_uint64());
                                    candlestick.open.push_back(std::stod(jsonData[i][1].as_string()));
                                    candlestick.high.push_back(std::stod(jsonData[i][2].as_string()));
                                    candlestick.low.push_back(std::stod(jsonData[i][3].as_string()));
                                    candlestick.close.push_back(std::stod(jsonData[i][4].as_string()));
                                    candlestick.volume.push_back(std::stod(jsonData[i][5].as_string()));
                                    candlestick.closeTime.push_back(jsonData[i][6].as_number().to_uint64());
                                }

                                if (callback != NULL)
                                {
                                    callback(candlestick);
                                }
                            })
                      .wait();
              })
        .wait();
}

void BotData::getAllOrders(std::string symbol)
{
    long timestamp;
    getTime(timestamp);
    std::vector<std::pair<std::string, std::string>> params;
    params.push_back(std::make_pair("symbol", symbol));
    params.push_back(std::make_pair("timestamp", std::to_string(timestamp)));
    //for SIGNED endpoints
    std::string message, signature;
    apiService.getQueryString(params, message);
    bot.HMACsha256(message, bot.secretKey, signature);
    params.push_back(std::make_pair("signature", signature));
    //apiService.getQueryString(params, message);
    //std::cout << message << std::endl;

    apiService.request(methods::GET, "/allOrders", params)
        .then([](http_response response)
              { response.extract_string()
                    .then([](std::string res)
                          { std::cout << res << std::endl; })
                    .wait(); })
        .wait();
}

void BotData::getOrderBook(std::string symbol, int limit)
{
    std::vector<std::pair<std::string, std::string>> params;
    params.push_back(std::make_pair("symbol", symbol));
    params.push_back(std::make_pair("limit", std::to_string(limit)));

    apiService.request(methods::GET, "/depth", params, true, "order_book_info.json");
}

void BotData::getAllTradingPairs(void callback(std::list<std::string> &))
{
    std::vector<std::pair<std::string, std::string>> params;
    apiService.request(methods::GET, "/exchangeInfo", params)
        .then([&](http_response response)
              {
                  response.extract_json()
                      .then([&](json::value jsonData)
                            {
                                std::list<std::string> tradingPairs;
                                web::json::array symbols = jsonData.at("symbols").as_array();
                                for (int i = 0; i < symbols.size(); i++)
                                {
                                    std::string pair = symbols[i].at("symbol").as_string();
                                    std::string status = symbols[i].at("status").as_string();

                                    if (status.compare("TRADING") == 0)
                                    {
                                        tradingPairs.push_back(pair);
                                    }
                                }

                                if (callback != NULL)
                                {
                                    callback(tradingPairs);
                                }
                            })
                      .wait();
              })
        .wait();
}

void BotData::setUpKeys()
{
    std::string line;
    bool isSecretKey = true;
    //ifstream_t secrets("secrets.txt");
    ifstream_t secrets("secrets_spot_testnet.txt");
    if (secrets.is_open())
    {
        while (getline(secrets, line))
        {
            if (isSecretKey)
            {
                bot.secretKey = line;
                isSecretKey = false;
            }
            else
            {
                bot.apiKey = line;
                apiService.setApiKey(line);
            }
        }
        secrets.close();
    }
}

void BotData::checkConnectivity()
{
    std::vector<std::pair<std::string, std::string>> params;
    apiService.request(methods::GET, "/ping", params)
        .then([&](http_response response)
              { printf("Received response status code:%u\n", response.status_code()); })
        .wait();
}

void init()
{
    bot.setUpKeys();
}

void printHistoricalData()
{
    for (double openPrice : technicalAnalysis.data.open)
    {
        std::cout << std::setprecision(10) << openPrice << "\n";
    }

    for (double closePrice : technicalAnalysis.data.close)
    {
        std::cout << std::setprecision(10) << closePrice << "\n";
    }
    for (double ema : technicalAnalysis.data.fiftyEMA)
    {
        std::cout << std::setprecision(10) << ema << ", ";
    }

    std::cout << std::endl
              << "------------------------" << std::endl;

    for (double ema : technicalAnalysis.tempData.close)
    {
        std::cout << std::setprecision(10) << ema << ", ";
    }

    std::cout << std::endl
              << "------------------------" << std::endl;

    for (double ema : technicalAnalysis.data.fiftyEMA)
    {
        std::cout << std::setprecision(10) << ema << ", ";
    }

    std::cout << std::endl;
}

void BotData::HMACsha256(std::string const &message, std::string const &key, std::string &signature)
{
    unsigned char result[EVP_MAX_MD_SIZE];
    unsigned result_len = 0;
    HMAC(EVP_sha256(), key.data(), key.size(),
         reinterpret_cast<unsigned char const *>(message.data()), message.size(), result, &result_len);
    signature = binary_to_hex(result, result_len);
}

void filterTradingPairs(std::list<std::string> &tradingPairs, double minPrice,
                        double maxPrice, double minDollarVol, double minChangePercent)
{
    std::list<std::string>::iterator it;
    std::list<std::string> validPairs;
    for (it = tradingPairs.begin(); it != tradingPairs.end(); ++it)
    {
        std::string currPair = *it;
        std::vector<std::pair<std::string, std::string>> params;
        params.push_back(std::make_pair("symbol", currPair));

        //capture by refs for local scope's params
        apiService.request(methods::GET, "/ticker/24hr", params)
            .then([&tradingPairs, &validPairs, &it, &minPrice, &maxPrice, &minDollarVol, &minChangePercent](http_response response)
                  {
                      response.extract_json()
                          .then([&](json::value jsonData)
                                {
                                    try
                                    {
                                        double latestPrice = std::stod(jsonData.at("lastPrice").as_string());
                                        double volume = std::stod(jsonData.at("volume").as_string());
                                        double priceChangePercent = std::stod(jsonData.at("priceChangePercent").as_string());

                                        if (latestPrice >= minPrice && latestPrice <= maxPrice && latestPrice * volume >= minDollarVol && priceChangePercent >= minChangePercent)
                                        {
                                            validPairs.push_back(*it);
                                        }
                                    }
                                    catch (web::json::json_exception e)
                                    {
                                        printf("%s\n", e.what());
                                    }

                                    tradingPairs = validPairs;
                                })
                          .wait();
                  })
            .wait();
    }
}

void run()
{
    bot.getAllTradingPairs(
        [](std::list<std::string> &tradingPairs) -> void
        {
            double minPrice = 2.0, maxPrice = 13.0, minDollarVol = 500000, minChangePercent = 3.5;
            filterTradingPairs(tradingPairs, minPrice, maxPrice, minDollarVol, minChangePercent);

            std::list<std::string>::iterator it;
            for (it = tradingPairs.begin(); it != tradingPairs.end(); ++it)
            {
                std::cout << *it << std::endl;
            }
        });
}

void execOnSinglePair(const std::string &pair)
{
    bot.getPriceAction(
        pair, "5m", -1, -1, 250, [](HistoricalData &x) -> void
        { technicalAnalysis.setData(x); });
    while (1)
    {
        //thread t1 is used to prepare data for next time frame
        std::thread t1(&BotData::getPriceAction, bot, pair, "5m", -1, -1, 250,
                       [](HistoricalData &x) -> void
                       {
                           technicalAnalysis.setTempData(x);
                       });

        //other threads produce technical indicators
        std::thread t2(&TechnicalAnalysis::calcEMA, technicalAnalysis, 200, std::ref(technicalAnalysis.data.twoHundredEMA));
        std::thread t3(&TechnicalAnalysis::calcPSAR, technicalAnalysis, std::ref(technicalAnalysis.data.pSar));
        std::thread t4(&TechnicalAnalysis::setUpHeikinAshi, technicalAnalysis, std::ref(technicalAnalysis.data));

        t2.join();
        t3.join();
        t4.join();

        int signal = strategy.simpleHeikinAshiPsarEMA(
            technicalAnalysis.data.openHa, technicalAnalysis.data.closeHa,
            technicalAnalysis.data.highHa, technicalAnalysis.data.lowHa,
            technicalAnalysis.data.pSar, technicalAnalysis.data.twoHundredEMA);

        //std::cout << signal << std::endl;

        //if the heikin-ashi + psar + ema algo signals long position, use psar as stop loss and set rr to 2
        //if (signal == 1)
        //{
        int currentIndex = technicalAnalysis.data.pSar.size() - 1;
        const double risk = technicalAnalysis.data.close[currentIndex] - technicalAnalysis.data.pSar[currentIndex];
        const double reward = risk * 2;

        Order order1;
        order1.symbol = "BTCUSDT";
        order1.type = "MARKET";
        order1.side = "BUY";
        getTime(order1.timestamp);
        order1.quantity = 0.001;
        order1.price = -1;
        order1.stopPrice = -1;
        bot.newOrder(order1);
        //sleep(600);
        //}

        /*for (int i = 0; i < technicalAnalysis.data.pSar.size(); i++)
        {
            double psarValue = technicalAnalysis.data.pSar[i];
            double high = technicalAnalysis.data.high[i];
            double low = technicalAnalysis.data.low[i];
            std::cout << std::setprecision(10) << psarValue << " " << high << " " << low << std::endl;
        }*/

        t1.join();
        technicalAnalysis.setData(technicalAnalysis.tempData);

        bot.getAllOrders("BTCUSDT");

        //printHistoricalData();

        //wait for 5 mins
        //sleep(300);
        break;
    }
}

int main(int argc, char *argv[])
{
    benchmarkPerformance(
        []() -> void
        {
            init();
            run();
        });

    return 0;
}