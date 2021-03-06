/* The binomial distribution as an \c apop_model.
Copyright (c) 2006--2007, 2010--11 by Ben Klemens.  Licensed under the modified GNU GPL v2; see COPYING and COPYING2. 

 \amodel apop_binomial The multi-draw generalization of the Bernoulli; the two-bin special case of the \ref apop_multinomial "Multinomial distribution".
This differs from the \ref apop_multinomial only in the input data format.

It is implemented as an alias of the \ref apop_multinomial model, except that it has
a CDF, <tt>.vbase==2</tt> and <tt>.dsize==1</tt> (i.e., we know it has two parameters
and a draw returns a scalar).

\adoc    Parameter_format   a vector, v[0]=\f$n\f$; v[1]=\f$p_1\f$. Thus, \f$p_0\f$
        isn't written down; see \ref apop_multinomial for further discussion.
        If you input $v[1]>1$ and <tt>apop_opts.verbose >=1</tt>, the log likelihood
        function will throw a warning.

\adoc    Input_format Each row of the matrix is one observation, consisting of two elements.
  The number of draws of type zero (sometimes read as `misses' or `failures') are in column zero, 
  the number of draws of type one (`hits', `successes') in column one.

\adoc    RNG The RNG returns a single number representing the success count, not a
    vector of length two giving both the failure bin and success bin. This is notable
    because it differs from the input data format, but it tends to be what people expect
    from a Binomial RNG. For draws with both dimensions, use a \ref apop_multinomial model
    with <tt>.vbase =2</tt>.
*/

#include "apop_internal.h"

/* \adoc cdf At the moment, only implemented for the Binomial.
  Let the first element of the data set (top of the vector or point (0,0) in the
  matrix, your pick) be $L$; then I return the sum of the odds of a draw from the given
  Binomial distribution returning $0, 1, \dots, L$ hits.  */
static double binomial_cdf(apop_data *d, apop_model *est){
    Nullcheck_mpd(d, est, GSL_NAN)
    Get_vmsizes(d); //firstcol
    double hitcount = apop_data_get(d, .col=firstcol);
    double n = gsl_vector_get(est->parameters->vector, 0);
    double p = gsl_vector_get(est->parameters->vector, 1);
    return gsl_cdf_binomial_P(hitcount, p, n);
}

static void make_covar(apop_model *est){
    int size = est->parameters->vector->size;
    //the trick where we turn the params into a p-vector
    double * pv = est->parameters->vector->data;
    int n = pv[0];
    pv[0] = 1 - (apop_sum(est->parameters->vector)-n);

    apop_data *cov = apop_data_add_page(est->parameters, 
                            apop_data_alloc(size, size), "<Covariance>");
    for (int i=0; i < size; i++){
        double p = apop_data_get(est->parameters, i, -1);
        apop_data_set(cov, i, i, n * p *(1-p));
        for (int j=i+1; j < size; j++){
            double pj = apop_data_get(est->parameters, j, -1);
            double thiscell = -n*p*pj;
            apop_data_set(cov, i, j, thiscell);
            apop_data_set(cov, j, i, thiscell);
        }
    }
    pv[0]=n;
}

static double multinomial_constraint(apop_data *data, apop_model *b){
  //constraint is that 0 < all elmts 
    return apop_linear_constraint(b->parameters->vector, .margin = 1e-3);
}

static double binomial_ll(gsl_vector *hits, void *paramv){
    return log(gsl_ran_binomial_pdf(hits->data[1], ((gsl_vector*)paramv)->data[1], ((gsl_vector*)paramv)->data[0]));
}

double multinomial_ll(gsl_vector *v, void *params){
    double *pv = ((apop_model*)params)->parameters->vector->data;
    size_t size = ((apop_model*)params)->parameters->vector->size;
    unsigned int hv[v->size]; //The GSL wants our hit count in an int*.
    for (size_t i=0; i < v->size; i ++)
        hv[i] = gsl_vector_get(v, i);
    return gsl_ran_multinomial_lnpdf(size, pv, hv);
}

static double multinomial_log_likelihood(apop_data *d, apop_model *params){
    Nullcheck_mpd(d, params, GSL_NAN);
    double *pv = params->parameters->vector->data;
    double n = pv[0]; 
    Apop_assert_c(pv[1] <=1, GSL_NAN, 1, "The input parameters should be [n, p_1, (...)], but "
        "element 1 of the parameter vector is >1.") //mostly makes sense for the binomial.
    if (n==2)
        return apop_map_sum(d, .fn_vp=binomial_ll, .param=params->parameters->vector);

    pv[0] = 1 - (apop_sum(params->parameters->vector)-n);//making the params a p-vector. Put n back at the end.
    double out = apop_map_sum(d, .fn_vp=multinomial_ll, .param=params);
    pv[0]=n;
    return out;
}

/*
static apop_model *multinomial_paramdist(apop_data *d, apop_model *m){
    apop_pm_settings *settings = Apop_settings_get_group(m, apop_pm);
    if (settings->index!=-1){
        int i = settings->index;
        double mu = apop_data_get(m->parameters, i, -1);
        double sigma = sqrt(apop_data_get(m->parameters, i, i, .page="<Covariance>"));
        int df = apop_data_get(m->info, .rowname="df", .page = "info");
        return apop_model_set_parameters(apop_t_distribution, mu, sigma, df);
    }

}
*/

static void multinomial_rng(double *out, gsl_rng *r, apop_model* est){
    Nullcheck_mp(est, );
    double * p = est->parameters->vector->data;
    //the trick where we turn the params into a p-vector
    int N = p[0];

    if (est->parameters->vector->size == 2) {
        *out = gsl_ran_binomial_knuth(r, gsl_vector_get(est->parameters->vector, 1), N);
        return;
    }
    //else, multinomial
    //cut/pasted/modded from the GSL. Copyright them.
    p[0] = 1 - (apop_sum(est->parameters->vector)-N);
    double sum_p = 0.0;
    int draw, ctr = 0;
    unsigned int sum_n = 0;

    for (int i = 0; sum_n < N; i++) {
        if (p[i] > 0)
            draw = gsl_ran_binomial (r, p[i] / (1 - sum_p), N - sum_n);
        else
            draw = 0;
        for (int j=0; j< draw; j++)
            out[ctr++] = i;
        sum_p += p[i];
        sum_n += draw;
    }
    p[0] = N;
}

static void multinomial_show(apop_model *est){
    double * p = est->parameters->vector->data;
    int N=p[0];
    p[0] = 1 - (apop_sum(est->parameters->vector)-N);
    fprintf(apop_opts.output_pipe, "%s, with %i draws.\nBin odds:\n", est->name, N);
    apop_vector_print(est->parameters->vector, .output_pipe=apop_opts.output_pipe);
    p[0]=N;
}

double avs(gsl_vector *v){return (double) apop_vector_sum(v);}

/* \amodel apop_multinomial The \f$n\f$--option generalization of the \ref apop_binomial "Binomial distribution".

\adoc estimated_parameters  As per the parameter format. Has a <tt>\<Covariance\></tt> page with the covariance matrix for the \f$p\f$s (\f$n\f$ effectively has no variance).  */
/* \adoc estimated_info   Reports <tt>log likelihood</tt>. */
static apop_model * multinomial_estimate(apop_data * data,  apop_model *est){
    Nullcheck_mpd(data, est, NULL);
    Get_vmsizes(data); //vsize, msize1
    est->parameters= apop_map(data, .fn_v=avs, .part='c');
    gsl_vector *v = est->parameters->vector;
    int n = apop_sum(v)/data->matrix->size1; //size of one row
    apop_vector_normalize(v);
    gsl_vector_set(v, 0, n);
    apop_name_add(est->parameters->names, "n", 'r');
    char name[100];
    for(int i=1; i < v->size; i ++){
        sprintf(name, "p%i", i);
        apop_name_add(est->parameters->names, name, 'r');
    }
    est->dsize = data->matrix->size1;
    make_covar(est);
    apop_data_add_named_elmt(est->info, "log likelihood", multinomial_log_likelihood(data, est));
    return est;
}


/* \adoc    Input_format Each row of the matrix is one observation: a set of draws from a single bin.
  The number of draws of type zero are in column zero, the number of draws of type one in column one, et cetera.

\li You may have a set of several Bernoulli-type draws, which could be summed together to form a single Binomial draw.
See \ref apop_data_rank_compress to do the aggregation.

\adoc    Parameter_format
        The parameters are kept in the vector element of the \c apop_model parameters element. \c parameters->vector->data[0]==n;
        \c parameters->vector->data[1...]==p_1....

The numeraire is bin zero, meaning that \f$p_0\f$ is not explicitly listed, but is
\f$p_0=1-\sum_{i=1}^{k-1} p_i\f$, where \f$k\f$ is the number of bins. Conveniently enough,
the zeroth element of the parameters vector holds \f$n\f$, and so a full probability vector can
easily be produced by overwriting that first element. Continuing the above example: 
\code 
int n = apop_data_get(estimated->parameters, 0, -1); 
apop_data_set(estimated->parameters, 0, 1 - (apop_sum(estimated->parameters)-n)); 
\endcode
And now the parameter vector is a proper list of probabilities.

\li Because an observation is typically a single row, the value of \f$N\f$ is set to equal the length of
the first row (counting both vector and matrix elements, as appropriate). Thus, if your
data is entirely in the vector or a one-column matrix, then the \f$p\f$s are estimated
using all data, but \f$N=1\f$. The covariances are calculated accordingly, and a random
draw would return a single bin. 

\adoc    Estimate_results  Parameters are estimated. Covariance matrix is filled.   
\adoc    RNG Returns a single vector of length \f$k\f$, the result of an imaginary tossing of \f$N\f$ balls into \f$k\f$ urns, with the
            given probabilities.
            */

apop_model apop_multinomial = {"Multinomial distribution", -1,0,0, .dsize=-1,
	.estimate = multinomial_estimate, .log_likelihood = multinomial_log_likelihood, 
   .constraint = multinomial_constraint, .draw = multinomial_rng, .print=multinomial_show};

apop_model apop_binomial = {"Binomial distribution", 2,0,0, .dsize=1,
	.estimate = multinomial_estimate, .log_likelihood = multinomial_log_likelihood, 
   .constraint = multinomial_constraint, .draw = multinomial_rng, .print=multinomial_show, .cdf= binomial_cdf};
