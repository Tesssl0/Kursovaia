#include "httplib.h"
#include "json.hpp"
#include <libpq-fe.h>

#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <ctime>
#include "picosha2.h"
using json = nlohmann::json;


std::string hash_password(const std::string& pass) {
    std::string salt = "SOME_RANDOM_SALT_123"; // можешь заменить
    return picosha2::hash256_hex_string(pass + salt);
}

bool check_password(const std::string& pass, const std::string& hash) {
    std::string salt = "SOME_RANDOM_SALT_123";
    return hash == picosha2::hash256_hex_string(pass + salt);
}

std::string load_conn_string() {
    std::ifstream f("config.ini");
    if (!f.is_open()) {
        std::cerr << "[ERROR] config.ini not found!" << std::endl;
        return "";
    }

    std::string line;
    while (std::getline(f, line)) {
        if (line.rfind("DB_CONN=", 0) == 0) {
            return line.substr(8); 
        }
    }

    std::cerr << "[ERROR] DB_CONN not found in config.ini!" << std::endl;
    return "";
}

std::string CONN_STR = load_conn_string();

void add_cors(httplib::Response& res) {
    res.set_header("Access-Control-Allow-Origin", "*");
    res.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
    res.set_header("Access-Control-Allow-Headers", "Content-Type, Role");
}

PGconn* open_conn() {
    if (CONN_STR.empty()) {
        std::cerr << "[ERROR] Connection string is empty!" << std::endl;
        return nullptr;
    }

    PGconn* conn = PQconnectdb(CONN_STR.c_str());
    if (PQstatus(conn) != CONNECTION_OK) {
        std::cerr << "DB error: " << PQerrorMessage(conn) << std::endl;
        return nullptr;
    }
    return conn;
}


void handle_get_flights(const httplib::Request&, httplib::Response& res) {
    PGconn* conn = open_conn();
    if (!conn) {
        res.status = 500;
        res.set_content("DB connection failed", "text/plain");
        return;
    }

    const char* q =
        "SELECT id, number, origin, destination, "
        "departure_time, arrival_time, aircraft, status, seats, "
        "terminal, gate FROM flights ORDER BY id;";

    PGresult* r = PQexec(conn, q);

    if (PQresultStatus(r) != PGRES_TUPLES_OK) {
        std::string err = PQerrorMessage(conn);
        PQclear(r);
        PQfinish(conn);
        res.status = 500;
        res.set_content(err, "text/plain");
        return;
    }

    int rows = PQntuples(r);
    json flights = json::array();

    for (int i = 0; i < rows; i++) {
        json flight;
        flight["id"]            = std::stoi(PQgetvalue(r, i, 0));
        flight["number"]        = PQgetvalue(r, i, 1);
        flight["origin"]        = PQgetvalue(r, i, 2);
        flight["destination"]   = PQgetvalue(r, i, 3);
        flight["departureTime"] = PQgetvalue(r, i, 4);
        flight["arrivalTime"]   = PQgetvalue(r, i, 5);
        flight["aircraft"]      = PQgetvalue(r, i, 6);
        flight["status"]        = PQgetvalue(r, i, 7);
        flight["seats"]         = std::stoi(PQgetvalue(r, i, 8));

        char* term = PQgetvalue(r, i, 9);
        char* gate = PQgetvalue(r, i, 10);
        flight["terminal"] = (term && *term) ? term : nullptr;
        flight["gate"]     = (gate && *gate) ? gate : nullptr;

        flights.push_back(flight);
    }

    PQclear(r);
    PQfinish(conn);

    add_cors(res);
    res.set_content(flights.dump(), "application/json; charset=utf-8");
}

void handle_post_flight(const httplib::Request& req, httplib::Response& res) {
    add_cors(res);

    std::string role = req.get_header_value("Role");
    if (role != "staff") {
        res.status = 403;
        res.set_content("{\"error\":\"Forbidden\"}", "application/json");
        return;
    }

    if (req.body.empty()) {
        res.status = 400;
        res.set_content("Empty body", "text/plain");
        return;
    }

    std::cout << "[DEBUG] POST /flights body: " << req.body << std::endl;

    json data;
    try {
        data = json::parse(req.body);
    } catch (...) {
        res.status = 400;
        res.set_content("Invalid JSON", "text/plain");
        return;
    }

    std::string number        = data.value("number", "");
    std::string origin        = data.value("origin", "");
    std::string destination   = data.value("destination", "");
    std::string departureTime = data.value("departureTime", "");
    std::string arrivalTime   = data.value("arrivalTime", "");
    int         aircraft_id   = data.value("aircraft", 0);   // ЧИСЛО
    std::string status        = data.value("status", "scheduled");
    std::string terminal      = data.value("terminal", "");
    std::string gate          = data.value("gate", "");
    int         seats         = data.value("seats", 0);

    if (number.empty() || origin.empty() || destination.empty() ||
        departureTime.empty() || arrivalTime.empty() || aircraft_id == 0) {
        res.status = 400;
        res.set_content("Missing required fields", "text/plain");
        return;
    }

    PGconn* conn = open_conn();
    if (!conn) {
        res.status = 500;
        res.set_content("DB connection failed", "text/plain");
        return;
    }

    const char* q =
        "INSERT INTO flights (number, origin, destination, departure_time, arrival_time, "
        "aircraft, status, seats, terminal, gate) "
        "VALUES ($1,$2,$3,$4,$5,$6,$7,$8,$9,$10) RETURNING id;";

    std::string aircraft_str = std::to_string(aircraft_id);
    std::string seats_str    = std::to_string(seats);

    const char* params[10] = {
        number.c_str(),
        origin.c_str(),
        destination.c_str(),
        departureTime.c_str(),
        arrivalTime.c_str(),
        aircraft_str.c_str(),                 // aircraft как число, но передаём строкой
        status.c_str(),
        seats_str.c_str(),
        terminal.empty() ? "" : terminal.c_str(),
        gate.empty()     ? "" : gate.c_str()
    };

    PGresult* r = PQexecParams(
        conn,
        q,
        10,
        nullptr,
        params,
        nullptr,
        nullptr,
        0
    );

    if (PQresultStatus(r) != PGRES_TUPLES_OK) {
        std::string err = PQerrorMessage(conn);
        PQclear(r);
        PQfinish(conn);
        res.status = 500;
        res.set_content("Database error: " + err, "text/plain");
        return;
    }

    int id = std::stoi(PQgetvalue(r, 0, 0));

    PQclear(r);
    PQfinish(conn);

    json response = {
        {"id", id},
        {"number", number},
        {"origin", origin},
        {"destination", destination},
        {"departureTime", departureTime},
        {"arrivalTime", arrivalTime},
        {"aircraft", aircraft_id},
        {"status", status},
        {"seats", seats},
        {"terminal", terminal.empty() ? nullptr : json(terminal)},
        {"gate", gate.empty() ? nullptr : json(gate)}
    };


// === Сохранение в back/logs/flights.json ===
try {
    std::string path = "logs/flights.json";

    json fileData = json::array();

    std::ifstream in(path);
    if (in.is_open()) {
        try {
            in >> fileData;
        } catch (...) {
            fileData = json::array();
        }
        in.close();
    }

    json newFlight = {
        {"id", id},
        {"number", number},
        {"origin", origin},
        {"destination", destination},
        {"departureTime", departureTime},
        {"arrivalTime", arrivalTime},
        {"aircraft", aircraft_id},
        {"status", status},
        {"seats", seats},
        {"terminal", terminal.empty() ? nullptr : json(terminal)},
        {"gate", gate.empty() ? nullptr : json(gate)}
    };

    fileData.push_back(newFlight);
        std::ofstream out(path);
        out << fileData.dump(4);
        out.close();

    } 
    catch (const std::exception& e) {
        std::cerr << "[ERROR] Failed to write back/logs/flights.json: " << e.what() << std::endl;
    }
    
    res.set_content(response.dump(), "application/json; charset=utf-8");
}

void handle_delete_flight(const httplib::Request& req, httplib::Response& res) {
    // CORS — вызываем только один раз
    add_cors(res);

    // Проверка роли
    std::string role = req.get_header_value("Role");
    if (role != "staff") {
        res.status = 403;
        res.set_content("{\"error\":\"Forbidden\"}", "application/json");
        return;
    }

    // Проверка ID
    if (!req.matches.size()) {
        res.status = 400;
        res.set_content("Missing ID", "text/plain");
        return;
    }

    int id = std::stoi(req.matches[1]);
    std::string id_str = std::to_string(id);

    // Подключение к БД
    PGconn* conn = open_conn();
    if (!conn) {
        res.status = 500;
        res.set_content("DB connection failed", "text/plain");
        return;
    }

    const char* q = "DELETE FROM flights WHERE id = $1;";
    const char* params[1] = { id_str.c_str() };

    PGresult* r = PQexecParams(
        conn,
        q,
        1,
        nullptr,
        params,
        nullptr,
        nullptr,
        0
    );

    if (PQresultStatus(r) != PGRES_COMMAND_OK) {
        std::string err = PQerrorMessage(conn);
        PQclear(r);
        PQfinish(conn);
        res.status = 500;
        res.set_content(err, "text/plain");
        return;
    }

    PQclear(r);
    PQfinish(conn);

    // === Удаляем рейс из JSON-файла logs/flights.json ===
    try {
        std::string path = "logs/flights.json";

        json fileData = json::array();

        // 1. Читаем файл
        std::ifstream in(path);
        if (in.is_open()) {
            try {
                in >> fileData;
            } catch (...) {
                fileData = json::array();
            }
            in.close();
        }

        // 2. Создаём новый массив без удалённого рейса
        json newData = json::array();
        for (auto& f : fileData) {
            if (f.contains("id") && f["id"] == id) {
                continue; // пропускаем удаляемый рейс
            }
            newData.push_back(f);
        }

        // 3. Перезаписываем файл
        std::ofstream out(path);
        out << newData.dump(4);
        out.close();

     } catch (const std::exception& e) {
        std::cerr << "[ERROR] Failed to update logs/flights.json: " << e.what() << std::endl;
    }

    json response = { {"status", "deleted"}, {"id", id} };
    res.set_content(response.dump(), "application/json; charset=utf-8");
}


void handle_put_status(const httplib::Request& req, httplib::Response& res) {
    add_cors(res);

    if (!req.matches.size()) {
        res.status = 400;
        res.set_content("Missing ID", "text/plain");
        return;
    }

    int id = std::stoi(req.matches[1]);

    if (req.body.empty()) {
        res.status = 400;
        res.set_content("Empty body", "text/plain");
        return;
    }

    json data;
    try {
        data = json::parse(req.body);
    } catch (...) {
        res.status = 400;
        res.set_content("Invalid JSON", "text/plain");
        return;
    }

    std::string status = data.value("status", "");

    if (status.empty()) {
        res.status = 400;
        res.set_content("Missing status", "text/plain");
        return;
    }

    PGconn* conn = open_conn();
    if (!conn) {
        res.status = 500;
        res.set_content("DB connection failed", "text/plain");
        return;
    }

    const char* q = "UPDATE flights SET status = $1 WHERE id = $2;";
    std::string id_str = std::to_string(id);

    const char* params[2] = {
        status.c_str(),
        id_str.c_str()
    };

    int lengths[2] = {0};
    int formats[2] = {0};

    PGresult* r = PQexecParams(
        conn,
        q,
        2,
        nullptr,
        params,
        lengths,
        formats,
        0
    );

    if (PQresultStatus(r) != PGRES_COMMAND_OK) {
        std::string err = PQerrorMessage(conn);
        PQclear(r);
        PQfinish(conn);
        res.status = 500;
        res.set_content(err, "text/plain");
        return;
    }

    PQclear(r);
    PQfinish(conn);

    json response = {
        {"status", "ok"},
        {"id", id},
        {"newStatus", status}
    };

    res.set_content(response.dump(), "application/json; charset=utf-8");
}

void handle_put_flight(const httplib::Request& req, httplib::Response& res) {
    add_cors(res);

    std::string role = req.get_header_value("Role");
    if (role != "staff") {
        res.status = 403;
        res.set_content("{\"error\":\"Forbidden\"}", "application/json");
        return;
    }

    if (!req.matches.size()) {
        res.status = 400;
        res.set_content("Missing ID", "text/plain");
        return;
    }

    int id = std::stoi(req.matches[1]);

    if (req.body.empty()) {
        res.status = 400;
        res.set_content("Empty body", "text/plain");
        return;
    }

    json data;
    try {
        data = json::parse(req.body);
    } catch (...) {
        res.status = 400;
        res.set_content("Invalid JSON", "text/plain");
        return;
    }

    std::string number        = data.value("number", "");
    std::string origin        = data.value("origin", "");
    std::string destination   = data.value("destination", "");
    std::string departureTime = data.value("departureTime", "");
    std::string arrivalTime   = data.value("arrivalTime", "");
    int         aircraft_id   = data.value("aircraft", 0);   // ЧИСЛО
    std::string status        = data.value("status", "");
    std::string terminal      = data.value("terminal", "");
    std::string gate          = data.value("gate", "");
    int         seats         = data.value("seats", 0);

    PGconn* conn = open_conn();
    if (!conn) {
        res.status = 500;
        res.set_content("DB connection failed", "text/plain");
        return;
    }

    const char* q =
        "UPDATE flights SET "
        "number=$1, origin=$2, destination=$3, "
        "departure_time=$4, arrival_time=$5, "
        "aircraft=$6, status=$7, seats=$8, "
        "terminal=$9, gate=$10 "
        "WHERE id=$11;";

    std::string id_str       = std::to_string(id);
    std::string seats_str    = std::to_string(seats);
    std::string aircraft_str = std::to_string(aircraft_id);

    const char* params[11] = {
        number.c_str(),
        origin.c_str(),
        destination.c_str(),
        departureTime.c_str(),
        arrivalTime.c_str(),
        aircraft_str.c_str(),
        status.c_str(),
        seats_str.c_str(),
        terminal.empty() ? "" : terminal.c_str(),
        gate.empty()     ? "" : gate.c_str(),
        id_str.c_str()
    };

    PGresult* r = PQexecParams(
        conn,
        q,
        11,
        nullptr,
        params,
        nullptr,
        nullptr,
        0
    );

    if (PQresultStatus(r) != PGRES_COMMAND_OK) {
        std::string err = PQerrorMessage(conn);
        PQclear(r);
        PQfinish(conn);
        res.status = 500;
        res.set_content(err, "text/plain");
        return;
    }

    PQclear(r);
    PQfinish(conn);

    // === Обновляем рейс в JSON-файле logs/flights.json ===
    try {
        std::string path = "logs/flights.json";

        json fileData = json::array();

        // 1. Читаем файл
        std::ifstream in(path);
        if (in.is_open()) {
            try {
                in >> fileData;
            } catch (...) {
                fileData = json::array();
            }
            in.close();
        }

        // 2. Создаём новый массив с обновлённым рейсом
        json newData = json::array();
        for (auto& f : fileData) {
            if (f.contains("id") && f["id"] == id) {
                // заменяем старый объект новым
                json updated = {
                    {"id", id},
                    {"number", number},
                    {"origin", origin},
                    {"destination", destination},
                    {"departureTime", departureTime},
                    {"arrivalTime", arrivalTime},
                    {"aircraft", aircraft_id},
                    {"status", status},
                    {"seats", seats},
                    {"terminal", terminal.empty() ? nullptr : json(terminal)},
                    {"gate", gate.empty() ? nullptr : json(gate)}
                };
                newData.push_back(updated);
            } else {
                newData.push_back(f);
            }
        }

        // 3. Перезаписываем файл
        std::ofstream out(path);
        out << newData.dump(4);
        out.close();

    } catch (const std::exception& e) {
        std::cerr << "[ERROR] Failed to update logs/flights.json: " << e.what() << std::endl;
    }


    json response = {
        {"status", "updated"},
        {"id", id}
    };

    res.set_content(response.dump(), "application/json; charset=utf-8");
}

void handle_get_aircraft(const httplib::Request&, httplib::Response& res) {
    add_cors(res);

    PGconn* conn = open_conn();
    if (!conn) {
        res.status = 500;
        res.set_content("DB connection failed", "text/plain");
        return;
    }

    const char* q = "SELECT id, model, manufacturer, seats FROM aircraft ORDER BY id;";
    PGresult* r = PQexec(conn, q);

    if (PQresultStatus(r) != PGRES_TUPLES_OK) {
        std::string err = PQerrorMessage(conn);
        PQclear(r);
        PQfinish(conn);
        res.status = 500;
        res.set_content(err, "text/plain");
        return;
    }

    json arr = json::array();
    int rows = PQntuples(r);

    for (int i = 0; i < rows; i++) {
        json item;
        item["id"] = std::stoi(PQgetvalue(r, i, 0));
        item["model"] = PQgetvalue(r, i, 1);
        item["manufacturer"] = PQgetvalue(r, i, 2);
        item["seats"] = std::stoi(PQgetvalue(r, i, 3));
        arr.push_back(item);
    }

    PQclear(r);
    PQfinish(conn);

    res.set_content(arr.dump(), "application/json; charset=utf-8");
}

void handle_post_aircraft(const httplib::Request& req, httplib::Response& res) {
    add_cors(res);

    if (req.body.empty()) {
        res.status = 400;
        res.set_content("Empty body", "text/plain");
        return;
    }

    json data;
    try {
        data = json::parse(req.body);
    } catch (...) {
        res.status = 400;
        res.set_content("Invalid JSON", "text/plain");
        return;
    }

    std::string model = data.value("model", "");
    std::string manufacturer = data.value("manufacturer", "");
    int seats = data.value("seats", 0);

    if (model.empty() || manufacturer.empty() || seats <= 0) {
        res.status = 400;
        res.set_content("Missing fields", "text/plain");
        return;
    }

    PGconn* conn = open_conn();
    if (!conn) {
        res.status = 500;
        res.set_content("DB connection failed", "text/plain");
        return;
    }

    const char* q =
        "INSERT INTO aircraft (model, manufacturer, seats) "
        "VALUES ($1, $2, $3) RETURNING id;";

    std::string seats_str = std::to_string(seats);

    const char* params[3] = {
        model.c_str(),
        manufacturer.c_str(),
        seats_str.c_str()
    };

    int lengths[3] = {0};
    int formats[3] = {0};

    PGresult* r = PQexecParams(
        conn,
        q,
        3,
        nullptr,
        params,
        lengths,
        formats,
        0
    );

    if (PQresultStatus(r) != PGRES_TUPLES_OK) {
        std::string err = PQerrorMessage(conn);
        PQclear(r);
        PQfinish(conn);
        res.status = 500;
        res.set_content(err, "text/plain");
        return;
    }

    int id = std::stoi(PQgetvalue(r, 0, 0));

    PQclear(r);
    PQfinish(conn);

    json response = {
        {"id", id},
        {"model", model},
        {"manufacturer", manufacturer},
        {"seats", seats}
    };

    res.set_content(response.dump(), "application/json; charset=utf-8");
}

void handle_delete_aircraft(const httplib::Request& req, httplib::Response& res) {
    add_cors(res);

    if (!req.matches.size()) {
        res.status = 400;
        res.set_content("Missing ID", "text/plain");
        return;
    }

    int id = std::stoi(req.matches[1]);

    PGconn* conn = open_conn();
    if (!conn) {
        res.status = 500;
        res.set_content("DB connection failed", "text/plain");
        return;
    }

    const char* q = "DELETE FROM aircraft WHERE id = $1;";
    std::string id_str = std::to_string(id);

    const char* params[1] = { id_str.c_str() };
    int lengths[1] = {0};
    int formats[1] = {0};

    PGresult* r = PQexecParams(
        conn,
        q,
        1,
        nullptr,
        params,
        lengths,
        formats,
        0
    );

    if (PQresultStatus(r) != PGRES_COMMAND_OK) {
        std::string err = PQerrorMessage(conn);
        PQclear(r);
        PQfinish(conn);
        res.status = 500;
        res.set_content(err, "text/plain");
        return;
    }

    PQclear(r);
    PQfinish(conn);

    json response = { {"status", "deleted"}, {"id", id} };
    res.set_content(response.dump(), "application/json; charset=utf-8");
}

void handle_put_aircraft(const httplib::Request& req, httplib::Response& res) {
    add_cors(res);

    if (!req.matches.size()) {
        res.status = 400;
        res.set_content("Missing ID", "text/plain");
        return;
    }

    int id = std::stoi(req.matches[1]);

    if (req.body.empty()) {
        res.status = 400;
        res.set_content("Empty body", "text/plain");
        return;
    }

    json data;
    try {
        data = json::parse(req.body);
    } catch (...) {
        res.status = 400;
        res.set_content("Invalid JSON", "text/plain");
        return;
    }

    std::string model = data.value("model", "");
    std::string manufacturer = data.value("manufacturer", "");
    int seats = data.value("seats", 0);

    if (model.empty() || manufacturer.empty() || seats <= 0) {
        res.status = 400;
        res.set_content("Missing fields", "text/plain");
        return;
    }

    PGconn* conn = open_conn();
    if (!conn) {
        res.status = 500;
        res.set_content("DB connection failed", "text/plain");
        return;
    }

    const char* q =
        "UPDATE aircraft SET model=$1, manufacturer=$2, seats=$3 WHERE id=$4;";

    std::string seats_str = std::to_string(seats);
    std::string id_str = std::to_string(id);

    const char* params[4] = {
        model.c_str(),
        manufacturer.c_str(),
        seats_str.c_str(),
        id_str.c_str()
    };

    int lengths[4] = {0};
    int formats[4] = {0};

    PGresult* r = PQexecParams(
        conn,
        q,
        4,
        nullptr,
        params,
        lengths,
        formats,
        0
    );

    if (PQresultStatus(r) != PGRES_COMMAND_OK) {
        std::string err = PQerrorMessage(conn);
        PQclear(r);
        PQfinish(conn);
        res.status = 500;
        res.set_content(err, "text/plain");
        return;
    }

    PQclear(r);
    PQfinish(conn);

    json response = {
        {"status", "updated"},
        {"id", id}
    };

    res.set_content(response.dump(), "application/json; charset=utf-8");
}

void handle_get_airports(const httplib::Request&, httplib::Response& res) {
    add_cors(res);

    PGconn* conn = open_conn();
    if (!conn) {
        res.status = 500;
        res.set_content("DB connection failed", "text/plain");
        return;
    }

    const char* q = "SELECT id, code, name, city, country FROM airports ORDER BY id;";
    PGresult* r = PQexec(conn, q);

    if (PQresultStatus(r) != PGRES_TUPLES_OK) {
        std::string err = PQerrorMessage(conn);
        PQclear(r);
        PQfinish(conn);
        res.status = 500;
        res.set_content(err, "text/plain");
        return;
    }

    json arr = json::array();
    int rows = PQntuples(r);

    for (int i = 0; i < rows; i++) {
        json item;
        item["id"]      = std::stoi(PQgetvalue(r, i, 0));
        item["code"]    = PQgetvalue(r, i, 1);
        item["name"]    = PQgetvalue(r, i, 2);
        item["city"]    = PQgetvalue(r, i, 3);
        item["country"] = PQgetvalue(r, i, 4);
        arr.push_back(item);
    }

    PQclear(r);
    PQfinish(conn);

    res.set_content(arr.dump(), "application/json; charset=utf-8");
}

void handle_post_airport(const httplib::Request& req, httplib::Response& res) {
    add_cors(res);

    if (req.body.empty()) {
        res.status = 400;
        res.set_content("Empty body", "text/plain");
        return;
    }

    json data;
    try {
        data = json::parse(req.body);
    } catch (...) {
        res.status = 400;
        res.set_content("Invalid JSON", "text/plain");
        return;
    }

    std::string code    = data.value("code", "");
    std::string name    = data.value("name", "");
    std::string city    = data.value("city", "");
    std::string country = data.value("country", "");

    if (code.empty() || name.empty() || city.empty() || country.empty()) {
        res.status = 400;
        res.set_content("Missing fields", "text/plain");
        return;
    }

    PGconn* conn = open_conn();
    if (!conn) {
        res.status = 500;
        res.set_content("DB connection failed", "text/plain");
        return;
    }

    const char* q =
        "INSERT INTO airports (code, name, city, country) "
        "VALUES ($1, $2, $3, $4) RETURNING id;";

    const char* params[4] = {
        code.c_str(),
        name.c_str(),
        city.c_str(),
        country.c_str()
    };

    int lengths[4] = {0};
    int formats[4] = {0};

    PGresult* r = PQexecParams(
        conn,
        q,
        4,
        nullptr,
        params,
        lengths,
        formats,
        0
    );

    if (PQresultStatus(r) != PGRES_TUPLES_OK) {
        std::string err = PQerrorMessage(conn);
        PQclear(r);
        PQfinish(conn);
        res.status = 500;
        res.set_content(err, "text/plain");
        return;
    }

    int id = std::stoi(PQgetvalue(r, 0, 0));

    PQclear(r);
    PQfinish(conn);

    json response = {
        {"id", id},
        {"code", code},
        {"name", name},
        {"city", city},
        {"country", country}
    };

    res.set_content(response.dump(), "application/json; charset=utf-8");
}

void handle_delete_airport(const httplib::Request& req, httplib::Response& res) {
    add_cors(res);

    if (!req.matches.size()) {
        res.status = 400;
        res.set_content("Missing ID", "text/plain");
        return;
    }

    int id = std::stoi(req.matches[1]);
    std::string id_str = std::to_string(id);

    PGconn* conn = open_conn();
    if (!conn) {
        res.status = 500;
        res.set_content("DB connection failed", "text/plain");
        return;
    }

    const char* q = "DELETE FROM airports WHERE id = $1;";
    const char* params[1] = { id_str.c_str() };

    int lengths[1] = {0};
    int formats[1] = {0};

    PGresult* r = PQexecParams(
        conn,
        q,
        1,
        nullptr,
        params,
        lengths,
        formats,
        0
    );

    if (PQresultStatus(r) != PGRES_COMMAND_OK) {
        std::string err = PQerrorMessage(conn);
        PQclear(r);
        PQfinish(conn);
        res.status = 500;
        res.set_content(err, "text/plain");
        return;
    }

    PQclear(r);
    PQfinish(conn);

    json response = { {"status", "deleted"}, {"id", id} };
    res.set_content(response.dump(), "application/json; charset=utf-8");
}

void handle_put_airport(const httplib::Request& req, httplib::Response& res) {
    add_cors(res);

    if (!req.matches.size()) {
        res.status = 400;
        res.set_content("Missing ID", "text/plain");
        return;
    }

    int id = std::stoi(req.matches[1]);
    std::string id_str = std::to_string(id);

    if (req.body.empty()) {
        res.status = 400;
        res.set_content("Empty body", "text/plain");
        return;
    }

    json data;
    try {
        data = json::parse(req.body);
    } catch (...) {
        res.status = 400;
        res.set_content("Invalid JSON", "text/plain");
        return;
    }

    std::string code    = data.value("code", "");
    std::string name    = data.value("name", "");
    std::string city    = data.value("city", "");
    std::string country = data.value("country", "");

    if (code.empty() || name.empty() || city.empty() || country.empty()) {
        res.status = 400;
        res.set_content("Missing fields", "text/plain");
        return;
    }

    PGconn* conn = open_conn();
    if (!conn) {
        res.status = 500;
        res.set_content("DB connection failed", "text/plain");
        return;
    }

    const char* q =
        "UPDATE airports SET code=$1, name=$2, city=$3, country=$4 WHERE id=$5;";

    const char* params[5] = {
        code.c_str(),
        name.c_str(),
        city.c_str(),
        country.c_str(),
        id_str.c_str()
    };

    int lengths[5] = {0};
    int formats[5] = {0};

    PGresult* r = PQexecParams(
        conn,
        q,
        5,
        nullptr,
        params,
        lengths,
        formats,
        0
    );

    if (PQresultStatus(r) != PGRES_COMMAND_OK) {
        std::string err = PQerrorMessage(conn);
        PQclear(r);
        PQfinish(conn);
        res.status = 500;
        res.set_content(err, "text/plain");
        return;
    }

    PQclear(r);
    PQfinish(conn);

    json response = {
        {"status", "updated"},
        {"id", id}
    };

    res.set_content(response.dump(), "application/json; charset=utf-8");
}

void handle_stats_flights(const httplib::Request&, httplib::Response& res) {
    add_cors(res);

    PGconn* conn = open_conn();
    if (!conn) {
        res.status = 500;
        res.set_content("DB connection failed", "text/plain");
        return;
    }

    const char* q =
        "SELECT status, COUNT(*) "
        "FROM flights "
        "GROUP BY status;";

    PGresult* r = PQexec(conn, q);

    if (PQresultStatus(r) != PGRES_TUPLES_OK) {
        std::string err = PQerrorMessage(conn);
        PQclear(r);
        PQfinish(conn);
        res.status = 500;
        res.set_content(err, "text/plain");
        return;
    }

    json stats = json::object();

    int rows = PQntuples(r);
    for (int i = 0; i < rows; i++) {
        std::string status = PQgetvalue(r, i, 0);
        int count = std::stoi(PQgetvalue(r, i, 1));
        stats[status] = count;
    }

    PQclear(r);
    PQfinish(conn);

    res.set_content(stats.dump(), "application/json; charset=utf-8");
}

void handle_stats_terminals(const httplib::Request&, httplib::Response& res) {
    add_cors(res);

    PGconn* conn = open_conn();
    if (!conn) {
        res.status = 500;
        res.set_content("DB connection failed", "text/plain");
        return;
    }

    const char* q =
        "SELECT terminal, COUNT(*) "
        "FROM flights "
        "WHERE terminal IS NOT NULL "
        "GROUP BY terminal "
        "ORDER BY terminal;";

    PGresult* r = PQexec(conn, q);

    if (PQresultStatus(r) != PGRES_TUPLES_OK) {
        std::string err = PQerrorMessage(conn);
        PQclear(r);
        PQfinish(conn);
        res.status = 500;
        res.set_content(err, "text/plain");
        return;
    }

    json stats = json::object();

    int rows = PQntuples(r);
    for (int i = 0; i < rows; i++) {
        std::string terminal = PQgetvalue(r, i, 0);
        int count = std::stoi(PQgetvalue(r, i, 1));
        stats[terminal] = count;
    }

    PQclear(r);
    PQfinish(conn);

    res.set_content(stats.dump(), "application/json; charset=utf-8");
}

void handle_stats_upcoming(const httplib::Request&, httplib::Response& res) {
    add_cors(res);

    PGconn* conn = open_conn();
    if (!conn) {
        res.status = 500;
        res.set_content("DB connection failed", "text/plain");
        return;
    }

    const char* q =
        "SELECT id, number, origin, destination, departure_time, status "
        "FROM flights "
        "WHERE departure_time > NOW() "
        "ORDER BY departure_time "
        "LIMIT 10;";

    PGresult* r = PQexec(conn, q);

    if (PQresultStatus(r) != PGRES_TUPLES_OK) {
        std::string err = PQerrorMessage(conn);
        PQclear(r);
        PQfinish(conn);
        res.status = 500;
        res.set_content(err, "text/plain");
        return;
    }

    json arr = json::array();
    int rows = PQntuples(r);

    for (int i = 0; i < rows; i++) {
        json f;
        f["id"]            = std::stoi(PQgetvalue(r, i, 0));
        f["number"]        = PQgetvalue(r, i, 1);
        f["origin"]        = PQgetvalue(r, i, 2);
        f["destination"]   = PQgetvalue(r, i, 3);
        f["departureTime"] = PQgetvalue(r, i, 4);
        f["status"]        = PQgetvalue(r, i, 5);
        arr.push_back(f);
    }

    PQclear(r);
    PQfinish(conn);

    res.set_content(arr.dump(), "application/json; charset=utf-8");
}

void handle_register(const httplib::Request& req, httplib::Response& res) {
    add_cors(res);

    if (req.body.empty()) {
        res.status = 400;
        res.set_content("Empty body", "text/plain");
        return;
    }

    json data;
    try {
        data = json::parse(req.body);
    } catch (...) {
        res.status = 400;
        res.set_content("Invalid JSON", "text/plain");
        return;
    }

    std::string username = data.value("username", "");
    std::string password = data.value("password", "");

    if (username.empty() || password.empty()) {
        res.status = 400;
        res.set_content("Missing username or password", "text/plain");
        return;
    }

    PGconn* conn = open_conn();
    if (!conn) {
        res.status = 500;
        res.set_content("DB connection failed", "text/plain");
        return;
    }

    // Проверяем, существует ли пользователь
    const char* check_q = "SELECT id FROM users WHERE username=$1;";
    const char* check_params[1] = { username.c_str() };

    PGresult* r = PQexecParams(conn, check_q, 1, nullptr, check_params, nullptr, nullptr, 0);

    if (PQntuples(r) > 0) {
        PQclear(r);
        PQfinish(conn);
        res.status = 409;
        res.set_content("User already exists", "text/plain");
        return;
    }
    PQclear(r);

    // Хэшируем пароль
    std::string hash = hash_password(password);

    // Добавляем пользователя
    const char* q =
        "INSERT INTO users (username, password_hash, role) "
        "VALUES ($1, $2, 'user');";

    const char* params[2] = { username.c_str(), hash.c_str() };

    r = PQexecParams(conn, q, 2, nullptr, params, nullptr, nullptr, 0);

    if (PQresultStatus(r) != PGRES_COMMAND_OK) {
        std::string err = PQerrorMessage(conn);
        PQclear(r);
        PQfinish(conn);
        res.status = 500;
        res.set_content(err, "text/plain");
        return;
    }

    PQclear(r);
    PQfinish(conn);

    json response = {
        {"status", "registered"},
        {"username", username},
        {"role", "user"}
    };

    res.set_content(response.dump(), "application/json");
}


void handle_login(const httplib::Request& req, httplib::Response& res) {
    add_cors(res);

    if (req.body.empty()) {
        res.status = 400;
        res.set_content("{\"error\":\"Empty body\"}", "application/json");
        return;
    }

    json data;
    try {
        data = json::parse(req.body);
    } catch (...) {
        res.status = 400;
        res.set_content("{\"error\":\"Invalid JSON\"}", "application/json");
        return;
    }

    std::string username = data.value("username", "");
    std::string password = data.value("password", "");

    // Отладка: выводим полученные данные
    std::cout << "[DEBUG] Login attempt: username=" << username 
              <</* ", password=" << password <<*/ std::endl;

    PGconn* conn = open_conn();
    if (!conn) {
        res.status = 500;
        res.set_content("{\"error\":\"DB connection failed\"}", "application/json");
        return;
    }

    const char* q = "SELECT password_hash, role FROM users WHERE username=$1;";
    const char* params[1] = { username.c_str() };

    PGresult* r = PQexecParams(conn, q, 1, nullptr, params, nullptr, nullptr, 0);

    if (PQntuples(r) == 0) {
        PQclear(r);
        PQfinish(conn);
        res.status = 401;
        res.set_content("{\"error\":\"User not found\"}", "application/json");
        std::cout << "[DEBUG] User not found: " << username << std::endl;
        return;
    }

    std::string hash = PQgetvalue(r, 0, 0);
    std::string role = PQgetvalue(r, 0, 1);

    PQclear(r);
    PQfinish(conn);

    // Отладка: выводим хэш из базы и вычисленный хэш
    std::cout << "[DEBUG] DB hash: " << hash << std::endl;
    std::cout << "[DEBUG] Computed hash: HASH_" << password << std::endl;
    std::cout << "[DEBUG] Check result: " << check_password(password, hash) << std::endl;

    if (!check_password(password, hash)) {
        res.status = 403;
        res.set_content("{\"error\":\"Wrong password\"}", "application/json");
        return;
    }

    json response = {
        {"status", "ok"},
        {"username", username},
        {"role", role}
    };

    res.set_content(response.dump(), "application/json");
}

void handle_update_status(const httplib::Request& req, httplib::Response& res) {
    add_cors(res);

    std::string role = req.get_header_value("Role");
    if (role != "staff") {
        res.status = 403;
        res.set_content("{\"error\":\"Forbidden\"}", "application/json");
        return;
    }

    if (!req.matches.size()) {
        res.status = 400;
        res.set_content("Missing ID", "text/plain");
        return;
    }

    int id = std::stoi(req.matches[1]);
    std::string id_str = std::to_string(id);

    json data;
    try {
        data = json::parse(req.body);
    } catch (...) {
        res.status = 400;
        res.set_content("Invalid JSON", "text/plain");
        return;
    }

    if (!data.contains("status")) {
        res.status = 400;
        res.set_content("Missing status", "text/plain");
        return;
    }

    std::string new_status = data["status"];

    PGconn* conn = open_conn();
    if (!conn) {
        res.status = 500;
        res.set_content("DB connection failed", "text/plain");
        return;
    }

    const char* q = "UPDATE flights SET status = $1 WHERE id = $2;";
    const char* params[2] = { new_status.c_str(), id_str.c_str() };

    PGresult* r = PQexecParams(conn, q, 2, nullptr, params, nullptr, nullptr, 0);

    if (PQresultStatus(r) != PGRES_COMMAND_OK) {
        std::string err = PQerrorMessage(conn);
        PQclear(r);
        PQfinish(conn);
        res.status = 500;
        res.set_content(err, "text/plain");
        return;
    }

    PQclear(r);
    PQfinish(conn);

    json response = { {"status", "updated"}, {"id", id}, {"new_status", new_status} };
    res.set_content(response.dump(), "application/json; charset=utf-8");
}

void handle_import_flights(const httplib::Request& req, httplib::Response& res) {
    add_cors(res);
    
    // Проверка роли
    std::string role = req.get_header_value("Role");
    if (role != "staff") {
        res.status = 403;
        res.set_content("{\"error\":\"Forbidden\"}", "application/json");
        return;
    }
    
    // Получить содержимое файла из тела запроса
    std::string fileContent = req.body;
    
    if (fileContent.empty()) {
        res.status = 400;
        res.set_content("{\"error\":\"Empty file content\"}", "application/json");
        return;
    }
    
    PGconn* conn = open_conn();
    if (!conn) {
        res.status = 500;
        res.set_content("{\"error\":\"DB connection failed\"}", "application/json");
        return;
    }
    
    // Парсинг CSV
    std::vector<std::string> lines;
    std::stringstream ss(fileContent);
    std::string line;
    
    int successCount = 0;
    int errorCount = 0;
    std::vector<std::string> errors;
    
    while (std::getline(ss, line)) {
        if (line.empty()) continue;
        
        // Разбор строки (формат: номер,откуда,куда,время_вылета,время_прилета,самолет,статус,мест,терминал,gate)
        std::vector<std::string> fields;
        std::stringstream lineStream(line);
        std::string field;
        
        while (std::getline(lineStream, field, ',')) {
            fields.push_back(field);
        }
        
        if (fields.size() < 10) {
            errorCount++;
            errors.push_back("Invalid format: " + line);
            continue;
        }
        
        std::string number = fields[0];
        std::string origin = fields[1];
        std::string destination = fields[2];
        std::string departureTime = fields[3];
        std::string arrivalTime = fields[4];
        int aircraft = std::stoi(fields[5]);
        std::string status = fields[6];
        int seats = std::stoi(fields[7]);
        std::string terminal = fields[8];
        std::string gate = fields[9];
        
        // INSERT в базу
        const char* q = 
            "INSERT INTO flights (number, origin, destination, departure_time, arrival_time, "
            "aircraft, status, seats, terminal, gate) "
            "VALUES ($1,$2,$3,$4,$5,$6,$7,$8,$9,$10)";
        
        std::string aircraft_str = std::to_string(aircraft);
        std::string seats_str = std::to_string(seats);
        
        const char* params[10] = {
            number.c_str(), origin.c_str(), destination.c_str(),
            departureTime.c_str(), arrivalTime.c_str(),
            aircraft_str.c_str(), status.c_str(), seats_str.c_str(),
            terminal.c_str(), gate.c_str()
        };
        
        PGresult* r = PQexecParams(conn, q, 10, nullptr, params, nullptr, nullptr, 0);
        
        if (PQresultStatus(r) != PGRES_COMMAND_OK) {
            errorCount++;
            std::string err = PQerrorMessage(conn);
            errors.push_back("DB error for " + number + ": " + err);
        } else {
            successCount++;
        }
        
        PQclear(r);
    }
    
    PQfinish(conn);
    
    json response = {
        {"success", successCount},
        {"errors", errorCount},
        {"errorList", errors}
    };
    
    res.set_content(response.dump(), "application/json; charset=utf-8");
}

// Функция записи в лог-файл
void log_flight_to_file(const json& flight, const std::string& action) {
    std::string log_dir = "logs";
    
    // Создаем папку logs если её нет
    system(("mkdir -p " + log_dir).c_str());
    
    std::string filename = log_dir + "/flights.log";
    std::ofstream file(filename, std::ios::app); // app = добавление в конец
    
    if (file.is_open()) {
        // Получаем текущее время
        time_t now = time(nullptr);
        char timestamp[20];
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));
        
        // Записываем в лог
        file << "[" << timestamp << "] " << action << ": ";
        file << flight.dump() << std::endl;
        
        file.close();
        std::cout << "[LOG] Записано в файл: " << filename << std::endl;
    } else {
        std::cerr << "[ERROR] Не удалось открыть файл: " << filename << std::endl;
    }
}

// Функция сохранения всех рейсов в JSON (бэкап)
void backup_all_flights_to_json(PGconn* conn) {
    // Получаем все рейсы
    const char* q = "SELECT id, number, origin, destination, departure_time, arrival_time, "
                    "aircraft, status, seats, terminal, gate FROM flights ORDER BY id;";
    
    PGresult* r = PQexec(conn, q);
    if (PQresultStatus(r) != PGRES_TUPLES_OK) {
        PQclear(r);
        return;
    }
    
    json flights = json::array();
    int rows = PQntuples(r);
    
    for (int i = 0; i < rows; i++) {
        json flight;
        flight["id"] = std::stoi(PQgetvalue(r, i, 0));
        flight["number"] = PQgetvalue(r, i, 1);
        flight["origin"] = PQgetvalue(r, i, 2);
        flight["destination"] = PQgetvalue(r, i, 3);
        flight["departureTime"] = PQgetvalue(r, i, 4);
        flight["arrivalTime"] = PQgetvalue(r, i, 5);
        flight["aircraft"] = PQgetvalue(r, i, 6);
        flight["status"] = PQgetvalue(r, i, 7);
        flight["seats"] = std::stoi(PQgetvalue(r, i, 8));
        
        char* term = PQgetvalue(r, i, 9);
        char* gate = PQgetvalue(r, i, 10);
        flight["terminal"] = (term && *term) ? term : nullptr;
        flight["gate"] = (gate && *gate) ? gate : nullptr;
        
        flights.push_back(flight);
    }
    
    PQclear(r);
    
    // Сохраняем в JSON файл
    std::string backup_dir = "backups";
    system(("mkdir -p " + backup_dir).c_str());
    
    time_t now = time(nullptr);
    char timestamp[20];
    strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", localtime(&now));
    
    std::string filename = backup_dir + "/flights_backup_" + timestamp + ".json";
    std::ofstream file(filename);
    
    if (file.is_open()) {
        file << flights.dump(4); // pretty print с отступом 4
        file.close();
        std::cout << "[BACKUP] Сохранено в: " << filename << std::endl;
    }
}

int main() {

    httplib::Server svr;
    svr.Post("/flights/import", handle_import_flights);
    
    svr.Get("/flights", handle_get_flights);
    svr.Post("/flights", handle_post_flight);
	svr.Delete(R"(/flights/(\d+))", handle_delete_flight);
	svr.Put(R"(/flights/(\d+)/status)", handle_put_status);
	svr.Put(R"(/flights/(\d+))", handle_put_flight);

    svr.Get("/aircraft", handle_get_aircraft);
    svr.Post("/aircraft", handle_post_aircraft);
    svr.Delete(R"(/aircraft/(\d+))", handle_delete_aircraft);
    svr.Put(R"(/aircraft/(\d+))", handle_put_aircraft);

    svr.Get("/airports", handle_get_airports);
    svr.Post("/airports", handle_post_airport);
    svr.Delete(R"(/airports/(\d+))", handle_delete_airport);
    svr.Put(R"(/airports/(\d+))", handle_put_airport);

    svr.Get("/stats/flights", handle_stats_flights);
    svr.Get("/stats/terminals", handle_stats_terminals);
    svr.Get("/stats/upcoming", handle_stats_upcoming);

    svr.Post("/register", handle_register);
    svr.Post("/login", handle_login);


    // CORS preflight
    svr.Options(R"(/.*)", [](const httplib::Request&, httplib::Response& res) {
        add_cors(res);
        res.status = 200;
    });

    std::cout << "Server started: http://localhost:8080\n";
    svr.listen("0.0.0.0", 8080);

    return 0;
}