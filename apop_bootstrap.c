/** \file apop_bootstrap.c

Copyright (c) 2006--2007 by Ben Klemens.  Licensed under the modified GNU GPL v2; see COPYING and COPYING2.  */

#include "apop_internal.h"

/** Initialize a \c gsl_rng.
 
  Uses the Tausworth routine.

\param  seed    The seed. No need to get funny with it: 0, 1, and 2 will produce wholly different streams.
\return The RNG ready for your use.
\ingroup convenience_fns
*/
gsl_rng *apop_rng_alloc(int seed){
    static int first_use    = 1;
    if (first_use){
       first_use = 0;
       gsl_rng_env_setup();
    }
    gsl_rng *setme  =  gsl_rng_alloc(gsl_rng_taus2);
    gsl_rng_set(setme, seed);
    return setme;
}

/** Give me a data set and a model, and I'll give you the jackknifed covariance matrix of the model parameters.

The basic algorithm for the jackknife (with many details glossed over): create a sequence of data
sets, each with exactly one observation removed, and then produce a new set of parameter estimates 
using that slightly shortened data set. Then, find the covariance matrix of the derived parameters.

Jackknife or bootstrap? As a broad rule of thumb, the jackknife works best on models that are closer to linear. The worse a linear approximation does (at the given data), the worse the jackknife approximates the variance.

Sample usage:
\code
apop_data_show(apop_jackknife_cov(your_data, your_model));
\endcode
 
\param in	    The data set. An \ref apop_data set where each row is a single data point.
\param model    An \ref apop_model, that will be used internally by \ref apop_estimate.
            
\exception out->error=='n'   \c NULL input data.
\return         An \c apop_data set whose matrix element is the estimated covariance matrix of the parameters.
\see apop_bootstrap_cov
 */
apop_data * apop_jackknife_cov(apop_data *in, apop_model model){
    Apop_stopif(!in, apop_data *out = apop_data_alloc(); out->error='n'; return out, 0, "The data input can't be NULL.");
    Get_vmsizes(in); //msize1, msize2, vsize
    apop_model *e              = apop_model_copy(model);
    int         i, n           = GSL_MAX(msize1, GSL_MAX(vsize, in->textsize[0]));
    apop_model *overall_est    = e->parameters ? e : apop_estimate(in, *e);//if not estimated, do so
    gsl_vector *overall_params = apop_data_pack(overall_est->parameters);
    gsl_vector_scale(overall_params, n); //do it just once.
    int         paramct        = overall_params->size;
    gsl_vector  *pseudoval     = gsl_vector_alloc(paramct);

    //Copy the original, minus the first row.
    Apop_data_rows(in, 1, n-1, allbutfirst);
    apop_data *subset = apop_data_copy(allbutfirst);
    apop_name *tmpnames = in->names; 
    in->names = NULL;  //save on some copying below.

    apop_data *array_of_boots = apop_data_alloc(n, paramct);

    for(i = -1; i< n-1; i++){
        //Get a view of row i, and copy it to position i-1 in the short matrix.
        if (i >= 0){
            Apop_data_row(in, i, onerow);
            Apop_data_row(subset, i, subsetrow);
            apop_data_memcpy(subsetrow, onerow);
        }
        apop_model *est = apop_estimate(subset, *e);
        gsl_vector *estp = apop_data_pack(est->parameters);
        gsl_vector_memcpy(pseudoval, overall_params);// *n above.
        gsl_vector_scale(estp, n-1);
        gsl_vector_sub(pseudoval, estp);
        gsl_matrix_set_row(array_of_boots->matrix, i+1, pseudoval);
        apop_model_free(est);
        gsl_vector_free(estp);
    }
    in->names = tmpnames;
    apop_data   *out    = apop_data_covariance(array_of_boots);
    gsl_matrix_scale(out->matrix, 1./(n-1.));
    apop_data_free(subset);
    gsl_vector_free(pseudoval);
    if (e!=overall_est)
        apop_model_free(overall_est);
    apop_model_free(e);
    gsl_vector_free(overall_params);
    return out;
}


/** Give me a data set and a model, and I'll give you the bootstrapped covariance matrix of the parameter estimates.

\param data	    The data set. An \c apop_data set where each row is a single data point. (No default)
\param model    An \ref apop_model, whose \c estimate method will be used here. (No default)
\param iterations How many bootstrap draws should I make? (default: 1,000) 
\param rng        An RNG that you have initialized, probably with \c apop_rng_alloc. (Default: see \ref autorng)
\param keep_boots  If 'y', then add a page to the output \ref apop_data set with the statistics calculated for each bootstrap iteration.
They are packed via \ref apop_data_pack, so use \ref apop_data_unpack if needed. (Default: 'n')
\code
apop_data *boot_output = apop_bootstrap_cov(your_data, your_model, .keep_boots='y');
apop_data *boot_stats = apop_data_get_page(boot_output, "<bootstrapped statistics>");

Apop_row(boot_stats, 27, row_27)
//If the output statistic is not just a vector, you'll need to use apop_data_unpack to put
//it into the right shape. Let's assume for now that it's just a vector:
printf("The statistics calculated on the 28th iteration:\n");
apop_vector_print(row_27);
\endcode
\param ignore_nans If \c 'y' and any of the elements in the estimation return \c NaN, then I will throw out that draw and try again. If \c 'n', then I will write that set of statistics to the list, \c NaN and all. I keep count of throw-aways; if there are more than \c iterations elements thrown out, then I throw an error and return with estimates using data I have so far. That is, I assume that \c NaNs are rare edge cases; if they are as common as good data, you might want to rethink how you are using the bootstrap mechanism. (Default: 'n')
\return         An \c apop_data set whose matrix element is the estimated covariance matrix of the parameters.
\exception out->error=='n'   \c NULL input data.
\li This function uses the \ref designated syntax for inputs.
\see apop_jackknife_cov
 */
APOP_VAR_HEAD apop_data * apop_bootstrap_cov(apop_data * data, apop_model model, gsl_rng *rng, int iterations, char keep_boots, char ignore_nans) {
    static gsl_rng *spare = NULL;
    apop_data * apop_varad_var(data, NULL);
    apop_model model = varad_in.model;
    int apop_varad_var(iterations, 1000);
    Apop_stopif(!data, apop_data *out = apop_data_alloc(); out->error='n'; return out, 0, "The data input can't be NULL.");
    gsl_rng * apop_varad_var(rng, NULL);
    if (!rng && !spare) 
        spare = apop_rng_alloc(++apop_opts.rng_seed);
    if (!rng)  rng = spare;
    char apop_varad_var(keep_boots, 'n');
    char apop_varad_var(ignore_nans, 'n');
APOP_VAR_END_HEAD
    Get_vmsizes(data); //vsize, msize1, msize2
    apop_model *e       = apop_model_copy(model);
    apop_data  *subset  = apop_data_copy(data);
    apop_data  *array_of_boots = NULL,
               *summary;
    //prevent and infinite regression of covariance calculation.
    Apop_model_add_group(e, apop_parts_wanted); //default wants for nothing.
    size_t	   i, nan_draws=0;
    apop_name *tmpnames = data->names; //save on some copying below.
    data->names = NULL;  

    int height = GSL_MAX(msize1, GSL_MAX(vsize, data->textsize[0]));
	for (i=0; i<iterations && nan_draws < iterations; i++){
		for (size_t j=0; j< height; j++){       //create the data set
			size_t row	= gsl_rng_uniform_int(rng, height);
			Apop_data_row(data, row, random_data_row);
			Apop_data_row(subset, j, subset_row_j);
            apop_data_memcpy(subset_row_j, random_data_row);
		}
		//get the parameter estimates.
		apop_model *est = apop_estimate(subset, *e);
        gsl_vector *estp = apop_data_pack(est->parameters);
        if (!gsl_isnan(apop_sum(estp))){
            if (i==0){
                array_of_boots	      = apop_data_alloc(iterations, estp->size);
                apop_name_stack(array_of_boots->names, est->parameters->names, 'c', 'v');
                apop_name_stack(array_of_boots->names, est->parameters->names, 'c', 'c');
                apop_name_stack(array_of_boots->names, est->parameters->names, 'c', 'r');
            }
            gsl_matrix_set_row(array_of_boots->matrix, i, estp);
        } else {
            i--; 
            nan_draws++;
        }
        apop_model_free(est);
        gsl_vector_free(estp);
	}
    data->names = tmpnames;
    apop_data_free(subset);
    apop_model_free(e);
    if (nan_draws == iterations){
        Apop_notify(1, "I ran into %i NaNs, and so stopped. Returning results based "
                       "on %zu bootstrap iterations.", iterations, i);
        apop_matrix_realloc(array_of_boots->matrix, i, array_of_boots->matrix->size2);
    }
	summary	= apop_data_covariance(array_of_boots);
    gsl_matrix_scale(summary->matrix, 1./i);
    if (keep_boots == 'n' || keep_boots == 'N')
        apop_data_free(array_of_boots);
    else
        apop_data_add_page(summary, array_of_boots, "<Bootstrapped statistics>");
	return summary;
}
