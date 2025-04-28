#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include "database.h"

namespace py = pybind11;

PYBIND11_MODULE(database, m) {
    py::class_<Database>(m, "Database")
        .def(py::init<const std::string&, const std::string&, int>())
        .def("add_record", &Database::add_record, "Add a record to the database")
        .def("remove_record", &Database::remove_record, "Remove a record by ID")
        .def("find_record_by_id", [](Database& db, int id) {
            std::string name;
            double value;
            bool result = db.find_record_by_id(id, name, value);
            return py::make_tuple(result, name, value);
        }, "Find a record by ID")
        .def("find_record_by_name", [](Database& db, const std::string& name) {
            std::vector<int> ids;
            std::vector<double> values;
            bool result = db.find_record_by_name(name, ids, values);
            return py::make_tuple(result, ids, values);
        }, "Find records by name")
        .def("update_record", &Database::update_record, "Update a record")
        .def("get_all_records", [](Database& db) {
            std::vector<int> ids;
            std::vector<std::string> names;
            std::vector<double> values;
            db.get_all_records(ids, names, values);
            return py::make_tuple(ids, names, values);
        }, "Get all records")
        .def("analyze_query", &Database::analyze_query, "Analyze a query");
}