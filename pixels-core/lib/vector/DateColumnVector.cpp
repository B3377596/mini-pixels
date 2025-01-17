//
// Created by yuly on 06.04.23.
//

#include "vector/DateColumnVector.h"
#include <stdexcept>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <string>

DateColumnVector::DateColumnVector(uint64_t len, bool encoding): ColumnVector(len, encoding) {
    posix_memalign(reinterpret_cast<void **>(&dates), 32,len * sizeof(int32_t));
	memoryUsage += (long) sizeof(int) * len;
}

void DateColumnVector::close() {
	if(!closed) {
		if(encoding && dates != nullptr) {
			free(dates);
		}
		dates = nullptr;
		ColumnVector::close();
	}
}

void DateColumnVector::print(int rowCount) {
	for(int i = 0; i < rowCount; i++) {
		std::cout<<dates[i]<<std::endl;
	}
}

DateColumnVector::~DateColumnVector() {
	if(!closed) {
		DateColumnVector::close();
	}
}

/**
     * Set a row from a value, which is the days from 1970-1-1 UTC.
     * We assume the entry has already been isRepeated adjusted.
     *
     * @param elementNum
     * @param days
 */
void DateColumnVector::set(int elementNum, int days) {
	if(elementNum >= writeIndex) {
		writeIndex = elementNum + 1;
	}
	dates[elementNum] = days;
}

void * DateColumnVector::current() {
    if(dates == nullptr) {
        return nullptr;
    } else {
        return dates + readIndex;
    }
}

void DateColumnVector::add(std::string &value) {
    std::cout<<"enter add function"<<std::endl;
    std::cout<<"writeindex is "<<writeIndex<<" length is "<<length<<std::endl;
	if (writeIndex >= length) {
        ensureSize(writeIndex * 2, true);
    }
    std::tm tm = {};
    std::istringstream ss(value);
    int index = writeIndex++;
    // 将字符串解析为 tm 结构
    ss >> std::get_time(&tm, "%Y-%m-%d");
    if (ss.fail()) {
        throw std::invalid_argument("Invalid date format");
    }
    std::time_t time = std::mktime(&tm);
    if (time == -1) {
        throw std::runtime_error("Error converting to time_t");
    }
    int days = static_cast<int>(time / (60 * 60 * 24));
    std::cout<<"days is "<<days<<std::endl;
    dates[index]=days;
	isNull[index] = false;
    std::cout<<"out add"<<std::endl;
}

void DateColumnVector::add(int value) {
    std::cout<<"writeindex is "<<writeIndex<<" length is "<<length<<std::endl;
	if (writeIndex >= length) {
        ensureSize(writeIndex * 2, true);
    }
    // 直接将天数值添加到 dates 数组中
	int index = writeIndex++;
    dates[index]=value;
	isNull[index] = false;
}

void DateColumnVector::ensureSize(uint64_t size, bool preserveData) {
    ColumnVector::ensureSize(size, preserveData);
    if (length < size) {
        int *oldVector = dates;
        posix_memalign(reinterpret_cast<void **>(&dates), 32, size * sizeof(int));
        if (preserveData) {
            std::copy(oldVector, oldVector + length, dates);
        }
        delete[] oldVector;
        memoryUsage += static_cast<long>(sizeof(int) * (size - length));
        resize(size);
    }
}