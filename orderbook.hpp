#ifndef ORDERBOOK_H
#define ORDERBOOK_H 

#include <vector>
#include <cstdint>
#include <stdexcept>
#include <map>
#include <list>
#include <unordered_map>
#include <memory>
#include <functional>
#include <algorithm>

enum class OrderType {
    goodTillCancel,
    fillAndKill
};

enum class Side {
    buy,
    sell
};

using Price = std::uint32_t;
using Quantity = std::uint32_t;
using OrderId = std::uint64_t;

struct LevelInfo {
    Price price_;
    Quantity quantity_;
};

using LevelInfos = std::vector<LevelInfo>;

class OrderBookLevelInfos {
public:
    OrderBookLevelInfos(const LevelInfos& asks, const LevelInfos& bids)
        : asks_(asks), bids_(bids) 
    {}

    const LevelInfos& getAsks() const { return asks_; }
    const LevelInfos& getBids() const { return bids_; }

private:
    LevelInfos asks_;
    LevelInfos bids_;
};

class Order {
public:
    Order(OrderId orderId, OrderType orderType, Side side, Price price, Quantity quantity)
        : id_(orderId), 
          orderType_(orderType), 
          side_(side), 
          price_(price), 
          initialQuantity_(quantity),
          remainingQuantity_(quantity)
    {}

    OrderId getId() const { return id_; }
    OrderType getOrderType() const { return orderType_; }
    Side getSide() const { return side_; }
    Price getPrice() const { return price_; }

    Quantity getInitialQuantity() const { return initialQuantity_; }
    Quantity getRemainingQuantity() const { return remainingQuantity_; }

    Quantity getFilledQuantity() const { 
        return getInitialQuantity() - getRemainingQuantity(); 
    }

    bool isFilled() const {
        return remainingQuantity_ == 0;
    }

    void fill(Quantity quantity) { 
        if (quantity > getRemainingQuantity()) {
            throw std::logic_error("Cannot fill more than remaining quantity.");
        } 

        remainingQuantity_ -= quantity; 
    }

private:
    OrderId id_;
    OrderType orderType_;
    Side side_;
    Price price_;
    Quantity initialQuantity_;
    Quantity remainingQuantity_;
};

using OrderPointer = std::shared_ptr<Order>;
using OrderPointers = std::list<OrderPointer>;

struct OrderModify {
    OrderId orderId_;
    Price price_;
    Quantity quantity_;
    Side side_;
};

class OrderBook {
public:
    void addOrder(OrderPointer order);
    void cancelOrder(OrderId orderId);
    void modifyOrder(const OrderModify& order);

    std::size_t size() const;
    OrderBookLevelInfos getOrderInfos() const;

private:
    bool canMatch(Side side, Price price) const;
    void matchOrders();

private:
    std::map<Price, OrderPointers, std::greater<Price>> bids_;
    std::map<Price, OrderPointers, std::less<Price>> asks_;

    std::unordered_map<OrderId, OrderPointer> orders_;
};

#endif // ORDERBOOK_H