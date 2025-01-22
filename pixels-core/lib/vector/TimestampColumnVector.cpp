//
// Created by liyu on 12/23/23.
//

#include "vector/TimestampColumnVector.h"
#include <string>
#include <ctime>
#include <stdexcept>
#include <sstream>
#include <iomanip>

TimestampColumnVector::TimestampColumnVector(int precision=0, bool encoding): ColumnVector(VectorizedRowBatch::DEFAULT_SIZE, encoding) {
    TimestampColumnVector(VectorizedRowBatch::DEFAULT_SIZE, precision, encoding);
}

TimestampColumnVector::TimestampColumnVector(uint64_t len, int precision=0, bool encoding): ColumnVector(len, encoding) {
    this->precision = precision;
    posix_memalign(reinterpret_cast<void **>(&this->times), 64,len * sizeof(long));
    memoryUsage += (long) sizeof(long) * len;
}


void TimestampColumnVector::close() {
    if(!closed) {
        ColumnVector::close();
        if(encoding && this->times != nullptr) {
            free(this->times);
        }
        this->times = nullptr;
    }
}

void TimestampColumnVector::print(int rowCount) {
    throw InvalidArgumentException("not support print longcolumnvector.");
//    for(int i = 0; i < rowCount; i++) {
//        std::cout<<longVector[i]<<std::endl;
//		std::cout<<intVector[i]<<std::endl;
//    }
}

TimestampColumnVector::~TimestampColumnVector() {
    if(!closed) {
        TimestampColumnVector::close();
    }
}

void * TimestampColumnVector::current() {
    if(this->times == nullptr) {
        return nullptr;
    } else {
        return this->times + readIndex;
    }
}

/**
     * Set a row from a value, which is the days from 1970-1-1 UTC.
     * We assume the entry has already been isRepeated adjusted.
     *
     * @param elementNum
     * @param days
 */
void TimestampColumnVector::set(int elementNum, long ts) {
    if(elementNum >= writeIndex) {
        writeIndex = elementNum + 1;
    }
    times[elementNum] = ts;
    // TODO: isNull
}

void TimestampColumnVector::add(std::string &value) {
    if (writeIndex >= length) {
        ensureSize(writeIndex * 2, true);
    }
    std::tm tm = {};
    std::istringstream ss(value);
    int index = writeIndex++;
    ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    if (ss.fail()) {
        throw std::invalid_argument("Invalid timestamp format");
    }
    std::time_t time = std::mktime(&tm);
    if (time == -1) {
        throw std::runtime_error("Error converting to time_t");
    }
    std::cout<<"time is "<<time<<std::endl;
    times[index]=time;
    isNull[index] = false;  // 标记该值非空
}

void TimestampColumnVector::add(int value) {
    if (writeIndex >= length) {
        ensureSize(writeIndex * 2, true);
    }

    int index = writeIndex++;
    set(index, static_cast<long>(value)); 
    isNull[index] = false;
}

void TimestampColumnVector::ensureSize(uint64_t size, bool preserveData) {
    ColumnVector::ensureSize(size, preserveData);

    if (length < size) {
        long *oldVector = times;
        posix_memalign(reinterpret_cast<void **>(&times), 32, size * sizeof(long));

        if (preserveData) {
            std::copy(oldVector, oldVector + length, times);
        }

        delete[] oldVector;
        memoryUsage += static_cast<long>(sizeof(long) * (size - length));
        length = size;
    }
}
