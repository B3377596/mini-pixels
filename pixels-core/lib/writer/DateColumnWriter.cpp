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
#include "writer/DateColumnWriter.h"
#include "utils/BitUtils.h"

DateColumnWriter::DateColumnWriter(std::shared_ptr<TypeDescription> type, std::shared_ptr<PixelsWriterOption> writerOption)
    : ColumnWriter(type, writerOption), curPixelVector(pixelStride)
{
    runlengthEncoding = writerOption->getEncodingLevel().ge(EncodingLevel::Level::EL2);
    runlengthEncoding=false;
    if (runlengthEncoding) {
        encoder = std::make_unique<RunLenIntEncoder>();
    }
}

int DateColumnWriter::write(std::shared_ptr<ColumnVector> vector, int length) {
    std::cout << "In DateColumnWriter" << std::endl;
    auto columnVector = std::static_pointer_cast<DateColumnVector>(vector);
    if (!columnVector) {
        throw std::invalid_argument("Invalid vector type");
    }
    long* values = (long*)columnVector->dates;  // 获取日期列向量的数据
    int curPartLength;
    int curPartOffset = 0;
    int nextPartLength = length;

    while ((curPixelIsNullIndex + nextPartLength) >= pixelStride) {
        curPartLength = pixelStride - curPixelIsNullIndex;
        writeCurPartTime(columnVector, values, curPartLength, curPartOffset);
        newPixel();
        curPartOffset += curPartLength;
        nextPartLength = length - curPartOffset;
    }

    curPartLength = nextPartLength;
    writeCurPartTime(columnVector, values, curPartLength, curPartOffset);

    return outputStream->getWritePos();
}

void DateColumnWriter::close() {
    if (runlengthEncoding && encoder) {
        encoder->clear();
    }
    ColumnWriter::close();
}

void DateColumnWriter::newPixel() {
    if (runlengthEncoding) {
        std::vector<byte> buffer(curPixelVector.size() * sizeof(int));
        int resLen;
        encoder->encode(curPixelVector.data(), buffer.data(), curPixelVector.size(), resLen);
        outputStream->putBytes(buffer.data(), resLen);
    } else {
        std::shared_ptr<ByteBuffer> curVecPartitionBuffer =
            std::make_shared<ByteBuffer>(curPixelVector.size() * sizeof(int));
        EncodingUtils encodingUtils;

        for (int i = 0; i < curPixelVector.size(); i++) {
            encodingUtils.writeIntLE(curVecPartitionBuffer, curPixelVector[i]);
        }
        outputStream->putBytes(curVecPartitionBuffer->getPointer(), curVecPartitionBuffer->getWritePos());
    }

    ColumnWriter::newPixel();
}

void DateColumnWriter::writeCurPartTime(std::shared_ptr<ColumnVector> columnVector, long* values, int curPartLength, int curPartOffset) {
    int* v=(int*)values;
    for (int i = 0; i < curPartLength; i++) {
        curPixelEleIndex++;
        std::cout<<"vectorindex is "<<curPixelVectorIndex<<" size is "<<curPixelVector.size()<<std::endl;
        if (columnVector->isNull[i + curPartOffset]) {
            hasNull = true;
            if (nullsPadding) {
                curPixelVector[curPixelVectorIndex++] = 0; 
            }
        } else {
            std::cout<<"write "<<v[i + curPartOffset]<<std::endl;
            curPixelVector[curPixelVectorIndex++] = v[i + curPartOffset];
        }
    }
    std::copy(columnVector->isNull + curPartOffset, columnVector->isNull + curPartOffset + curPartLength, isNull.begin() + curPixelIsNullIndex);
    curPixelIsNullIndex += curPartLength;
}

bool DateColumnWriter::decideNullsPadding(std::shared_ptr<PixelsWriterOption> writerOption) {
    if (writerOption->getEncodingLevel().ge(EncodingLevel::Level::EL2)) {
        return false;
    }
    return writerOption->isNullsPadding();
}

pixels::proto::ColumnEncoding DateColumnWriter::getColumnChunkEncoding() const {
    pixels::proto::ColumnEncoding columnEncoding;
    if (runlengthEncoding) {
        columnEncoding.set_kind(pixels::proto::ColumnEncoding::Kind::ColumnEncoding_Kind_RUNLENGTH);
    } else {
        columnEncoding.set_kind(pixels::proto::ColumnEncoding::Kind::ColumnEncoding_Kind_NONE);
    }
    return columnEncoding;
}