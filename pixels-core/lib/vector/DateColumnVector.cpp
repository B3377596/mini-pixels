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
	if(encoding) {
        posix_memalign(reinterpret_cast<void **>(&dates), 32,
                       len * sizeof(int32_t));
	} else {
		this->dates = nullptr;
	}
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
	// TODO: isNull
}

void * DateColumnVector::current() {
    if(dates == nullptr) {
        return nullptr;
    } else {
        return dates + readIndex;
    }
}

void DateColumnVector::add(std::string &value) {
	if (writeIndex >= length) {
        ensureSize(writeIndex * 2, true);
    }
    // 假设日期格式为 "YYYY-MM-DD"
    std::tm tm = {};
    std::istringstream ss(value);
    int index = writeIndex++;
    // 将字符串解析为 tm 结构
    ss >> std::get_time(&tm, "%Y-%m-%d");
    if (ss.fail()) {
        throw std::invalid_argument("Invalid date format");
    }

    // 将 tm 转换为 time_t，表示自1970年1月1日以来的秒数
    std::time_t time = std::mktime(&tm);
    if (time == -1) {
        throw std::runtime_error("Error converting to time_t");
    }

    // 计算距离1970年1月1日的天数
    int days = static_cast<int>(time / (60 * 60 * 24));
    
    // 将计算得到的天数添加到 dates 数组中
    set(index, days);  // 假设 currentIndex 是当前正在操作的索引
	isNull[index] = false;
}

void DateColumnVector::add(int value) {
	if (writeIndex >= length) {
        ensureSize(writeIndex * 2, true);
    }
    // 直接将天数值添加到 dates 数组中
	int index = writeIndex++;
    set(index, value);
	isNull[index] = false;
}

void DateColumnVector::ensureSize(uint64_t size, bool preserveData) {
    // 调用基类的 ensureSize 函数处理通用逻辑
    ColumnVector::ensureSize(size, preserveData);
    if (length < size) {
        // 创建新的日期数组
        int *oldVector = dates;
        // 使用 posix_memalign 确保内存对齐为 32 字节
        posix_memalign(reinterpret_cast<void **>(&dates), 32, size * sizeof(int));

        // 如果需要保留数据，则将旧数据复制到新数组
        if (preserveData) {
            std::copy(oldVector, oldVector + length, dates);
        }

        // 删除旧的数组
        delete[] oldVector;

        // 更新内存使用量
        memoryUsage += static_cast<long>(sizeof(int) * (size - length));

        // 更新长度
        length = size;
    }
}