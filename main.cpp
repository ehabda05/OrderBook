#include "orderbook.hpp"

#include <iostream>
#include <memory>
#include <optional>
#include <string>

std::string sideToString(Side side) {
    switch (side) {
        case Side::buy:
            return "BUY";
        case Side::sell:
            return "SELL";
    }

    return "UNKNOWN";
}

std::string orderStatusToString(OrderStatus status) {
    switch (status) {
        case OrderStatus::accepted:
            return "ACCEPTED";
        case OrderStatus::rejected:
            return "REJECTED";
        case OrderStatus::resting:
            return "RESTING";
        case OrderStatus::partiallyFilled:
            return "PARTIALLY_FILLED";
        case OrderStatus::filled:
            return "FILLED";
        case OrderStatus::canceled:
            return "CANCELED";
        case OrderStatus::modified:
            return "MODIFIED";
        case OrderStatus::stopPending:
            return "STOP_PENDING";
    }

    return "UNKNOWN";
}

std::string executionReportTypeToString(ExecutionReportType type) {
    switch (type) {
        case ExecutionReportType::accepted:
            return "ACCEPTED";
        case ExecutionReportType::rejected:
            return "REJECTED";
        case ExecutionReportType::filled:
            return "FILLED";
        case ExecutionReportType::partiallyFilled:
            return "PARTIALLY_FILLED";
        case ExecutionReportType::canceled:
            return "CANCELED";
        case ExecutionReportType::modified:
            return "MODIFIED";
    }

    return "UNKNOWN";
}

void printOptionalPrice(const std::string& label, const std::optional<Price>& value) {
    std::cout << label << ": ";

    if (value.has_value()) {
        std::cout << *value << '\n';
    } else {
        std::cout << "N/A\n";
    }
}

void printOptionalDouble(const std::string& label, const std::optional<double>& value) {
    std::cout << label << ": ";

    if (value.has_value()) {
        std::cout << *value << '\n';
    } else {
        std::cout << "N/A\n";
    }
}

void printBook(const OrderBook& book) {
    OrderBookLevelInfos infos = book.getOrderInfos();

    std::cout << "\n========== ORDER BOOK ==========\n";

    std::cout << "\nASKS:\n";
    if (infos.getAsks().empty()) {
        std::cout << "No asks.\n";
    } else {
        for (const auto& ask : infos.getAsks()) {
            std::cout << "Price: " << ask.price_
                      << " Quantity: " << ask.quantity_ << '\n';
        }
    }

    std::cout << "\nBIDS:\n";
    if (infos.getBids().empty()) {
        std::cout << "No bids.\n";
    } else {
        for (const auto& bid : infos.getBids()) {
            std::cout << "Price: " << bid.price_
                      << " Quantity: " << bid.quantity_ << '\n';
        }
    }

    std::cout << "\nOpen orders: " << book.size() << '\n';
}

void printTrades(const OrderBook& book) {
    const auto& trades = book.getTrades();

    std::cout << "\n========== TRADES ==========\n";

    if (trades.empty()) {
        std::cout << "No trades.\n";
        return;
    }

    for (const auto& trade : trades) {
        std::cout << "Trade ID: " << trade.tradeId_
                  << " | Bid Order ID: " << trade.bidOrderId_
                  << " | Ask Order ID: " << trade.askOrderId_
                  << " | Price: " << trade.price_
                  << " | Quantity: " << trade.quantity_
                  << " | Timestamp: " << trade.timestamp_
                  << '\n';
    }
}

void printExecutionReports(const OrderBook& book) {
    const auto& reports = book.getExecutionReports();

    std::cout << "\n========== EXECUTION REPORTS ==========\n";

    if (reports.empty()) {
        std::cout << "No execution reports.\n";
        return;
    }

    for (const auto& report : reports) {
        std::cout << "Order ID: " << report.orderId_
                  << " | Type: " << executionReportTypeToString(report.type_)
                  << " | Status: " << orderStatusToString(report.status_)
                  << " | Price: " << report.price_
                  << " | Filled: " << report.filledQuantity_
                  << " | Remaining: " << report.remainingQuantity_
                  << " | Timestamp: " << report.timestamp_;

        if (!report.reason_.empty()) {
            std::cout << " | Reason: " << report.reason_;
        }

        std::cout << '\n';
    }
}

void printAnalytics(const OrderBook& book) {
    std::cout << "\n========== MARKET ANALYTICS ==========\n";

    printOptionalPrice("Best bid", book.getBestBid());
    printOptionalPrice("Best ask", book.getBestAsk());
    printOptionalPrice("Spread", book.getSpread());
    printOptionalDouble("Mid price", book.getMidPrice());
    printOptionalDouble("Top-3 imbalance", book.getImbalance(3));

    std::cout << "\nTop 3 bids:\n";
    LevelInfos topBids = book.getTopNBids(3);

    if (topBids.empty()) {
        std::cout << "No bid depth.\n";
    } else {
        for (const auto& level : topBids) {
            std::cout << "Price: " << level.price_
                      << " Quantity: " << level.quantity_ << '\n';
        }
    }

    std::cout << "\nTop 3 asks:\n";
    LevelInfos topAsks = book.getTopNAsks(3);

    if (topAsks.empty()) {
        std::cout << "No ask depth.\n";
    } else {
        for (const auto& level : topAsks) {
            std::cout << "Price: " << level.price_
                      << " Quantity: " << level.quantity_ << '\n';
        }
    }
}

void printSnapshot(const std::string& title, const OrderBook& book) {
    std::cout << "\n\n========================================\n";
    std::cout << title << '\n';
    std::cout << "========================================\n";

    printBook(book);
    printTrades(book);
    printAnalytics(book);
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

    printSnapshot("Initial book", book);

    book.addOrder(std::make_shared<Order>(
        4,
        OrderType::goodTillCancel,
        Side::buy,
        105,
        12
    ));

    printSnapshot("After limit buy order 4 at 105 for quantity 12", book);

    book.addOrder(std::make_shared<Order>(
        5,
        OrderType::immediateOrCancel,
        Side::buy,
        101,
        10
    ));

    printSnapshot("After IOC buy order 5 at 101 for quantity 10", book);

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

    printSnapshot("After sell order 7 at 100 hits resting bid 105", book);

    OrderModify modifyOrder{
        3,
        102,
        20,
        Side::buy
    };

    book.modifyOrder(modifyOrder);

    printSnapshot("After modifying order 3 to buy at 102 for quantity 20", book);

    book.cancelOrder(3);

    printSnapshot("After canceling order 3", book);

    book.addOrder(std::make_shared<Order>(
        8,
        OrderType::goodTillCancel,
        Side::sell,
        110,
        10
    ));

    printSnapshot("After adding resting sell order 8 at 110", book);

    book.addOrder(std::make_shared<Order>(
        9,
        OrderType::postOnly,
        Side::buy,
        110,
        5
    ));

    printSnapshot("After rejected post-only buy order 9 at 110", book);

    book.addOrder(std::make_shared<Order>(
        10,
        OrderType::postOnly,
        Side::buy,
        98,
        10
    ));

    printSnapshot("After accepted post-only buy order 10 at 98", book);

    book.addOrder(std::make_shared<Order>(
        11,
        OrderType::market,
        Side::sell,
        0,
        5
    ));

    printSnapshot("After market sell order 11 for quantity 5", book);

    book.addOrder(std::make_shared<Order>(
        12,
        OrderType::fillOrKill,
        Side::buy,
        101,
        100
    ));

    printSnapshot("After rejected FOK buy order 12 that cannot fully fill", book);

    book.addOrder(std::make_shared<Order>(
        13,
        OrderType::fillOrKill,
        Side::buy,
        110,
        5
    ));

    printSnapshot("After successful FOK buy order 13 at 110 for quantity 5", book);

    printExecutionReports(book);

    return 0;
}