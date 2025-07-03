# Restaurant Simulation Project

## Overview
This project simulates the operations of a restaurant using multithreading in C++. It models various aspects of a restaurant, including customer arrivals, table management, order processing, kitchen operations, and ingredient restocking. The simulation is designed to demonstrate proficiency in multithreading, synchronization, and JSON-based configuration handling.

## Features
- **Customer Arrival Simulation**: Randomly generates customer groups and attempts to seat them at available tables.
- **Table Management**: Tracks table occupancy and assigns customer groups based on size and availability.
- **Order Processing**: Waiters collect orders from tables and send them to the kitchen for preparation.
- **Kitchen Operations**: Processes orders, checks ingredient availability, and prepares dishes.
- **Ingredient Restocking**: Periodically restocks ingredients to ensure the kitchen can continue preparing orders.
- **Logging**: Logs the state of the restaurant (tables, kitchen queue, completed orders, waiting queue, ingredient stock) to a CSV file for analysis.

## Technologies Used
- **C++**: Core programming language for the simulation.
- **Multithreading**: Utilizes `std::thread` for concurrent execution of customer arrival, waiter, kitchen, and delivery operations.
- **Synchronization**: Uses `std::mutex` and `std::condition_variable` to ensure thread-safe access to shared resources.
- **JSON Configuration**: Reads restaurant settings (tables, menu, ingredient stock, etc.) from a JSON file using the [nlohmann/json](https://github.com/nlohmann/json) library.
- **File I/O**: Logs simulation data to a CSV file for analysis.

## Application Logic
1. **Configuration Loading**:
   - Reads the `config.json` file to initialize restaurant settings, including tables, menu items, ingredient stock, and simulation parameters.

2. **Customer Arrival**:
   - Generates random customer groups based on the configured group size range.
   - Attempts to seat groups at available tables. If no table is available, the group is added to a waiting queue.

3. **Waiter Operations**:
   - Waiters collect orders from occupied tables and add them to the kitchen queue.
   - Waiters deliver completed orders to tables and free up tables for new customer groups.

4. **Kitchen Operations**:
   - Processes orders from the kitchen queue, checks ingredient availability, and prepares dishes.
   - Requeues orders if ingredients are unavailable.

5. **Ingredient Delivery**:
   - Periodically restocks ingredients to ensure the kitchen can continue preparing orders.

6. **Logging**:
   - Logs the state of the restaurant (tables, kitchen queue, completed orders, waiting queue, ingredient stock) to a CSV file every second.

## Methods Used
- **Multithreading**:
  - Threads are used for customer arrival, waiter operations, kitchen processing, ingredient delivery, and logging.
  - Synchronization is achieved using `std::mutex` and `std::condition_variable`.

- **Random Number Generation**:
  - `std::random_device` and `std::mt19937` are used to generate random values for customer group sizes, menu item selection, and arrival delays.

- **JSON Parsing**:
  - The [nlohmann/json](https://github.com/nlohmann/json) library is used to parse the `config.json` file and load restaurant settings.

- **File Logging**:
  - Logs simulation data to a CSV file using `std::ofstream`.

## Instructions to Run
### Prerequisites
1. **C++ Compiler**: Ensure you have a C++ compiler that supports C++17 or later (e.g., GCC, Clang, or MSVC).
2. **Dependencies**:
   - Install the [nlohmann/json](https://github.com/nlohmann/json) library for JSON parsing.
   - Ensure the `config.json` file is present in the project directory.

### Steps to Run
1. **Clone the Repository**:
   ```bash
   git clone https://github.com/ksemk/restaurant-threads
   cd restaurant-threads
   ```

2. **Prepare the Configuration File**:
   - Create a `config.json` file in the project directory with the following structure:
     ```json
     {
       "tables": [
         { "id": 1, "size": 4 },
         { "id": 2, "size": 6 }
       ],
       "num_waiters": 3,
       "menu": [
         { "name": "Pizza", "prep_time_ms": 5000, "ingredients": ["Dough", "Cheese", "Tomato"] },
         { "name": "Pasta", "prep_time_ms": 4000, "ingredients": ["Pasta", "Cheese", "Sauce"] }
       ],
       "group_size_range": { "min": 2, "max": 6 },
       "arrival_delay_range_ms": { "min": 1000, "max": 5000 },
       "simulation_time": 60,
       "ingredients_stock": { "Dough": 10, "Cheese": 10, "Tomato": 10, "Pasta": 10, "Sauce": 10 },
       "delivery_interval_ms": 10000,
       "delivery_amount": 5
     }
     ```

3. **Build the Project**:
   - Compile the project using your preferred C++ compiler:
     ```bash
     g++ -std=c++17 -pthread -o restaurant_simulation main.cpp
     ```

4. **Run the Simulation**:
   - Execute the compiled binary:
     ```bash
     ./restaurant_simulation
     ```

5. **View Logs**:
   - Check the `restaurant_log.csv` file in the project directory for simulation data.

### Notes
- The simulation runs indefinitely unless configured otherwise. To stop the simulation, terminate the program manually.
- Ensure the `config.json` file is correctly formatted to avoid JSON parsing errors.
