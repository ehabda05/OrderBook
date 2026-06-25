#ifndef ORDERBOOK_HPP
#define ORDERBOOK_HPP

#include <algorithm>
#include <cstdint>
#include <cstddef>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

enum class OrderType {
    goodTillCancel,
    market,
    immediateOrCancel,
    fillOrKill,
    fillAndKill,
    postOnly,
    stopLoss,
    stopLimit
};

enum class Side {
    buy,
    sell
};

enum class OrderStatus {
    accepted,
    rejected,
    resting,
    stopPending,
    partiallyFilled,
    filled,
    canceled,
    modified
};

enum class ExecutionReportType {
    accepted,
    rejected,
    filled,
    partiallyFilled,
    canceled,
    modified
};

using Price = std::uint32_t;
using Quantity = std::uint32_t;
using OrderId = std::uint64_t;
using TradeId = std::uint64_t;
using Timestamp = std::uint64_t;

struct LevelInfo {
    Price price_{};
    Quantity quantity_{};
};

using LevelInfos = std::vector<LevelInfo>;

class OrderBookLevelInfos {
public:
    OrderBookLevelInfos(const LevelInfos& asks, const LevelInfos& bids);

    const LevelInfos& getAsks() const;
    const LevelInfos& getBids() const;

private:
    LevelInfos asks_;
    LevelInfos bids_;
};

struct Trade {
    TradeId tradeId_{};
    OrderId bidOrderId_{};
    OrderId askOrderId_{};
    Price price_{};
    Quantity quantity_{};
    Timestamp timestamp_{};
};

struct ExecutionReport {
    OrderId orderId_{};
    ExecutionReportType type_{};
    OrderStatus status_{};
    Price price_{};
    Quantity filledQuantity_{};
    Quantity remainingQuantity_{};
    Timestamp timestamp_{};
    std::string reason_;
};

class Order {
public:
    Order(
        OrderId orderId,
        OrderType orderType,
        Side side,
        Price price,
        Quantity quantity
    );

    Order(
        OrderId orderId,
        OrderType orderType,
        Side side,
        Price price,
        Quantity quantity,
        Price stopPrice
    );

    OrderId getId() const;
    OrderType getOrderType() const;
    Side getSide() const;
    Price getPrice() const;
    Quantity getInitialQuantity() const;
    Quantity getRemainingQuantity() const;
    Quantity getFilledQuantity() const;
    Price getStopPrice() const;
    Timestamp getTimestamp() const;
    OrderStatus getStatus() const;

    bool isFilled() const;
    bool isMarketOrder() const;
    bool isImmediateOrCancel() const;
    bool isStopOrder() const;

    void fill(Quantity quantity);
    void setTimestamp(Timestamp timestamp);
    void setStatus(OrderStatus status);
    void setOrderType(OrderType orderType);

private:
    OrderId id_;
    OrderType orderType_;
    Side side_;
    Price price_;
    Quantity initialQuantity_;
    Quantity remainingQuantity_;
    Price stopPrice_;
    Timestamp timestamp_;
    OrderStatus status_;
};

using OrderPointer = std::shared_ptr<Order>;
using OrderPointers = std::list<OrderPointer>;

struct OrderModify {
    OrderId orderId_{};
    Price price_{};
    Quantity quantity_{};
    Side side_{};
};

class OrderBook {
public:
    void addOrder(OrderPointer order);
    void cancelOrder(OrderId orderId);
    void modifyOrder(const OrderModify& order);

    std::size_t size() const;
    OrderBookLevelInfos getOrderInfos() const;

    const std::vector<Trade>& getTrades() const;
    const std::vector<ExecutionReport>& getExecutionReports() const;

    std::optional<Price> getBestBid() const;
    std::optional<Price> getBestAsk() const;
    std::optional<Price> getSpread() const;
    std::optional<double> getMidPrice() const;

    LevelInfos getTopNBids(std::size_t n) const;
    LevelInfos getTopNAsks(std::size_t n) const;
    std::optional<double> getImbalance(std::size_t depth) const;

private:
    struct OrderLocation {
        Side side_;
        Price price_;
        OrderPointers::iterator iterator_;
    };

    bool canMatch(const Order& order) const;
    bool canMatch(Side side, Price price) const;
    bool canFullyFill(const Order& order) const;
    bool wouldTriggerStop(const Order& order, Price lastTradePrice) const;

    void processIncomingOrder(OrderPointer order);
    void restOrder(OrderPointer order);
    void matchIncomingOrder(OrderPointer incomingOrder);
    void removeRestingOrder(OrderId orderId, bool reportCancel);
    void triggerStopOrders(Price lastTradePrice);

    Quantity totalQuantityAtLevels(const LevelInfos& levels, std::size_t depth) const;
    LevelInfos getTopNLevels(const LevelInfos& levels, std::size_t n) const;

    void addExecutionReport(
        OrderPointer order,
        ExecutionReportType type,
        OrderStatus status,
        const std::string& reason = ""
    );

private:
    std::map<Price, OrderPointers, std::greater<Price>> bids_;
    std::map<Price, OrderPointers, std::less<Price>> asks_;

    std::unordered_map<OrderId, OrderLocation> orderLocations_;
    std::vector<OrderPointer> stopOrders_;

    std::vector<Trade> trades_;
    std::vector<ExecutionReport> executionReports_;

    TradeId nextTradeId_ = 1;
    Timestamp currentTimestamp_ = 0;
};

#endif // ORDERBOOK_HPP
