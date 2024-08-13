#include <stdio.h>
#include <stdlib.h>
#include <R.h>
#include <Rinternals.h>
#include <Rmath.h> //Rmath includes math.
#include <R_ext/Rdynload.h>

SEXP _psKahanSum(SEXP input);
SEXP _psNeumaierSum(SEXP input);

SEXP _psPythonSum(SEXP input);
SEXP _psProd(SEXP input);
SEXP _psSum(SEXP input);
SEXP _psSetSum(SEXP input);
SEXP _psSetProd(SEXP input);
SEXP _psPairwiseSum(SEXP input);
double PreciseSums_sum(double *input, int len);
double PreciseSums_prod(double *input, int len);

double PreciseSums_prod_r(double *input, double *p, int n, int type);
double PreciseSums_sum_r(double *input, int n, double *p, int m, int type);

static R_NativePrimitiveArgType PreciseSums_Sum_t[] = {
  REALSXP, INTSXP
};


int PreciseSums_sum_get(void);
int PreciseSums_prod_get(void);

SEXP _PreciseSumsPtr(void) {
  int pro = 0;  // Counter for the number of PROTECT calls

  SEXP PreciseSums_sum_ptr = PROTECT(R_MakeExternalPtrFn((DL_FUNC)&PreciseSums_sum, R_NilValue, R_NilValue)); pro++;
  SEXP PreciseSums_prod_ptr = PROTECT(R_MakeExternalPtrFn((DL_FUNC)&PreciseSums_prod, R_NilValue, R_NilValue)); pro++;

  SEXP PreciseSums_sum_r_ptr = PROTECT(R_MakeExternalPtrFn((DL_FUNC)&PreciseSums_sum_r, R_NilValue, R_NilValue)); pro++;
  SEXP PreciseSums_prod_r_ptr = PROTECT(R_MakeExternalPtrFn((DL_FUNC)&PreciseSums_prod_r, R_NilValue, R_NilValue)); pro++;

  SEXP PreciseSums_sum_get_ptr = PROTECT(R_MakeExternalPtrFn((DL_FUNC)&PreciseSums_sum_get, R_NilValue, R_NilValue)); pro++;
  SEXP PreciseSums_prod_get_ptr = PROTECT(R_MakeExternalPtrFn((DL_FUNC)&PreciseSums_prod_get, R_NilValue, R_NilValue)); pro++;

  SEXP ret = PROTECT(allocVector(VECSXP, 6)); pro++;
  SET_VECTOR_ELT(ret, 0, PreciseSums_sum_ptr);
  SET_VECTOR_ELT(ret, 1, PreciseSums_prod_ptr);
  SET_VECTOR_ELT(ret, 2, PreciseSums_sum_r_ptr);
  SET_VECTOR_ELT(ret, 3, PreciseSums_prod_r_ptr);
  SET_VECTOR_ELT(ret, 4, PreciseSums_sum_get_ptr);
  SET_VECTOR_ELT(ret, 5, PreciseSums_prod_get_ptr);

  SEXP retn = PROTECT(allocVector(STRSXP, 6)); pro++;
  SET_STRING_ELT(retn, 0, mkChar("PreciseSums_sum"));
  SET_STRING_ELT(retn, 1, mkChar("PreciseSums_prod"));
  SET_STRING_ELT(retn, 2, mkChar("PreciseSums_sum_r"));
  SET_STRING_ELT(retn, 3, mkChar("PreciseSums_prod_r"));
  SET_STRING_ELT(retn, 4, mkChar("PreciseSums_sum_get"));
  SET_STRING_ELT(retn, 5, mkChar("PreciseSums_prod_get"));

  setAttrib(ret, R_NamesSymbol, retn);

  UNPROTECT(pro);
  return ret;
}

void R_init_PreciseSums(DllInfo *info){
  R_CallMethodDef callMethods[]  = {
    {"_PreciseSumsPtr", (DL_FUNC) &_PreciseSumsPtr, 0},
    {"_psKahanSum", (DL_FUNC) &_psKahanSum,1},
    {"_psNeumaierSum", (DL_FUNC) &_psNeumaierSum,1},
    {"_psPythonSum", (DL_FUNC) &_psPythonSum, 1},
    {"_psPairwiseSum", (DL_FUNC) &_psPairwiseSum, 1},
    {"_psSum", (DL_FUNC) &_psSum, 1},
    {"_psProd", (DL_FUNC) &_psProd, 1},
    {"_psSetSum",(DL_FUNC) &_psSetSum, 1},
    {"_psSetProd",(DL_FUNC) &_psSetProd, 1},
    {NULL, NULL, 0}
  };

  R_RegisterCCallable("PreciseSums","PreciseSums_sum",    (DL_FUNC) PreciseSums_sum);
  R_RegisterCCallable("PreciseSums","PreciseSums_prod",   (DL_FUNC) PreciseSums_prod);

  R_RegisterCCallable("PreciseSums","PreciseSums_prod_r", (DL_FUNC) PreciseSums_prod_r);
  R_RegisterCCallable("PreciseSums","PreciseSums_sum_r",  (DL_FUNC) PreciseSums_sum_r);
  R_RegisterCCallable("PreciseSums", "PreciseSums_prod_get", (DL_FUNC) PreciseSums_prod_get);
  R_RegisterCCallable("PreciseSums", "PreciseSums_sum_get", (DL_FUNC) PreciseSums_sum_get);

  static const R_CMethodDef cMethods[] = {
    {"PreciseSums_sum", (DL_FUNC) &PreciseSums_sum, 2, PreciseSums_Sum_t},
    {"PreciseSums_prod", (DL_FUNC) &PreciseSums_prod, 2, PreciseSums_Sum_t},
    {NULL, NULL, 0, NULL}
  };

  R_registerRoutines(info, cMethods, callMethods, NULL, NULL);
  R_useDynamicSymbols(info, FALSE);

}
