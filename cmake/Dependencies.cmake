IF (PROJECT_NAME STREQUAL "xSDKTrilinos")

  TRIBITS_PACKAGE_DEFINE_DEPENDENCIES(
    LIB_REQUIRED_TPLS TrilinosPkgsTpl TrilinosTplsTpl
    LIB_OPTIONAL_TPLS MPI PETSC HYPRE
    )

ELSE()

  TRIBITS_PACKAGE_DEFINE_DEPENDENCIES(
    LIB_REQUIRED_PACKAGES Teuchos Tpetra Belos Ifpack2
    LIB_OPTIONAL_PACKAGES Epetra
    LIB_OPTIONAL_TPLS PETSC HYPRE
    TEST_OPTIONAL_PACKAGES MueLu Anasazi Amesos2 EpetraExt
    )

ENDIF()
