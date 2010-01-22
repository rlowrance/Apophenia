/** \file apop_missing_data.c Some missing data handlers. */
/* Copyright (c) 2007, 2009 by Ben Klemens.  Licensed under the modified GNU GPL v2; see COPYING and COPYING2.  */

#include "model.h"
#include "internal.h"
#include "variadic.h"
#include "likelihoods.h"

/** If there is an NaN anywhere in the row of numeric data (including the matrix, the vector, and the weights) then delete the row from the data set.

The function returns a new data set with the NaNs removed, so the original data set is left unmolested. You may want to \c apop_data_free the original immediately after this function.

\li If every row has an NaN, then this returns \c NULL.
\li If there is text, it gets pruned as well.
\li If \c inplace = 'y', then I'll free each element of the input data
    set and refill it with the pruned elements. Again, I'll take up (up to)
    twice the size of the data set in memory during the function. If
    every row has an NaN, then your \c apop_data set will have a lot of
    \c NULL elements.
\li This function uses the \ref designated syntax for inputs.

    \param d    The data, with NaNs
    \param inplace If \c 'y', clear out the pointer-to-\ref apop_data that
    you sent in and refill with the pruned data. If \c 'n', leave the
    set alone and return a new data set.
    \return     A (potentially shorter) copy of the data set, without
    NaNs. If <tt>inplace=='y'</tt>, redundant with the input.
*/
APOP_VAR_HEAD apop_data * apop_data_listwise_delete(apop_data *d, char inplace){
    apop_data * apop_varad_var(d, NULL);
    if (!d) return NULL;
    char apop_varad_var(inplace, 'n');
    return apop_data_listwise_delete_base(d, inplace);
APOP_VAR_ENDHEAD
    Get_vmsizes(d) //defines vsize, wsize, msize1, msize2.
    int i, j, to_rm;
    int max = d->matrix ? d->matrix->size2: 0;
    int min = d->vector ? -1 : 0;
    apop_assert(msize1 || vsize, NULL, 0, 'c', 
            "You sent to apop_data_listwise_delete a data set with void matrix and vector. Confused, it is returning NULL.\n");
    //find out where the NaNs are
  gsl_vector *marked = gsl_vector_calloc(msize1);
    for (i=0; i< msize1; i++)
        for (j=min; j <max; j++)
            if (gsl_isnan(apop_data_get(d, i, j))){
                    gsl_vector_set(marked, i, 1);
                    break;
            }
    for (i=0; i< wsize; i++)
        if (gsl_isnan(gsl_vector_get(d->weights, i)))
                gsl_vector_set(marked, i, 1);
    to_rm   = apop_sum(marked);
    //copy the good data.
    if (to_rm  == msize1)
        return NULL;
  apop_data *out = apop_data_alloc(0, msize1-to_rm, msize1 ? max : -1);
    if (wsize)
        out->weights = gsl_vector_alloc(wsize - to_rm);

    apop_name_free(out->names);
    out->names  = apop_name_copy(d->names); 
    for(size_t k=0; k < d->names->rowct; k ++)
        free(out->names->row[k]);
    out->names->rowct = 0;
    free(out->names->row);
    out->names->row   = NULL;

    if (vsize && msize1)
        out->vector = gsl_vector_alloc(msize1 - to_rm);
    j   = 0;
    for (i=0; i< msize1; i++)
        if (!gsl_vector_get(marked, i)){
            if (vsize)
                gsl_vector_set(out->vector, j, gsl_vector_get(d->vector, i));
            if (msize1){
                Apop_row(d, i, v);
                gsl_matrix_set_row(out->matrix, j, v);
                if (d->names->row && d->names->rowct > i)
                    apop_name_add(out->names, d->names->row[i], 'r');
            }
            if (i < d->textsize[0]){
                out->text = realloc(out->text, sizeof(char**) * (j+1));
                out->text[j] = malloc(sizeof(char*) * d->textsize[1]);
                out->textsize[0]++;
                if (!out->textsize[1])
                    out->textsize[1] = d->textsize[1];
                for (int k=0; k< d->textsize[1]; k++)
                    apop_text_add(out, j, k, d->text[i][k]);
            }
            if (wsize)
                gsl_vector_set(out->weights, j, gsl_vector_get(d->weights, i));
            j++;
        }
    gsl_vector_free(marked);
    if (inplace=='n')
        return out;
    if (vsize) gsl_vector_free(d->vector);
    if (wsize) gsl_vector_free(d->weights);
    if (msize1) gsl_matrix_free(d->matrix);
    if (d->textsize[0]) apop_text_free(d->text, d->textsize[0], d->textsize[1]);
    apop_name_free(d->names);
    if (vsize) d->vector = out->vector;
    if (wsize) d->weights = out->weights;
    if (msize1) d->matrix = out->matrix;
    out->names = d->names;
    out->textsize[0] = d->textsize[0];
    out->textsize[1] = d->textsize[1];
    free(out);
    return d;
}

//ML imputation

/** \hideinitializer */
#define Switch_back    \
    apop_data *real_data = ml_model->parameters;   \
    apop_model *actual_base = ml_model->more; \
    actual_base->parameters = d; 

static apop_model * i_est(apop_data *d, apop_model *ml_model){
    Switch_back
    return apop_estimate(real_data, *actual_base);
}

#include "mapply.h"
static double i_ll(apop_data *d, apop_model *ml_model){
    Switch_back
    return apop_log_likelihood(real_data, actual_base);
}

static double i_p(apop_data *d, apop_model *ml_model){
    Switch_back
    return apop_p(real_data, actual_base);
}

static apop_model apop_ml_impute_model = {"Internal ML imputation model", .estimate=i_est, .p = i_p, .log_likelihood=i_ll};

/** Impute the most likely data points to replace NaNs in the data, and insert them into 
the given data. That is, the data set is modified in place.

How it works: this uses the machinery for \c apop_model_fix_params. The only difference is 
that this searches over the data space and takes the parameter space as fixed, while basic 
fix params model searches parameters and takes data as fixed. So this function just does the
necessary data-parameter switching to make that happen.

\param  d       The data set. It comes in with NaNs and leaves entirely filled in.
\param  mvn A parametrized \c apop_model from which you expect the data was derived.
if \c NULL, then I'll use the Multivariate Normal that best fits the data after listwise deletion.

\return An estimated <tt>apop_ml_impute_model</tt>. Also, the data input will be filled in and ready to use.
*/
apop_model * apop_ml_impute(apop_data *d,  apop_model* mvn){
    if (!mvn){
        apop_data *list_d = apop_data_listwise_delete(d);
        apop_assert(list_d, NULL, 0, 's', "Listwise deletion returned no whole rows, "
                            "so I couldn't fit a Multivariate Normal to your data. "
                            "Please provide a pre-estimated initial model.");
        mvn = apop_estimate(list_d, apop_multivariate_normal);
        apop_data_free(list_d);
    }
    apop_model *impute_me = apop_model_copy(apop_ml_impute_model);
    impute_me->parameters = d;
    impute_me->more = mvn;
    apop_model *fixed = apop_model_fix_params(impute_me);
//    Apop_model_add_group(fixed, apop_mle, .want_cov='n', .dim_cycle_tolerance=1);
    apop_model *m = apop_estimate(mvn->parameters, *fixed);
    apop_data_memcpy(d, m->parameters); //A bit inefficient.
    return m;
}
