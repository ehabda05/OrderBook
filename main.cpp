#include "orderbook.hpp"

#include <iostream>
#include <memory>

void printBook(const OrderBook& book) {
    OrderBookLevelInfos infos = book.getOrderInfos();

    std::cout << "\nASKS:\n";
    for (const auto& ask : infos.getAsks()) {
        std::cout << "Price: " << ask.price_
                  << " Quantity: " << ask.quantity_ << '\n';
    }

    std::cout << "\nBIDS:\n";
    for (const auto& bid : infos.getBids()) {
        std::cout << "Price: " << bid.price_
                  << " Quantity: " << bid.quantity_ << '\n';
    }

    std::cout << "\nOpen orders: " << book.size() << '\n';
}

void printTrades(const OrderBook& book) {
    const auto& trades = book.getTrades();

    std::cout << "\nTRADES:\n";

    if (trades.empty()) {
        std::cout << "No trades.\n";
        return;
    }

    for (const auto& trade : trades) {
        std::cout << "Bid Order ID: " << trade.bidOrderId_
                  << " Ask Order ID: " << trade.askOrderId_
                  << " Price: " << trade.price_
                  << " Quantity: " << trade.quantity_
                  << '\n';
    }
}

int main() {
    OrderBook book;

    book.addOrder(std::make_shared<Order>(
        1,
        OrderType::goodTillCancel,
        Side::sell,
        100,
        10
    ));

    book.addOrder(std::make_shared<Order>(
        2,
        OrderType::goodTillCancel,
        Side::sell,
        101,
        5
    ));

    book.addOrder(std::make_shared<Order>(
        3,
        OrderType::goodTillCancel,
        Side::buy,
        99,
        7
    ));

    std::cout << "Initial book:\n";
    printBook(book);
    printTrades(book);

    book.addOrder(std::make_shared<Order>(
        4,
        OrderType::goodTillCancel,
        Side::buy,
        105,
        12
    ));

    std::cout << "\nAfter buy order 4 at 105 for quantity 12:\n";
    printBook(book);
    printTrades(book);

    book.addOrder(std::make_shared<Order>(
        5,
        OrderType::fillAndKill,
        Side::buy,
        101,
        10
    ));

    std::cout << "\nAfter fillAndKill buy order 5 at 101 for quantity 10:\n";
    printBook(book);
    printTrades(book);

    book.addOrder(std::make_shared<Order>(
        6,
        OrderType::goodTillCancel,
        Side::buy,
        105,
        20
    ));

    book.addOrder(std::make_shared<Order>(
        7,
        OrderType::goodTillCancel,
        Side::sell,
        100,
        4
    ));

    std::cout << "\nAfter sell order 7 at 100 hits resting bid 105:\n";
    printBook(book);
    printTrades(book);

    OrderModify modifyOrder{
        3,
        102,
        20,
        Side::buy
    };

    book.modifyOrder(modifyOrder);

    std::cout << "\nAfter modifying order 3 to buy at 102 for quantity 20:\n";
    printBook(book);
    printTrades(book);

    book.cancelOrder(3);

    std::cout << "\nAfter canceling order 3:\n";
    printBook(book);
    printTrades(book);

    return 0;
}