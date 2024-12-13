#include <iostream>
#include <vector>
#include <string>
#include <random>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <atomic>
#include <chrono>
#include <algorithm>
#include <map>

// Структура для представления продукта
struct Product {
    std::string name;
    double weight;
    std::string packagingType;

    Product(std::string n, double w, std::string pt)
        : name(n), weight(w), packagingType(pt) {}
};

// Класс для представления завода
class Factory {
public:
    std::string name;
    Product product;
    double productionRate;

    Factory(std::string n, Product p, double rate)
        : name(n), product(p), productionRate(rate) {}

    void produce(std::queue<std::pair<std::string, int>>& warehouse, std::mutex& warehouseMutex,
                 std::condition_variable& cv, std::atomic<bool>& shouldStop) {
        while (!shouldStop) {
            int amount = static_cast<int>(productionRate);
            {
                std::lock_guard<std::mutex> lock(warehouseMutex);
                warehouse.push({product.name, amount});
                std::cout << "Factory " << name << " produced " << amount << " units of " << product.name << std::endl;
            }
            cv.notify_one();
            std::this_thread::sleep_for(std::chrono::seconds(1)); // Производство каждую секунду
        }
    }
};

// Класс для представления грузовика
class Truck {
public:
    int capacity;

    Truck(int cap) : capacity(cap) {}

    void transport(std::queue<std::pair<std::string, int>>& warehouse, std::mutex& warehouseMutex,
                   std::condition_variable& cv, std::atomic<bool>& shouldStop,
                   std::vector<std::pair<std::string, int>>& statistics) {
        while (!shouldStop) {
            std::unique_lock<std::mutex> lock(warehouseMutex);
            cv.wait(lock, [&]{ return !warehouse.empty() || shouldStop; });

            if (shouldStop && warehouse.empty()) break;

            int totalLoaded = 0;
            std::vector<std::pair<std::string, int>> loadedProducts;

            while (!warehouse.empty() && totalLoaded < capacity) {
                auto& front = warehouse.front();
                int toLoad = std::min(capacity - totalLoaded, front.second);
                totalLoaded += toLoad;
                loadedProducts.push_back({front.first, toLoad});

                if (toLoad == front.second) {
                    warehouse.pop();
                } else {
                    front.second -= toLoad;
                }
            }

            lock.unlock();

            // Симуляция транспортировки
            std::this_thread::sleep_for(std::chrono::seconds(2));

            // Обновление статистики
            {
                std::lock_guard<std::mutex> statsLock(warehouseMutex);
                for (const auto& product : loadedProducts) {
                    statistics.push_back(product);
                }
            }

            std::cout << "Truck transported " << totalLoaded << " units" << std::endl;
        }
    }
};

int main() {
    const int M = 100; // Минимальный множитель для вместимости склада
    const int n = 50;  // Минимальное количество единиц продукции в час для первого завода

    // Создание продуктов
    Product productA("A", 1.0, "Box");
    Product productB("B", 1.2, "Bag");
    Product productC("C", 0.8, "Container");

    // Создание заводов
    std::vector<Factory> factories = {
        Factory("Factory A", productA, n),
        Factory("Factory B", productB, 1.1 * n),
        Factory("Factory C", productC, 1.2 * n)
    };

    // Вычисление общей производительности
    double totalProduction = 0;
    for (const auto& factory : factories) {
        totalProduction += factory.productionRate;
    }

    // Вместимость склада
    int warehouseCapacity = static_cast<int>(M * totalProduction);

    // Очередь для хранения продукции на складе
    std::queue<std::pair<std::string, int>> warehouse;
    std::mutex warehouseMutex;
    std::condition_variable cv;

    // Флаг для остановки симуляции
    std::atomic<bool> shouldStop(false);

    // Создание грузовиков
    std::vector<Truck> trucks = {
        Truck(50),
        Truck(100)
    };

    // Статистика перевозок
    std::vector<std::pair<std::string, int>> transportStatistics;

    // Запуск потоков для заводов
    std::vector<std::thread> factoryThreads;
    for (auto& factory : factories) {
        factoryThreads.emplace_back(&Factory::produce, &factory, std::ref(warehouse),
                                    std::ref(warehouseMutex), std::ref(cv), std::ref(shouldStop));
    }

    // Запуск потоков для грузовиков
    std::vector<std::thread> truckThreads;
    for (auto& truck : trucks) {
        truckThreads.emplace_back(&Truck::transport, &truck, std::ref(warehouse),
                                  std::ref(warehouseMutex), std::ref(cv), std::ref(shouldStop),
                                  std::ref(transportStatistics));
    }

    // Симуляция работы в течение некоторого времени
    std::this_thread::sleep_for(std::chrono::seconds(60));

    // Остановка симуляции
    shouldStop = true;
    cv.notify_all();

    // Ожидание завершения всех потоков
    for (auto& thread : factoryThreads) {
        thread.join();
    }
    for (auto& thread : truckThreads) {
        thread.join();
    }

    // Вывод статистики
    std::cout << "\nTransport Statistics:\n";
    std::map<std::string, int> totalTransported;
    for (const auto& stat : transportStatistics) {
        totalTransported[stat.first] += stat.second;
    }

    for (const auto& item : totalTransported) {
        std::cout << "Product " << item.first << ": " << item.second << " units transported\n";
    }

    int totalTrips = transportStatistics.size() / trucks.size();
    std::cout << "Average units per trip: " << (totalTransported["A"] + totalTransported["B"] + totalTransported["C"]) / totalTrips << std::endl;

    return 0;
}
