// @HEADER
// ***********************************************************************
//
//       xSDKTrilinos: Extreme-scale Software Development Kit Package
//                 Copyright (2016) Sandia Corporation
//
// Under terms of Contract DE-AC04-94AL85000 with Sandia Corporation,
// the U.S. Government retains certain rights in this software.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// 1. Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// 3. Neither the name of the Corporation nor the names of the
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY SANDIA CORPORATION "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL SANDIA CORPORATION OR THE
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Questions? Contact Alicia Klinvex    (amklinv@sandia.gov)
//                    James Willenbring (jmwille@sandia.gov)
//                    Michael Heroux    (maherou@sandia.gov)         
//
// ***********************************************************************
// @HEADER

/*#############################################################################
# CVS File Information
#    Current revision: $Revision$
#    Last modified:    $Date$
#    Modified by:      $Author$
#############################################################################*/

// TODO: support non-square matrices

#ifndef _TPETRA_PETSCAIJMATRIX_H_
#define _TPETRA_PETSCAIJMATRIX_H_

#include "Tpetra_ConfigDefs.hpp"
#include "Tpetra_CrsMatrix.hpp"
#include "Tpetra_PETScAIJGraph.hpp"
#ifdef HAVE_MPI
#include "Teuchos_DefaultMpiComm.hpp"
#else
#include "Teuchos_DefaultSerialComm.hpp"
#endif
//Petsc headers.
#include <petscmat.h>
#include <type_traits>


namespace xSDKTrilinos {

template<class Scalar, class LO, class GO, class Node>
Teuchos::RCP<Tpetra::CrsMatrix<Scalar,LO,GO,Node> > deepCopyPETScAIJMatrixToTpetraCrsMatrix(const Mat& A)
{
  using Teuchos::RCP;
  typedef Tpetra::Map<LO,GO,Node>                   Map;
  typedef Tpetra::CrsMatrix<Scalar,LO,GO,Node>      CrsMatrix;

  PetscErrorCode ierr;

  // Get the communicator
  RCP< Teuchos::Comm<int> > TrilinosComm;
#ifdef HAVE_MPI
  MPI_Comm PETScComm;
  PetscObjectGetComm( (PetscObject)A, &PETScComm);
  TrilinosComm = rcp(new Teuchos::MpiComm<int>(PETScComm));
#else
  TrilinosComm = rcp(new Teuchos::SerialComm<int>());
#endif

  // Get information about the distribution from PETSc
  // Note that this is only valid for a block row distribution
  PetscInt numLocalRows, numLocalCols;
  ierr = MatGetLocalSize(A,&numLocalRows,&numLocalCols); CHKERRCONTINUE(ierr);
  PetscInt numGlobalRows, numGlobalCols;
  ierr = MatGetSize(A,&numGlobalRows,&numGlobalCols); CHKERRCONTINUE(ierr);

  // Create a Tpetra map reflecting this distribution
  RCP<Map> map = rcp(new Map(numGlobalRows,numLocalRows,0,TrilinosComm));

  // Create an array containing the number of entries in each row
  LO minLocalIndex = map->getMinGlobalIndex();
  Teuchos::ArrayRCP<size_t> ncolsPerRow(numLocalRows);
  for(int i=0; i < numLocalRows; i++)
  {
    ierr = MatGetRow(A,minLocalIndex+i,&numLocalCols,NULL,NULL); CHKERRCONTINUE(ierr);
    ncolsPerRow[i] = numLocalCols;
    ierr = MatRestoreRow(A,minLocalIndex+i,&numLocalCols,NULL,NULL); CHKERRCONTINUE(ierr);
  }

  // Create the matrix and set its values
  RCP<CrsMatrix> TrilinosMat = rcp(new CrsMatrix(map,ncolsPerRow,Tpetra::StaticProfile));
  const PetscInt * cols;
  const PetscScalar * vals;
  for(int i=0; i < numLocalRows; i++)
  {
    ierr = MatGetRow(A,i+minLocalIndex,&numLocalCols,&cols,&vals); CHKERRCONTINUE(ierr);
    Teuchos::ArrayView<const LO> colsToInsert(cols,numLocalCols);
    Teuchos::ArrayView<const Scalar> valsToInsert(vals,numLocalCols);
    TrilinosMat->insertGlobalValues(minLocalIndex+i,colsToInsert,valsToInsert);
    ierr = MatRestoreRow(A,minLocalIndex+i,&numLocalCols,&cols,&vals); CHKERRCONTINUE(ierr);
  }

  // Let the matrix know you're done changing it
  TrilinosMat->fillComplete();

  return TrilinosMat;
}

template<class Scalar, class LO, class GO, class Node>
Teuchos::RCP<Tpetra::Vector<Scalar,LO,GO,Node> > deepCopyPETScVecToTpetraVector(const Vec& v)
{
  using Teuchos::ArrayView;
  using Teuchos::RCP;
  using Teuchos::rcp;
  typedef Tpetra::Vector<Scalar,LO,GO,Node>         Vector;
  typedef Tpetra::Map<LO,GO,Node>                   Map;

  PetscErrorCode ierr;
  PetscScalar *vals, *valscopy;
  PetscInt length, numLocalRows, numGlobalRows;

  // Get information about the distribution from PETSc
  // Note that this is only valid for a block row distribution
  ierr = VecGetLocalSize(v,&numLocalRows); CHKERRCONTINUE(ierr);
  ierr = VecGetSize(v,&numGlobalRows); CHKERRCONTINUE(ierr);

  // Get the communicator
  RCP< Teuchos::Comm<int> > TrilinosComm;
#ifdef HAVE_MPI
  MPI_Comm PETScComm;
  PetscObjectGetComm( (PetscObject)v, &PETScComm);
  TrilinosComm = rcp(new Teuchos::MpiComm<int>(PETScComm));
#else
  TrilinosComm = rcp(new Teuchos::SerialComm<int>());
#endif

  // Create a Tpetra map reflecting this distribution
  RCP<Map> map = rcp(new Map(numGlobalRows,numLocalRows,0,TrilinosComm));

  ierr = VecGetArray(v,&vals); CHKERRCONTINUE(ierr);
  ierr = VecGetLocalSize(v,&length); CHKERRCONTINUE(ierr);
  valscopy = (PetscScalar*) malloc(length*sizeof(PetscScalar));
  memcpy(valscopy,vals,length*sizeof(PetscScalar));
  ierr = VecRestoreArray(v,&vals); CHKERRCONTINUE(ierr);
  ArrayView<PetscScalar> epuView(valscopy,length);
  RCP<Vector> tpvec = rcp(new Vector(map,epuView));

  return tpvec;
}

template<class Scalar, class LO, class GO, class Node>
Vec* deepCopyTpetraVectorToPETScVec(Teuchos::RCP<const Tpetra::Vector<Scalar,LO,GO,Node> > v)
{
  PetscErrorCode        ierr;
  Vec*                  petscVec;
  PetscInt              localSize;
  PetscInt              globalSize;
  PetscScalar*          petscData;
  const PetscScalar*    tpetraData;

  localSize = v->getLocalLength();
  globalSize = v->getGlobalLength();

  petscVec = new Vec;
  ierr = VecCreate(PETSC_COMM_WORLD,petscVec);CHKERRCONTINUE(ierr);
  ierr = VecSetSizes(*petscVec,localSize,globalSize);CHKERRCONTINUE(ierr);
  ierr = VecSetFromOptions(*petscVec);CHKERRCONTINUE(ierr);

  ierr = VecGetArray(*petscVec, &petscData); CHKERRCONTINUE(ierr);

  tpetraData = v->getData().get();
  for(PetscInt i=0; i<localSize; i++) petscData[i] = tpetraData[i];

  ierr = VecRestoreArray(*petscVec,&petscData); CHKERRCONTINUE(ierr);

  return petscVec;
}

} // end namespace xSDKTrilinos


namespace Tpetra {

//! PETScAIJMatrix: A class for constructing and using real-valued sparse compressed row matrices.

/*! The PETScAIJMatrix is a wrapper class for PETSc sequential or parallel AIJ matrices.  It is
    derived from the Tpetra_RowMatrix class, and so provides PETSc users access to Trilinos preconditioners.
    This class is lightweight, i.e., there are no deep copies of matrix data.  Whenever possible, class
    methods utilize callbacks to native PETSc functions.  Currently, only sequential and parallel point AIJ
    PETSc matrix types are supported.
*/    

template<class Scalar = Details::DefaultTypes::scalar_type, 
         class LO = Details::DefaultTypes::local_ordinal_type, 
         class GO = Details::DefaultTypes::global_ordinal_type, 
         class Node = Details::DefaultTypes::node_type>
class PETScAIJMatrix : 
  virtual public RowMatrix<Scalar,LO,GO,Node> 
{
private:
  typedef Teuchos::Comm<int>                   Comm;
  typedef MultiVector<Scalar,LO,GO,Node>       MV;
  typedef PETScAIJGraph<LO,GO,Node>  Graph;
      
public:
  typedef Scalar scalar_type;
  typedef LO local_ordinal_type;
  typedef GO global_ordinal_type; 
  typedef Node node_type;
  typedef typename RowMatrix<Scalar,LO,GO,Node>::mag_type mag_type;

   //! @name Constructors/Destructor
  //@{ 
  //! PETScAIJMatrix constructor.
  /*! Creates a PETScAIJMatrix object by encapsulating an existing PETSc matrix.
    
    \param In
           Amat - A completely constructed PETSc SEQAIJ or MPIAIJ matrix.
  */
  PETScAIJMatrix(Mat Amat);

  //! PETScAIJMatrix Destructor
  ~PETScAIJMatrix() {};
  //@}
  
  //! @name Extraction methods
  //@{ 

    //! The current number of entries on the calling process in the specified global row.
    size_t getNumEntriesInGlobalRow(GO globalRow) const { return graph_->getNumEntriesInGlobalRow(globalRow); };

    //! The current number of entries on the calling process in the specified local row.
    size_t getNumEntriesInLocalRow(LO localRow) const { return graph_->getNumEntriesInLocalRow(localRow); };

    //! Get a copy of the given local row's entries. 
    void getLocalRowCopy(LO LocalRow, const Teuchos::ArrayView<LO> & Indices, const Teuchos::ArrayView<Scalar> & Values, size_t & NumEntries) const;

    //! Get a copy of the given global row's entries. 
    void getGlobalRowCopy(GO GlobalRow, const Teuchos::ArrayView<GO> & Indices, const Teuchos::ArrayView<Scalar> & Values, size_t & NumEntries) const;

    //! Get a constant, nonpersisting, globally indexed view of the given row of the matrix.
    void getGlobalRowView(GO GlobalRow, Teuchos::ArrayView<const GO> & indices, Teuchos::ArrayView<const Scalar> & values) const {}; // TODO: throw exception

    //! Get a constant, nonpersisting, locally indexed view of the given row of the matrix.
    void getLocalRowView(LO LocalRow, Teuchos::ArrayView<const LO> & indices, Teuchos::ArrayView<const Scalar> & values) const {}; // TODO: throw exception

    //! Get a copy of the diagonal entries, distributed by the row Map. 
    void getLocalDiagCopy(Vector<Scalar,LO,GO,Node> & diag) const;
    //@}

    //! @name Computational methods
  //@{ 

    //! Computes the operator-multivector application.
    void apply(const MV & X, 
               MV & Y, 
               Teuchos::ETransp mode = Teuchos::NO_TRANS, 
               Scalar alpha = Teuchos::ScalarTraits<Scalar>::one(), 
               Scalar beta = Teuchos::ScalarTraits<Scalar>::zero()
              ) const;

    //! Scale the RowMatrix on the left with the given Vector x.
    void leftScale(const Vector<Scalar,LO,GO,Node> & x);

    //! Scale the RowMatrix on the right with the given Vector x.
    void rightScale(const Vector<Scalar,LO,GO,Node> & x);

  //@}

  //! @name Matrix Properties Query Methods
  //@{ 


    //! Whether fillComplete() has been called. 
    bool isFillComplete() const { return graph_->isFillComplete(); };

    //! Whether this matrix is lower triangular. 
    bool isLowerTriangular() const { return graph_->isLowerTriangular(); };

    //! Whether this matrix is upper triangular. 
    bool isUpperTriangular() const { return graph_->isUpperTriangular(); };

    //! Whether matrix indices are locally indexed.
    bool isLocallyIndexed() const { return graph_->isLocallyIndexed(); };

    //! Whether matrix indices are globally indexed.
    bool isGloballyIndexed() const {return graph_->isGloballyIndexed(); };

    //! Whether this object implements getLocalRowView() and getGlobalRowView(). 
    bool supportsRowViews() const { return false; };

  //@}
  
  //! @name Attribute access functions
  //@{ 

    //! The Frobenius norm of the matrix.
    mag_type getFrobeniusNorm() const;

    //! The index base for global indices in this matrix.
    GO getIndexBase() const { return getRowMap()->getIndexBase(); };

    //! The global number of stored (structurally nonzero) entries. 
    global_size_t getGlobalNumEntries() const { return graph_->getGlobalNumEntries(); };

    //! The global number of rows of this matrix. 
    global_size_t getGlobalNumRows() const { return graph_->getGlobalNumRows(); };

    //! The global number of columns of this matrix. 
    global_size_t getGlobalNumCols() const { return graph_->getGlobalNumCols(); };

    //! The number of global diagonal entries, based on global row/column index comparisons.
    global_size_t getGlobalNumDiags() const { return graph_->getGlobalNumDiags(); };
    
    //! The local number of stored (structurally nonzero) entries. 
    size_t getNodeNumEntries() const { return graph_->getNodeNumEntries(); };

    //! The number of rows owned by the calling process. 
    size_t getNodeNumRows() const { return graph_->getNodeNumRows(); };

    //! The number of columns needed to apply the forward operator on this node. 
    size_t getNodeNumCols() const { return graph_->getNodeNumCols(); };

    //! The number of local diagonal entries, based on global row/column index comparisons. 
    size_t getNodeNumDiags() const { return graph_->getNodeNumDiags(); };

    //! The Map associated with the domain of this operator, which must be compatible with X.getMap(). 
    Teuchos::RCP<const Map<LO,GO,Node> > getDomainMap() const { return graph_->getDomainMap(); };

    //! The Map associated with the range of this operator, which must be compatible with Y.getMap(). 
    Teuchos::RCP<const Map<LO,GO,Node> > getRangeMap() const  { return graph_->getRangeMap(); };

    //! The Map that describes the distribution of rows over processes.
    Teuchos::RCP<const Map<LO,GO,Node> > getRowMap() const { return graph_->getRowMap(); }

    //! The Map that describes the distribution of columns over processes. 
    Teuchos::RCP<const Map<LO,GO,Node> > getColMap() const { return graph_->getColMap(); };

    //! The RowGraph associated with this matrix.
    Teuchos::RCP<const RowGraph<LO,GO,Node> > getGraph() const { return graph_; };

    //! The communicator over which this matrix is distributed. 
    Teuchos::RCP<const Teuchos::Comm<int> > getComm() const { return graph_->getComm(); };

    //! The Kokkos Node instance.
    Teuchos::RCP<Node> getNode() const { return graph_->getNode(); };
  //@}
  
  //! @name Additional methods required to implement RowMatrix interface
  //@{ 

    //! The maximum number of entries across all rows/columns on all nodes. 
    size_t getGlobalMaxNumRowEntries() const { return graph_->getGlobalMaxNumRowEntries(); };

    //! The maximum number of entries across all rows/columns on this node. 
    size_t getNodeMaxNumRowEntries() const { return graph_->getNodeMaxNumRowEntries(); };

    //! Whether this matrix has a well-defined column map.
    bool hasColMap() const { return graph_->hasColMap(); };
  //@}

 private:

    Mat Amat_; // general PETSc matrix type

    Teuchos::RCP<Graph> graph_;
    
 //! Copy constructor (not accessible to users).
  //FIXME we need a copy ctor
  //PETScAIJMatrix(const PETScAIJMatrix & Matrix) {(void)Matrix;}
};

//==============================================================================
template<class Scalar, class LO, class GO, class Node>
PETScAIJMatrix<Scalar,LO,GO,Node>::PETScAIJMatrix(Mat Amat)
  : Amat_(Amat)
{
  graph_ = Teuchos::rcp(new PETScAIJGraph<LO,GO,Node>(Amat));
} //PETScAIJMatrix(Mat Amat)



//! Get a copy of the given local row's entries. 
//==============================================================================
template<class Scalar, class LO, class GO, class Node>
void PETScAIJMatrix<Scalar,LO,GO,Node>::getLocalRowCopy(LO LocalRow, const Teuchos::ArrayView<LO> & Indices, const Teuchos::ArrayView<Scalar> & Values, size_t & NumEntries) const
{
  GO globalRow = LocalRow + getDomainMap()->getMinGlobalIndex();
  
  getGlobalRowCopy(globalRow, Indices, Values, NumEntries);
} //ExtractMyRowCopy()



//! Get a copy of the given global row's entries. 
//==============================================================================
template<class Scalar, class LO, class GO, class Node>
void PETScAIJMatrix<Scalar,LO,GO,Node>::getGlobalRowCopy(GO GlobalRow, const Teuchos::ArrayView<GO> & Indices, const Teuchos::ArrayView<Scalar> & Values, size_t & NumEntries) const
{
  PetscErrorCode ierr;
  PetscInt ncols;
  const PetscInt * cols;
  const PetscScalar * vals;

  // Check whether the requested row is valid on this process
  TEUCHOS_TEST_FOR_EXCEPTION(!getRowMap()->isNodeGlobalElement(GlobalRow), std::runtime_error,
         Teuchos::typeName (*this) << "::getGlobalRowCopy(): Requested row is not owned by this process.");

  // Check whether we have enough space to store the row
  NumEntries = getNumEntriesInGlobalRow(GlobalRow);
  TEUCHOS_TEST_FOR_EXCEPTION(Indices.size() < (GO)NumEntries || Values.size() < (GO)NumEntries, std::runtime_error,
         Teuchos::typeName (*this) << "::getGlobalRowCopy(): ArrayViews are not large enough to store the requested data.");

  // Get PETSc's row
  ierr = MatGetRow(Amat_,GlobalRow,&ncols,&cols,&vals);CHKERRV(ierr);
  NumEntries = ncols;

  // Copy it to a Trilinos Array
  Teuchos::Array<LO> tpCols(ncols);
  Teuchos::Array<Scalar> tpVals(ncols);
  for(LO i=0; i<ncols; i++)
  {
    Indices[i] = cols[i];
    Values[i] = vals[i];
  }
  ierr = MatRestoreRow(Amat_,GlobalRow,&ncols,&cols,&vals);CHKERRV(ierr);
}



//! Get a copy of the diagonal entries, distributed by the row Map. 
//==============================================================================
template<class Scalar, class LO, class GO, class Node>
void PETScAIJMatrix<Scalar,LO,GO,Node>::getLocalDiagCopy(Vector<Scalar,LO,GO,Node> & diag) const
{
  PetscErrorCode ierr;
  Vec v;
  PetscScalar * diagonal;

  size_t nlocal = getDomainMap()->getNodeNumElements();

  // Get PETSc's diagonal
  ierr = MatCreateVecs(Amat_,&v,NULL);CHKERRV(ierr);
  ierr = MatGetDiagonal(Amat_,v);CHKERRV(ierr);
  ierr = VecGetArray(v,&diagonal);CHKERRV(ierr);

  // Copy it to a Trilinos Vector
  for(size_t i=0; i<nlocal; i++)
  {
    diag.replaceLocalValue(i,diagonal[i]);
  }

  ierr = VecDestroy(&v);CHKERRV(ierr);
}



//! Computes the operator-multivector application.
//==============================================================================
template<class Scalar, class LO, class GO, class Node>
void PETScAIJMatrix<Scalar,LO,GO,Node>::apply(const MV & X, MV & Y, Teuchos::ETransp mode, Scalar alpha, Scalar beta) const
{
  using Teuchos::ArrayRCP;

  TEUCHOS_TEST_FOR_EXCEPTION(!isFillComplete(), std::runtime_error,
         Teuchos::typeName (*this) << "::apply(): underlying matrix is not fill-complete.");
  TEUCHOS_TEST_FOR_EXCEPTION(X.getNumVectors () != Y.getNumVectors (), std::runtime_error,
         Teuchos::typeName (*this) << "::apply(X,Y): X and Y must have the same number of vectors.");

  int numVectors = X.getNumVectors();

  ArrayRCP< ArrayRCP<const Scalar> > xView = X.get2dView();
  ArrayRCP< ArrayRCP<Scalar> > yView = Y.get2dViewNonConst();

  double *vals=0;
  int length;
  Vec petscX, petscY;
  int ierr;
  for(int i=0; i < numVectors; i++)
  {
#   ifdef HAVE_MPI
    ierr=VecCreateMPIWithArray(getRawMpiComm(*getComm()),1, X.getLocalLength(),X.getGlobalLength(),xView[i].get(),&petscX);CHKERRV(ierr);
    ierr=VecCreateMPIWithArray(getRawMpiComm(*getComm()),1, Y.getLocalLength(),Y.getGlobalLength(),yView[i].get(),&petscY);CHKERRV(ierr);
#   else //FIXME  untested
    ierr=VecCreateSeqWithArray(getRawMpiComm(*getComm()),1, X.getLocalLength(),X.getGlobalLength(),xView[i].get(),&petscX);CHKERRV(ierr);
    ierr=VecCreateSeqWithArray(getRawMpiComm(*getComm()),1, Y.getLocalLength(),Y.getGlobalLength(),yView[i].get(),&petscY);CHKERRV(ierr);
#   endif

    if(alpha != Teuchos::ScalarTraits<Scalar>::one()) {
      ierr=VecScale(petscX,alpha);CHKERRV(ierr);
    }

    if(beta == Teuchos::ScalarTraits<Scalar>::zero())
    {
      if(mode == Teuchos::NO_TRANS) {
        ierr = MatMult(Amat_,petscX,petscY);CHKERRV(ierr);
      }
      else if(mode == Teuchos::TRANS) {
        ierr = MatMultTranspose(Amat_,petscX,petscY);CHKERRV(ierr);
      }
      else { // mode == Teuchos::CONJ_TRANS 
        ierr = MatMultHermitianTranspose(Amat_,petscX,petscY);CHKERRV(ierr);
      }
    }
    else
    {
      ierr=VecScale(petscY,beta);CHKERRV(ierr);

      if(mode == Teuchos::NO_TRANS) {
        ierr = MatMultAdd(Amat_,petscX,petscY,petscY);CHKERRV(ierr);
      }
      else if(mode == Teuchos::TRANS) {
        ierr = MatMultTransposeAdd(Amat_,petscX,petscY,petscY);CHKERRV(ierr);
      }
      else { // mode == Teuchos::CONJ_TRANS 
        ierr = MatMultHermitianTransposeAdd(Amat_,petscX,petscY,petscY);CHKERRV(ierr);
      }
    }

    ierr = VecGetArray(petscY,&vals);CHKERRV(ierr);
    ierr = VecGetLocalSize(petscY,&length);CHKERRV(ierr);
    for (int j=0; j<length; j++) yView[i][j] = vals[j];
    ierr = VecRestoreArray(petscY,&vals);CHKERRV(ierr);
  }

  VecDestroy(&petscX); VecDestroy(&petscY);
}



//! Scale the RowMatrix on the left with the given Vector x.
//==============================================================================
template<class Scalar, class LO, class GO, class Node>
void PETScAIJMatrix<Scalar,LO,GO,Node>::leftScale(const Vector<Scalar,LO,GO,Node> & x)
{
  PetscErrorCode ierr;
  Vec petscX;

  // Get the data from x
  Teuchos::ArrayRCP<const Scalar> xView = x.get1dView();

  // Copy x to petscX
#   ifdef HAVE_MPI
    ierr=VecCreateMPIWithArray(getRawMpiComm(*getComm()),1, x.getLocalLength(),x.getGlobalLength(),xView.get(),&petscX);CHKERRV(ierr);
#   else //FIXME  untested
    ierr=VecCreateSeqWithArray(getRawMpiComm(*getComm()),1, x.getLocalLength(),x.getGlobalLength(),xView.get(),&petscX);CHKERRV(ierr);
#   endif

  // Scale the matrix
  ierr = MatDiagonalScale(Amat_,petscX,NULL);CHKERRV(ierr);
}



//! Scale the RowMatrix on the right with the given Vector x.
//==============================================================================
template<class Scalar, class LO, class GO, class Node>
void PETScAIJMatrix<Scalar,LO,GO,Node>::rightScale(const Vector<Scalar,LO,GO,Node> & x)
{
  PetscErrorCode ierr;
  Vec petscX;

  // Get the data from x
  Teuchos::ArrayRCP<const Scalar> xView = x.get1dView();

  // Copy x to petscX
#   ifdef HAVE_MPI
    ierr=VecCreateMPIWithArray(getRawMpiComm(*getComm()),1, x.getLocalLength(),x.getGlobalLength(),xView.get(),&petscX);CHKERRV(ierr);
#   else //FIXME  untested
    ierr=VecCreateSeqWithArray(getRawMpiComm(*getComm()),1, x.getLocalLength(),x.getGlobalLength(),xView.get(),&petscX);CHKERRV(ierr);
#   endif

  // Scale the matrix
  ierr = MatDiagonalScale(Amat_,NULL,petscX);CHKERRV(ierr);
}



//! The Frobenius norm of the matrix.
//==============================================================================
template<class Scalar, class LO, class GO, class Node>
typename PETScAIJMatrix<Scalar,LO,GO,Node>::mag_type PETScAIJMatrix<Scalar,LO,GO,Node>::getFrobeniusNorm() const
{
  PetscErrorCode ierr;
  PetscReal nrm;
  ierr = MatNorm(Amat_,NORM_FROBENIUS,&nrm);CHKERRQ(ierr);
  return nrm;
}



} // namespace Tpetra
#endif /* _TPETRA_PETSCAIJMATRIX_H_ */
