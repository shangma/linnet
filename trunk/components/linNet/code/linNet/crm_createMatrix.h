#ifndef CRM_CREATEMATRIX_INCLUDED
#define CRM_CREATEMATRIX_INCLUDED
/**
 * @file crm_createMatrix.h
 * Generic implementation of a pair of functions that creates and deletes a
 * rectangular array of elements with run-time determined size. The functions are
 * implemented as macro #CRM_CREATE_MATRIX. Include this header and call the macro
 * once in order to generate the code for the pair of functions.\n
 *   The generated functions are declared by the other macro
 * #CRM_DECLARE_CREATE_MATRIX. Call this macro to generate the prototypes in your
 * code.
 *
 * Copyright (C) 2014 Peter Vranken (mailto:Peter_Vranken@Yahoo.de)
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Include files
 */


/*
 * Defines
 */

/** This macro instantiates a pair of functions that create and delete a matrix [m,n]
    of elements.\n
      The file crm_createMatrix.h should be included from the header file of the
    module, that needs the functions. Then place one call of the macro with
    appropriate macro parameters in the implementation file. This will then generate
    the code. */
#define CRM_CREATE_MATRIX( /* context */ mmm                                                \
                         , elementType_t                                                    \
                         , initialElementValue                                              \
                         , fctFreeElement                                                   \
                         )                                                                  \
                                                                                            \
/**                                                                                         \
 * Allocate memory for a two dimensional matrix. Initialize all coeefficients to the        \
 * same initial value.\n                                                                    \
 *   After use the matrix needs to be deleted by void crm_deleteMatrix().                   \
 *   @return                                                                                \
 * The matrix is returned as an array with double index.                                    \
 *   @param noRows                                                                          \
 * The number of rows in the matrix.                                                        \
 *   @param noCols                                                                          \
 * The number of columns in the matrix.                                                     \
 *   @remark                                                                                \
 * The matrix is implemented as array of pointers to rows. For rectangular shapes it can    \
 * significantly save memory if noRows << noCols; consider to transpose your matrix in      \
 * case.                                                                                    \
 *   @remark                                                                                \
 * This function is implemented by expansion of the generic macro #CRM_CREATE_MATRIX.       \
 * The type of the matrix elements an their common initial value are passed as macro        \
 * parameters and are thus hard-coded at compile time.                                      \
 *   @see void crm_deleteMatrix()                                                           \
 */                                                                                         \
                                                                                            \
elementType_t **mmm##_createMatrix(unsigned int noRows, unsigned int noCols)                \
{                                                                                           \
    /* The array of pointers to the matrix rows and the 2D array of actual matrix           \
       element objects are allocated at once as a single chunk of memory. This is not       \
       perfectly proper. To avoid the risk of an alignment problem the element part of      \
       the chunk is placed after an even number of row pointers. This guarantees the        \
       common alignment to the matrix element array on all systems, where the               \
       alignment of a pointer is identical either to the common alignment or to the         \
       half of the common alignment. This should be the case for most systems. */           \
    const unsigned int noRowPointers = (noRows+1) & ~1u                                     \
                     , sizeOfChunk = noRowPointers*(sizeof(elementType_t*))                 \
                                     + noRows*noCols*sizeof(elementType_t);                 \
                                                                                            \
    elementType_t **A = smalloc(sizeOfChunk, __FILE__, __LINE__);                           \
                                                                                            \
    /* Initialize the row pointers. */                                                      \
    elementType_t *pRow = (elementType_t*)((char*)A                                         \
                                           + noRowPointers*sizeof(elementType_t*)           \
                                          );                                                \
    unsigned m, n;                                                                          \
    for(m=0; m<noRows; ++m)                                                                 \
        A[m] = pRow + m*noCols;                                                             \
    assert((char*)A[noRows-1] + noCols*sizeof(elementType_t) == (char*)A + sizeOfChunk      \
           &&  (char*)&A[noRows-1][noCols-1] + sizeof(elementType_t)                        \
               == (char*)A + sizeOfChunk                                                    \
          );                                                                                \
                                                                                            \
    /* Initialize the array. */                                                             \
    for(m=0; m<noRows; ++m)                                                                 \
        for(n=0; n<noCols; ++n)                                                             \
            A[m][n] = (initialElementValue);                                                \
                                                                                            \
    return A;                                                                               \
                                                                                            \
} /* End of crm_createMatrix */                                                             \
                                                                                            \
                                                                                            \
                                                                                            \
                                                                                            \
                                                                                            \
/**                                                                                         \
 * Delete a matrix that had been created with elementType_t **crm_createMatrix(unsigned     \
 * int noRows, unsigned int noCols).                                                        \
 *   @param A                                                                               \
 * The matrix as got from crm_createMatrix.                                                 \
 *   @param noRows                                                                          \
 * The number of rows in the matrix. Needs to be the same value as had been passed at       \
 * matrix creation time.                                                                    \
 *   @param noCols                                                                          \
 * The number of columns in the matrix. Needs to be the same value as had been passed       \
 * at matrix creation time.                                                                 \
 */                                                                                         \
                                                                                            \
void mmm##_deleteMatrix(elementType_t **A, unsigned int noRows, unsigned int noCols)        \
{                                                                                           \
    if(A == NULL)                                                                           \
        return;                                                                             \
                                                                                            \
    unsigned int m, n;                                                                      \
    for(m=0; m<noRows; ++m)                                                                 \
        for(n=0; n<noCols; ++n)                                                             \
            (fctFreeElement)(A[m][n]);                                                      \
    free(A);                                                                                \
                                                                                            \
} /* End of crm_deleteMatrix */                                                             \
                                                                                            \
/* End of macro CRM_CREATE_MATRIX */




/** This macro, when called, declares the pair of functions generated by the other
    macro #CRM_CREATE_MATRIX.\n
      Place one call of the macro with appropriate macro parameters in the header of
    the client module. This will then generate the prototypes of the generated
    functions. */
#define CRM_DECLARE_CREATE_MATRIX(/* modulePrefix */ mmm, elementType_t)                    \
                                                                                            \
/** Create a rectangular matrix. */                                                         \
elementType_t **mmm##_createMatrix(unsigned int noRows, unsigned int noCols);               \
                                                                                            \
/** Delete a matrix after use. */                                                           \
void mmm##_deleteMatrix(elementType_t **A, unsigned int noRows, unsigned int noCols);       \
                                                                                            \
/* End of macro CRM_DECLARE_CREATE_MATRIX */




/*
 * Global type definitions
 */



/*
 * Global data declarations
 */


/*
 * Global prototypes
 */



#endif  /* CRM_CREATEMATRIX_INCLUDED */
