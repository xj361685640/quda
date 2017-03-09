#ifndef _QUDACXX_H
#define _QUDACXX_H

/**
 * @file  quda.h
 * @brief Main header file for the QUDA library
 *
 * Note to QUDA developers: When adding new members to QudaGaugeParam
 * and QudaInvertParam, be sure to update lib/check_params.h as well
 * as the Fortran interface in lib/quda_fortran.F90.
 */

#include <enum_quda.h>
#include <stdio.h> /* for FILE */
#include <quda_constants.h>

void computeKSLinkQuda(cudaGaugeField* cudaFatLink, cudaGaugeField* cudaLongLink, cudaGaugeField* cudaUnitarizedLink, cudaGaugeField* cudaInLink, double *path_coeff, QudaGaugeParam *param) {


#endif /* _QUDACXX_H */