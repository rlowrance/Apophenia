//regression.h	Copyright (c) 2005--2007 by Ben Klemens.  Licensed under the modified GNU GPL v2; see COPYING and COPYING2.

#ifndef apop_regression_h
#define  apop_regression_h

#include "types.h"
#include "variadic.h"
#include <gsl/gsl_matrix.h>

#undef __BEGIN_DECLS    /* extern "C" stuff cut 'n' pasted from the GSL. */
#undef __END_DECLS
#ifdef __cplusplus
# define __BEGIN_DECLS extern "C" {
# define __END_DECLS }
#else
# define __BEGIN_DECLS /* empty */
# define __END_DECLS /* empty */
#endif

__BEGIN_DECLS

/** Settings for least-squares type models */
typedef struct {
    int destroy_data;
    gsl_vector *weights;
    apop_data *instruments;
    int want_cov;
    int want_expected_value;
    void *copy;
    void *free;
} apop_ls_settings;

apop_model *apop_fixed_effects_OLS(apop_data *data, gsl_vector *categories);

#define apop_F_test apop_f_test

apop_data *	apop_t_test(gsl_vector *a, gsl_vector *b);
apop_data *	apop_paired_t_test(gsl_vector *a, gsl_vector *b);


apop_data * apop_text_unique_elements(const apop_data *d, size_t col);
gsl_vector * apop_vector_unique_elements(const gsl_vector *v);
apop_data *apop_text_to_factors(apop_data *d, size_t textcol, int datacol);

#ifdef APOP_NO_VARIADIC
apop_data * apop_data_to_dummies(apop_data *d, int col, char type, int keep_first);
apop_data *apop_f_test (apop_model *est, apop_data *contrast);
#else
apop_data * apop_data_to_dummies_base(apop_data *d, int col, char type, int keep_first);
apop_varad_declare(apop_data *, apop_data_to_dummies,apop_data *d; int col; char type; int keep_first );
#define apop_data_to_dummies(...) apop_varad_link(apop_data_to_dummies, __VA_ARGS__)

apop_data *apop_f_test_base (apop_model *est, apop_data *contrast);
apop_varad_declare(apop_data *, apop_f_test, apop_model *est; apop_data *contrast);
#define apop_f_test(...) apop_varad_link(apop_f_test, __VA_ARGS__)
#endif

double apop_two_tailify(double in);
//My convenience fn to turn the results from a symmetric one-tailed table lookup
//into a two-tailed confidence interval.

apop_model *apop_estimate_fixed_effects_OLS(apop_data *data, gsl_vector *categories);

apop_data *apop_estimate_coefficient_of_determination (apop_model *in);
apop_data *apop_estimate_r_squared (apop_model *in);
void apop_estimate_parameter_t_tests (apop_model *est);

//apop_testing.c
apop_data* apop_anova(char *table, char *data, char *grouping1, char *grouping2);


#define apop_ANOVA(table, data, grouping1, grouping2) apop_anova(table, data, grouping1, grouping2)

__END_DECLS
#endif
