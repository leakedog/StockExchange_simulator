#include <cstdint>
#include <istream>
#include <iterator>
#include <limits>
#include <ostream>
#include <random>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <list>
#include <variant>
#include <memory>
#include <map>
#include <optional>
#include <glog/logging.h>
#include "enums.pb.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/numbers.h"


namespace std {
    template <>
    struct hash<std::pair<uint32_t, uint32_t>> {
        size_t operator()(const std::pair<uint32_t, uint32_t>& k) const {
            return (static_cast<size_t>(k.first) << 31) ^ k.second;
        }
    };
}


class LimitOrder { 
protected:
    OrderType type_;
    OrderSide side_;
    uint32_t id_;
    uint16_t price_;
    uint32_t quantity_;
public:
    LimitOrder (OrderSide side, uint32_t id, uint16_t price, uint32_t quantity)
                : type_(LIMIT_ORDER), side_(side), id_(id), price_(price), quantity_(quantity) {
    }

    static bool MatchesPrice(std::shared_ptr<LimitOrder> order, std::shared_ptr<LimitOrder> opposite_order) {
        if (order->side() == BUY_OS) {
            return order->price() >= opposite_order->price();
        } else {
            return order->price() <= opposite_order->price();
        }
    }

    static uint32_t CalculatePrice(std::shared_ptr<LimitOrder> order, std::shared_ptr<LimitOrder> opposite_order) {
        return opposite_order->price(); 
    }

    virtual void Fill(uint32_t other_quantity) {
        if (quantity_ < other_quantity) {
            LOG(FATAL) << absl::StrFormat("Remaining quantity of order is less than filled quantity: %u vs %u", quantity_, other_quantity);
        }
        quantity_ -= other_quantity;
    }

    OrderType type() const {
        return type_;
    }

    uint16_t price() const {
        return price_;
    }

    OrderSide side() const {
        return side_;
    }

    uint32_t& mutable_quantity() & {
        return quantity_;
    }

    uint32_t quantity() const {
        return quantity_;
    }

    uint32_t id() const {
        return id_;
    }
    
    virtual ~LimitOrder() = default;
};

class IcebergOrder : public LimitOrder {
private:
    uint32_t peak_size_;
    uint32_t hidden_full_volume_;
public:
    IcebergOrder(OrderSide side, uint32_t id, uint16_t price, uint32_t hidden_full_volume, uint32_t peak_size) 
                 : LimitOrder(side, id, price, 0), peak_size_(peak_size), hidden_full_volume_(hidden_full_volume) {
        type_ = ICEBERG_ORDER;
    }

    uint32_t peak_size() const {
        return peak_size_;
    }

    uint32_t hidden_full_volume() const {
        return hidden_full_volume_;
    }

    
    void Fill(uint32_t other_quantity) override {
        LimitOrder::Fill(other_quantity);
        hidden_full_volume_ -= other_quantity;
    }

    ~IcebergOrder() override = default;
};


class OrderParser {
public:
    static std::optional<std::unique_ptr<LimitOrder>> Parse(const std::string& line) {
        auto it = std::find_if(line.begin(), line.end(), [](char c) {
            return !std::isspace(c);
        });
        std::string_view considering_line(it, line.end());
        if (considering_line.size() == 0 || considering_line.starts_with("#")) {
            return std::nullopt;
        }
        std::vector<std::string> strings = absl::StrSplit(considering_line, ',');
        if (strings.size() != 4 && strings.size() != 5) {
           return std::nullopt; 
        }
        OrderSide side;
        if (strings[0] == "B")  side = BUY_OS;
        else if (strings[0] == "S") side = SELL_OS;
        else LOG(FATAL) << "Unknown side during parse: " << line;

        uint32_t id, quantity;
        uint32_t price;

        if (!absl::SimpleAtoi(strings[1], &id)) LOG(FATAL) << "Unknown id during parse: " << line;
        if (!absl::SimpleAtoi(strings[2], &price)) LOG(FATAL) << "Unknown price during parse: " << line;
        if (price >= std::numeric_limits<uint16_t>::max()) LOG(FATAL) << "Price is too large: " << line;
        if (!absl::SimpleAtoi(strings[3], &quantity)) LOG(FATAL) << "Unknown quantity during parse: " << line;
        if (strings.size() == 4) {  // limit order
            return std::make_unique<LimitOrder>(side, id, price, quantity);
        } else if (strings.size() == 5) { // iceberg order
            uint32_t peak_size;
            if (!absl::SimpleAtoi(strings[4], &peak_size)) LOG(FATAL) << "Unknown peak_size during parse: " << line;
            return std::make_unique<IcebergOrder>(side, id, price, quantity, peak_size);
        } else{
            LOG(FATAL) << "Unreachable condition reached";
        }
    }
};

struct Trade {
    uint32_t buy_id; 
    uint32_t sell_id;
    uint16_t price;
    uint32_t quantity;

    std::string ToString() const {
        return absl::StrFormat("%u,%u,%hu,%u", buy_id, sell_id, price, quantity);
    }

    friend std::ostream& operator<< (std::ostream& out, const Trade& trade) {
        out << trade.ToString();
        return out;
    }
};

struct TradeManager {
    using MapType = std::unordered_map<std::pair<uint32_t, uint32_t>, std::pair<uint16_t, uint32_t>>;
    MapType trades_;
public:
    void Append(uint32_t order_id, uint32_t opposite_order_id, OrderSide side, uint16_t price, uint32_t quantity) {
        uint32_t buy_id = (side == BUY_OS ? order_id : opposite_order_id);
        uint32_t sell_id = (side == SELL_OS ? order_id : opposite_order_id);
        MapType::iterator it;
        if ((it = trades_.find(std::make_pair(buy_id, sell_id))) != trades_.end()) {
            if (it->second.first != price) {
                LOG(FATAL) << absl::StrFormat("Price mismatch during TradeManager::Append %hu vs %hu", it->second.first, price); 
            }
            it->second.second += quantity;
        } else {
            trades_[{buy_id, sell_id}] = {price, quantity};
        }
    }

    void NotifyAll() {
        for (auto&& [ids, trade_stat] : trades_) {
            auto [buy_id, sell_id] = ids;
            auto [price, quantity] = trade_stat;
            Trade trade {.buy_id = buy_id, .sell_id = sell_id,  .price = price, .quantity = quantity};
            std::cout << trade << '\n';
        }
        trades_.clear();
    }
};


class OrderBook {
    using ListType = std::list<std::shared_ptr<LimitOrder>>;
    using OrderBookType = std::map<uint32_t, ListType>;
    OrderBookType sells_book_;
    OrderBookType buys_book_;
public:

    std::optional<std::shared_ptr<LimitOrder>> GetOpposite(const OrderSide& order_side) const {
        if  (order_side == SELL_OS) {
            if (buys_book_.empty()) {
                return std::nullopt;
            }
            return buys_book_.rbegin()->second.front();
        } else if (order_side == BUY_OS) {
            if (sells_book_.empty()) {
                return std::nullopt;
            }
            return sells_book_.begin()->second.front();
        } else {
            LOG(FATAL) << absl::StrFormat("OrderSide is not supported %s", OrderSide_Name(order_side));
            return std::nullopt;
        }
    }

    void Add(std::shared_ptr<LimitOrder> order) {
        if (order->quantity() == 0) {
            return;
        }
        auto opposite_order = GetOpposite(order->side());
        if (opposite_order && LimitOrder::MatchesPrice(order, opposite_order.value())) {
            LOG(FATAL) << absl::StrFormat("Adding order with price %hu while exists opposite order with price: %hu", order->price(), opposite_order.value()->price());
        }
        if (order->side() == SELL_OS) {
            sells_book_[order->price()].push_back(order);
        } else if (order->side() == BUY_OS) {
            buys_book_[order->price()].push_back(order);  
        }
    }

    void PopOpposite(const OrderSide& order_side) {
        auto deleted_order = GetOpposite(order_side).value();
        
        if  (order_side == SELL_OS) {
            buys_book_.rbegin()->second.pop_front();
            if (buys_book_.rbegin()->second.empty()) {
                buys_book_.erase(buys_book_.rbegin()->first);
            }
        } else if (order_side == BUY_OS) {
            sells_book_.begin()->second.pop_front();
            if (sells_book_.begin()->second.empty()) {
                sells_book_.erase(sells_book_.begin()->first);
            }
        }

        if (deleted_order->type() == ICEBERG_ORDER) {
          std::shared_ptr<IcebergOrder> iceberg_deleted_order = std::static_pointer_cast<IcebergOrder>(deleted_order);
          uint32_t new_quantity = std::min(iceberg_deleted_order->hidden_full_volume(), iceberg_deleted_order->peak_size());
          iceberg_deleted_order->mutable_quantity() = new_quantity;
          Add(iceberg_deleted_order);
        }
    }

    std::string ToString() const {
        std::ostringstream result;

        // Header
        result << "+-----------------------------------------------------------------+\n";
        result << "| BUY                            | SELL                           |\n";
        result << "| Id       | Volume      | Price | Price | Volume      | Id       |\n";
        result << "+----------+-------------+-------+-------+-------------+----------+\n";
        
        // Data rows
        std::reverse_iterator<OrderBookType::const_iterator> buys_iterator = buys_book_.rbegin();
        std::optional<ListType::const_iterator> buys_list_iterator = std::nullopt;
        OrderBookType::const_iterator sells_iterator = sells_book_.begin(); 
        std::optional<ListType::const_iterator> sells_list_iterator = std::nullopt;

        if (buys_iterator != buys_book_.rend()) {
            buys_list_iterator.emplace(buys_iterator->second.begin());
        }

        if (sells_iterator != sells_book_.end()) {
            sells_list_iterator.emplace(sells_iterator->second.begin());
        }

        auto moveIterator = [](auto&& list_iterator, auto&& book_iterator, auto&& order_book_finish) {
            if (list_iterator) {
                list_iterator.value()++;
                if (list_iterator == book_iterator->second.end()) {
                    book_iterator++;
                    if (book_iterator == order_book_finish) {
                        list_iterator.reset();
                    } else {
                        list_iterator.emplace(book_iterator->second.begin());
                    }
                }
            }
        };

        auto formatWithThousandSeparator = [](int64_t number) {
            std::string number_str = absl::StrCat(number);
            std::reverse(number_str.begin(), number_str.end()); 
            std::vector<std::string> groups = absl::StrSplit(number_str, absl::ByLength(3));
            std::string formatted_number = absl::StrJoin(groups, ",");
            std::reverse(formatted_number.begin(), formatted_number.end());
            return formatted_number;
        };


        auto printBuysIterator = [&result, &formatWithThousandSeparator](auto&& list_iterator)  {
            if (list_iterator) {
                auto&& order = list_iterator.value()->get();
                result << absl::StrFormat("%10d|%13s|%7s", order->id(), formatWithThousandSeparator(order->quantity()), formatWithThousandSeparator(order->price()));
            } else {
                result << absl::StrFormat("%10s|%13s|%7s", "", "", "");
            }
        };

        auto printSellsIterator = [&result, &formatWithThousandSeparator](auto&& list_iterator)  {
            if (list_iterator) {
                auto&& order = list_iterator.value()->get();
                result << absl::StrFormat("%7s|%13s|%10d",  formatWithThousandSeparator(order->price()), formatWithThousandSeparator(order->quantity()), order->id());
            } else {
                result << absl::StrFormat("%7s|%13s|%10s", "", "", "");
            }
        };
        

        while (buys_list_iterator || sells_list_iterator) {
            result << '|';
            printBuysIterator(buys_list_iterator);
            result << '|';
            printSellsIterator(sells_list_iterator);
            result << "|\n";

            moveIterator(buys_list_iterator, buys_iterator, buys_book_.rend());            
            moveIterator(sells_list_iterator, sells_iterator, sells_book_.end());
        }

        // Footer
        result << "+-----------------------------------------------------------------+\n";  

        return result.str();
    }

    friend std::ostream& operator<< (std::ostream& out, const OrderBook& order_book) {
        out << order_book.ToString();
        return out;
    }
};


class MatchingSystem {
public:
    void AddOrder(std::shared_ptr<LimitOrder> order) {
        if (order->type() == ICEBERG_ORDER) {
            std::shared_ptr<IcebergOrder> iceberg_order = std::static_pointer_cast<IcebergOrder>(order);
            ProcessIcebergOrder(iceberg_order);
        } else if (order->type() == LIMIT_ORDER) {
            ProcessLimitOrder(order);
        } else { // Unknown
            LOG(FATAL) << absl::StrFormat("Error during AddOrder, type of order is not supported %s", OrderType_Name(order->type()));
        }

        std::cout << order_book_;
    }

private:
    
    template<typename T>
    std::enable_if_t<std::is_base_of_v<LimitOrder, T>, void> ProcessOrder(std::shared_ptr<T> order) {
        while (order->quantity() && order_book_.GetOpposite(order->side()) && LimitOrder::MatchesPrice(order, order_book_.GetOpposite(order->side()).value())) {
            auto opposite_order = order_book_.GetOpposite(order->side()).value();
            uint32_t price = LimitOrder::CalculatePrice(order, opposite_order);
            uint32_t quantity = std::min(order->quantity(), opposite_order->quantity());
            opposite_order->Fill(quantity);
            order->Fill(quantity);
            
            trades_manager_.Append(order->id(), opposite_order->id(), order->side(), price, quantity);
    
            if (opposite_order->quantity() == 0) {
                order_book_.PopOpposite(order->side());
            }
        }
    }

    void ProcessLimitOrder(std::shared_ptr<LimitOrder> order)  {
        ProcessOrder(order);
        trades_manager_.NotifyAll();
        order_book_.Add(order);
    }

    void ProcessIcebergOrder(std::shared_ptr<IcebergOrder> order) {
        order->mutable_quantity() = order->hidden_full_volume();

        ProcessOrder(order);
        
        order->mutable_quantity() = std::min(order->peak_size(), order->hidden_full_volume());

        trades_manager_.NotifyAll();
        order_book_.Add(order);
    }

    OrderBook order_book_;
    TradeManager trades_manager_;
};


