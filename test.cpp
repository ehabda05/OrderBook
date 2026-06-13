#include "orderbook.hpp"

#include <cassert>
#include <iostream>
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
}

void testFullMatchRemovesBothOrders() {
    OrderBook book;

    book.addOrder(makeOrder(1, OrderType::goodTillCancel, Side::sell, 100, 10));
    book.addOrder(makeOrder(2, OrderType::goodTillCancel, Side::buy, 100, 10));

    assert(book.size() == 0);

    OrderBookLevelInfos infos = book.getOrderInfos();
    assert(infos.getBids().empty());
    assert(infos.getAsks().empty());
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
}

void testCancelOrder() {
    OrderBook book;

    book.addOrder(makeOrder(1, OrderType::goodTillCancel, Side::buy, 100, 10));
    book.addOrder(makeOrder(2, OrderType::goodTillCancel, Side::sell, 105, 20));

    book.cancelOrder(1);
    book.cancelOrder(999); // should be harmless

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
}

void testFillAndKillPartialMatchCancelsRemainder() {
    OrderBook book;

    book.addOrder(makeOrder(1, OrderType::goodTillCancel, Side::sell, 100, 5));
    book.addOrder(makeOrder(2, OrderType::fillAndKill, Side::buy, 100, 10));

    assert(book.size() == 0);

    OrderBookLevelInfos infos = book.getOrderInfos();
    assert(infos.getBids().empty());
    assert(infos.getAsks().empty());
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

} // namespace

int main() {
    runTest("add non-crossing orders", testAddNonCrossingOrders);
    runTest("aggregate same price level", testAddAggregatesSamePriceLevel);
    runTest("full match removes both orders", testFullMatchRemovesBothOrders);
    runTest("partial match leaves remaining quantity", testPartialMatchLeavesRemainingQuantity);
    runTest("best ask is matched first", testBetterBidMatchesBestAskFirst);
    runTest("cancel order", testCancelOrder);
    runTest("modify order", testModifyOrder);
    runTest("modify missing order does nothing", testModifyMissingOrderDoesNothing);
    runTest("fill-and-kill unmatched does not rest", testFillAndKillUnmatchedDoesNotRest);
    runTest("fill-and-kill partial match cancels remainder", testFillAndKillPartialMatchCancelsRemainder);
    runTest("duplicate order id throws", testDuplicateOrderIdThrows);
    runTest("null order throws", testNullOrderThrows);
    runTest("order fill rejects overfill", testOrderFillRejectsOverfill);

    std::cout << "\nAll OrderBook unit tests passed.\n";
    return 0;
}
