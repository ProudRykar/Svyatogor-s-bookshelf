#include "database.h"
#include <iostream>
#include <sstream>
#include <string>
#include <iomanip>
#include <vector>
#include <algorithm>
#include <locale>
#include <clocale>
#include <cwchar>
#include <regex>
#include <limits>

size_t display_width(const std::string& utf8) {
    static bool locale_initialized = []() {
        const char* locale = std::setlocale(LC_CTYPE, "");
        if (locale == nullptr || std::string(locale).find("UTF-8") == std::string::npos) {
            std::cerr << "Предупреждение: локаль не поддерживает UTF-8. Устанавливаем C.UTF-8.\n";
            std::setlocale(LC_CTYPE, "C.UTF-8");
        }
        return true;
    }();

    size_t width = 0;
    const char* ptr = utf8.c_str();
    size_t len = utf8.length();

    for (size_t i = 0; i < len;) {
        unsigned char c = static_cast<unsigned char>(ptr[i]);
        size_t char_len = 0;

        if ((c & 0x80) == 0) { char_len = 1; }
        else if ((c & 0xE0) == 0xC0) { char_len = 2; }
        else if ((c & 0xF0) == 0xE0) { char_len = 3; }
        else if ((c & 0xF8) == 0xF0) { char_len = 4; }
        else {
            i++;
            width += 1;
            continue;
        }

        if (i + char_len > len) {
            width += 1;
            break;
        }

        std::vector<wchar_t> wbuf(char_len + 1);
        size_t converted = std::mbstowcs(wbuf.data(), ptr + i, char_len + 1);
        if (converted == static_cast<size_t>(-1)) {
            width += 1;
            i += char_len;
            continue;
        }

        int w = wcwidth(wbuf[0]);
        if (w >= 0) { width += w; }
        else { width += 1; }

        i += char_len;
    }

    return width;
}

void print_padded(const std::string& str, size_t display_w, bool align_right = false) {
    size_t disp_width = display_width(str);
    size_t padding_needed = display_w - disp_width;

    if (align_right) {
        std::cout << std::string(padding_needed, ' ') << str;
    } else {
        std::cout << str << std::string(padding_needed, ' ');
    }
}

void print_help() {
    std::cout << "Команды:\n"
              << "  ПРИЛОЖИ <номер> <имя> <значение>           - добавить запись (имя в кавычках, если содержит пробелы)\n"
              << "  ИЗЫМИ   <номер>                            - удалить запись\n"
              << "  ОБРЕСТИ <номер>                            - найти запись по ID\n"
              << "  ОБРЕСТИ <имя>                              - найти записи по имени (имя в кавычках, если содержит пробелы)\n"
              << "  ОБРЕСТИ_ЗНАЧЕНИЕ <значение>                - найти записи по точному значению\n"
              << "  ОБРЕСТИ_ДИАПАЗОН >мин<макс                 - найти записи в диапазоне значений (формат: >100<200)\n"
              << "  ОБРЕСТИ_ДИАПАЗОН больше <мин> и меньше <макс> - найти записи в диапазоне значений\n"
              << "  ОБРЕСТИ_ДИАПАЗОН больше <число>            - найти записи со значением больше числа\n"
              << "  ОБРЕСТИ_ДИАПАЗОН меньше <число>            - найти записи со значением меньше числа\n"
              << "  ОБРЕСТИ_ДИАПАЗОН >число                    - найти записи со значением больше числа\n"
              << "  ОБРЕСТИ_ДИАПАЗОН <число                    - найти записи со значением меньше числа\n"
              << "  ОБНОВИ  <номер> <имя> <значение>           - обновить запись (имя в кавычках, если содержит пробелы)\n"
              << "  ПЕРЕЧЕНЬ                                   - показать все записи\n"
              << "  АНАЛИЗ <команда> <параметры>               - проанализировать запрос\n"
              << "  ИСХОД                                      - выйти\n";
}

void print_records(const std::vector<int>& ids, const std::vector<std::string>& names, const std::vector<double>& values) {
    if (ids.empty()) {
        std::cout << "Записи не найдены.\n";
        return;
    }

    std::vector<std::string> id_strs, value_strs;
    id_strs.reserve(ids.size());
    value_strs.reserve(values.size());

    size_t w_id = display_width("НОМЕР");
    size_t w_name = display_width("ИМЯ");
    size_t w_value = display_width("ЗНАЧЕНИЕ");

    for (size_t i = 0; i < ids.size(); ++i) {
        id_strs.push_back(std::to_string(ids[i]));
        w_id = std::max(w_id, display_width(id_strs.back()));
        w_name = std::max(w_name, display_width(names[i]));

        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2) << values[i];
        value_strs.push_back(oss.str());
        w_value = std::max(w_value, display_width(value_strs.back()));
    }
    w_value = std::max(w_value, display_width("ЗНАЧЕНИЕ"));

    auto print_sep = [&](size_t w) {
        std::cout << "+" << std::string(w + 2, '-');
    };
    print_sep(w_id); print_sep(w_name); print_sep(w_value);
    std::cout << "+\n";

    std::cout << "| ";
    print_padded("НОМЕР", w_id);
    std::cout << " | ";
    print_padded("ИМЯ", w_name);
    std::cout << " | ";
    print_padded("ЗНАЧЕНИЕ", w_value, true);
    std::cout << " |\n";

    print_sep(w_id); print_sep(w_name); print_sep(w_value);
    std::cout << "+\n";

    for (size_t i = 0; i < ids.size(); ++i) {
        std::cout << "| ";
        print_padded(id_strs[i], w_id);
        std::cout << " | ";
        print_padded(names[i], w_name);
        std::cout << " | ";
        print_padded(value_strs[i], w_value, true);
        std::cout << " |\n";
    }

    print_sep(w_id); print_sep(w_name); print_sep(w_value);
    std::cout << "+\n";
}

bool extract_name(std::istringstream& iss, std::string& name) {
    std::string token;
    if (!(iss >> std::ws >> token)) {
        return false;
    }

    if (token.front() == '"') {
        if (token.back() == '"') {
            name = token.substr(1, token.length() - 2);
            return true;
        } else {
            name = token.substr(1);
            std::string rest;
            std::getline(iss, rest, '"');
            name += rest;
            return !name.empty();
        }
    } else {
        name = token;
        return true;
    }
}

int main() {
    std::cout << "Выберите режим логирования (0 - без логов, 1 - только файл, 2 - консоль, 3 - файл и консоль): ";
    int log_mode;
    std::cin >> log_mode;
    std::cin.ignore();

    Database db("database.bin", "mysecretkey", log_mode);
    print_help();

    std::string line;
    while (true) {
        std::cout << "> ";
        if (!std::getline(std::cin, line)) break;

        std::istringstream iss(line);
        std::string cmd;
        iss >> cmd;

        if (cmd == "ПРИЛОЖИ") {
            int id;
            std::string name;
            double value;
            if (iss >> id && extract_name(iss, name) && iss >> value) {
                std::cout << (db.add_record(id, name, value)
                              ? "Запись добавлена.\n"
                              : "ID уже существует.\n");
            } else {
                std::cout << "Неверный формат: ПРИЛОЖИ <id> <name> <value> (имя в кавычках, если содержит пробелы)\n";
            }
        } else if (cmd == "ИЗЫМИ") {
            int id;
            if (iss >> id) {
                std::cout << (db.remove_record(id)
                              ? "Запись удалена.\n"
                              : "Запись не найдена.\n");
            } else {
                std::cout << "Неверный формат: ИЗЫМИ <id>\n";
            }
        } else if (cmd == "ОБРЕСТИ") {
            std::string token;
            if (iss >> std::ws >> token) {
                bool is_number = !token.empty() &&
                                 (std::isdigit(token[0]) || (token[0] == '-' && token.size() > 1)) &&
                                 std::all_of(token.begin() + (token[0] == '-' ? 1 : 0), token.end(), ::isdigit);
        
                if (is_number) {
                    try {
                        int id = std::stoi(token);
                        std::string name;
                        double value = 0.0;
        
                        if (db.find_record_by_id(id, name, value)) {
                            std::vector<int> ids = {id};
                            std::vector<std::string> names = {name};
                            std::vector<double> values = {value};
                            print_records(ids, names, values);
                        } else {
                            std::cout << "Запись с ID " << id << " не найдена.\n";
                        }
                    } catch (const std::exception& e) {
                        std::cout << "Ошибка обработки ID: " << e.what() << "\n";
                    }
                } else {
                    std::string name;
                    std::string rest;
                    std::getline(iss, rest);
                    std::istringstream name_iss(token + rest);
                    if (extract_name(name_iss, name)) {
                        std::vector<int> ids;
                        std::vector<double> values;
        
                        if (db.find_record_by_name(name, ids, values)) {
                            std::vector<std::string> names(ids.size(), name);
                            print_records(ids, names, values);
                        } else {
                            std::cout << "Записи с именем '" << name << "' не найдены.\n";
                        }
                    } else {
                        std::cout << "Неверный формат: ОБРЕСТИ <имя> (имя в кавычках, если содержит пробелы)\n";
                    }
                }
            } else {
                std::cout << "Неверный формат: ОБРЕСТИ <ID> или ОБРЕСТИ <имя> (имя в кавычках, если содержит пробелы)\n";
            }
        } else if (cmd == "ОБРЕСТИ_ЗНАЧЕНИЕ") {
            double value;
            if (iss >> value) {
                std::vector<int> ids;
                std::vector<std::string> names;
                std::vector<double> values;
                if (db.find_records_by_value(value, ids, names, values)) {
                    print_records(ids, names, values);
                } else {
                    std::cout << "Записи со значением " << std::fixed << std::setprecision(2) << value << " не найдены.\n";
                }
            } else {
                std::cout << "Неверный формат: ОБРЕСТИ_ЗНАЧЕНИЕ <значение>\n";
            }
        } else if (cmd == "ОБРЕСТИ_ДИАПАЗОН") {
            std::string range_input;
            std::getline(iss, range_input);
            range_input.erase(0, range_input.find_first_not_of(" \t"));

            std::regex range_regex(R"(>\s*(\d+\.?\d*)\s*<\s*(\d+\.?\d*))");
            std::smatch range_match;
            if (std::regex_match(range_input, range_match, range_regex)) {
                try {
                    double min_value = std::stod(range_match[1].str());
                    double max_value = std::stod(range_match[2].str());
                    std::vector<int> ids;
                    std::vector<std::string> names;
                    std::vector<double> values;
                    if (db.find_records_by_value_range(min_value, max_value, true, true, ids, names, values)) {
                        print_records(ids, names, values);
                    } else {
                        std::cout << "Записи в диапазоне значений от " << std::fixed << std::setprecision(2) << min_value 
                                  << " до " << max_value << " не найдены.\n";
                    }
                } catch (const std::exception& e) {
                    std::cout << "Ошибка обработки диапазона: " << e.what() << "\n";
                }
            }
            else {
                std::istringstream range_iss(range_input);
                std::string keyword1, keyword2, keyword3;
                double value, min_value, max_value;

                std::stringstream check_remainder(range_input);
                check_remainder >> keyword1;

                if (keyword1 == "больше") {
                    check_remainder >> value;
                    if (!(check_remainder >> keyword2)) {
                        try {
                            std::vector<int> ids;
                            std::vector<std::string> names;
                            std::vector<double> values;
                            if (db.find_records_by_value_range(value, std::numeric_limits<double>::max(), true, false, ids, names, values)) {
                                print_records(ids, names, values);
                            } else {
                                std::cout << "Записи со значением больше " << std::fixed << std::setprecision(2) << value 
                                          << " не найдены.\n";
                            }
                        } catch (const std::exception& e) {
                            std::cout << "Ошибка обработки значения: " << e.what() << "\n";
                        }
                    } else if (keyword2 == "и" && check_remainder >> keyword3 >> max_value && keyword3 == "меньше") {
                        min_value = value;
                        try {
                            std::vector<int> ids;
                            std::vector<std::string> names;
                            std::vector<double> values;
                            if (db.find_records_by_value_range(min_value, max_value, true, true, ids, names, values)) {
                                print_records(ids, names, values);
                            } else {
                                std::cout << "Записи в диапазоне значений от " << std::fixed << std::setprecision(2) << min_value 
                                          << " до " << max_value << " не найдены.\n";
                            }
                        } catch (const std::exception& e) {
                            std::cout << "Ошибка обработки диапазона: " << e.what() << "\n";
                        }
                    } else {
                        std::cout << "Неверный формат: ожидается 'больше <число>' или 'больше <мин> и меньше <макс>'\n";
                    }
                }
                else if (keyword1 == "меньше" && check_remainder >> value) {
                    try {
                        std::vector<int> ids;
                        std::vector<std::string> names;
                        std::vector<double> values;
                        if (db.find_records_by_value_range(std::numeric_limits<double>::lowest(), value, false, true, ids, names, values)) {
                            print_records(ids, names, values);
                        } else {
                            std::cout << "Записи со значением меньше " << std::fixed << std::setprecision(2) << value 
                                      << " не найдены.\n";
                        }
                    } catch (const std::exception& e) {
                        std::cout << "Ошибка обработки значения: " << e.what() << "\n";
                    }
                }
                else if (std::regex_match(range_input, range_match, std::regex(R"(>\s*(\d+\.?\d*))"))) {
                    try {
                        double min_value = std::stod(range_match[1].str());
                        std::vector<int> ids;
                        std::vector<std::string> names;
                        std::vector<double> values;
                        if (db.find_records_by_value_range(min_value, std::numeric_limits<double>::max(), true, false, ids, names, values)) {
                            print_records(ids, names, values);
                        } else {
                            std::cout << "Записи со значением больше " << std::fixed << std::setprecision(2) << min_value 
                                      << " не найдены.\n";
                        }
                    } catch (const std::exception& e) {
                        std::cout << "Ошибка обработки значения: " << e.what() << "\n";
                    }
                }
                else if (std::regex_match(range_input, range_match, std::regex(R"(<\s*(\d+\.?\d*))"))) {
                    try {
                        double max_value = std::stod(range_match[1].str());
                        std::vector<int> ids;
                        std::vector<std::string> names;
                        std::vector<double> values;
                        if (db.find_records_by_value_range(std::numeric_limits<double>::lowest(), max_value, false, true, ids, names, values)) {
                            print_records(ids, names, values);
                        } else {
                            std::cout << "Записи со значением меньше " << std::fixed << std::setprecision(2) << max_value 
                                      << " не найдены.\n";
                        }
                    } catch (const std::exception& e) {
                        std::cout << "Ошибка обработки значения: " << e.what() << "\n";
                    }
                }
                else {
                    std::cout << "Неверный формат: ОБРЕСТИ_ДИАПАЗОН >мин<макс, ОБРЕСТИ_ДИАПАЗОН больше <мин> и меньше <макс>, "
                              << "ОБРЕСТИ_ДИАПАЗОН больше <число>, ОБРЕСТИ_ДИАПАЗОН меньше <число>, "
                              << "ОБРЕСТИ_ДИАПАЗОН >число или ОБРЕСТИ_ДИАПАЗОН <число\n";
                }
            }
        } else if (cmd == "ОБНОВИ") {
            int id;
            std::string name;
            double value;
            if (iss >> id && extract_name(iss, name) && iss >> value) {
                std::cout << (db.update_record(id, name, value)
                              ? "Запись обновлена.\n"
                              : "Запись с ID " + std::to_string(id) + " не найдена.\n");
            } else {
                std::cout << "Неверный формат: ОБНОВИ <id> <name> <value> (имя в кавычках, если содержит пробелы)\n";
            }
        } else if (cmd == "ПЕРЕЧЕНЬ") {
            std::vector<int> ids;
            std::vector<std::string> names;
            std::vector<double> values;
            db.get_all_records(ids, names, values);
            print_records(ids, names, values);
        } else if (cmd == "АНАЛИЗ") {
            std::string query, params;
            iss >> query;
            std::getline(iss, params);
            db.analyze_query(query, params);
        } else if (cmd == "ИСХОД") {
            break;
        } else {
            print_help();
        }
    }

    return 0;
}