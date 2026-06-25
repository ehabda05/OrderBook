#include "orderbook.hpp"

#include <limits>

OrderBookLevelInfos::OrderBookLevelInfos(const LevelInfos& asks, const LevelInfos& bids)
    : asks_(asks), bids_(bids)
{}

const LevelInfos& OrderBookLevelInfos::getAsks() const {
    return asks_;
}

const LevelInfos& OrderBookLevelInfos::getBids() const {
    return bids_;
}

Order::Order(
    OrderId orderId,
    OrderType orderType,
    Side side,
    Price price,
    Quantity quantity
)
    : id_(orderId),
      orderType_(orderType),
      side_(side),
      price_(price),
      initialQuantity_(quantity),
      remainingQuantity_(quantity),
      stopPrice_(0),
      timestamp_(0),
      status_(OrderStatus::accepted)
{
    if (quantity == 0) {
        throw std::logic_error("Order quantity must be positive.");
    }
}

Order::Order(
    OrderId orderId,
    OrderType orderType,
    Side side,
    Price price,
    Quantity quantity,
    Price stopPrice
)
    : id_(orderId),
      orderType_(orderType),
      side_(side),
      price_(price),
      initialQuantity_(quantity),
      remainingQuantity_(quantity),
      stopPrice_(stopPrice),
      timestamp_(0),
      status_(OrderStatus::accepted)
{
    if (quantity == 0) {
        throw std::logic_error("Order quantity must be positive.");
    }
}

OrderId Order::getId() const {
    return id_;
}

OrderType Order::getOrderType() const {
    return orderType_;
}

Side Order::getSide() const {
    return side_;
}

Price Order::getPrice() const {
    return price_;
}

Quantity Order::getInitialQuantity() const {
    return initialQuantity_;
}

Quantity Order::getRemainingQuantity() const {
    return remainingQuantity_;
}

Quantity Order::getFilledQuantity() const {
    return initialQuantity_ - remainingQuantity_;
}

Price Order::getStopPrice() const {
    return stopPrice_;
}

Timestamp Order::getTimestamp() const {
    return timestamp_;
}

OrderStatus Order::getStatus() const {
    return status_;
}

bool Order::isFilled() const {
    return remainingQuantity_ == 0;
}

bool Order::isMarketOrder() const {
    return orderType_ == OrderType::market;
}

bool Order::isImmediateOrCancel() const {
    return orderType_ == OrderType::immediateOrCancel ||
           orderType_ == OrderType::fillAndKill;
}

bool Order::isStopOrder() const {
    return orderType_ == OrderType::stopLoss ||
           orderType_ == OrderType::stopLimit;
}

void Order::fill(Quantity quantity) {
    if (quantity > remainingQuantity_) {
        throw std::logic_error("Cannot fill more than remaining quantity.");
    }

    remainingQuantity_ -= quantity;
}

void Order::setTimestamp(Timestamp timestamp) {
    timestamp_ = timestamp;
}

void Order::setStatus(OrderStatus status) {
    status_ = status;
}

void Order::setOrderType(OrderType orderType) {
    orderType_ = orderType;
}

void OrderBook::addOrder(OrderPointer order) {
    if (!order) {
        throw std::logic_error("Cannot add null order.");
    }

    if (orderLocations_.count(order->getId()) > 0) {
        order->setStatus(OrderStatus::rejected);
        addExecutionReport(order, ExecutionReportType::rejected, OrderStatus::rejected, "Duplicate active order ID.");
        throw std::logic_error("Order ID already exists.");
    }

    if (order->getRemainingQuantity() == 0) {
        order->setStatus(OrderStatus::rejected);
        addExecutionReport(order, ExecutionReportType::rejected, OrderStatus::rejected, "Cannot add order with zero remaining quantity.");
        throw std::logic_error("Cannot add order with zero remaining quantity.");
    }

    order->setTimestamp(++currentTimestamp_);
    order->setStatus(OrderStatus::accepted);
    addExecutionReport(order, ExecutionReportType::accepted, OrderStatus::accepted);

    if (order->isStopOrder()) {
        order->setStatus(OrderStatus::stopPending);
        stopOrders_.push_back(order);
        addExecutionReport(order, ExecutionReportType::accepted, OrderStatus::stopPending, "Stop order accepted and waiting for trigger.");
        return;
    }

    if (order->getOrderType() == OrderType::postOnly && canMatch(*order)) {
        order->setStatus(OrderStatus::rejected);
        addExecutionReport(order, ExecutionReportType::rejected, OrderStatus::rejected, "Post-only order would immediately execute.");
        return;
    }

    if (order->getOrderType() == OrderType::fillOrKill && !canFullyFill(*order)) {
        order->setStatus(OrderStatus::rejected);
        addExecutionReport(order, ExecutionReportType::rejected, OrderStatus::rejected, "Fill-or-kill order cannot fully execute.");
        return;
    }

    processIncomingOrder(order);
}

void OrderBook::cancelOrder(OrderId orderId) {
    removeRestingOrder(orderId, true);
}

void OrderBook::modifyOrder(const OrderModify& order) {
    auto locationIt = orderLocations_.find(order.orderId_);

    if (locationIt == orderLocations_.end()) {
        return;
    }

    if (order.quantity_ == 0) {
        throw std::logic_error("Modified order quantity must be positive.");
    }

    OrderPointer oldOrder = *locationIt->second.iterator_;
    OrderType oldOrderType = oldOrder->getOrderType();

    removeRestingOrder(order.orderId_, false);

    oldOrder->setStatus(OrderStatus::modified);
    addExecutionReport(oldOrder, ExecutionReportType::modified, OrderStatus::modified, "Order modified; old resting order removed.");

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
    return orderLocations_.size();
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

const std::vector<Trade>& OrderBook::getTrades() const {
    return trades_;
}

const std::vector<ExecutionReport>& OrderBook::getExecutionReports() const {
    return executionReports_;
}

std::optional<Price> OrderBook::getBestBid() const {
    if (bids_.empty()) {
        return std::nullopt;
    }

    return bids_.begin()->first;
}

std::optional<Price> OrderBook::getBestAsk() const {
    if (asks_.empty()) {
        return std::nullopt;
    }

    return asks_.begin()->first;
}

std::optional<Price> OrderBook::getSpread() const {
    auto bestBid = getBestBid();
    auto bestAsk = getBestAsk();

    if (!bestBid || !bestAsk) {
        return std::nullopt;
    }

    return *bestAsk - *bestBid;
}

std::optional<double> OrderBook::getMidPrice() const {
    auto bestBid = getBestBid();
    auto bestAsk = getBestAsk();

    if (!bestBid || !bestAsk) {
        return std::nullopt;
    }

    return (static_cast<double>(*bestBid) + static_cast<double>(*bestAsk)) / 2.0;
}

LevelInfos OrderBook::getTopNBids(std::size_t n) const {
    return getTopNLevels(getOrderInfos().getBids(), n);
}

LevelInfos OrderBook::getTopNAsks(std::size_t n) const {
    return getTopNLevels(getOrderInfos().getAsks(), n);
}

std::optional<double> OrderBook::getImbalance(std::size_t depth) const {
    OrderBookLevelInfos infos = getOrderInfos();

    Quantity bidDepth = totalQuantityAtLevels(infos.getBids(), depth);
    Quantity askDepth = totalQuantityAtLevels(infos.getAsks(), depth);
    Quantity totalDepth = bidDepth + askDepth;

    if (totalDepth == 0) {
        return std::nullopt;
    }

    return (static_cast<double>(bidDepth) - static_cast<double>(askDepth)) /
           static_cast<double>(totalDepth);
}

bool OrderBook::canMatch(const Order& order) const {
    if (order.isMarketOrder()) {
        return order.getSide() == Side::buy ? !asks_.empty() : !bids_.empty();
    }

    return canMatch(order.getSide(), order.getPrice());
}

bool OrderBook::canMatch(Side side, Price price) const {
    if (side == Side::buy) {
        if (asks_.empty()) {
            return false;
        }

        return price >= asks_.begin()->first;
    }

    if (bids_.empty()) {
        return false;
    }

    return price <= bids_.begin()->first;
}

bool OrderBook::canFullyFill(const Order& order) const {
    Quantity available = 0;
    Quantity needed = order.getRemainingQuantity();

    if (order.getSide() == Side::buy) {
        for (const auto& [askPrice, orders] : asks_) {
            if (!order.isMarketOrder() && askPrice > order.getPrice()) {
                break;
            }

            for (const auto& restingOrder : orders) {
                available += restingOrder->getRemainingQuantity();
                if (available >= needed) {
                    return true;
                }
            }
        }
    } else {
        for (const auto& [bidPrice, orders] : bids_) {
            if (!order.isMarketOrder() && bidPrice < order.getPrice()) {
                break;
            }

            for (const auto& restingOrder : orders) {
                available += restingOrder->getRemainingQuantity();
                if (available >= needed) {
                    return true;
                }
            }
        }
    }

    return false;
}

bool OrderBook::wouldTriggerStop(const Order& order, Price lastTradePrice) const {
    if (order.getSide() == Side::buy) {
        return lastTradePrice >= order.getStopPrice();
    }

    return lastTradePrice <= order.getStopPrice();
}

void OrderBook::processIncomingOrder(OrderPointer order) {
    matchIncomingOrder(order);

    if (order->isFilled()) {
        order->setStatus(OrderStatus::filled);
        addExecutionReport(order, ExecutionReportType::filled, OrderStatus::filled);
        return;
    }

    if (order->getFilledQuantity() > 0) {
        order->setStatus(OrderStatus::partiallyFilled);
        addExecutionReport(order, ExecutionReportType::partiallyFilled, OrderStatus::partiallyFilled);
    }

    if (order->isMarketOrder() || order->isImmediateOrCancel() || order->getOrderType() == OrderType::fillOrKill) {
        if (order->getRemainingQuantity() > 0) {
            order->setStatus(OrderStatus::canceled);
            addExecutionReport(order, ExecutionReportType::canceled, OrderStatus::canceled, "Unfilled remainder canceled.");
        }
        return;
    }

    restOrder(order);
}

void OrderBook::restOrder(OrderPointer order) {
    OrderPointers* level = nullptr;

    if (order->getSide() == Side::buy) {
        level = &bids_[order->getPrice()];
    } else {
        level = &asks_[order->getPrice()];
    }

    level->push_back(order);
    auto orderIt = std::prev(level->end());

    orderLocations_[order->getId()] = OrderLocation{
        order->getSide(),
        order->getPrice(),
        orderIt
    };

    if (order->getFilledQuantity() > 0) {
        order->setStatus(OrderStatus::partiallyFilled);
        addExecutionReport(order, ExecutionReportType::partiallyFilled, OrderStatus::partiallyFilled, "Partially filled order is resting with remaining quantity.");
    } else {
        order->setStatus(OrderStatus::resting);
        addExecutionReport(order, ExecutionReportType::accepted, OrderStatus::resting, "Order resting on book.");
    }
}

void OrderBook::matchIncomingOrder(OrderPointer incomingOrder) {
    while (incomingOrder->getRemainingQuantity() > 0) {
        if (incomingOrder->getSide() == Side::buy) {
            if (asks_.empty()) {
                break;
            }

            auto bestAskIt = asks_.begin();
            Price bestAskPrice = bestAskIt->first;

            if (!incomingOrder->isMarketOrder() && incomingOrder->getPrice() < bestAskPrice) {
                break;
            }

            OrderPointers& askOrders = bestAskIt->second;
            OrderPointer restingAsk = askOrders.front();

            Quantity quantityToFill = std::min(
                incomingOrder->getRemainingQuantity(),
                restingAsk->getRemainingQuantity()
            );

            trades_.push_back(Trade{
                nextTradeId_++,
                incomingOrder->getId(),
                restingAsk->getId(),
                bestAskPrice,
                quantityToFill,
                ++currentTimestamp_
            });

            incomingOrder->fill(quantityToFill);
            restingAsk->fill(quantityToFill);

            if (restingAsk->isFilled()) {
                restingAsk->setStatus(OrderStatus::filled);
                addExecutionReport(restingAsk, ExecutionReportType::filled, OrderStatus::filled);
                orderLocations_.erase(restingAsk->getId());
                askOrders.pop_front();
            } else {
                restingAsk->setStatus(OrderStatus::partiallyFilled);
                addExecutionReport(restingAsk, ExecutionReportType::partiallyFilled, OrderStatus::partiallyFilled);
            }

            if (askOrders.empty()) {
                asks_.erase(bestAskIt);
            }

            triggerStopOrders(bestAskPrice);
        } else {
            if (bids_.empty()) {
                break;
            }

            auto bestBidIt = bids_.begin();
            Price bestBidPrice = bestBidIt->first;

            if (!incomingOrder->isMarketOrder() && incomingOrder->getPrice() > bestBidPrice) {
                break;
            }

            OrderPointers& bidOrders = bestBidIt->second;
            OrderPointer restingBid = bidOrders.front();

            Quantity quantityToFill = std::min(
                incomingOrder->getRemainingQuantity(),
                restingBid->getRemainingQuantity()
            );

            trades_.push_back(Trade{
                nextTradeId_++,
                restingBid->getId(),
                incomingOrder->getId(),
                bestBidPrice,
                quantityToFill,
                ++currentTimestamp_
            });

            incomingOrder->fill(quantityToFill);
            restingBid->fill(quantityToFill);

            if (restingBid->isFilled()) {
                restingBid->setStatus(OrderStatus::filled);
                addExecutionReport(restingBid, ExecutionReportType::filled, OrderStatus::filled);
                orderLocations_.erase(restingBid->getId());
                bidOrders.pop_front();
            } else {
                restingBid->setStatus(OrderStatus::partiallyFilled);
                addExecutionReport(restingBid, ExecutionReportType::partiallyFilled, OrderStatus::partiallyFilled);
            }

            if (bidOrders.empty()) {
                bids_.erase(bestBidIt);
            }

            triggerStopOrders(bestBidPrice);
        }
    }
}

void OrderBook::removeRestingOrder(OrderId orderId, bool reportCancel) {
    auto locationIt = orderLocations_.find(orderId);

    if (locationIt == orderLocations_.end()) {
        return;
    }

    OrderLocation location = locationIt->second;
    OrderPointer order = *location.iterator_;

    if (location.side_ == Side::buy) {
        auto levelIt = bids_.find(location.price_);

        if (levelIt != bids_.end()) {
            levelIt->second.erase(location.iterator_);

            if (levelIt->second.empty()) {
                bids_.erase(levelIt);
            }
        }
    } else {
        auto levelIt = asks_.find(location.price_);

        if (levelIt != asks_.end()) {
            levelIt->second.erase(location.iterator_);

            if (levelIt->second.empty()) {
                asks_.erase(levelIt);
            }
        }
    }

    orderLocations_.erase(locationIt);

    if (reportCancel) {
        order->setStatus(OrderStatus::canceled);
        addExecutionReport(order, ExecutionReportType::canceled, OrderStatus::canceled);
    }
}

void OrderBook::triggerStopOrders(Price lastTradePrice) {
    std::vector<OrderPointer> triggeredOrders;
    std::vector<OrderPointer> remainingStopOrders;

    for (const auto& stopOrder : stopOrders_) {
        if (wouldTriggerStop(*stopOrder, lastTradePrice)) {
            triggeredOrders.push_back(stopOrder);
        } else {
            remainingStopOrders.push_back(stopOrder);
        }
    }

    stopOrders_ = std::move(remainingStopOrders);

    for (auto& order : triggeredOrders) {
        if (order->getOrderType() == OrderType::stopLoss) {
            order->setOrderType(OrderType::market);
        } else {
            order->setOrderType(OrderType::goodTillCancel);
        }

        order->setTimestamp(++currentTimestamp_);
        processIncomingOrder(order);
    }
}

Quantity OrderBook::totalQuantityAtLevels(const LevelInfos& levels, std::size_t depth) const {
    Quantity total = 0;
    std::size_t count = std::min(depth, levels.size());

    for (std::size_t i = 0; i < count; ++i) {
        total += levels[i].quantity_;
    }

    return total;
}

LevelInfos OrderBook::getTopNLevels(const LevelInfos& levels, std::size_t n) const {
    LevelInfos result;
    std::size_t count = std::min(n, levels.size());

    result.reserve(count);

    for (std::size_t i = 0; i < count; ++i) {
        result.push_back(levels[i]);
    }

    return result;
}

void OrderBook::addExecutionReport(
    OrderPointer order,
    ExecutionReportType type,
    OrderStatus status,
    const std::string& reason
) {
    executionReports_.push_back(ExecutionReport{
        order->getId(),
        type,
        status,
        order->getPrice(),
        order->getFilledQuantity(),
        order->getRemainingQuantity(),
        ++currentTimestamp_,
        reason
    });
}
