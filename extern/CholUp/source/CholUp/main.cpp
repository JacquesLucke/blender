#include <iostream>
#include <fstream>
#include <iterator>
#include <vector>

#include "SparseMatrix.h"
#include "SupernodalCholesky.h"

#include <Eigen/Sparse>
#include <unsupported/Eigen/SparseExtra>
#include "Timer.hpp"
#include "Ordering.h"
#include <Eigen/OrderingMethods>

using namespace std;

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

vector<Eigen::Triplet<double>>
loadTriplets(std::string fname)
{
    vector<Eigen::Triplet<double>> triplets;
    
    ifstream file(fname);
    int n;
    file >> n;
    
    for(int k = 0; k < n; ++k)
    {
        int i, j;
        double v;
        
        file >> i;
        file >> j;
        file >> v;
    
        triplets.emplace_back(i, j, v);
    }
    
    file.close();
    
    return triplets;
}

int
tripletDimensions(std::vector<Eigen::Triplet<double>>& triplets)
{
    const int N =  1 + std::max(std::max_element(triplets.begin(), triplets.end(),
                                             [](const Eigen::Triplet<double>& t0, const Eigen::Triplet<double>& t1){return t0.row() < t1.row();})->row(),

                                std::max_element(triplets.begin(), triplets.end(),
                                             [](const Eigen::Triplet<double>& t0, const Eigen::Triplet<double>& t1){return t0.col() < t1.col();})->col());

    return N;
}


void loadSimpleMatrix(Eigen::SparseMatrix<double>& A)
{
    std::vector<Eigen::Triplet<double>> triplets;
    triplets.emplace_back(0,0,1.1);
    triplets.emplace_back(0,1,-0.5);
    triplets.emplace_back(0,2,-0.5);

    triplets.emplace_back(1,0,-0.5);
    triplets.emplace_back(1,1,1.1);
    triplets.emplace_back(1,3,-0.5);

    triplets.emplace_back(2,0,-0.5);
    triplets.emplace_back(2,2,1.1);
    triplets.emplace_back(2,3,-0.5);

    triplets.emplace_back(3,1,-0.5);
    triplets.emplace_back(3,2,-0.5);
    triplets.emplace_back(3,3,1.1);

    A.resize(4,4);
    A.setFromTriplets(triplets.begin(), triplets.end());
}


void simpleExample()
{
    // minimal example

    Eigen::SparseMatrix<double> A;
    loadSimpleMatrix(A);
    vector<int> roiIds{1,3,0};

    CholUp::SupernodalCholesky<CholUp::SparseMatrix<double>> chol(A);
    auto cholPart0 = chol.dirichletPartialFactor(roiIds);

    CholUp::Matrix<double> rhs(4, 1);
    rhs.fill();
    rhs(3, 0) = 1.;
    rhs(0, 0) = 2.;

    auto rhs0 = rhs;

    cholPart0.solve(rhs);


    // check result
    Eigen::MatrixXd AII(3,3);
    Eigen::VectorXd b(3), x(3);

    for(int i = 0; i < 3; ++i)
        for(int j = 0; j < 3; ++j)
            AII(i, j) = A.coeffRef(roiIds[i], roiIds[j]);

    for(int i = 0; i < 3; ++i)
    {
        b(i) = rhs0(roiIds[i], 0);
        x(i) = rhs(roiIds[i], 0);

    }

    std::cout << "error: " << (AII * x - b).norm() << std::endl;
}

int main(int argc, const char * argv[])
{

    simpleExample();

    Eigen::SparseMatrix<double> A;
    Eigen::loadMarket(A, "../data/LTL.mtx");
    assert(A.isCompressed());

    //vector<int> roiIds{3,0};
    auto roiIds = loadIds("../data/ids");

    // factorize & update
    Timer t0("Factor");
    CholUp::SupernodalCholesky<CholUp::SparseMatrix<double>> chol(A);
    t0.printTime("full");
    t0.reset();
    
    auto cholPart0 = chol.dirichletPartialFactor(roiIds);
    t0.printTime("partial");

    // solve linear system involving cholPart0
    // setup rhs
    CholUp::Matrix<double> rhs(A.cols(), 3);
    rhs.fill();
   
    rhs(roiIds[0], 0) = rhs(roiIds[0], 1) = rhs(roiIds[0], 2) = 1;
    auto rhs0 = rhs;

    // solve. Permutation is automatically taken care of. Rhs defines values for boundary conditions, values in rhs[roiIds] are replaced by the solve.
    cholPart0.solve(rhs);

    // write out solution
    rhs0.write("../data/b");
    rhs.write("../data/x");

    // refactor
   /*
    Eigen::SparseMatrix<double> eigenM2;
    Eigen::loadMarket(eigenM2, "./data/subLTL.mtx");
    CholUp::SparseMatrix<double> A2(eigenM2);

    Timer t3("Full refactor");
    CholUp::SupernodalCholesky<CholUp::SparseMatrix<double>> chol2(A2);
    t3.printTime();
*/
    return 0;
}
