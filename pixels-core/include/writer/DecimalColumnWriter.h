/*
 * Copyright 2024 PixelsDB.
 *
 * This file is part of Pixels.
 *
 * Pixels is free software: you can redistribute it and/or modify
 * it under the terms of the Affero GNU General Public License as
 * published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * Pixels is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * Affero GNU General Public License for more details.
 *
 * You should have received a copy of the Affero GNU General Public
 * License along with Pixels.  If not, see
 * <https://www.gnu.org/licenses/>.
 */

//
// Created by whz on 12/9/24.
//

#ifndef DUCKDB_DECIMALCOLUMNWRITER_H
#define DUCKDB_DECIMALCOLUMNWRITER_H
#include "encoding/RunLenIntEncoder.h"
#include "ColumnWriter.h"
#include "utils/EncodingUtils.h"

class DecimalColumnWriter : public ColumnWriter {
public:
    // 构造函数
    DecimalColumnWriter(std::shared_ptr<TypeDescription> type, std::shared_ptr<PixelsWriterOption> writerOption);

    // 写入数据的主函数
    int write(std::shared_ptr<ColumnVector> vector, int length) override;

    // 判断是否需要填充null值
    bool decideNullsPadding(std::shared_ptr<PixelsWriterOption> writerOption) override;

    // 关闭写入器
    void close() override;

    // 新建一个像素块
    void newPixel() override;

    // 写入当前部分的Decimal数据
    void writeCurPartDecimal(std::shared_ptr<DecimalColumnVector> columnVector, long* values, int curPartLength, int curPartOffset);

    // 获取列的编码
    pixels::proto::ColumnEncoding getColumnChunkEncoding() const;

private:
    bool runlengthEncoding;                 // 是否使用RunLength编码
    std::unique_ptr<RunLenIntEncoder> encoder; // 用于RunLength编码的编码器
    std::vector<long> curPixelVector;        // 当前像素值向量
};
#endif //DUCKDB_DECIMALCOLUMNWRITER_H
