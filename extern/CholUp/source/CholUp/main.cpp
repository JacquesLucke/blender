#include <iostream>
#include <fstream>
#include <iterator>
#include "SparseMatrix.h"
#include "SupernodalCholesky.hpp"
#include <Eigen/Sparse>
#include <unsupported/Eigen/SparseExtra>
#include "Timer.hpp"


// just loads a bunch of binary integers
vector<int>
loadIds(std::string fname)
{
    ifstream file(fname, std::fstream::binary);

    if(!file.good())
    {
        std::cout << "file not found" << std::endl;
        return {};
    }

    std::vector<char> ids((std::istreambuf_iterator<char>(file)),
                          (std::istreambuf_iterator<char>()));

    int* idPtr = (int*)ids.data();
    int nids = (int)ids.size() / sizeof(int);

    return std::vector<int>(idPtr, idPtr + nids);
}

int main(int argc, const char * argv[])
{
    // load data for update
    Eigen::SparseMatrix<double> eigenM;
    Eigen::loadMarket(eigenM, "./data/LTL.mtx");
    auto roiIds = loadIds("./data/ids");
    std::sort(roiIds.begin(), roiIds.end());
    SparseMatrix<double> A(eigenM);

    // factorize & update
    SupernodalCholesky<SparseMatrix<double>> chol(A);
    auto cholPart0 = chol.dirichletPartialFactor(A, roiIds);

    // solve linear system involving cholPart0

    // setup rhs
    Matrix<double> rhs(cholPart0.L.numcols, 3);
    rhs.fill();
    rhs(0, 0) = rhs(0, 1) = rhs(0, 2) = 1;
    auto rhs0 = rhs;

    // solve
    cholPart0.solve(rhs);

    // write out solution
    rhs0.write("./data/b");
    rhs.write("./data/x");
    cholPart0.L.toSparseMatrix().writeMatrixMarket("./data/cholPart.mtx");

    return 0;
}
