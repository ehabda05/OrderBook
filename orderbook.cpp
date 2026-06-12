#include "orderbook.hpp"

void OrderBook::addOrder(OrderPointer order) {
    // Implementation for adding an order
}

void OrderBook::cancelOrder(OrderId orderId) {
    // Implementation for canceling an order
}

void OrderBook::modifyOrder(const OrderModify& order) {
    // Implementation for modifying an order
}

std::size_t OrderBook::size() const {
    // Implementation for getting the size of the order book
}

OrderBookLevelInfos OrderBook::getOrderInfos() const {
    // Implementation for getting order infos
}
bool OrderBook::canMatch(Side side, Price price) const {
    // Implementation for checking if an order can be matched
}
void OrderBook::matchOrders() {
    // Implementation for matching orders
}
