#ifndef DATABASE_H
#define DATABASE_H

#include <string>
#include <vector>
#include <unordered_map>
#include <map>

struct Name {
    int name_id;
    std::string name;
};

struct Record {
    int id;
    int name_id;
    double value;
};

class Database {
private:
    std::vector<Name> names; // Таблица Names
    std::vector<Record> records; // Таблица Records
    std::unordered_map<int, size_t> name_id_index; // Индекс по name_id в Names
    std::map<std::string, int> name_index; // Индекс по name в Names
    std::unordered_map<int, size_t> id_index; // Индекс по id в Records
    std::unordered_map<int, std::vector<size_t>> name_id_record_index; // Индекс по name_id в Records
    std::string filename;
    std::string key;
    int log_mode; // 0 - без логов, 1 - только файл, 2 - файл и консоль
    void encrypt_decrypt(std::string& data) const;
    void save();
    void load();
    void log_query(const std::string& query, const std::string& params, double duration_ms) const;
    int get_or_create_name_id(const std::string& name);

public:
    Database(const std::string& filename, const std::string& key, int log_mode);
    ~Database();
    bool add_record(int id, const std::string& name, double value); // ПРИЛОЖИ
    bool remove_record(int id); // ИЗЫМИ
    bool find_record_by_id(int id, std::string& name, double& value) const; // ОБРЕСТИ
    bool find_record_by_name(const std::string& name, std::vector<int>& ids, std::vector<double>& values) const;
    bool find_records_by_value(double value, std::vector<int>& ids, std::vector<std::string>& names, std::vector<double>& values) const; // Поиск по точному значению
    bool find_records_by_value_range(double min_value, double max_value, bool has_min, bool has_max, std::vector<int>& ids, std::vector<std::string>& names, std::vector<double>& values) const; // Поиск по диапазону значений
    bool update_record(int id, const std::string& name, double value); // ОБНОВИ
    void get_all_records(std::vector<int>& ids, std::vector<std::string>& names, std::vector<double>& values) const; // ПЕРЕЧЕНЬ
    void analyze_query(const std::string& query, const std::string& params) const; // Аналог EXPLAIN
};

#endif