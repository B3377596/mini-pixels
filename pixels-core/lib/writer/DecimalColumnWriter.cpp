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
#include "writer/DecimalColumnWriter.h"

long DecimalColumnVector::DEFAULT_UNSCALED_VALUE = 0;

DecimalColumnWriter::DecimalColumnWriter(std::shared_ptr<TypeDescription> type, std::shared_ptr<PixelsWriterOption> writerOption)
    : ColumnWriter(type, writerOption), curPixelVector(pixelStride)
{
    runlengthEncoding = writerOption->getEncodingLevel().ge(EncodingLevel::Level::EL2);
    runlengthEncoding =false;
    if (runlengthEncoding) {
        encoder = std::make_unique<RunLenIntEncoder>();
    }
}

int DecimalColumnWriter::write(std::shared_ptr<ColumnVector> vector, int length) {
    std::cout << "In DecimalColumnWriter" << std::endl;
    auto columnVector = std::static_pointer_cast<DecimalColumnVector>(vector);
    if (!columnVector) {
        throw std::invalid_argument("Invalid vector type");
    }

    long* values = columnVector->vector;  

    int curPartLength;
    int curPartOffset = 0;
    int nextPartLength = length;
    while ((curPixelIsNullIndex + nextPartLength) >= pixelStride) {
        std::cout<<"curindex is "<<curPixelIsNullIndex<<std::endl;

        curPartLength = pixelStride - curPixelIsNullIndex;
        writeCurPartDecimal(columnVector, values, curPartLength, curPartOffset); 
        newPixel();  
        curPartOffset += curPartLength;
        nextPartLength = length - curPartOffset;
    }

    curPartLength = nextPartLength;
    writeCurPartDecimal(columnVector, values, curPartLength, curPartOffset); 

    return outputStream->getWritePos();  // 返回写入位置
}

bool DecimalColumnWriter::decideNullsPadding(std::shared_ptr<PixelsWriterOption> writerOption) {
    return writerOption->isNullsPadding();  // 根据 PixelsWriterOption 来决定是否填充 null 值
}

/*void DecimalColumnWriter::writeCurPartDecimal(std::shared_ptr<DecimalColumnVector> columnVector, long* values, int curPartLength, int curPartOffset) {
    for (int i = 0; i < curPartLength; i++) {
        curPixelEleIndex++;
        if (columnVector->isNull[i + curPartOffset]) {
            hasNull = true;
            if (nullsPadding) {
                curPixelVector[curPixelVectorIndex++] = DecimalColumnVector::DEFAULT_UNSCALED_VALUE; 
            }
        } else {
            curPixelVector[curPixelVectorIndex++] = values[i + curPartOffset]; 
        }
    }
    std::copy(columnVector->isNull + curPartOffset, columnVector->isNull + curPartOffset + curPartLength, isNull.begin() + curPixelIsNullIndex);
    curPixelIsNullIndex += curPartLength;  
}*/
void DecimalColumnWriter::writeCurPartDecimal(
    std::shared_ptr<DecimalColumnVector> columnVector, 
    long* values, 
    int curPartLength, 
    int curPartOffset
) {
    // 检查 columnVector 是否为 nullptr
    if (!columnVector) {
        throw std::invalid_argument("columnVector is null");
    }

    // 检查 columnVector->isNull 是否为 nullptr
    if (!columnVector->isNull) {
        throw std::runtime_error("columnVector->isNull is not initialized");
    }

    // 检查 curPartOffset 和 curPartLength 的范围是否合法
    if (curPartOffset < 0 || curPartOffset + curPartLength > columnVector->length) {
        throw std::out_of_range("curPartOffset + curPartLength exceeds columnVector range");
    }

    // 检查 values 指针是否为 nullptr
    if (!values) {
        throw std::invalid_argument("values is null");
    }

    // 日志输出以便调试
    std::cout << "curPartOffset: " << curPartOffset << ", curPartLength: " << curPartLength << std::endl;
    std::cout << "columnVector->length: " << columnVector->length << std::endl;

    for (int i = 0; i < curPartLength; i++) {
        curPixelEleIndex++;
        std::cout<<"vectorindex is "<<curPixelVectorIndex<<" size is "<<curPixelVector.size()<<std::endl;
        if (curPixelVectorIndex >= curPixelVector.size()) {
            throw std::out_of_range("curPixelVectorIndex exceeds curPixelVector size");
        }

        if (columnVector->isNull[i + curPartOffset]) {
            hasNull = true;
            if (nullsPadding) {
                curPixelVector[curPixelVectorIndex++] = DecimalColumnVector::DEFAULT_UNSCALED_VALUE;
            }
        } else {
            std::cout<<"num is "<<values[i+curPartOffset]<<std::endl;
            curPixelVector[curPixelVectorIndex++] = values[i + curPartOffset];
        }
    }
    if (curPixelIsNullIndex + curPartLength > isNull.size()) {
        throw std::out_of_range("isNull capacity is insufficient");
    }
    std::copy(
        columnVector->isNull + curPartOffset,
        columnVector->isNull + curPartOffset + curPartLength,
        isNull.begin() + curPixelIsNullIndex
    );
    curPixelIsNullIndex += curPartLength;
}


void DecimalColumnWriter::close() {
    if (runlengthEncoding && encoder) {
        encoder->clear();
    }
    ColumnWriter::close();
}

void DecimalColumnWriter::newPixel() {
    // 写入当前像素的逻辑
    if (runlengthEncoding) {
        std::vector<byte> buffer(curPixelVectorIndex * sizeof(long));
        int resLen;
        encoder->encode(curPixelVector.data(), buffer.data(), curPixelVectorIndex, resLen);
        outputStream->putBytes(buffer.data(), resLen);
    } else {
        std::shared_ptr<ByteBuffer> curVecPartitionBuffer;
        EncodingUtils encodingUtils;

        curVecPartitionBuffer = std::make_shared<ByteBuffer>(curPixelVectorIndex * sizeof(long));
        for (int i = 0; i < curPixelVectorIndex; i++) {
            encodingUtils.writeLongLE(curVecPartitionBuffer, curPixelVector[i]);
        }
        outputStream->putBytes(curVecPartitionBuffer->getPointer(), curVecPartitionBuffer->getWritePos());
    }

    ColumnWriter::newPixel();
}

pixels::proto::ColumnEncoding DecimalColumnWriter::getColumnChunkEncoding() const {
    pixels::proto::ColumnEncoding columnEncoding;
    if (runlengthEncoding) {
        columnEncoding.set_kind(pixels::proto::ColumnEncoding::Kind::ColumnEncoding_Kind_RUNLENGTH);
    } else {
        columnEncoding.set_kind(pixels::proto::ColumnEncoding::Kind::ColumnEncoding_Kind_NONE);
    }
    return columnEncoding;
}