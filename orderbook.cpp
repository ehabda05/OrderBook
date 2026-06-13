#include "orderbook.hpp"

void OrderBook::addOrder(OrderPointer order) {
    if (!order) {
        throw std::logic_error("Cannot add null order.");
    }

    if (orders_.count(order->getId()) > 0) {
        throw std::logic_error("Order ID already exists.");
    }

    if (order->getOrderType() == OrderType::fillAndKill &&
        !canMatch(order->getSide(), order->getPrice())) {
        return;
    }

    if (order->getSide() == Side::buy) {
        bids_[order->getPrice()].push_back(order);
    } else {
        asks_[order->getPrice()].push_back(order);
    }

    orders_[order->getId()] = order;

    matchOrders();

    if (order->getOrderType() == OrderType::fillAndKill &&
        orders_.count(order->getId()) > 0) {
        cancelOrder(order->getId());
    }
}

void OrderBook::cancelOrder(OrderId orderId) {
    auto orderIt = orders_.find(orderId);

    if (orderIt == orders_.end()) {
        return;
    }

    OrderPointer order = orderIt->second;
    Price price = order->getPrice();
    Side side = order->getSide();

    if (side == Side::buy) {
        auto levelIt = bids_.find(price);

        if (levelIt != bids_.end()) {
            OrderPointers& ordersAtLevel = levelIt->second;

            for (auto it = ordersAtLevel.begin(); it != ordersAtLevel.end(); ++it) {
                if ((*it)->getId() == orderId) {
                    ordersAtLevel.erase(it);
                    break;
                }
            }

            if (ordersAtLevel.empty()) {
                bids_.erase(levelIt);
            }
        }
    } else {
        auto levelIt = asks_.find(price);

        if (levelIt != asks_.end()) {
            OrderPointers& ordersAtLevel = levelIt->second;

            for (auto it = ordersAtLevel.begin(); it != ordersAtLevel.end(); ++it) {
                if ((*it)->getId() == orderId) {
                    ordersAtLevel.erase(it);
                    break;
                }
            }

            if (ordersAtLevel.empty()) {
                asks_.erase(levelIt);
            }
        }
    }

    orders_.erase(orderIt);
}

void OrderBook::modifyOrder(const OrderModify& order) {
    auto it = orders_.find(order.orderId_);

    if (it == orders_.end()) {
        return;
    }

    OrderType oldOrderType = it->second->getOrderType();

    cancelOrder(order.orderId_);

    OrderPointer newOrder = std::make_shared<Order>(
        order.orderId_,
        oldOrderType,
        order.side_,
        order.price_,
        order.quantity_
    );

    addOrder(newOrder);
}

std::size_t OrderBook::size() const {
    return orders_.size();
}

OrderBookLevelInfos OrderBook::getOrderInfos() const {
    LevelInfos askInfos;
    LevelInfos bidInfos;

    for (const auto& [price, orders] : asks_) {
        Quantity totalQuantity = 0;

        for (const auto& order : orders) {
            totalQuantity += order->getRemainingQuantity();
        }

        askInfos.push_back(LevelInfo{price, totalQuantity});
    }

    for (const auto& [price, orders] : bids_) {
        Quantity totalQuantity = 0;

        for (const auto& order : orders) {
            totalQuantity += order->getRemainingQuantity();
        }

        bidInfos.push_back(LevelInfo{price, totalQuantity});
    }

    return OrderBookLevelInfos{askInfos, bidInfos};
}

bool OrderBook::canMatch(Side side, Price price) const {
    if (side == Side::buy) {
        if (asks_.empty()) {
            return false;
        }

        Price bestAsk = asks_.begin()->first;
        return price >= bestAsk;
    }

    if (side == Side::sell) {
        if (bids_.empty()) {
            return false;
        }

        Price bestBid = bids_.begin()->first;
        return price <= bestBid;
    }

    return false;
}

void OrderBook::matchOrders() {
    while (!bids_.empty() && !asks_.empty()) {
        auto bestBidIt = bids_.begin();
        auto bestAskIt = asks_.begin();

        Price bestBidPrice = bestBidIt->first;
        Price bestAskPrice = bestAskIt->first;

        // No crossing market.
        if (bestBidPrice < bestAskPrice) {
            break;
        }

        OrderPointers& bidOrders = bestBidIt->second;
        OrderPointers& askOrders = bestAskIt->second;

        OrderPointer bidOrder = bidOrders.front();
        OrderPointer askOrder = askOrders.front();

        Quantity quantityToFill = std::min(
            bidOrder->getRemainingQuantity(),
            askOrder->getRemainingQuantity()
        );

        bidOrder->fill(quantityToFill);
        askOrder->fill(quantityToFill);

        if (bidOrder->isFilled()) {
            orders_.erase(bidOrder->getId());
            bidOrders.pop_front();
        }

        if (askOrder->isFilled()) {
            orders_.erase(askOrder->getId());
            askOrders.pop_front();
        }

        if (bidOrders.empty()) {
            bids_.erase(bestBidIt);
        }

        if (askOrders.empty()) {
            asks_.erase(bestAskIt);
        }
    }
}