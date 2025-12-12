#include <iostream>
#include <chrono>
#include <thread>
#include <vector>
#include <string>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <cstdlib>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <sqlite3.h>

#define BUS "/dev/i2c-1"
#define ADD 0x44   // HS3003 I2C address

// ---- HUMIDITY SENSOR READING ----
float readHumidity() {
    int file = open(BUS, O_RDWR);
    if (file < 0) {
        std::cerr << "Failed to open bus\n";
        return -1;
    }

    if (ioctl(file, I2C_SLAVE, ADD) < 0) {
        std::cerr << "Failed to acquire bus access\n";
        close(file);
        return -1;
    }

    uint8_t cmd = 0x00;  // HS3003 measurement trigger
    if (write(file, &cmd, 1) != 1) {
        std::cerr << "Failed to write measurement command\n";
        close(file);
        return -1;
    }

    usleep(15000);  // Measurement delay

    uint8_t data[4];
    if (read(file, data, 4) != 4) {
        std::cerr << "Failed to read data\n";
        close(file);
        return -1;
    }

    close(file);

    uint16_t rawHum = (data[0] << 8) | data[1];
    float humidity = (rawHum / 65535.0f) * 100.0f;

    return humidity;
}

// ---- TIMESTAMP ----
std::string currentTime() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&t), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

// ---- TABLE CREATION ----
bool createTable(sqlite3* db) {
    const char* sql =
        "CREATE TABLE IF NOT EXISTS Humidity ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "timestamp TEXT NOT NULL,"
        "humidity REAL NOT NULL);";

    char* errMsg = nullptr;
    int rc = sqlite3_exec(db, sql, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        std::cerr << "Error creating table: " << errMsg << std::endl;
        sqlite3_free(errMsg);
        return false;
    }
    return true;
}

// ---- INSERT READING ----
bool insertReading(sqlite3* db, const std::string& timestamp, float humidity) {
    const char* sql = "INSERT INTO Humidity (timestamp, humidity) VALUES (?, ?);";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "Failed to prepare statement.\n";
        return false;
    }

    sqlite3_bind_text(stmt, 1, timestamp.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 2, humidity);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        std::cerr << "Failed to insert reading.\n";
        sqlite3_finalize(stmt);
        return false;
    }

    sqlite3_finalize(stmt);
    return true;
}

// ---- CLEAN OLD DATA ----
bool cleanOldData(sqlite3* db) {
    const char* sql =
        "DELETE FROM Humidity "
        "WHERE timestamp <= datetime('now', '-48 hours');";

    char* errMsg = nullptr;
    int rc = sqlite3_exec(db, sql, nullptr, nullptr, &errMsg);

    if (rc != SQLITE_OK) {
        std::cerr << "Cleanup Error: " << errMsg << std::endl;
        sqlite3_free(errMsg);
        return false;
    }
    return true;
}

// ---- Output file of data between two time stamps ----
bool exportData(sqlite3* db, std::string& startTime, std::string& endTime, std::string& outputFile) {
    std::ofstream out(outputFile);
    if (!out>is_open()) {
	std::cerr << "Failed to open output file: " << outputFile << std::endl;
	return false;
    }
    out << "timestamp,humidity\n";
    const char* sql = 
	"SELECT timestamp, humidity FROM Humidity "
	"WHERE timestamp >= ? AND timestamp <= ? "
	"ORDER BY timestamp ASC;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
	std::cerr << "Failed to prepare export query.\n";
	return false;
    }
    sqlite3_bind_text(stmt, 1, startTime.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, endTime.c_str(), -1, SQLITE_TRANSIENT);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
	std::string ts = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
	double hum = sqlite3_column_double(stmt, 1);
	out << ts << "," << hum << "\n";
    }
    sqlite3_finalize(stmt);
    out.close();
    std::cout << "Export complete... " << outputFile << std::endl;
    return true;
}

// ---- MAIN LOOP ----
int main() {
    sqlite3* db;
    if (sqlite3_open("Humidity.db", &db)) {
        std::cerr << "Error opening database.\n";
        return 1;
    }

    if (!createTable(db)) {
        return 1;
    }

    while (true) {
        float humidity = readHumidity();
        std::string timestamp = currentTime();

        if (humidity >= 0 && insertReading(db, timestamp, humidity)) {
            std::cout << "Logged: " << timestamp << " | "
                      << humidity << "%\n";
        }

        cleanOldData(db);
        std::this_thread::sleep_for(std::chrono::seconds(30));

	if(std::filesystem::exists("export_request.txt")) {
	    std::ifstream req("export_request.txt");
	    std::string start, end;
	    req.close();
	    std::filesystem::remove("export_request.txt");
	    exportData(db, start, end, "export_custom.csv");
	}
    }

    sqlite3_close(db);
    return 0;
}

