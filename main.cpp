#include "orderbook.hpp"
#include <iostream>

int main() {
    OrderBook book;

    book.addOrder(std::make_shared<Order>(1, OrderType::goodTillCancel, Side::sell, 100, 10));
    book.addOrder(std::make_shared<Order>(2,OrderType::goodTillCancel, Side::buy, 101, 5));

    std::cout << "Book size: " << book.size() << "\n";

    OrderBookLevelInfos infos = book.getOrderInfos();
    std::cout << "Asks:\n";
    for (const auto& level : infos.getAsks()) {
        std::cout << "Price: " << level.price_
                  << ", Quantity: " << level.quantity_ << "\n";
    }

    std::cout << "Bids:\n";
    for (const auto& level : infos.getBids()) {
        std::cout << "Price: " << level.price_
                  << ", Quantity: " << level.quantity_ << "\n";
    }

    return 0;
}