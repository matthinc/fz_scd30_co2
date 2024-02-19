#include "csv_writer.h"

#include <sstream>

CsvWriter::CsvWriter(std::string filename) : filename(std::move(filename)) {
    storage = reinterpret_cast<Storage*>(furi_record_open(RECORD_STORAGE));
    file = storage_file_alloc(storage);
}

CsvWriter::~CsvWriter() {
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

bool CsvWriter::add_row(std::vector<std::string> data) {
    if (!storage_file_open(file, filename.c_str(), FSAM_WRITE, FSOM_OPEN_APPEND)) {
        return false;
    }

    std::string ss;

    for (uint32_t i = 0; i < data.size(); i++) {
        std::string& element = data[i];

        ss += element;
        
        if (i == data.size() - 1) {
            ss += "\n";
        } else {
            ss += ",";
        }
    }

    if (!storage_file_write(file, ss.c_str(), ss.size())) {
        return false;
    }

    storage_file_close(file);

    return true;
}