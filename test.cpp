#include "orderbook.hpp"

#include <cassert>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

namespace {

OrderPointer makeOrder(
    OrderId id,
    OrderType type,
    Side side,
    Price price,
    Quantity quantity
) {
    return std::make_shared<Order>(id, type, side, price, quantity);
}

const LevelInfo* findLevel(const LevelInfos& levels, Price price) {
    for (const auto& level : levels) {
        if (level.price_ == price) {
            return &level;
        }
    }

    return nullptr;
}

void expectLevel(const LevelInfos& levels, Price price, Quantity quantity) {
    const LevelInfo* level = findLevel(levels, price);

    assert(level != nullptr);
    assert(level->quantity_ == quantity);
}

void expectNoLevel(const LevelInfos& levels, Price price) {
    assert(findLevel(levels, price) == nullptr);
}

void testAddNonCrossingOrders() {
    OrderBook book;

    book.addOrder(makeOrder(1, OrderType::goodTillCancel, Side::buy, 99, 10));
    book.addOrder(makeOrder(2, OrderType::goodTillCancel, Side::sell, 101, 15));

    assert(book.size() == 2);

    OrderBookLevelInfos infos = book.getOrderInfos();

    assert(infos.getBids().size() == 1);
    assert(infos.getAsks().size() == 1);

    expectLevel(infos.getBids(), 99, 10);
    expectLevel(infos.getAsks(), 101, 15);

    assert(book.getTrades().empty());
}

void testAddAggregatesSamePriceLevel() {
    OrderBook book;

    book.addOrder(makeOrder(1, OrderType::goodTillCancel, Side::buy, 100, 10));
    book.addOrder(makeOrder(2, OrderType::goodTillCancel, Side::buy, 100, 20));
    book.addOrder(makeOrder(3, OrderType::goodTillCancel, Side::sell, 105, 7));
    book.addOrder(makeOrder(4, OrderType::goodTillCancel, Side::sell, 105, 8));

    assert(book.size() == 4);

    OrderBookLevelInfos infos = book.getOrderInfos();

    assert(infos.getBids().size() == 1);
    assert(infos.getAsks().size() == 1);

    expectLevel(infos.getBids(), 100, 30);
    expectLevel(infos.getAsks(), 105, 15);

    assert(book.getTrades().empty());
}

void testBestBidOrdering() {
    OrderBook book;

    book.addOrder(makeOrder(1, OrderType::goodTillCancel, Side::buy, 99, 10));
    book.addOrder(makeOrder(2, OrderType::goodTillCancel, Side::buy, 105, 20));
    book.addOrder(makeOrder(3, OrderType::goodTillCancel, Side::buy, 101, 30));

    OrderBookLevelInfos infos = book.getOrderInfos();

    assert(infos.getBids().size() == 3);

    assert(infos.getBids()[0].price_ == 105);
    assert(infos.getBids()[0].quantity_ == 20);

    assert(infos.getBids()[1].price_ == 101);
    assert(infos.getBids()[1].quantity_ == 30);

    assert(infos.getBids()[2].price_ == 99);
    assert(infos.getBids()[2].quantity_ == 10);
}

void testBestAskOrdering() {
    OrderBook book;

    book.addOrder(makeOrder(1, OrderType::goodTillCancel, Side::sell, 105, 10));
    book.addOrder(makeOrder(2, OrderType::goodTillCancel, Side::sell, 101, 20));
    book.addOrder(makeOrder(3, OrderType::goodTillCancel, Side::sell, 103, 30));

    OrderBookLevelInfos infos = book.getOrderInfos();

    assert(infos.getAsks().size() == 3);

    assert(infos.getAsks()[0].price_ == 101);
    assert(infos.getAsks()[0].quantity_ == 20);

    assert(infos.getAsks()[1].price_ == 103);
    assert(infos.getAsks()[1].quantity_ == 30);

    assert(infos.getAsks()[2].price_ == 105);
    assert(infos.getAsks()[2].quantity_ == 10);
}

void testFullMatchRemovesBothOrders() {
    OrderBook book;

    book.addOrder(makeOrder(1, OrderType::goodTillCancel, Side::sell, 100, 10));
    book.addOrder(makeOrder(2, OrderType::goodTillCancel, Side::buy, 100, 10));

    assert(book.size() == 0);

    OrderBookLevelInfos infos = book.getOrderInfos();

    assert(infos.getBids().empty());
    assert(infos.getAsks().empty());

    const auto& trades = book.getTrades();

    assert(trades.size() == 1);
    assert(trades[0].bidOrderId_ == 2);
    assert(trades[0].askOrderId_ == 1);
    assert(trades[0].price_ == 100);
    assert(trades[0].quantity_ == 10);
}

void testPartialMatchLeavesRemainingQuantity() {
    OrderBook book;

    book.addOrder(makeOrder(1, OrderType::goodTillCancel, Side::sell, 100, 10));
    book.addOrder(makeOrder(2, OrderType::goodTillCancel, Side::buy, 101, 4));

    assert(book.size() == 1);

    OrderBookLevelInfos infos = book.getOrderInfos();

    assert(infos.getBids().empty());
    assert(infos.getAsks().size() == 1);

    expectLevel(infos.getAsks(), 100, 6);

    const auto& trades = book.getTrades();

    assert(trades.size() == 1);
    assert(trades[0].bidOrderId_ == 2);
    assert(trades[0].askOrderId_ == 1);
    assert(trades[0].price_ == 100);
    assert(trades[0].quantity_ == 4);
}

void testBetterBidMatchesBestAskFirst() {
    OrderBook book;

    book.addOrder(makeOrder(1, OrderType::goodTillCancel, Side::sell, 101, 5));
    book.addOrder(makeOrder(2, OrderType::goodTillCancel, Side::sell, 100, 7));
    book.addOrder(makeOrder(3, OrderType::goodTillCancel, Side::buy, 101, 7));

    assert(book.size() == 1);

    OrderBookLevelInfos infos = book.getOrderInfos();

    assert(infos.getBids().empty());
    assert(infos.getAsks().size() == 1);

    expectLevel(infos.getAsks(), 101, 5);
    expectNoLevel(infos.getAsks(), 100);

    const auto& trades = book.getTrades();

    assert(trades.size() == 1);
    assert(trades[0].bidOrderId_ == 3);
    assert(trades[0].askOrderId_ == 2);
    assert(trades[0].price_ == 100);
    assert(trades[0].quantity_ == 7);
}

void testIncomingBuyTradesAtRestingAskPrice() {
    OrderBook book;

    book.addOrder(makeOrder(1, OrderType::goodTillCancel, Side::sell, 100, 10));
    book.addOrder(makeOrder(2, OrderType::goodTillCancel, Side::buy, 105, 4));

    const auto& trades = book.getTrades();

    assert(trades.size() == 1);
    assert(trades[0].bidOrderId_ == 2);
    assert(trades[0].askOrderId_ == 1);
    assert(trades[0].price_ == 100);
    assert(trades[0].quantity_ == 4);
}

void testIncomingSellTradesAtRestingBidPrice() {
    OrderBook book;

    book.addOrder(makeOrder(1, OrderType::goodTillCancel, Side::buy, 105, 10));
    book.addOrder(makeOrder(2, OrderType::goodTillCancel, Side::sell, 100, 4));

    const auto& trades = book.getTrades();

    assert(trades.size() == 1);
    assert(trades[0].bidOrderId_ == 1);
    assert(trades[0].askOrderId_ == 2);
    assert(trades[0].price_ == 105);
    assert(trades[0].quantity_ == 4);
}

void testPriceTimePrioritySameLevel() {
    OrderBook book;

    book.addOrder(makeOrder(1, OrderType::goodTillCancel, Side::sell, 100, 5));
    book.addOrder(makeOrder(2, OrderType::goodTillCancel, Side::sell, 100, 5));

    book.addOrder(makeOrder(3, OrderType::goodTillCancel, Side::buy, 100, 7));

    const auto& trades = book.getTrades();

    assert(trades.size() == 2);

    assert(trades[0].bidOrderId_ == 3);
    assert(trades[0].askOrderId_ == 1);
    assert(trades[0].price_ == 100);
    assert(trades[0].quantity_ == 5);

    assert(trades[1].bidOrderId_ == 3);
    assert(trades[1].askOrderId_ == 2);
    assert(trades[1].price_ == 100);
    assert(trades[1].quantity_ == 2);

    OrderBookLevelInfos infos = book.getOrderInfos();

    assert(book.size() == 1);
    assert(infos.getBids().empty());
    assert(infos.getAsks().size() == 1);

    expectLevel(infos.getAsks(), 100, 3);
}

void testCancelOrder() {
    OrderBook book;

    book.addOrder(makeOrder(1, OrderType::goodTillCancel, Side::buy, 100, 10));
    book.addOrder(makeOrder(2, OrderType::goodTillCancel, Side::sell, 105, 20));

    book.cancelOrder(1);
    book.cancelOrder(999);

    assert(book.size() == 1);

    OrderBookLevelInfos infos = book.getOrderInfos();

    assert(infos.getBids().empty());
    assert(infos.getAsks().size() == 1);

    expectLevel(infos.getAsks(), 105, 20);
}

void testModifyOrder() {
    OrderBook book;

    book.addOrder(makeOrder(1, OrderType::goodTillCancel, Side::buy, 100, 10));

    OrderModify modify{1, 99, 25, Side::sell};

    book.modifyOrder(modify);

    assert(book.size() == 1);

    OrderBookLevelInfos infos = book.getOrderInfos();

    assert(infos.getBids().empty());
    assert(infos.getAsks().size() == 1);

    expectLevel(infos.getAsks(), 99, 25);
}

void testModifyOrderCanTriggerMatch() {
    OrderBook book;

    book.addOrder(makeOrder(1, OrderType::goodTillCancel, Side::buy, 100, 10));
    book.addOrder(makeOrder(2, OrderType::goodTillCancel, Side::sell, 105, 5));

    OrderModify modify{2, 99, 5, Side::sell};

    book.modifyOrder(modify);

    assert(book.size() == 1);

    OrderBookLevelInfos infos = book.getOrderInfos();

    assert(infos.getAsks().empty());
    assert(infos.getBids().size() == 1);

    expectLevel(infos.getBids(), 100, 5);

    const auto& trades = book.getTrades();

    assert(trades.size() == 1);
    assert(trades[0].bidOrderId_ == 1);
    assert(trades[0].askOrderId_ == 2);
    assert(trades[0].price_ == 100);
    assert(trades[0].quantity_ == 5);
}

void testModifyMissingOrderDoesNothing() {
    OrderBook book;

    book.addOrder(makeOrder(1, OrderType::goodTillCancel, Side::buy, 100, 10));

    OrderModify modify{999, 90, 50, Side::sell};

    book.modifyOrder(modify);

    assert(book.size() == 1);

    OrderBookLevelInfos infos = book.getOrderInfos();

    assert(infos.getBids().size() == 1);
    assert(infos.getAsks().empty());

    expectLevel(infos.getBids(), 100, 10);
}

void testFillAndKillUnmatchedDoesNotRest() {
    OrderBook book;

    book.addOrder(makeOrder(1, OrderType::fillAndKill, Side::buy, 100, 10));

    assert(book.size() == 0);

    OrderBookLevelInfos infos = book.getOrderInfos();

    assert(infos.getBids().empty());
    assert(infos.getAsks().empty());
    assert(book.getTrades().empty());
}

void testFillAndKillPartialMatchCancelsRemainder() {
    OrderBook book;

    book.addOrder(makeOrder(1, OrderType::goodTillCancel, Side::sell, 100, 5));
    book.addOrder(makeOrder(2, OrderType::fillAndKill, Side::buy, 100, 10));

    assert(book.size() == 0);

    OrderBookLevelInfos infos = book.getOrderInfos();

    assert(infos.getBids().empty());
    assert(infos.getAsks().empty());

    const auto& trades = book.getTrades();

    assert(trades.size() == 1);
    assert(trades[0].bidOrderId_ == 2);
    assert(trades[0].askOrderId_ == 1);
    assert(trades[0].price_ == 100);
    assert(trades[0].quantity_ == 5);
}

void testFillAndKillFullMatch() {
    OrderBook book;

    book.addOrder(makeOrder(1, OrderType::goodTillCancel, Side::sell, 100, 5));
    book.addOrder(makeOrder(2, OrderType::fillAndKill, Side::buy, 100, 5));

    assert(book.size() == 0);

    OrderBookLevelInfos infos = book.getOrderInfos();

    assert(infos.getBids().empty());
    assert(infos.getAsks().empty());

    const auto& trades = book.getTrades();

    assert(trades.size() == 1);
    assert(trades[0].bidOrderId_ == 2);
    assert(trades[0].askOrderId_ == 1);
    assert(trades[0].price_ == 100);
    assert(trades[0].quantity_ == 5);
}

void testDuplicateOrderIdThrows() {
    OrderBook book;

    book.addOrder(makeOrder(1, OrderType::goodTillCancel, Side::buy, 100, 10));

    bool threw = false;

    try {
        book.addOrder(makeOrder(1, OrderType::goodTillCancel, Side::sell, 105, 20));
    } catch (const std::logic_error&) {
        threw = true;
    }

    assert(threw);
    assert(book.size() == 1);
}

void testNullOrderThrows() {
    OrderBook book;

    bool threw = false;

    try {
        book.addOrder(nullptr);
    } catch (const std::logic_error&) {
        threw = true;
    }

    assert(threw);
    assert(book.size() == 0);
}

void testZeroQuantityOrderThrows() {
    bool threw = false;

    try {
        Order order(1, OrderType::goodTillCancel, Side::buy, 100, 0);
    } catch (const std::logic_error&) {
        threw = true;
    }

    assert(threw);
}

void testModifyZeroQuantityThrows() {
    OrderBook book;

    book.addOrder(makeOrder(1, OrderType::goodTillCancel, Side::buy, 100, 10));

    bool threw = false;

    try {
        OrderModify modify{1, 100, 0, Side::buy};
        book.modifyOrder(modify);
    } catch (const std::logic_error&) {
        threw = true;
    }

    assert(threw);
    assert(book.size() == 1);

    OrderBookLevelInfos infos = book.getOrderInfos();

    assert(infos.getBids().size() == 1);
    expectLevel(infos.getBids(), 100, 10);
}

void testOrderFillRejectsOverfill() {
    Order order(1, OrderType::goodTillCancel, Side::buy, 100, 10);

    order.fill(4);

    assert(order.getRemainingQuantity() == 6);
    assert(order.getFilledQuantity() == 4);
    assert(!order.isFilled());

    bool threw = false;

    try {
        order.fill(7);
    } catch (const std::logic_error&) {
        threw = true;
    }

    assert(threw);
    assert(order.getRemainingQuantity() == 6);
}

void runTest(const std::string& name, void (*test)()) {
    test();
    std::cout << "[PASS] " << name << '\n';
}

}

int main() {
    runTest("add non-crossing orders", testAddNonCrossingOrders);
    runTest("aggregate same price level", testAddAggregatesSamePriceLevel);
    runTest("best bid ordering", testBestBidOrdering);
    runTest("best ask ordering", testBestAskOrdering);
    runTest("full match removes both orders", testFullMatchRemovesBothOrders);
    runTest("partial match leaves remaining quantity", testPartialMatchLeavesRemainingQuantity);
    runTest("best ask is matched first", testBetterBidMatchesBestAskFirst);
    runTest("incoming buy trades at resting ask price", testIncomingBuyTradesAtRestingAskPrice);
    runTest("incoming sell trades at resting bid price", testIncomingSellTradesAtRestingBidPrice);
    runTest("price-time priority same level", testPriceTimePrioritySameLevel);
    runTest("cancel order", testCancelOrder);
    runTest("modify order", testModifyOrder);
    runTest("modify order can trigger match", testModifyOrderCanTriggerMatch);
    runTest("modify missing order does nothing", testModifyMissingOrderDoesNothing);
    runTest("fill-and-kill unmatched does not rest", testFillAndKillUnmatchedDoesNotRest);
    runTest("fill-and-kill partial match cancels remainder", testFillAndKillPartialMatchCancelsRemainder);
    runTest("fill-and-kill full match", testFillAndKillFullMatch);
    runTest("duplicate order id throws", testDuplicateOrderIdThrows);
    runTest("null order throws", testNullOrderThrows);
    runTest("zero quantity order throws", testZeroQuantityOrderThrows);
    runTest("modify zero quantity throws", testModifyZeroQuantityThrows);
    runTest("order fill rejects overfill", testOrderFillRejectsOverfill);

    std::cout << "\nAll OrderBook unit tests passed.\n";

    return 0;
}