
#pragma once

#include <vector>
#include <fstream>
#include <iostream>
#include <iomanip>

namespace CholUp {

template<class T>
class Matrix
{
public:
    T* data = 0;
    size_t nrows, ncols;

    Matrix()
    : nrows(0), ncols(0)
    {}

    Matrix(const Matrix<T>& m)
    : nrows(m.nrows), ncols(m.ncols)
    {
        data = new T[nrows * ncols];
        std::copy_n(m.data, nrows * ncols, data);
    }

    Matrix& operator=(const Matrix<T>& m)
    {
        if(this != &m)
        {
            nrows = m.nrows;
            ncols = m.cols;

            if(data) delete[] data;

            data = new T[nrows * ncols];
            std::copy_n(m.data, nrows * ncols, data);
        }

        return *this;
    }

    ~Matrix()
    {
        if(data) delete[] data;
    }

    Matrix(size_t _rows, size_t _cols)
    : data(new T[_rows * _cols]), nrows(_rows), ncols(_cols)
    {
    }

    void resize(size_t _rows, size_t _cols)
    {
        nrows = _rows;
        ncols = _cols;

        if(data) delete[] data;
        data = new T[nrows * ncols];
    }

    void resize(size_t _rows, size_t _cols, T init)
    {
        nrows = _rows;
        ncols = _cols;

        if(data) delete[] data;
        data = new T[nrows * ncols];

        std::fill_n(data, nrows * ncols, init);
    }

    T& operator()(const int i, const int j)
    {
        return data[j * nrows + i];
    }

    const T& operator()(const int i, const int j) const
    {
        return data[j * nrows + i];
    }

    int cols() const
    {
        return ncols;
    }

    int rows() const
    {
        return nrows;
    }

    void clear()
    {
        nrows = 0;
        ncols = 0;
        if(data) delete[] data;
    }

    void fill(const T val = T(0))
    {
        std::fill_n(data, nrows * ncols, val);
    }

    void write(std::string fname) const
    {
        std::ofstream file(fname);
        auto ptr = data;

        file << std::setiosflags(std::ios::fixed);
        file.precision(20);

        for(int i = 0; i < ncols; ++i)
        {
            for(int j = 0; j < nrows; ++j)
                file << *ptr++ << " ";


            file << "\n";
        }

        file.close();
    }

};

} /* namespace CholUp */