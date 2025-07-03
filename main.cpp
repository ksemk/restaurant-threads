#include <iostream>
#include <fstream>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <random>
#include <chrono>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <nlohmann/json.hpp>

using json = nlohmann::json;
using namespace std::chrono;

// Random number generation
std::random_device rd;
std::mt19937 gen(rd());

// Restaurant data structures
struct Table {
    int id;
    int size;
    bool is_occupied;
    int group_id;
    std::vector<std::string> orders;
    std::mutex mutex;

    Table() : id(0), size(0), is_occupied(false), group_id(0) {}

    Table(int id_, int size_) : id(id_), size(size_), is_occupied(false), group_id(0) {}

    Table(const Table &) = delete;

    Table &operator=(const Table &) = delete;

    Table(Table &&other) noexcept
            : id(other.id),
              size(other.size),
              is_occupied(other.is_occupied),
              group_id(other.group_id),
              orders(std::move(other.orders)) {
        // Mutex is default-constructed
    }

    Table &operator=(Table &&other) noexcept {
        if (this != &other) {
            id = other.id;
            size = other.size;
            is_occupied = other.is_occupied;
            group_id = other.group_id;
            orders = std::move(other.orders);
            // Mutex is default-constructed
        }
        return *this;
    }
};

struct Order {
    int group_id;
    int table_id;
    std::string dish;
    milliseconds prep_time;
};

struct CustomerGroup {
    int id;
    int size;
    std::vector<std::string> orders;
};

// Global state
struct Restaurant {
    std::map<std::string, int> ingredients_stock;
    std::map<std::string, std::vector<std::string>> recipe;
    std::mutex ingredient_mutex;
    int delivery_interval_ms;
    int delivery_amount;
    std::vector<Table> tables;
    std::mutex tables_mutex;
    int num_waiters;
    std::vector<std::string> menu_items;
    std::vector<milliseconds> prep_times;
    int group_size_min, group_size_max;
    int arrival_delay_min_ms, arrival_delay_max_ms;
    int simulation_time;
    std::queue<CustomerGroup> waiting_queue;
    std::queue<Order> kitchen_queue;
    std::queue<Order> completed_orders; // New queue for prepared orders
    std::mutex queue_mutex;
    std::mutex kitchen_mutex;
    std::mutex completed_orders_mutex; // Mutex for completed_orders
    std::condition_variable kitchen_cv;
    bool running;
    std::mutex running_mutex;
    int next_group_id;
    std::mutex group_id_mutex;
};

// Read configuration from JSON file
void load_config(const std::string &filename, Restaurant &restaurant) {
    std::ifstream file(filename);
    json j;
    file >> j;

    restaurant.next_group_id = 1;
    restaurant.running = true;

    // Load tables
    restaurant.tables.clear();
    restaurant.tables.reserve(j["tables"].size());
    for (const auto &table: j["tables"]) {
        restaurant.tables.emplace_back(table["id"], table["size"]);
    }

    // Load waiters
    restaurant.num_waiters = j["num_waiters"];

    // Load menu
    for (const auto &item: j["menu"]) {
        restaurant.menu_items.push_back(item["name"]);
        restaurant.prep_times.push_back(milliseconds(item["prep_time_ms"]));
    }

    // Load group size range
    restaurant.group_size_min = j["group_size_range"]["min"];
    restaurant.group_size_max = j["group_size_range"]["max"];

    // Load arrival delay range
    restaurant.arrival_delay_min_ms = j["arrival_delay_range_ms"]["min"];
    restaurant.arrival_delay_max_ms = j["arrival_delay_range_ms"]["max"];

    // Load simulation time
    restaurant.simulation_time = j["simulation_time"];
    // Load recipes
    for (const auto &item: j["menu"]) {
        std::string dish = item["name"];
        restaurant.recipe[dish] = item["ingredients"].get<std::vector<std::string>>();
    }

    // Load initial ingredient stock
    restaurant.ingredients_stock = j["ingredients_stock"].get<std::map<std::string, int>>();

    // Delivery config
    restaurant.delivery_interval_ms = j["delivery_interval_ms"];
    restaurant.delivery_amount = j["delivery_amount"];

}

// Generate random number in range
int random_int(int min, int max) {
    std::uniform_int_distribution<> dis(min, max);
    return dis(gen);
}

// Generate random customer group
CustomerGroup generate_group(Restaurant &restaurant) {
    std::lock_guard<std::mutex> lock(restaurant.group_id_mutex);
    CustomerGroup group;
    group.id = restaurant.next_group_id++;
    group.size = random_int(restaurant.group_size_min, restaurant.group_size_max);
    int num_orders = group.size;
    for (int i = 0; i < num_orders; ++i) {
        int menu_idx = random_int(0, restaurant.menu_items.size() - 1);
        group.orders.push_back(restaurant.menu_items[menu_idx]);
    }
//    std::cout << "[Debug] Generated group " << group.id << " with size " << group.size << "\n";
    return group;
}

// Try to seat a group at a table
bool seat_group(Restaurant &restaurant, CustomerGroup &group) {
    std::lock_guard<std::mutex> tables_lock(restaurant.tables_mutex);
    for (auto &table: restaurant.tables) {
        std::lock_guard<std::mutex> lock(table.mutex);
        if (!table.is_occupied && table.size >= group.size) {
            table.is_occupied = true;
            table.group_id = group.id;
            table.orders = group.orders;
//            std::cout << "[Debug] Seated group " << group.id << " at table " << table.id << "\n";
            return true;
        }
    }
//    std::cout << "[Debug] Group " << group.id << " added to waiting queue\n";
    return false;
}

// Customer arrival thread
void customer_arrival(Restaurant &restaurant) {
    while (true) {
        {
            std::lock_guard<std::mutex> lock(restaurant.running_mutex);
            if (!restaurant.running) break;
        }

        CustomerGroup group = generate_group(restaurant);
        bool seated = seat_group(restaurant, group);
        if (!seated) {
            std::lock_guard<std::mutex> lock(restaurant.queue_mutex);
            restaurant.waiting_queue.push(group);
        }

        int delay = random_int(restaurant.arrival_delay_min_ms, restaurant.arrival_delay_max_ms);
        std::this_thread::sleep_for(milliseconds(delay));
    }
}

// Waiter thread
void waiter(Restaurant &restaurant, int waiter_id) {
    while (true) {
        {
            std::lock_guard<std::mutex> lock(restaurant.running_mutex);
            if (!restaurant.running) break;
        }

        // Check for orders to take
        {
            std::lock_guard<std::mutex> tables_lock(restaurant.tables_mutex);
            for (auto &table: restaurant.tables) {
                std::lock_guard<std::mutex> lock(table.mutex);
                if (table.is_occupied && !table.orders.empty()) {
                    std::vector<Order> orders;
                    for (const auto &dish: table.orders) {
                        int menu_idx = std::find(restaurant.menu_items.begin(), restaurant.menu_items.end(), dish) -
                                       restaurant.menu_items.begin();
                        orders.push_back({table.group_id, table.id, dish, restaurant.prep_times[menu_idx]});
                    }
                    table.orders.clear();
                    {
                        std::lock_guard<std::mutex> kitchen_lock(restaurant.kitchen_mutex);
                        for (const auto &order: orders) {
                            restaurant.kitchen_queue.push(order);
//                            std::cout << "[Debug] Waiter " << waiter_id << " submitted order " << order.dish << " for group " << order.group_id << "\n";
                        }
                        restaurant.kitchen_cv.notify_one();
                    }
                    continue;
                }
            }
        }

        // Check for completed orders to deliver
        std::vector<Order> to_deliver;
        {
            std::unique_lock<std::mutex> lock(restaurant.completed_orders_mutex);
            while (!restaurant.completed_orders.empty() && to_deliver.size() < 3) {
                to_deliver.push_back(restaurant.completed_orders.front());
                restaurant.completed_orders.pop();
            }
        }
        if (!to_deliver.empty()) {
            std::lock_guard<std::mutex> tables_lock(restaurant.tables_mutex);
            for (const auto &order: to_deliver) {
                for (auto &table: restaurant.tables) {
                    std::lock_guard<std::mutex> lock(table.mutex);
                    if (table.group_id == order.group_id && table.id == order.table_id) {
                        // Simulate delivery and eating
                        std::this_thread::sleep_for(milliseconds(500));
                        table.is_occupied = false;
                        table.group_id = 0;
//                        std::cout << "[Debug] Waiter " << waiter_id << " delivered " << order.dish << " to group " << order.group_id << " at table " << table.id << "\n";
                        // Try to seat a waiting group
                        CustomerGroup group;
                        bool has_group = false;
                        {
                            std::lock_guard<std::mutex> queue_lock(restaurant.queue_mutex);
                            if (!restaurant.waiting_queue.empty()) {
                                group = restaurant.waiting_queue.front();
                                restaurant.waiting_queue.pop();
                                has_group = true;
                            }
                        }
                        if (has_group && table.size >= group.size) {
                            table.is_occupied = true;
                            table.group_id = group.id;
                            table.orders = group.orders;
//                            std::cout << "[Debug] Seated waiting group " << group.id << " at table " << table.id << "\n";
                        }
                        break;
                    }
                }
            }
        }

        std::this_thread::sleep_for(milliseconds(500));
    }
}

// Kitchen thread
void kitchen(Restaurant &restaurant) {
    while (true) {
        std::unique_lock<std::mutex> lock(restaurant.kitchen_mutex);
        restaurant.kitchen_cv.wait(lock, [&] {
            return !restaurant.kitchen_queue.empty() || !restaurant.running;
        });
        {
            std::lock_guard<std::mutex> running_lock(restaurant.running_mutex);
            if (!restaurant.running && restaurant.kitchen_queue.empty()) break;
        }
        if (!restaurant.kitchen_queue.empty()) {
            Order order = restaurant.kitchen_queue.front();
            restaurant.kitchen_queue.pop();
            lock.unlock();
            bool can_prepare = true;
            {
                std::lock_guard<std::mutex> lock(restaurant.ingredient_mutex);
                for (const auto &ingredient: restaurant.recipe[order.dish]) {
                    if (restaurant.ingredients_stock[ingredient] <= 0) {
                        can_prepare = false;
                        break;
                    }
                }
                if (can_prepare) {
                    for (const auto &ingredient: restaurant.recipe[order.dish]) {
                        restaurant.ingredients_stock[ingredient]--;
                    }
                }
            }

            if (can_prepare) {
                std::this_thread::sleep_for(order.prep_time);
                std::lock_guard<std::mutex> completed_lock(restaurant.completed_orders_mutex);
                restaurant.completed_orders.push(order);
            } else {
                // requeue order if ingredients missing
                std::lock_guard<std::mutex> kitchen_lock(restaurant.kitchen_mutex);
                restaurant.kitchen_queue.push(order);
                std::this_thread::sleep_for(milliseconds(200)); // avoid tight loop
            }

            {
                std::lock_guard<std::mutex> completed_lock(restaurant.completed_orders_mutex);
                restaurant.completed_orders.push(order);
//                std::cout << "[Debug] Kitchen completed order " << order.dish << " for group " << order.group_id << "\n";
            }
        }
    }
}

void ingredient_delivery(Restaurant &restaurant) {
    while (true) {
        {
            std::lock_guard<std::mutex> lock(restaurant.running_mutex);
            if (!restaurant.running) break;
        }

        {
            std::lock_guard<std::mutex> lock(restaurant.ingredient_mutex);
            for (auto &pair: restaurant.ingredients_stock) {
                pair.second += restaurant.delivery_amount;
            }
//            std::cout << "[Delivery] Ingredients restocked\n";
        }

        std::this_thread::sleep_for(milliseconds(restaurant.delivery_interval_ms));
    }
}

void logger(Restaurant &restaurant) {
    auto start = std::chrono::system_clock::now();
    std::ofstream log_file("restaurant_log.csv", std::ios::app);

    // Write CSV header
    log_file << "Timestamp,Tables,Kitchen Queue,Completed Orders,Waiting Queue,Ingredients,Waiters\n";

    while (true) {
        std::stringstream tables_ss, kitchen_ss, completed_ss, waiting_ss, ingredients_ss;
        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::system_clock::to_time_t(now);

        // Tables status
        std::cout << "Tables:\n";
        tables_ss << "\"";
        {
            std::lock_guard<std::mutex> tables_lock(restaurant.tables_mutex);
            for (auto &table : restaurant.tables) {
                std::lock_guard<std::mutex> lock(table.mutex);
                std::cout << "  Table " << table.id << " (size " << table.size << "): "
                          << (table.is_occupied ? "Occupied (Group " + std::to_string(table.group_id) + ")" : "Free");
                tables_ss << "Table " << table.id << " (size " << table.size << "): "
                          << (table.is_occupied ? "Occupied (Group " + std::to_string(table.group_id) + ")" : "Free");
                if (!table.orders.empty()) {
                    std::cout << ", Orders: ";
                    tables_ss << ", Orders: ";
                    for (const auto &order : table.orders) {
                        std::cout << order << " ";
                        tables_ss << order << " ";
                    }
                }
                std::cout << "\n";
                tables_ss << ";";
            }
        }
        tables_ss << "\"";

        // Kitchen Queue
        std::cout << "Kitchen Queue: ";
        kitchen_ss << "\"";
        {
            std::lock_guard<std::mutex> lock(restaurant.kitchen_mutex);
            auto temp = restaurant.kitchen_queue;
            while (!temp.empty()) {
                auto order = temp.front();
                std::cout << order.dish << "(Group " << order.group_id << ") ";
                kitchen_ss << order.dish << "(Group " << order.group_id << ") ";
                temp.pop();
            }
        }
        std::cout << "\n";
        kitchen_ss << "\"";

        // Completed Orders
        std::cout << "Completed Orders: ";
        completed_ss << "\"";
        {
            std::lock_guard<std::mutex> lock(restaurant.completed_orders_mutex);
            auto temp = restaurant.completed_orders;
            while (!temp.empty()) {
                auto order = temp.front();
                std::cout << order.dish << "(Group " << order.group_id << ") ";
                completed_ss << order.dish << "(Group " << order.group_id << ") ";
                temp.pop();
            }
        }
        std::cout << "\n";
        completed_ss << "\"";

        // Waiting Queue
        std::cout << "Waiting Queue: ";
        waiting_ss << "\"";
        {
            std::lock_guard<std::mutex> lock(restaurant.queue_mutex);
            auto temp = restaurant.waiting_queue;
            while (!temp.empty()) {
                auto group = temp.front();
                std::cout << "Group " << group.id << "(size " << group.size << ") ";
                waiting_ss << "Group " << group.id << "(size " << group.size << ") ";
                temp.pop();
            }
        }
        std::cout << "\n";
        waiting_ss << "\"";

        // Ingredient Stock
        std::cout << "Ingredients Stock:\n";
        ingredients_ss << "\"";
        {
            std::lock_guard<std::mutex> lock(restaurant.ingredient_mutex);
            for (const auto &pair : restaurant.ingredients_stock) {
                std::cout << "  " << pair.first << ": " << pair.second << "\n";
                ingredients_ss << pair.first << ":" << pair.second << ";";
            }
        }
        ingredients_ss << "\"";

        // Waiter count
        std::cout << "Waiters: " << restaurant.num_waiters << " active\n";
        std::string waiters_str = std::to_string(restaurant.num_waiters) + " active";

        // Write to CSV
        log_file << std::put_time(std::localtime(&timestamp), "%Y-%m-%d %H:%M:%S") << ","
                 << tables_ss.str() << ","
                 << kitchen_ss.str() << ","
                 << completed_ss.str() << ","
                 << waiting_ss.str() << ","
                 << ingredients_ss.str() << ","
                 << "\"" << waiters_str << "\"\n";
        log_file.flush();

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

int main() {
    Restaurant restaurant;
    load_config("../config.json", restaurant);
    std::thread customer_thread(customer_arrival, std::ref(restaurant));
    std::vector<std::thread> waiter_threads;
    std::thread delivery_thread(ingredient_delivery, std::ref(restaurant));
    for (int i = 1; i <= restaurant.num_waiters; ++i) {
        waiter_threads.emplace_back(waiter, std::ref(restaurant), i);
    }
    std::thread kitchen_thread(kitchen, std::ref(restaurant));
    std::thread logger_thread(logger, std::ref(restaurant));

    customer_thread.join();
    for (auto &t: waiter_threads) t.join();
    kitchen_thread.join();
    logger_thread.join();
    delivery_thread.join();

    return 0;
}