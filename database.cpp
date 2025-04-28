#include "database.h"
#include <fstream>
#include <stdexcept>
#include <iostream>
#include <sstream>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <algorithm>
#include <limits>

Database::Database(const std::string& filename, const std::string& key, int log_mode)
    : filename(filename), key(key), log_mode(log_mode) {
    load();
}

Database::~Database() {
    save();
}

void Database::encrypt_decrypt(std::string& data) const {
    for (size_t i = 0; i < data.size(); ++i) {
        data[i] ^= key[i % key.size()];
    }
}

void Database::save() {
    std::ofstream out(filename, std::ios::binary);
    if (!out) throw std::runtime_error("Cannot open file for writing");

    size_t name_count = names.size();
    out.write(reinterpret_cast<char*>(&name_count), sizeof(name_count));
    if (log_mode == 2 || log_mode == 3) {
        std::cout << "Saving " << name_count << " names\n";
    }
    for (const auto& name : names) {
        out.write(reinterpret_cast<const char*>(&name.name_id), sizeof(name.name_id));
        size_t name_len = name.name.size();
        out.write(reinterpret_cast<char*>(&name_len), sizeof(name_len));
        std::string encrypted_name = name.name;
        encrypt_decrypt(encrypted_name);
        out.write(encrypted_name.c_str(), name_len);
        if (log_mode == 2 || log_mode == 3) {
            std::cout << "Saving name: ID=" << name.name_id << ", Name=" << name.name << "\n";
        }
    }

    size_t record_count = records.size();
    out.write(reinterpret_cast<char*>(&record_count), sizeof(record_count));
    if (log_mode == 2 || log_mode == 3) {
        std::cout << "Saving " << record_count << " records\n";
    }
    for (const auto& record : records) {
        out.write(reinterpret_cast<const char*>(&record.id), sizeof(record.id));
        out.write(reinterpret_cast<const char*>(&record.name_id), sizeof(record.name_id));
        out.write(reinterpret_cast<const char*>(&record.value), sizeof(record.value));
        if (log_mode == 2 || log_mode == 3) {
            std::cout << "Saving record: ID=" << record.id << ", NameID=" << record.name_id << ", Value=" << record.value << "\n";
        }
    }
}

void Database::load() {
    std::ifstream in(filename, std::ios::binary);
    if (!in) {
        if (log_mode == 2 || log_mode == 3) {
            std::cout << "No database file found, starting with empty database\n";
        }
        return;
    }

    names.clear();
    records.clear();
    name_id_index.clear();
    name_index.clear();
    id_index.clear();
    name_id_record_index.clear();

    size_t name_count;
    in.read(reinterpret_cast<char*>(&name_count), sizeof(name_count));
    if (log_mode == 2 || log_mode == 3) {
        std::cout << "Loading " << name_count << " names\n";
    }
    for (size_t i = 0; i < name_count; ++i) {
        Name n;
        in.read(reinterpret_cast<char*>(&n.name_id), sizeof(n.name_id));
        size_t name_len;
        in.read(reinterpret_cast<char*>(&name_len), sizeof(name_len));
        std::string encrypted_name(name_len, '\0');
        in.read(&encrypted_name[0], name_len);
        encrypt_decrypt(encrypted_name);
        n.name = encrypted_name;
        names.push_back(n);
        name_id_index[n.name_id] = names.size() - 1;
        name_index[n.name] = n.name_id;
        if (log_mode == 2 || log_mode == 3) {
            std::cout << "Loaded name: ID=" << n.name_id << ", Name=" << n.name << "\n";
        }
    }

    size_t record_count;
    in.read(reinterpret_cast<char*>(&record_count), sizeof(record_count));
    if (log_mode == 2 || log_mode == 3) {
        std::cout << "Loading " << record_count << " records\n";
    }
    for (size_t i = 0; i < record_count; ++i) {
        Record r;
        in.read(reinterpret_cast<char*>(&r.id), sizeof(r.id));
        in.read(reinterpret_cast<char*>(&r.name_id), sizeof(r.name_id));
        in.read(reinterpret_cast<char*>(&r.value), sizeof(r.value));
        records.push_back(r);
        id_index[r.id] = records.size() - 1;
        name_id_record_index[r.name_id].push_back(records.size() - 1);
        if (log_mode == 2 || log_mode == 3) {
            std::cout << "Loaded record: ID=" << r.id << ", NameID=" << r.name_id << ", Value=" << r.value << "\n";
        }
    }
}

void Database::log_query(const std::string& query, const std::string& params, double duration_ms) const {
    if (log_mode == 0) return;

    std::stringstream log_message;
    log_message << "Query: " << query << ", Params: " << params 
                << ", Duration: " << std::fixed << std::setprecision(3) << duration_ms << " ms";

    if (log_mode == 1 || log_mode == 3) {
        std::ofstream log_file("application_log.txt", std::ios_base::app);
        if (log_file) {
            auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
            log_file << std::put_time(std::localtime(&now), "%Y-%m-%d %H:%M:%S") 
                     << " - " << log_message.str() << "\n";
        }
    }
    if (log_mode == 2 || log_mode == 3) {
        std::cout << log_message.str() << "\n";
    }
}

int Database::get_or_create_name_id(const std::string& name) {
    auto it = name_index.find(name);
    if (it != name_index.end()) {
        return it->second;
    }

    int name_id = names.empty() ? 1 : names.back().name_id + 1;
    names.push_back({name_id, name});
    name_id_index[name_id] = names.size() - 1;
    name_index[name] = name_id;
    return name_id;
}

bool Database::add_record(int id, const std::string& name, double value) {
    auto start = std::chrono::high_resolution_clock::now();

    if (id_index.find(id) != id_index.end()) {
        log_query("ПРИЛОЖИ", std::to_string(id) + ", " + name + ", " + std::to_string(value), 0);
        return false;
    }

    int name_id = get_or_create_name_id(name);
    records.push_back({id, name_id, value});
    id_index[id] = records.size() - 1;
    name_id_record_index[name_id].push_back(records.size() - 1);

    if (log_mode == 2 || log_mode == 3) {
        std::cout << "Added record: ID=" << id << ", Name=" << name << ", NameID=" << name_id << ", Value=" << value << "\n";
    }

    auto end = std::chrono::high_resolution_clock::now();
    double duration_ms = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() / 1000.0;
    log_query("ПРИЛОЖИ", std::to_string(id) + ", " + name + ", " + std::to_string(value), duration_ms);

    save();
    return true;
}

bool Database::remove_record(int id) {
    auto start = std::chrono::high_resolution_clock::now();

    auto it = id_index.find(id);
    if (it == id_index.end()) {
        log_query("ИЗЫМИ", std::to_string(id), 0);
        return false;
    }

    size_t idx = it->second;
    int name_id = records[idx].name_id;
    records.erase(records.begin() + idx);
    id_index.erase(id);
    name_id_record_index[name_id].erase(
        std::remove(name_id_record_index[name_id].begin(), name_id_record_index[name_id].end(), idx),
        name_id_record_index[name_id].end()
    );

    if (name_id_record_index[name_id].empty()) {
        auto name_it = name_id_index.find(name_id);
        if (name_it != name_id_index.end()) {
            size_t name_idx = name_it->second;
            std::string name = names[name_idx].name;
            names.erase(names.begin() + name_idx);
            name_id_index.erase(name_id);
            name_index.erase(name);
        }
    }

    for (size_t i = idx; i < records.size(); ++i) {
        id_index[records[i].id] = i;
        for (auto& pair : name_id_record_index) {
            for (size_t& index : pair.second) {
                if (index > idx) --index;
            }
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    double duration_ms = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() / 1000.0;
    log_query("ИЗЫМИ", std::to_string(id), duration_ms);

    save();
    return true;
}

bool Database::find_record_by_id(int id, std::string& name, double& value) const {
    auto start = std::chrono::high_resolution_clock::now();

    auto it = id_index.find(id);
    if (it == id_index.end()) {
        log_query("ОБРЕСТИ", std::to_string(id), 0);
        return false;
    }

    size_t idx = it->second;
    int name_id = records[idx].name_id;
    auto name_it = name_id_index.find(name_id);
    if (name_it == name_id_index.end()) {
        log_query("ОБРЕСТИ", std::to_string(id), 0);
        return false;
    }

    name = names[name_it->second].name;
    value = records[idx].value;

    auto end = std::chrono::high_resolution_clock::now();
    double duration_ms = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() / 1000.0;
    log_query("ОБРЕСТИ", std::to_string(id), duration_ms);

    return true;
}

bool Database::find_record_by_name(const std::string& name, std::vector<int>& ids, std::vector<double>& values) const {
    auto start = std::chrono::high_resolution_clock::now();

    ids.clear();
    values.clear();

    auto name_it = name_index.find(name);
    if (name_it == name_index.end()) {
        log_query("ОБРЕСТИ ПО ИМЕНИ", name, 0);
        return false;
    }

    int name_id = name_it->second;

    auto record_it = name_id_record_index.find(name_id);
    if (record_it == name_id_record_index.end()) {
        log_query("ОБРЕСТИ ПО ИМЕНИ", name, 0);
        return false;
    }

    for (size_t idx : record_it->second) {
        ids.push_back(records[idx].id);
        values.push_back(records[idx].value);
    }

    auto end = std::chrono::high_resolution_clock::now();
    double duration_ms = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() / 1000.0;
    log_query("ОБРЕСТИ ПО ИМЕНИ", name, duration_ms);

    return true;
}

bool Database::find_records_by_value(double value, std::vector<int>& ids, std::vector<std::string>& names, std::vector<double>& values) const {
    auto start = std::chrono::high_resolution_clock::now();

    ids.clear();
    names.clear();
    values.clear();

    for (const auto& record : records) {
        if (std::abs(record.value - value) < 1e-6) {
            auto name_it = name_id_index.find(record.name_id);
            if (name_it != name_id_index.end()) {
                ids.push_back(record.id);
                names.push_back(this->names[name_it->second].name);
                values.push_back(record.value);
            }
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    double duration_ms = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() / 1000.0;
    log_query("ОБРЕСТИ ПО ЗНАЧЕНИЮ", std::to_string(value), duration_ms);

    return !ids.empty();
}

bool Database::find_records_by_value_range(double min_value, double max_value, bool has_min, bool has_max, std::vector<int>& ids, std::vector<std::string>& names, std::vector<double>& values) const {
    auto start = std::chrono::high_resolution_clock::now();

    ids.clear();
    names.clear();
    values.clear();

    for (const auto& record : records) {
        bool matches = true;
        if (has_min && record.value < min_value) {
            matches = false;
        }
        if (has_max && record.value > max_value) {
            matches = false;
        }
        if (matches) {
            auto name_it = name_id_index.find(record.name_id);
            if (name_it != name_id_index.end()) {
                ids.push_back(record.id);
                names.push_back(this->names[name_it->second].name);
                values.push_back(record.value);
            }
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    double duration_ms = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() / 1000.0;
    std::string params;
    if (has_min && has_max) {
        params = std::to_string(min_value) + " - " + std::to_string(max_value);
    } else if (has_min) {
        params = "> " + std::to_string(min_value);
    } else if (has_max) {
        params = "< " + std::to_string(max_value);
    }
    log_query("ОБРЕСТИ ПО ДИАПАЗОНУ", params, duration_ms);

    return !ids.empty();
}

bool Database::update_record(int id, const std::string& name, double value) {
    auto start = std::chrono::high_resolution_clock::now();

    auto it = id_index.find(id);
    if (it == id_index.end()) {
        log_query("ОБНОВИ", std::to_string(id) + ", " + name + ", " + std::to_string(value), 0);
        return false;
    }

    size_t idx = it->second;
    int old_name_id = records[idx].name_id;
    auto old_name_it = name_id_index.find(old_name_id);
    std::string old_name = old_name_it != name_id_index.end() ? names[old_name_it->second].name : "";
    if (old_name != name) {
        name_id_record_index[old_name_id].erase(
            std::remove(name_id_record_index[old_name_id].begin(), name_id_record_index[old_name_id].end(), idx),
            name_id_record_index[old_name_id].end()
        );
        int new_name_id = get_or_create_name_id(name);
        records[idx].name_id = new_name_id;
        name_id_record_index[new_name_id].push_back(idx);
    }
    records[idx].value = value;

    auto end = std::chrono::high_resolution_clock::now();
    double duration_ms = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() / 1000.0;
    log_query("ОБНОВИ", std::to_string(id) + ", " + name + ", " + std::to_string(value), duration_ms);

    save();
    return true;
}

void Database::get_all_records(std::vector<int>& ids, std::vector<std::string>& names, std::vector<double>& values) const {
    auto start = std::chrono::high_resolution_clock::now();

    ids.clear();
    names.clear();
    values.clear();

    std::vector<std::tuple<int, std::string, double>> temp_records;

    for (const auto& record : records) {
        auto name_it = name_id_index.find(record.name_id);
        if (name_it != name_id_index.end()) {
            temp_records.emplace_back(record.id, this->names[name_it->second].name, record.value);
        } else {
            std::stringstream log_message;
            log_message << "Warning: Record with ID " << record.id << " has invalid name_id " << record.name_id;
            if (log_mode == 1 || log_mode == 3) {
                std::ofstream log_file("application_log.txt", std::ios_base::app);
                if (log_file) {
                    auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
                    log_file << std::put_time(std::localtime(&now), "%Y-%m-%d %H:%M:%S") 
                             << " - " << log_message.str() << "\n";
                }
            }
            if (log_mode == 2 || log_mode == 3) {
                std::cout << log_message.str() << "\n";
            }
            temp_records.emplace_back(record.id, "[Неизвестное имя]", record.value);
        }
    }

    if (log_mode == 2 || log_mode == 3) {
        std::cout << "Records before sorting:\n";
        for (const auto& [id, name, value] : temp_records) {
            std::cout << "ID=" << id << ", Name=" << name << ", Value=" << value << "\n";
        }
    }

    std::sort(temp_records.begin(), temp_records.end(),
              [](const auto& a, const auto& b) {
                  return std::get<0>(a) < std::get<0>(b);
              });

    if (log_mode == 2 || log_mode == 3) {
        std::cout << "Records after sorting:\n";
        for (const auto& [id, name, value] : temp_records) {
            std::cout << "ID=" << id << ", Name=" << name << ", Value=" << value << "\n";
        }
    }

    for (const auto& [id, name, value] : temp_records) {
        ids.push_back(id);
        names.push_back(name);
        values.push_back(value);
    }

    auto end = std::chrono::high_resolution_clock::now();
    double duration_ms = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() / 1000.0;
    log_query("ПЕРЕЧЕНЬ", "", duration_ms);
}

void Database::analyze_query(const std::string& query, const std::string& params) const {
    std::cout << "Анализ запроса: " << query << ", Параметры: " << params << "\n";
    if (query == "ОБРЕСТИ" || query == "ИЗЫМИ" || query == "ОБНОВИ") {
        std::cout << "Используется индекс по id (хеш-таблица, O(1)). Доступ к имени через name_id_index (O(1)).\n";
    } else if (query == "ПЕРЕЧЕНЬ") {
        std::cout << "Полное сканирование таблицы Records с объединением с Names через name_id_index (O(n)).\n";
    } else if (query == "ПРИЛОЖИ") {
        std::cout << "Проверка уникальности id через индекс (O(1)), добавление в таблицы Names и Records, обновление индексов.\n";
    } else if (query == "ОБРЕСТИ ПО ЗНАЧЕНИЮ") {
        std::cout << "Полное сканирование таблицы Records (O(n)).\n";
    } else if (query == "ОБРЕСТИ ПО ДИАПАЗОНУ") {
        std::cout << "Полное сканирование таблицы Records с фильтрацией по диапазону значений (O(n)).\n";
    }
}