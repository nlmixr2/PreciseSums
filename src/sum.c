#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#define R_NO_REMAP
#include <R.h>
#include <Rinternals.h>
#include <Rmath.h> //Rmath includes math.

// These are more precise sum algorithms.
// They are adapted from pseudo-code on wikipedia
// https://en.wikipedia.org/wiki/Kahan_summation_algorithm

//Not used: https://sourceforge.net/p/gmat/git/ci/264a12acad195e6a2467cfdc68abdcee801f73fc/tree/prototype/OptimalControl/ThirdParty/Intlab_V6/accsumdot/FastAccSum.m

extern double PreciseSums_DoubleSum(double *input, int n){
  long double sum = (long double)input[0];
  for (int i = 1; i < n; i++){
    sum += (long double)input[i];
  }
  return (double)sum;
}


#define PW_BLOCKSIZE    128
// Adapted from numpy https://github.com/juliantaylor/numpy/blob/b0bc01275cac04483e6df021211c1fa2ba65eaa3/numpy/core/src/umath/loops.c.src
/*
 * Pairwise summation, rounding error O(lg n) instead of O(n).
 * The recursion depth is O(lg n) as well.
 */

extern double
PreciseSums_pairwise_add_DOUBLE(double *a, int n)
{
  if (n < 8) {
    int i;
    double res = 0.;
    for (i = 0; i < n; i++) {
      res += a[i];
    }
    return res;
  }
  else if (n <= PW_BLOCKSIZE) {
    int i;
    double r[8], res;

    /*
     * sum a block with 8 accumulators
     * 8 times unroll reduces blocksize to 16 and allows vectorization with
     * avx without changing summation ordering
     */
    r[0] = a[0];
    r[1] = a[1];
    r[2] = a[2];
    r[3] = a[3];
    r[4] = a[4];
    r[5] = a[5];
    r[6] = a[6];
    r[7] = a[7];

    for (i = 8; i < n - (n % 8); i += 8) {
      r[0] += a[(i + 0)];
      r[1] += a[(i + 1)];
      r[2] += a[(i + 2)];
      r[3] += a[(i + 3)];
      r[4] += a[(i + 4)];
      r[5] += a[(i + 5)];
      r[6] += a[(i + 6)];
      r[7] += a[(i + 7)];
    }

    /* accumulate now to avoid stack spills for single peel loop */
    res = ((r[0] + r[1]) + (r[2] + r[3])) +
      ((r[4] + r[5]) + (r[6] + r[7]));

    /* do non multiple of 8 rest */
    for (; i < n; i++) {
      res += a[i];
    }
    return res;
  }
  else {
    /* divide by two but avoid non-multiples of unroll factor */
    int n2 = n / 2;
    n2 -= n2 % 8;
    return PreciseSums_pairwise_add_DOUBLE(a, n2) +
      PreciseSums_pairwise_add_DOUBLE(a + n2, n - n2);
  }
}

SEXP _psPairwiseSum(SEXP input){
  int len = Rf_length(input);
  double *dinput = REAL(input);
  SEXP rets = PROTECT(Rf_allocVector(REALSXP,1));
  REAL(rets)[0] = PreciseSums_pairwise_add_DOUBLE(dinput, len);
  UNPROTECT(1);
  return rets;
}

extern double PreciseSums_KahanSum(double *input, int len){
  volatile double sum = 0.0;
  volatile double y;
  volatile double t, c = 0.0; // A running compensation for lost low-order bits.
  int i;

  for (i = 0; i < len; i++){
    y = (double)input[i] - c;
    t = sum + y;
    c = (t - sum) - y;       // (t - sum) cancels the high-order part of y; subtracting y recovers negative (low part of y)
    sum = t;                 // Algebraically, c should always be zero. Beware overly-aggressive optimizing compilers!
  }
  return (double)sum;
}

SEXP _psKahanSum(SEXP input){
  int len = Rf_length(input);
  double *dinput = REAL(input);
  SEXP rets = PROTECT(Rf_allocVector(REALSXP,1));
  REAL(rets)[0] = PreciseSums_KahanSum(dinput, len);
  UNPROTECT(1);
  return rets;
}

extern double PreciseSums_NeumaierSum(double *input, int len){
  double sum = input[0];
  volatile double t,  c = 0.0; // A running compensation for lost low-order bits.
  int i;
  for (i = 1; i < len; i++){
    t = sum + (double)input[i];
    if (fabsl(sum) >= fabsl((double)input[i])){
      c += (sum - t) + (double)input[i]; // If sum is bigger, low-order digits of input[i] are lost.
    } else {
      c += ((double)input[i] - t) + sum; // Else low-order digits of sum are lost
    }
    sum = t;
  }
  return sum + c; // Correction only applied once in the very end
}

extern double PreciseSums_KleinSum(double *input, int len){
  // https://en.wikipedia.org/wiki/Kahan_summation_algorithm
  volatile double s = 0.0, c=0.0,cc=0.0,  cs = 0.0, ccs= 0.0, t;
  for (int i = 0;i < len; ++i){
    t = s+input[i];
    if (fabs(s) >= fabs(input[i])){
      c = (s - t) + input[i];
    } else {
      c = (input[i] - t) + s;
    }
    s = t;
    t = cs + c;
    if(fabs(cs) >= fabs(c)){
      cc = (cs - t) + c;
    } else {
      cc = (c - t) + cs;
    }
    cs = t;
    ccs = ccs + cc;
  }
  return s + cs + ccs;
}

SEXP _psNeumaierSum(SEXP input){
  int len = Rf_length(input);
  double *dinput = REAL(input);
  SEXP rets = PROTECT(Rf_allocVector(REALSXP,1));
  REAL(rets)[0] = PreciseSums_NeumaierSum(dinput, len);
  UNPROTECT(1);
  return rets;
}

#define NUM_PARTIALS  32  /* initial partials array size, on stack */

extern double PreciseSums_Python_fsum_r(double *iterable, int iterable_len, double *p, int m){
  // See http://code.activestate.com/recipes/393090-binary-floating-point-summation-accurate-to-full-p/
  // Also https://github.com/python/cpython/blob/a0ce375e10b50f7606cb86b072fed7d8cd574fe7/Modules/mathmodule.c
  // Mostly the same as python's math.fsum
  double x, y, t;
  double xsave, special_sum = 0.0, inf_sum = 0.0, sum = 0.0;
  volatile double hi, yr, lo;
  int ix, i, j, n = 0;//, m = NUM_PARTIALS;
  //double *p = Calloc(NUM_PARTIALS, double);
  // for x in input
  for (ix = 0; ix < iterable_len; ix++){
    x = (double) iterable[ix];
    xsave = x;
    for (i = j = 0; j < n; j++) {
      y = p[j];
      if (fabsl(x) < fabsl(y)) {
        t = x; x = y; y = t;
      }
      hi = x + y;
      yr = hi - x;
      lo = y - yr;
      if (lo != 0.0)
        p[i++] = lo;
      x = hi;
    }

    n = i;
    if (x != 0.0) {
      if (!R_FINITE(x)) {
        /* a nonfinite x could arise either as
           a result of intermediate overflow, or
           as a result of a nan or inf in the
           summands */
        if (R_FINITE(xsave) || ISNAN(xsave)) {
	  if (m > 0) R_Free(p);
	  return PreciseSums_KleinSum(iterable, iterable_len);
          /* error("intermediate overflow in fsum"); */
        } else {
          inf_sum += xsave;
        }
        special_sum += xsave;
        /* reset partials */
        n = 0;
      } else {
        if (m > 0 && n >= m){
          //&& _fsum_realloc(&p, n, ps, &m)
          // Doubles the size of array.
          m += m;
          p = R_Realloc(p, m, double);
        } else if (m < 0 && n >= -m){
	  /* if (m > 0) R_Free(p); */
	  return PreciseSums_KleinSum(iterable, iterable_len);
	  /* error("The size of the saved partials is too small to calculate the sum."); */
	}
        p[n++] = x;
      }
    }
  }
  if (special_sum != 0.0) {
    if (ISNAN(inf_sum)){
      if (m > 0) R_Free(p);
      Rf_error("-inf + inf in fsum");
    }
    sum = special_sum;
    return sum;
  }

  hi = 0.0;
  j = n;
  if (n > 0) {
    hi = p[--n];
    /* sum_exact(ps, hi) from the top, stop when the sum becomes
       inexact. */
    while (n > 0) {
      x = hi;
      y = p[--n];
      if (fabs(y) >= fabs(x)){
	if (m > 0) R_Free(p);
	return PreciseSums_KleinSum(iterable, iterable_len);
        /* Rprintf("Partial Sums:\n"); */
        /* for (i = 0; i < j; i++){ */
        /*   Rprintf("p[%d] = %f\n",i,p[i]); */
        /* } */
        /* Rprintf("Assertion Error:\n"); */
        /* Rprintf("fabs(y) >= fabs(x) or %f >= %f\n",fabs(y),fabs(x)); */
        /* if (m > 0) R_Free(p); */
        /* error("Error in parital sums."); */
      }
      hi = x + y;
      yr = hi - x;
      lo = y - yr;
      if (lo != 0.0)
        break;
    }
    /* Make half-even rounding work across multiple partials.
       Needed so that sum([1e-16, 1, 1e16]) will round-up the last
       digit to two instead of down to zero (the 1e-16 makes the 1
       slightly closer to two).  With a potential 1 ULP rounding
       error fixed-up, math.fsum() can guarantee commutativity. */
    if (n > 0 && ((lo < 0.0 && p[n-1] < 0.0) ||
                  (lo > 0.0 && p[n-1] > 0.0))) {
      y  = lo * 2.0;
      x  = hi + y;
      yr = x - hi;
      if (y == yr)
        hi = x;
    }
  }
  sum = hi;
  return sum;
}

extern double PreciseSums_Python_fsum(double *iterable, int iterable_len){
  double *p = R_Calloc(NUM_PARTIALS, double);
  int m = NUM_PARTIALS;
  double ret = PreciseSums_Python_fsum_r(&iterable[0], iterable_len, p, m);
  R_Free(p);
  return ret;
}

SEXP _psPythonSum(SEXP input){
  int len = Rf_length(input);
  double *dinput = REAL(input);
  SEXP rets = PROTECT(Rf_allocVector(REALSXP,1));
  REAL(rets)[0] = PreciseSums_Python_fsum(dinput, len);
  UNPROTECT(1);
  return rets;
}

int PreciseSums_sum_type = 1;
extern double PreciseSums_sum (double *input, int n){
  switch (PreciseSums_sum_type){
  case 6:
    return PreciseSums_KleinSum(input, n);
  case 5:
    return PreciseSums_DoubleSum(input, n);
    break;
  case 1:
    return PreciseSums_pairwise_add_DOUBLE(input, n);
    break;
  case 2:
    return PreciseSums_Python_fsum(input, n);
    break;
  case 3:
    return PreciseSums_KahanSum(input, n);
    break;
  case 4:
    return PreciseSums_NeumaierSum(input, n);
    break;
  }
  //PreciseSums_KahanSum;
  // PreciseSums_NeumaierSum
  Rf_error("Unknown sum type.");
  return 0;
}

extern double PreciseSums_sum_r(double *input, int n, double *p, int m, int type){
  switch (type){
  case 6:
    return PreciseSums_KleinSum(input, n);
  case 5:
    return PreciseSums_DoubleSum(input, n);
    break;
  case 1:
    return PreciseSums_pairwise_add_DOUBLE(input, n);
    break;
  case 2:
    return PreciseSums_Python_fsum_r(input, n, p, m);
    break;
  case 3:
    return PreciseSums_KahanSum(input, n);
    break;
  case 4:
    return PreciseSums_NeumaierSum(input, n);
    break;
  }
  //PreciseSums_KahanSum;
  // PreciseSums_NeumaierSum
  Rf_error("Unknown sum type.");
  return 0;
}

extern void PreciseSums_sum_set(int i){
  PreciseSums_sum_type = i;
}

extern int PreciseSums_sum_get(void){
  return PreciseSums_sum_type;
}


SEXP _psSetSum(SEXP input){
  PreciseSums_sum_type = (int) INTEGER(input)[0];
  return R_NilValue;
}

SEXP _psSum(SEXP input){
  int len = Rf_length(input);
  double *dinput = REAL(input);
  SEXP rets = PROTECT(Rf_allocVector(REALSXP,1));
  REAL(rets)[0] = PreciseSums_sum(dinput, len);
  UNPROTECT(1);
  return rets;
}

extern double PreciseSums_sumV(int n, ...){
  va_list valist;
  va_start(valist, n);
  double *p = R_Calloc(n, double);
  for (unsigned int i = 0; i < n; i++){
    p[i] = va_arg(valist, double);
  }
  va_end(valist);
  double s = PreciseSums_sum(p, n);
  R_Free(p);
  return s;
}


extern double PreciseSums_sumV_r(double *p, int n, ...){
  va_list valist;
  va_start(valist, n);
  for (unsigned int i = 0; i < n; i++){
    p[i] = va_arg(valist, double);
  }
  va_end(valist);
  double s = PreciseSums_sum(p, n);
  return s;
}
