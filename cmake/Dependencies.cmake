TRIBITS_PACKAGE_DEFINE_DEPENDENCIES(
  LIB_REQUIRED_PACKAGES Teuchos Tpetra Belos Ifpack2
  LIB_OPTIONAL_PACKAGES Epetra
  LIB_OPTIONAL_TPLS HDF5 UMFPACK AMD PETSC HYPRE
  TEST_OPTIONAL_PACKAGES MueLu Anasazi Amesos2 EpetraExt
  )
