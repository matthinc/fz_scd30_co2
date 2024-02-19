#pragma once

#include <string>
#include <vector>

#include <storage/storage.h>

class CsvWriter {
public:
    explicit CsvWriter(std::string filename);
    virtual ~CsvWriter();

    bool add_row(std::vector<std::string> data);

private:
    std::string filename;
    Storage *storage;
    File *file;
};