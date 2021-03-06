/** \file apop_conversions.c	The various functions to convert from one format to another. */
/* Copyright (c) 2006--2010, 2012 by Ben Klemens.  Licensed under the modified GNU GPL v2; see COPYING and COPYING2.  */
#include "apop_internal.h"
#include <gsl/gsl_math.h> //GSL_NAN
#include <regex.h>
#include <assert.h>
#include <sqlite3.h>

/*extend a string. this prevents a minor leak you'd get if you did
 asprintf(&q, "%s is a teapot.", q);
 q may be NULL, which prints the string "null", so use the little XN macro below when using this function.

 This is internal to apop. right now. 
*/
void xprintf(char **q, char *format, ...){ 
    va_list ap; 
    char *r = *q; 
    va_start(ap, format); 
    vasprintf(q, format, ap);
    va_end(ap);
    free(r);
}

/** \defgroup conversions Conversion functions
The functions to shunt data between text files, database tables, GSL matrices, and plain old arrays.*/

/** Converts a \c gsl_vector to an array.

\param in A \c gsl_vector
\return A \c double*, which will be <tt>malloc</tt>ed inside the function.

\li If you send in a \c NULL vector, you get a \c NULL pointer in return. I warn you of this if <tt>apop_opts.verbosity >=1 </tt>.

\ingroup conversions
*/
double * apop_vector_to_array(const gsl_vector *in){
//Does not use memcpy, because we don't know the stride of the vector.
    Apop_assert_c(in, NULL, 1, "You sent me a NULL vector; returning NULL");
    double *out = malloc(sizeof(double) * in->size);
	for (size_t i=0; i < in->size; i++)
		out[i]	= gsl_vector_get(in, i);
	return out;
}

/** Just copies a one-dimensional array to a <tt>gsl_vector</tt>. The input array is undisturbed.

\param in     An array of <tt>double</tt>s. (No default. Must not be \c NULL);
\param size 	How long \c line is. If this is zero or omitted, I'll
guess using the <tt>sizeof(line)/sizeof(line[0])</tt> trick, which will
work for most arrays allocated using <tt>double []</tt> and won't work
for those allocated using <tt>double *</tt>. (default = auto-guess)
\return         A <tt>gsl_vector</tt> (which I will allocate for you).

\li If you send in a \c NULL vector, you get a \c NULL pointer in return. I warn you of this if <tt>apop_opts.verbosity >=1 </tt>.

\ingroup conversions
This function uses the \ref designated syntax for inputs.

*/ 
APOP_VAR_HEAD gsl_vector * apop_array_to_vector(double *in, int size){
    double * apop_varad_var(in, NULL);
    Apop_assert_c(in, NULL, 1, "You sent me NULL data; returning NULL.");
    int apop_varad_var(size, sizeof(in)/sizeof(in[0]));
APOP_VAR_END_HEAD
    gsl_vector      *out = gsl_vector_alloc(size);
    gsl_vector_view	v	 = gsl_vector_view_array((double*)in, size);
	gsl_vector_memcpy(out,&(v.vector));
    return out;
}

/** Mathematically, a vector of size \f$N\f$ and a matrix of size \f$N \times 1 \f$ are equivalent, but they're two different types in C. This function copies the data in a vector to a new one-column (or one-row) matrix and returns the newly-allocated and filled matrix.

  For the reverse, try \ref apop_data_pack.

\param in a \c gsl_vector (No default. If \c NULL, I return \c NULL, with a warning if <tt>apop_opts.verbose >=1 </tt>)
\param row_col If \c 'r', then this will be a row (1 x N) instead of the default, a column (N x 1). (default: \c 'c')
\return a newly-allocated <tt>gsl_matrix</tt> with one column.

\li If you send in a \c NULL vector, you get a \c NULL pointer in return. I warn you of this if <tt>apop_opts.verbosity >=1 </tt>.
\li If \c gsl_matrix_alloc fails and <tt>apop_opts.stop_on_warn=='n'</tt>, you get a \c NULL pointer in return.

This function uses the \ref designated syntax for inputs.
 \ingroup conversions
 */
APOP_VAR_HEAD gsl_matrix * apop_vector_to_matrix(const gsl_vector *in, char row_col){
    const gsl_vector * apop_varad_var(in, NULL);
    Apop_assert_c(in,  NULL, 1, "Converting NULL vector to NULL matrix.");
    char apop_varad_var(row_col, 'c');
APOP_VAR_ENDHEAD
    gsl_matrix *out = 
        (row_col == 'r' || row_col == 'R') 
           ? gsl_matrix_alloc(1, in->size)
           : gsl_matrix_alloc(in->size, 1);
    Apop_assert(out,  "gsl_matrix_alloc failed; probably out of memory.");
    if (row_col == 'r' || row_col == 'R') 
        gsl_matrix_set_row(out, 0, in);
    else
        gsl_matrix_set_col(out, 0, in);
    return out;
}

static void convert_array_to_line(const double **in, double **out, const int rows, const int cols){
	//go from in[i][j] form to the GSL's preferred out[i*cols + j] form
	*out = malloc(sizeof(double) * rows * cols);
	for (size_t i=0; i<rows; i++)
		for (size_t j=0; j<cols; j++)
			(*out)[i * cols + j]	= in[i][j];
}

/** Convert a <tt>double **</tt> array to a <tt>gsl_matrix</tt>

\param in	the array to read in
\param rows, cols	the size of the array.
\return the <tt>gsl_matrix</tt>, allocated for you and ready to use.

usage: \code gsl_matrix *m = apop_array_to_matrix(indata, 34, 4); \endcode

If you want to initialize on the allocation line, this isn't what you want. See \ref apop_line_to_matrix.
\ingroup conversions
*/
gsl_matrix * apop_array_to_matrix(const double **in, const int rows, const int cols){
    double *line;
    gsl_matrix  *out = gsl_matrix_alloc(rows, cols);
	convert_array_to_line(in, &line, rows, cols);
    gsl_matrix_view m = gsl_matrix_view_array(line, rows,cols);
	gsl_matrix_memcpy(out,&(m.matrix));
	free(line);
    return out;
}

/** Convert a <tt>double **</tt> array to an \ref apop_data set. It will
have no names. Input data is copied.

\param in	the array to read in
\param rows, cols	the size of the array.
\return the \ref apop_data set, allocated for you and ready to use.

usage: \code apop_data *d = apop_array_to_data(indata, 34, 4); \endcode
\ingroup conversions

If you want to initialize on the allocation line, this isn't what you want. See \ref apop_line_to_data.
*/
apop_data * apop_array_to_data(const double **in, const int rows, const int cols){
    return apop_matrix_to_data(apop_array_to_matrix(in, rows, cols));
}

/** Convert a <tt>double *</tt> array to a <tt>gsl_matrix</tt>. Input data is copied.

\param line	the array to read in
\param rows, cols	the size of the array.
\return the <tt>gsl_matrix</tt>, allocated for you and ready to use.

usage: \code gsl_matrix *m = apop_line_to_matrix(indata, 34, 4); \endcode
\see apop_arrary_to_matrix
\ingroup conversions
*/
gsl_matrix * apop_line_to_matrix(double *line, int rows, int cols){
    gsl_matrix *out= gsl_matrix_alloc(rows, cols);
    gsl_matrix_view	m = gsl_matrix_view_array(line, rows,cols);
	gsl_matrix_memcpy(out, &(m.matrix));
    return out;
}

/** A convenience function to convert a <tt>double *</tt> array to an \ref apop_data set. It will
have no names. The input data is copied, not pointed to.

See also \ref apop_line_to_matrix or \ref apop_array_to_vector; this function will use these and then wrap an \ref apop_data struct around the output(s).

\param in	The array to read in. If there were appropriately placed line breaks, then this would look like the eventual data set. For example,
\code
double params[] = {0, 1, 2
                   3, 4, 5};
apop_data *out = apop_line_to_data(params, 2, 2, 2);
\endcode
will produce an \ref apop_data set with a vector \f$\left[\matrix{0 \cr 3}\right]\f$ and a matrix \f$\left[\matrix{1 & 2 \cr 4 & 5}\right]\f$.
\param vsize    The vector size. If there are also rows/cols, I expect this to equal the number or rows.
\param rows, cols	the size of the array.
\return the \ref apop_data set, allocated for you and ready to use.
\exception out->error=='d' Dimension error: vector and matrix heights have to be the same.
\ingroup conversions
*/
apop_data * apop_line_to_data(double *in, int vsize, int rows, int cols){
    if (vsize==0 && (rows>0 && cols>0))
        return apop_matrix_to_data(apop_line_to_matrix(in, rows, cols));
    if ((rows==0 || cols==0) && vsize>0)
        return apop_vector_to_data(apop_array_to_vector(in, vsize));
    Apop_stopif(vsize!=rows, apop_data *out=apop_data_alloc(); out->error='d'; return out, 
            0, "apop_line_to_data expects either only a matrix, only a vector, or that matrix "
            "row count and vector size are equal. You gave me a row size of %i and a vector "
            "size of %i. Returning NULL.\n", rows, vsize);
  return apop_data_fill_base(apop_data_alloc(vsize, rows, cols), in);
}

static int find_cat_index(char **d, char * r, int start_from, int size){
//used for apop_db_to_crosstab.
    int i = start_from % size;	//i is probably the same or i+1.
	do {
		if(!strcmp(d[i], r)) return i;
		i++;
		i %= size;	//loop around as necessary.
	} while (i!=start_from); 
    Apop_assert_c(0, -2, 0, "Something went wrong in the crosstabbing; couldn't find %s.", r);
}

/**Give the name of a table in the database, and names of three of its
columns: the x-dimension, the y-dimension, and the data.
the output is a 2D matrix with rows indexed by r1 and cols by
r2.

\param tabname The database table I'm querying. Anything that will work inside a \c from clause is OK, such as a subquery in parens.
\param r1 The column of the data set that will indicate the rows of the output crosstab
\param r2 The column of the data set that will indicate the columns of the output crosstab
\param datacol The column of the data set holding the data for the cells of the crosstab

\li  If the query to get data to fill the table (select r1, r2, datacol from tabname) returns an empty data set, then I will return a \c NULL data set and if <tt>apop_opts.verbosity >= 1</tt> print a warning.

\li This setup presumes that there is one value for each (row, col) coordinate in the data. You may want an aggregate instead. There are two ways to do this, both of which hack the fact that this function runs a simple \c select query to generate the data. One is to specify an ad hoc table to pull from:

\code
apop_data * out = apop_db_to_crosstab("(select row, col, count(*) ct from base_data group by row, col)", "row", "col",  "ct");
\endcode

The other is to use the fact that the table name will be at the end of the query, so you can add conditions to the table:

\code
apop_data * out = apop_db_to_crosstab("base_data group by row, col", "row", "col", "count(*)");
//which will expand to "select row, col, count(*) from base_data group by row, col"
\endcode

\see \ref apop_crosstab_to_db

\exception out->error='n' Name not found error.
\exception out->error='q' Query returned an empty table (which might mean that it just failed).

\ingroup db
*/
apop_data *apop_db_to_crosstab(char *tabname, char *r1, char *r2, char *datacol){
    gsl_matrix *out=NULL;
    int	i, j=0;
    apop_data *pre_d1=NULL, *pre_d2=NULL, *datachars=NULL;
    apop_data *outdata = apop_data_alloc();

    char p = apop_opts.db_name_column[0];
    apop_opts.db_name_column[0]= '\0';//we put this back at the end.
    datachars = apop_query_to_text("select %s, %s, %s from %s", r1, r2, datacol, tabname);
    Apop_stopif(!datachars, return NULL, 1, "selecting %s, %s, %s from %s returned an empty table.",  r1, r2, datacol, tabname);
    Apop_stopif(datachars->error, goto bailout, 0, "error selecting %s, %s, %s from %s.",  r1, r2, datacol, tabname);

    //A bit inefficient, but well-encapsulated.
    //Pull the distinct (sorted) list of headers, copy into outdata->names.
    pre_d1 = apop_query_to_text("select distinct %s, 1 from %s order by %s", r1, tabname, r1);
    Apop_stopif(!pre_d1||pre_d1->error, outdata->error='q'; goto bailout, 0, "Error querying %s from %s.", r1, tabname);
    for (i=0; i < pre_d1->textsize[0]; i++)
        apop_name_add(outdata->names, pre_d1->text[i][0], 'r');

	pre_d2 = apop_query_to_text("select distinct %s from %s order by %s", r2, tabname, r2);
    Apop_stopif(!pre_d2||pre_d2->error, outdata->error='q'; goto bailout, 0, "Error querying %s from %s.", r1, tabname);
    for (i=0; i < pre_d2->textsize[0]; i++)
        apop_name_add(outdata->names, pre_d2->text[i][0], 'c');

	out	= gsl_matrix_calloc(pre_d1->textsize[0], pre_d2->textsize[0]);
	for (size_t k =0; k< datachars->textsize[0]; k++){
		i = find_cat_index(outdata->names->row, datachars->text[k][0], i, pre_d1->textsize[0]);
		j = find_cat_index(outdata->names->column, datachars->text[k][1], j, pre_d2->textsize[0]);
        Apop_stopif(i==-2 || j == -2, outdata->error='n'; goto bailout, 0, "Something went wrong in the crosstabbing; "
                                                 "couldn't find %s or %s.", datachars->text[k][0], datachars->text[k][1]);
		gsl_matrix_set(out, i, j, atof(datachars->text[k][2]));
	}
    bailout:
    apop_data_free(pre_d1);
    apop_data_free(pre_d2);
    apop_data_free(datachars);
    outdata->matrix = out;
    apop_opts.db_name_column[0]= p;
	return outdata;
}

/** See \ref apop_db_to_crosstab for the storyline; this is the complement, which takes a
  crosstab and writes its values to the database.

For example, I would take
<table frame=box>                                                                                                              
<tr>                                                                                                                           
<td> </td><td> c0</td><td>c1</td>
</tr><tr valign=bottom>
<td align=center> </td></tr> 
<tr><td>r0</td><td>2</td><td>3</td></tr> 
<tr><td>r1</td><td>0</td><td>4</td></tr> 
</table> 

and do the following writes to the database:

\code
insert into your_table values ('r0', 'c0', 2);
insert into your_table values ('r0', 'c1', 3);
insert into your_table values ('r1', 'c0', 3);
insert into your_table values ('r1', 'c1', 4);
\endcode


\li If your data set does not have names (or not enough names), I will use the scheme above, filling in names of the form <tt>r0</tt>, <tt>r1</tt>, ... <tt>c0</tt>, <tt>c1</tt>, .... Text columns get their own numbering system, <tt>t0</tt>, <tt>t1</tt>, ..., which is a little more robust than continuing the column count from the matrix.

\li I handle only the matrix and text. 
 \ingroup db
 */
void apop_crosstab_to_db(apop_data *in,  char *tabname, char *row_col_name, 
						char *col_col_name, char *data_col_name){
    apop_name *n = in->names;
    char *colname, *rowname;
    Get_vmsizes(in); //msize1, msize2
    int maxcol= GSL_MAX(msize2, in->textsize[1]);
    char sparerow[msize1 > 0 ? (int)log10(msize1)+1 : 0];
    char sparecol[maxcol > 0 ? (int)log10(maxcol)+1 : 0];
	apop_query("CREATE TABLE %s (%s , %s , %s);", tabname, 
            apop_strip_dots(row_col_name, 'd'), 
            apop_strip_dots(col_col_name, 'd'), 
            apop_strip_dots(data_col_name, 'd'));
	apop_query("begin;");
    for (int i=0; i< msize1; i++){
        rowname = (n->rowct > i) ?  n->row[i] : (sprintf(sparerow, "r%i", i), sparerow);
        for (int j=0; j< msize2; j++){
            colname = (n->colct > j) ? n->column[j] : (sprintf(sparecol, "c%i", j), sparecol);
            double x = gsl_matrix_get(in->matrix, i, j); 
            if (!isnan(x)) apop_query("INSERT INTO %s VALUES ('%s', '%s', %g);", 
                                                tabname, rowname, colname, x);
            else apop_query("INSERT INTO %s VALUES ('%s', '%s', 0/0);", 
                                        tabname, rowname, colname);
        }
    }
    for (int i=0; i< in->textsize[0]; i++){
        rowname = (n->rowct > i) ? n->row[i] : (sprintf(sparerow, "r%i", i), sparerow);
        for (int j=0; j< in->textsize[1]; j++){
            colname = (n->textct > j) ? n->text[j] : (sprintf(sparecol, "t%i", j), sparecol);
            apop_query("INSERT INTO %s VALUES ('%s', '%s', '%s');", tabname, 
                rowname, colname, in->text[i][j]);
        }
    }
	apop_query("commit;");
}


/** One often finds data where the column indicates the value of the data point. There may
be two columns, and a mark in the first indicates a miss while a mark in the second is a
hit. Or say that we have the following list of observations:

\code
2 3 3 2 1 1 2 1 1 2 1 1
\endcode
Then we could write this as:
\code
0  1  2  3
----------
0  6  4  2
\endcode
because there are six 1s observed, four 2s observed, and two 3s observed. We call this
rank format, because 1 (or zero) is typically the most common, 2 is second most common, et cetera.

This function takes in a list of observations, and aggregates them into a single row in rank format.

\li For the complement, see \ref apop_data_rank_expand.

\li You may be interested in \ref apop_data_to_factors to convert real numbers or text into a
matrix of categories.

\li The number of bins is simply the largest number found. So if there
are bins {0, 1, 2} and your data set happens to consist of <tt>0 0 1 1 0</tt>, then
I won't know to generate results with three bins where the last bin has probability zero.

\include test_ranks.c
*/
apop_data *apop_data_rank_compress (apop_data *in){
    Get_vmsizes(in);
    int upper_bound = GSL_MAX(in->matrix ? gsl_matrix_max(in->matrix) : 0, 
                              in->vector ? gsl_vector_max(in->vector) : 0);
    apop_data *out = apop_data_calloc(1, upper_bound+1);
    for (int i=0; i< msize1; i++)
        for (int j=0; j< msize2; j++)
            apop_matrix_increment(out->matrix, 0, apop_data_get(in, i, j));
    for (int i=0; i< vsize; i++)
        apop_matrix_increment(out->matrix, 0, apop_data_get(in, i, -1));
    return out;
}

/** The complement to this is \ref apop_data_rank_compress; see that function's
  documentation for the story and an example.

  This function takes in a data set where the zeroth column includes the count(s)
  of times that zero was observed, the first gives the count(s) of times that one was
  observed, et cetera. It outputs a data set whose vector element includes a list that
  has exactly the given frequency of zeros, ones, et cetera.
*/
apop_data *apop_data_rank_expand (apop_data *in){
    int total_ct = in->matrix ? apop_matrix_sum(in->matrix) : 0
                 + in->vector ? apop_vector_sum(in->vector) : 0;
    if (total_ct == 0)
        return NULL;
    apop_data *out = apop_data_alloc(total_ct);
    int posn = 0;
    for (int i=0; i< in->matrix->size1; i++)
        for (int k=0; k< in->matrix->size2; k++)
            for (int j=0; j< gsl_matrix_get(in->matrix, i, k); j++)
                gsl_vector_set(out->vector, posn++, k); 
    return out;
}

/** \page dbtomatrix Converting from database table to <tt>gsl_matrix</tt> or \ref apop_data

Use <tt>fill_me = apop_query_to_matrix("select * from table_name;");</tt>
or <tt>fill_me = apop_query_to_data("select * from table_name;");</tt>. [See \ref apop_query_to_matrix; \ref apop_query_to_data.]
\ingroup conversions
*/


/** Copy one  <tt>gsl_vector</tt> to another. That is, all data is duplicated.
 Unlike <tt>gsl_vector_memcpy</tt>, this function allocates and returns the destination, so you can use it like this:

 \code
 gsl_vector *a_copy = apop_vector_copy(original);
 \endcode

  \param in    the input data
  \return       a structure that this function will allocate and fill. If \c gsl_vector_alloc fails, returns \c NULL.
\ingroup convenience_fns
  */
gsl_vector *apop_vector_copy(const gsl_vector *in){
    if (!in) return NULL;
    gsl_vector *out = gsl_vector_alloc(in->size);
    Apop_stopif(!out, return NULL, 0, "failed to allocate a gsl_vector of size %zu. Out of memory?", in->size);
    gsl_vector_memcpy(out, in);
    return out;
}

/** Copy one  <tt>gsl_matrix</tt> to another. That is, all data is duplicated.
Unlike <tt>gsl_matrix_memcpy</tt>, this function allocates and returns the destination, so you can use it like this:

\code
gsl_matrix *a_copy = apop_matrix_copy(original);
\endcode

\param in  the input data
\return    a structure that this function will allocate and fill. If \c gsl_matrix_alloc fails, returns \c NULL.
\ingroup convenience_fns
  */
gsl_matrix *apop_matrix_copy(const gsl_matrix *in){
    if (!in) return NULL;
    gsl_matrix *out = gsl_matrix_alloc(in->size1, in->size2);
    Apop_stopif(!out, return NULL, 0, "failed to allocate a gsl_matrix of size %zu x %zu. Out of memory?", in->size1, in->size2);
    gsl_matrix_memcpy(out, in);
    return out;
}


///////////////The text processing section
static int  use_names_in_file;

/** \page text_format Notes on input text file formatting

Each row of the file will be converted to one record in the database or one row in the matrix. Values on one row are separated by delimiters. Fixed-width input is also OK; see below.

By default, the delimiters are set to "|,\t", meaning that a pipe, comma, or tab
will delimit separate entries.  To change the default, please use an argument to
\ref apop_text_to_db or \ref apop_text_to_data like <tt>.delimiters=" \t"</tt> or
<tt>.delimiters="|"</tt>. \c apop_opts.input_delimiters is deprecated.

The input text file must be UTF-8 or traditional ASCII encoding. Delimiters must be ASCII characters. 
If your data is in another encoding, try the POSIX-standard \c iconv program to filter the data to UTF-8.

\li The character after a backslash is read as a normal character, even if it is a delimiter, \c #, \c ', or \c ".

\li If a field contains several such special characters, surround it by \c 's or \c "s. The surrounding marks are stripped and the text read verbatim.

\li Text does not need to be delimited by quotes (unless there are special characters). If a text field is quote-delimited, I'll strip them.
E.g., "Males, 30-40", is an OK column name, as is "Males named \\"Joe\\"".

\li Everything after a # is taken to be comments and ignored. 

\li Blank lines (empty or consisting only of white space) are also ignored.

\li If you are reading into an array or <tt>gsl_matrix</tt> or \ref apop_data set, all text fields are taken as zeros. You will be warned of such substitutions unless you set \code apop_opts.verbose==0\endcode beforehand.

\li There are often two delimiters in a row, e.g., "23, 32,, 12". When it's two commas
like this, the user typically means that there is a missing value and the system should
insert an NAN; when it is two tabs in a row, this is typically just a formatting
glitch. Thus, if there are multiple delimiters in a row, I check whether the second
(and subsequent) is a space or a tab; if it is, then it is ignored, and if it is any
other delimiter (including the end of the line) then a NaN is inserted.

If this rule doesn't work for your situation, you can explicitly insert a note that there is a missing data
point. E.g., try: \code
		perl -pi.bak -e 's/,,/,NaN,/g' data_file
\endcode

If you have missing data delimiters, you will need to set \ref apop_opts_type
"apop_opts.db_nan" to text that matches the given format. E.g.,

\code
//Apophenia's default NaN string, matching NaN, nan, or NAN, but not Nancy:
strcpy(apop_opts.db_nan, "NaN");
strcpy(apop_opts.db_nan, "Missing");
strcpy(apop_opts.db_nan, ".");
\endcode

SQLite stores these NaN-type values internally as \c NULL; that means that functions like
\ref apop_query_to_data will convert both your db_nan string and \c NULL to an \c NaN value.

\li The system uses the standards for C's \c atof() function for
floating-point numbers: INFINITY, -INFINITY, and NaN work as expected.
I use some tricks to get SQLite to accept these values, but they work.

\li If there are row names and column names, then the input will not be perfectly square: there should be no first entry in the row with column names like 'row names'. That is, for a 100x100 data set with row and column names, there are 100 names in the top row, and 101 entries in each subsequent row (name plus 100 data points).

\li White space before or after a field is ignored. So <tt>1, 2,3, 4 , 5, " six ",7 </tt>
is eqivalent to <tt>1,2,3,4,5," six ",7</tt>.

\li NUL characters are treated as white space, so if your fields have NULs as padding, you should have no problem. NULs inside of a string will probably break.

\li Fixed-width formats are supported (for plain ASCII encoding only), but you have to provide a list of field ending positions. For example, given
\code
NUMLEOL
123AABB
456CCDD
\endcode
we have three columns, named NUM, LE, and OL. The names can be read from the first row if you so specify. You will have to provide a list of integers giving the end of each field: 3, 5, 7.
*/

static int prep_text_reading(char const *text_file, FILE **infile){
    *infile = !strcmp(text_file, "-")
                    ? stdin
	                : fopen(text_file, "r");
    Apop_assert_c(*infile, 1,  0, "Trouble opening %s. Returning NULL.", text_file);
    return 0;
}

/////New text file reading

typedef struct {int ct; int eof;} line_parse_t;

static line_parse_t parse_a_fixed_line(FILE *infile, apop_data *fn, int const *field_ends){
    char c = fgetc(infile);
    int ct = 0, posn=0, thisflen=0, needfield=1;
    while(c!='\n' && c !=EOF){
        posn++;
        if (needfield){//start a new field
            if (++ct > fn->textsize[0])
                apop_text_alloc(fn, ct, 1);//realloc text portion.
            thisflen = 
            needfield = 0;
        }

        //extend field:
        *fn->text[ct-1] = realloc(*fn->text[ct-1], ++thisflen);
        fn->text[ct-1][0][thisflen-1] = c;

        if (posn==*field_ends){ //close off this field.
            *fn->text[ct-1] = realloc(*fn->text[ct-1], thisflen+1);
            fn->text[ct-1][0][thisflen] = '\0';
            thisflen = 0;
            field_ends++;
            needfield=1;
        } 
        c = fgetc(infile);
    }
    if (needfield==0){//user didn't give last field end.
        *fn->text[ct-1] = realloc(*fn->text[ct-1], thisflen+1);
        fn->text[ct-1][0][thisflen] = '\0';
    }
    return (line_parse_t) {.ct=ct, .eof= (c == EOF)};
}

typedef struct{
    char c, type;
} apop_char_info;

static apop_char_info parse_next_char(FILE *f, char const *delimiters){
    char c = fgetc(f);
    int is_delimiter = !!strchr(delimiters, c);
    return (apop_char_info){.c=c, 
            .type = (c==' '||c=='\t' || c==0)? (is_delimiter ? 'W'  : 'w')
                    :is_delimiter    ? 'd'
                    :(c == '\n')     ? 'n'
                    :(c == '"')      ? '"'
                    :(c == '\'')     ? '\''
                    :(c == '\\')     ? '\\'
                    :(c == EOF)      ? 'E'
                    :(c == '#')      ? '#'
                                     : 'r'
            };
}

//fills fn with a list of strings.
//returns the count of elements. Negate the count if we're at EOF.
//fn must already be allocated via apop_data_alloc() [no args].
static line_parse_t parse_a_line(FILE *infile, apop_data *fn, int const *field_ends, char const *delimiters){
    int ct=0, thisflen=0, inq=0, inqq=0, infield=0, mlen=5,
            lastwhite=0, lastnonwhite=0; 
    if (field_ends) return parse_a_fixed_line(infile, fn, field_ends);
    apop_char_info ci;
    do {
        ci = parse_next_char(infile, delimiters);
        //comments are to end of line, so they're basically a newline.
        if (ci.type=='#' && !(inq||inqq)){
            for(char c='x'; (c!='\n' && c!=EOF); )
                c = fgetc(infile);
            ci.type='n';
        }

        //The escape-type cases: \\ and '' and "".
        //If one applies, set the type to regular
        if (ci.type=='\\'){
            ci=parse_next_char(infile, delimiters);
            if (ci.type!='E')
                ci.type='r';
        }
        if (((inq && ci.type !='\'') ||(inqq && ci.type !='"')) && ci.type !='E')
            ci.type='r';
        if (ci.type=='\'') inq = !inq;
        else if (ci.type=='"') inqq = !inqq;

        if (ci.type=='W' && lastwhite==1) 
            continue; //compress these.
        lastwhite=(ci.type=='W');

        if (!infield){
            if (ci.type=='w') continue; //eat leading spaces.
            if (strchr("rd", ci.type)                    //new field; if 'dnE', blank field. 
                   || (strchr("nE", ci.type) && ct>0)){  //Blank fields only at end of lines that already have data; else all-blank line to ignore.
                if (++ct > fn->textsize[0]) apop_text_alloc(fn, ct, 1);//realloc text portion.
                *fn->text[ct-1] = realloc(*fn->text[ct-1], 5);
                thisflen = 0;
                mlen=5;
                infield=1;
            } 
        } 
        if (infield){
            if (ci.type=='d'||ci.type=='n' || ci.type=='E' || ci.type=='W'){
                //delimiter; close off this field.
                fn->text[ct-1][0][lastnonwhite] = '\0';
                infield =
                thisflen =
                lastnonwhite = 0;
            } else if (ci.type=='w' || ci.type=='r'){ //extend field
                thisflen++; //length of string
                if (thisflen+2 > mlen){
                    mlen *=2; //length of allocated memory
                    *fn->text[ct-1] = realloc(*fn->text[ct-1], mlen);
                }
                fn->text[ct-1][0][thisflen-1] = ci.c;
                if (ci.type!='w')
                    lastnonwhite = thisflen;
            }
        }
    } while (ci.type != 'n' && ci.type != 'E');
    return (line_parse_t) {.ct=ct, .eof= (ci.type == 'E')};
}

//On return, fn has copies of the field names, and add_this_line has the first data line.
static void get_field_names(int has_col_names, char **field_names, FILE *infile, 
                                apop_data *add_this_line, apop_data *fn, int const *field_ends, char const *delimiters){
    if (has_col_names && field_names == NULL){
        use_names_in_file++;
        while (fn->textsize[0] ==0) parse_a_line(infile, fn, field_ends, delimiters);
        parse_a_line(infile, add_this_line, field_ends, delimiters);
    } else{
        while (add_this_line->textsize[0] ==0) 
            parse_a_line(infile, add_this_line, field_ends, delimiters);
        fn	= apop_text_alloc(fn, add_this_line->textsize[0], 1);
        for (int i=0; i< fn->textsize[0]; i++)
            if (field_names) apop_text_add(fn, i, 0, field_names[i]);
            else             apop_text_add(fn, i, 0, "col_%i", i);
    }
}

/** Read a delimited text file into the matrix element of an \ref apop_data set.

  See \ref text_format.

\param text_file  = "-"  The name of the text file to be read in. If "-" (the default), use stdin.
\param has_row_names = 'n'. Does the lines of data have row names?
\param has_col_names = 'y'. Is the top line a list of column names? If there are row names, then there should be no first entry in this line like 'row names'. That is, for a 100x100 data set with row and column names, there are 100 names in the top row, and 101 entries in each subsequent row (name plus 100 data points).
\param field_ends If fields have a fixed size, give the end of each field, e.g. {3, 8 11}.
\param delimiters A string listing the characters that delimit fields. default = <tt>"|,\t"</tt>
\return 	Returns an apop_data set.
\exception out->error=='a' allocation error
\exception out->error=='t' text-reading error

<b>example:</b> See \ref apop_ols.

\li This function uses the \ref designated syntax for inputs.
\ingroup conversions	*/
APOP_VAR_HEAD apop_data * apop_text_to_data(char const*text_file, int has_row_names, int has_col_names, int const *field_ends, char const *delimiters){
    char const *apop_varad_var(text_file, "-")
    int apop_varad_var(has_row_names, 'n')
    int apop_varad_var(has_col_names, 'y')
    if (has_row_names==1||has_row_names=='Y') has_row_names ='y';
    if (has_col_names==1||has_col_names=='Y') has_col_names ='y';
    int const * apop_varad_var(field_ends, NULL);
    const char * apop_varad_var(delimiters, apop_opts.input_delimiters);
APOP_VAR_END_HEAD
    apop_data *set = NULL;
    FILE *infile = NULL;
    char *str;
    apop_data *add_this_line= apop_data_alloc();
    int row = 0,
        hasrows = (has_row_names == 'y');
    Apop_stopif(prep_text_reading(text_file, &infile), apop_data *out=apop_data_alloc();out->error='t'; return out,
            0, "trouble opening %s", text_file);

    line_parse_t L={ };
    //First, handle the top line, if we're told that it has column names.
    if (has_col_names=='y'){
        apop_data *field_names = apop_data_alloc();
        get_field_names(1, NULL, infile, add_this_line, field_names, field_ends, delimiters);
        L.ct = add_this_line->textsize[0];
        set = apop_data_alloc(0,1, L.ct - hasrows);
	    set->names->colct   = 0;
	    set->names->column	= malloc(sizeof(char*));
        for (int j=0; j< L.ct - hasrows; j++)
            apop_name_add(set->names, *field_names->text[j], 'c');
        apop_data_free(field_names);
    } 

    //Now do the body.
	while(!set || (L.ct && !L.eof)){
        if (!L.ct) { //skip blank lines
            L=parse_a_line(infile, add_this_line, field_ends, delimiters);
            continue;
        }
        if (!set) set = apop_data_alloc(0, 1, L.ct-hasrows); //for .has_col_names=='n'.
        row++;
        int cols = set->matrix  ? set->matrix->size2 : L.ct - hasrows;
        set->matrix = apop_matrix_realloc(set->matrix, row, cols);
        Apop_stopif(!set->matrix, set->error='a'; return set, 0, "allocation error.");
        if (hasrows) {
            apop_name_add(set->names, *add_this_line->text[0], 'r');
            Apop_stopif(L.ct-1 > set->matrix->size2, set->error='t'; return set, 1,
                 "row %i (not counting rownames) has %i elements (not counting the rowname), "
                 "but I thought this was a data set with %zu elements per row. "
                 "Stopping the file read; returning what I have so far.", row, L.ct-1, set->matrix->size2);
        } else Apop_stopif(L.ct > set->matrix->size2, set->error='t'; return set, 1,
                 "row %i has %i elements, "
                 "but I thought this was a data set with %zu elements per row. "
                 "Stopping the file read; returning what I have so far. Set has_row_names?", row, L.ct-1, set->matrix->size2);
        for (int col=hasrows; col < L.ct; col++){
            char *thisstr = *add_this_line->text[col];
            if (strlen(thisstr)){
                double val = strtod(thisstr, &str);
                if (thisstr != str)
                    gsl_matrix_set(set->matrix, row-1, col-hasrows, val);
                else {
                    gsl_matrix_set(set->matrix, row-1, col-hasrows, GSL_NAN);
                    Apop_notify(1, "trouble converting data item %i on data line %i [%s]; writing NaN.", col, row, thisstr);
                }
            } else gsl_matrix_set(set->matrix, row-1, col-hasrows, GSL_NAN);
        }
        L=parse_a_line(infile, add_this_line, field_ends, delimiters);
	}
    apop_data_free(add_this_line);
    if (strcmp(text_file,"-")) fclose(infile);
	return set;
}

/** This is the complement to \c apop_data_pack, qv. It writes the \c gsl_vector produced by that function back
    to the \c apop_data set you provide. It overwrites the data in the vector and matrix elements and, if present, the \c weights (and that's it, so names or text are as before).

\param in A \c gsl_vector of the form produced by \c apop_data_pack. No default; must not be \c NULL.
\param d  That data set to be filled. Must be allocated to the correct size. No default; must not be \c NULL.
\param use_info_pages Pages in XML-style brackets, such as <tt>\<Covariance\></tt> will
be ignored unless you set <tt>.use_info_pages='y'</tt>. Be sure that this is set to the
same thing when you both pack and unpack. Default: <tt>'n'</tt>.

\li If I get to the end of the first page and have more vector to unpack, and the data to
fill has a \c more element, then I will continue into subsequent pages.

This function uses the \ref designated syntax for inputs.
\ingroup conversions
*/
APOP_VAR_HEAD void apop_data_unpack(const gsl_vector *in, apop_data *d, char use_info_pages){
    const gsl_vector * apop_varad_var(in, NULL);
    apop_data* apop_varad_var(d, NULL);
    Apop_stopif(!d, return, 0, "the data set to be filled, d, must not be NULL");
    char apop_varad_var(use_info_pages, 'n');
APOP_VAR_ENDHEAD
    int offset = 0;
    gsl_vector vin, vout;
    if(d->vector){
        vin = gsl_vector_subvector((gsl_vector *)in, 0, d->vector->size).vector;
        gsl_vector_memcpy(d->vector, &vin);
        offset += d->vector->size;
    }
    if(d->matrix)
        for (size_t i=0; i< d->matrix->size1; i++){
            vin = gsl_vector_subvector((gsl_vector *)in, offset, d->matrix->size2).vector;
            vout = gsl_matrix_row(d->matrix, i).vector;
            gsl_vector_memcpy(&vout, &vin);
            offset += d->matrix->size2;
        }
    if(d->weights){
        vin = gsl_vector_subvector((gsl_vector *)in, offset, d->weights->size).vector;
        gsl_vector_memcpy(d->weights, &vin);
        offset += d->weights->size;
    }
    if (offset != in->size && d->more){
        vin = gsl_vector_subvector((gsl_vector *)in, offset, in->size - offset).vector;
        d = d->more;
        if (use_info_pages=='n')
            while (apop_regex(d->names->title, "^<.*>$"))
                d = d->more;
        apop_data_unpack(&vin, d);
    }
}

static size_t sizecount(const apop_data *in, const int all_pp, const int use_info_pp){ 
    if (!in) return 0;
    if (use_info_pp=='n' && apop_regex(in->names->title, "^<.*>$"))
        return (all_pp ? sizecount(in->more, all_pp, use_info_pp) : 0);
    return (in->vector ? in->vector->size : 0)
             + (in->matrix ? in->matrix->size1 * in->matrix->size2 : 0)
             + (in->weights ? in->weights->size : 0)
             + (all_pp ? sizecount(in->more, all_pp, use_info_pp) : 0);
}

/** This function takes in an \ref apop_data set and writes it as a single column of
numbers, outputting a \c gsl_vector.
 It is valid to use the \c out_vector->data element as an array of \c doubles of size
 \c out_vector->data->size (i.e. its <tt>stride==1</tt>).

 The complement is \c apop_data_unpack. I.e., 
\code
apop_data_unpack(apop_data_pack(in_data), data_copy) 
\endcode
will return the original data set (stripped of text and names).

 \param in an \c apop_data set. No default; if \c NULL, return \c NULL.
 \param out If this is not \c NULL, then put the output here. The dimensions must match exactly. If \c NULL, then allocate a new data set. Default = \c NULL. 
  \param all_pages If \c 'y', then follow the <tt> ->more</tt> pointer to fill subsequent
pages; else fill only the first page. Informational pages will still be ignored, unless you set <tt>.use_info_pages='y'</tt> as well.  Default = \c 'n'. 
\param use_info_pages Pages in XML-style brackets, such as <tt>\<Covariance\></tt> will
be ignored unless you set <tt>.use_info_pages='y'</tt>. Be sure that this is set to the
same thing when you both pack and unpack. Default: <tt>'n'</tt>.

 \return A \c gsl_vector with the vector data (if any), then each row of data (if any), then the weights (if any), then the same for subsequent pages (if any <tt>&& .all_pages=='y'</tt>). If \c out is not \c NULL, then this is \c out.
\exception NULL If you give me a vector as input, and its size is not correct, returns \c NULL.
This function uses the \ref designated syntax for inputs.
\ingroup conversions
 */
APOP_VAR_HEAD gsl_vector * apop_data_pack(const apop_data *in, gsl_vector *out, char all_pages, char use_info_pages){
    const apop_data * apop_varad_var(in, NULL);
    if (!in) return NULL;
    gsl_vector * apop_varad_var(out, NULL);
    char apop_varad_var(all_pages, 'n');
    char apop_varad_var(use_info_pages, 'n');
    if (out) {
        size_t total_size = sizecount(in, (all_pages == 'y' || all_pages == 'Y'), (use_info_pages =='n'));
        Apop_stopif(out->size != total_size, return NULL, 0, "The input data set has %zu elements, "
               "but the output vector you want to fill has size %zu. Please make "
               "these sizes equal.", total_size, out->size);
    }
APOP_VAR_ENDHEAD
    size_t total_size = sizecount(in, (all_pages == 'y' || all_pages == 'Y'), (use_info_pages =='n'));
    if (!total_size) return NULL;
    int offset = 0;
    if (!out) out = gsl_vector_alloc(total_size);
    gsl_vector vout, vin;
    if (in->vector){
        vout     = gsl_vector_subvector((gsl_vector *)out, 0, in->vector->size).vector;
        gsl_vector_memcpy(&vout, in->vector);
        offset  += in->vector->size;
    }
    if (in->matrix)
        for (size_t i=0; i< in->matrix->size1; i++){
            vin = gsl_matrix_row(in->matrix, i).vector;
            vout= gsl_vector_subvector((gsl_vector *)out, offset, in->matrix->size2).vector;
            gsl_vector_memcpy(&vout, &vin);
            offset  += in->matrix->size2;
        }
    if (in->weights){
        vout     = gsl_vector_subvector((gsl_vector *)out, offset, in->weights->size).vector;
        gsl_vector_memcpy(&vout, in->weights);
        offset  += in->weights->size;
    }
    if ((all_pages == 'y' ||all_pages =='Y') && in->more){
        vout = gsl_vector_subvector((gsl_vector *)out, offset, out->size - offset).vector;
        while (use_info_pages=='n' && in->more && apop_regex(in->more->names->title, "^<.*>$"))
            in = in->more;
        apop_data_pack(in->more, &vout, .all_pages='y');
    }
    return out;
}

/** \def apop_data_fill (in, ap)
Fill a pre-allocated data set with values.

  For example:

\code
#include <apop.h>

int main(){
  apop_data *a =apop_data_alloc(2,2,2);
  double    eight   = 8.0;
    apop_data_fill(a, 8, 2.2, eight/2,
                      0, 6.0, eight);
    apop_data_show(a);
}
\endcode

Warning: I need as many arguments as the size of the data set, and can't count them for you. Too many will be ignored; too few will produce unpredictable results, which may include padding your matrix with garbage or a simple segfault.

Underlying this function is a base function that takes a single list, as opposed to a set of unassociated numbers as above:

\code
#include <apop.h>

int main(){
  apop_data *a =apop_data_alloc(2,2,2);
  double    eight   = 8.0;
  double list[] = {8, 2.2, eight/2, 
                   0, 6.0, eight};
    apop_data_fill_base(a, list);
    apop_data_show(a);
}
\endcode

\param in   An \c apop_data set (that you have already allocated).
\param ...  A series of at least as many floating-point values as there are blanks in the data set.
\return     A pointer to the same data set that was input.

\li I assume that <tt>vector->size==matrix->size1</tt>; otherwise I just use \c matrix->size1.

\li to allocate and fill on one line, because the allocated pointer is returned, and so
there is no leak or loss to a form like this example, which generates a unit vector for three dimensions:
\code
apop_data *unit_vector = apop_data_fill(apop_data_alloc(3), 1, 1, 1);
\endcode

\see apop_line_to_data
*/

apop_data *apop_data_fill_base(apop_data *in, double ap[]){
/* In conversions.h, you'll find this header, which turns all but the first input into an array of doubles of indeterminate length:
#define apop_data_fill(in, ...) apop_data_fill_base((in), (double []) {__VA_ARGS__})
*/
    if (!in) return NULL;
    int k=0, start=0, fin=0, height=0;
    if (in->vector){
        start   = -1;
        height  = in->vector->size;
    }
    if (in->matrix){
        fin   = in->matrix->size2;
        height  = in->matrix->size1;
    }
    for (int i=0; i< height; i++)
        for (int j=start; j< fin; j++)
            apop_data_set(in, i, j, ap[k++]);
    return in;
}

/** \def apop_vector_fill(in, ap)
 Fill a pre-allocated \c gsl_vector with values.

  See \c apop_data_alloc for a relevant example. See also \c apop_matrix_alloc.

Warning: I need as many arguments as the size of the vector, and can't count them for you. Too many will be ignored; too few will produce unpredictable results, which may include padding your vector with garbage or a simple segfault.


\param in   A \c gsl_vector (that you have already allocated).
\param ...  A series of exactly as many values as there are spaces in the vector.
\return     A pointer to the same vector that was input.
*/
gsl_vector *apop_vector_fill_base(gsl_vector *in, double ap[]){
    if (!in) return NULL;
    for (int i=0; i< in->size; i++)
        gsl_vector_set(in, i, ap[i]);
    return in;
}

/** \def apop_matrix_fill(in, ap)
 Fill a pre-allocated \c gsl_matrix with values. 
 
The values should be in row-major order (i.e., list the entire first row, followed by the second row, et cetera). If your data is column-major, then try calling \c gsl_matrix_transpose after this function.

  See \c apop_data_alloc for a relevant example. See also \c apop_vector_alloc.

  I need as many arguments as the size of the matrix. Too many will be ignored; too few will produce unpredictable results, which may include padding your matrix with garbage or a simple segfault.

\param in   A \c gsl_matrix (that you have already allocated).
\param ...  A series of exactly as many floating-point values as there are blanks in the matrix.
\return     A pointer to the same matrix that was input.
*/
gsl_matrix *apop_matrix_fill_base(gsl_matrix *in, double ap[]){
    if (!in) return NULL;
    gsl_matrix_view	m = gsl_matrix_view_array(ap, in->size1, in->size2);
	gsl_matrix_memcpy(in,&(m.matrix));
    return in;
}



///////The rest of this file is for apop_text_to_db
extern sqlite3 *db;

static char *get_field_conditions(char *var, apop_data *field_params){
    if (field_params)
        for (int i=0; i<field_params->textsize[0]; i++)
            if (apop_regex(var, field_params->text[i][0]))
                return field_params->text[i][1];
    return (apop_opts.db_engine == 'm') ? "varchar(100)" : "numeric";
}

static int tab_create_mysql(char *tabname, int has_row_names, apop_data *field_params, char *table_params, apop_data const *fn){
    char *q = NULL;
    asprintf(&q, "CREATE TABLE %s", tabname);
    for (int i=0; i<fn->textsize[0]; i++){
        if (i==0) 	{
            if (has_row_names) xprintf(&q, "%s (row_names varchar(100), ", q);
            else               xprintf(&q, "%s (", q);
        } else xprintf(&q, "%s %s, ", q, get_field_conditions(*fn->text[i-1], field_params));
        xprintf(&q, "%s %s", q, fn[i]);
    }
    xprintf(&q, "%s %s%s%s);", q, get_field_conditions(*fn->text[fn->textsize[0]-1], field_params)
                                , table_params? ", ": "", XN(table_params));
    apop_query("%s", q);
    Apop_stopif(!apop_table_exists(tabname), return -1, 0, "query \"%s\" failed.", q);
    free(q);
    return 0;
}

static int tab_create_sqlite(char *tabname, int has_row_names, apop_data *field_params, char *table_params, apop_data *fn){
    char  *q = NULL;
    asprintf(&q, "create table %s", tabname);
    for (int i=0; i<fn->textsize[0]; i++){
        if (i==0){
            if (has_row_names) xprintf(&q, "%s ('row_names', ", q);
            else               xprintf(&q, "%s (", q);
        } else xprintf(&q, "%s' %s, ", q, get_field_conditions(*fn->text[i-1], field_params));
        xprintf(&q, "%s '%s", q, *fn->text[i]);
    }
    xprintf(&q, "%s' %s%s%s);", q, get_field_conditions(*fn->text[fn->textsize[0]-1], field_params)
                                , table_params? ", ": "", XN(table_params));
    apop_query("%s", q);
    Apop_stopif(!apop_table_exists(tabname), return -1, 0, "query \"%s\" failed.", q);
    free(q);
    apop_query("begin;");
    return 0;
}

/**
--If the string has zero length, then it's probably a missing value.
 --If the string isn't a number, it needs quotes
 */
char *prep_string_for_sqlite(int prepped_statements, char const *astring){
    if (!astring || astring[0]=='\0' || !strcasecmp(apop_opts.db_nan, astring))
        return NULL;

    char *out  = NULL,
		 *tail = NULL;
	if(strtod(astring, &tail)) /*do nothing.*/;
    if (*tail!='\0'){	//then it's not a number.
        if (!prepped_statements){
            if (strchr(astring, '\''))
                asprintf(&out,"\"%s\"", astring);
            else
                asprintf(&out,"'%s'", astring);
        } else  out = strdup(astring);
	} else {	    //number, maybe INF or NAN. Also, sqlite wants 0.1, not .1
		assert(*astring!='\0');
        if (isinf(atof(astring))==1)
			out = strdup("9e9999999");
        else if (isinf(atof(astring))==-1)
			out = strdup("-9e9999999");
        else if (gsl_isnan(atof(astring)))
			out = strdup("0.0/0.0");
        else if (astring[0]=='.')
			asprintf(&out, "0%s",astring);
		else out = strdup(astring);
	}
    return out;
}

static void line_to_insert(line_parse_t L, apop_data const*addme, char const *tabname, 
                             sqlite3_stmt *p_stmt, int row){
    if (!L.ct) return;
    int  field = 1;
    char comma = ' ';
    char *q  = NULL;
    if (!p_stmt) asprintf(&q, "INSERT INTO %s VALUES (", tabname);
    for (int col=0; col < L.ct; col++){
        char *prepped = prep_string_for_sqlite(!!p_stmt, *addme->text[col]);
        if (p_stmt){
            if (!prepped || !strlen(prepped))
                field++; //leave NULL and cleared
            else if (sqlite3_bind_text(p_stmt, field++, prepped, -1, SQLITE_TRANSIENT))
                Apop_notify(0, "Something wrong on line %i, field %i [%s].\n"
                                            , row, field-1, *addme->text[col]);
        } else {
            if (prepped && strlen(prepped)) 
                 xprintf(&q, "%s%c %s", q, comma,  prepped);
            else xprintf(&q, "%s%cNULL", q, comma);
            comma = ',';
        }
        free(prepped);
    }
    if (!p_stmt){
        apop_query("%s);",q); 
        free (q);
    }
}

/** Read a text file into a database table.

  See \ref text_format.

See the \ref apop_ols page for an example that uses this function to read in sample data (also listed on that page).

By the way, there is a begin/commit wrapper that bundles the process into bundles of 10000 inserts per transaction. if you want to change this to more or less frequent commits, you'll need to modify and recompile the code. 

\param text_file    The name of the text file to be read in. If \c "-", then read from \c STDIN. (default = "-")
\param tabname      The name to give the table in the database (default
= <tt> apop_strip_dots (text_file, 'd')</tt>; default in Python/R interfaces="t")
\param has_row_names Does the lines of data have row names? (default = 0)
\param has_col_names Is the top line a list of column names? All dots in the column names are converted to underscores, by the way. (default = 1)
\param field_names The list of field names, which will be the columns for the table. If <tt>has_col_names==1</tt>, read the names from the file (and just set this to <tt>NULL</tt>). If has_col_names == 1 && field_names !=NULL, I'll use the field names.  (default = NULL)
\param field_ends If fields have a fixed size, give the end of each field, e.g. {3, 8 11}.
\param field_params There is an implicit <tt>create table</tt> in setting up the database. If you want to add a type, constraint, or key, put that here. The relevant part of the input \ref apop_data set is the \c text grid, which should be \f$N \times 2\f$. The first item in each row (<tt>your_params->text[n][0]</tt>, for each \f$n\f$) is a regular expression to match against the variable names; the second item (<tt>your_params->text[n][1]</tt>) is the type, constraint, and/or key (i.e., what comes after the name in the \c create query). Not all variables need be mentioned; the default type if nothing matches is <tt>numeric</tt>. I go in order until I find a regex that matches the given field, so if you don't like the default, then set the last row to have name <tt>.*</tt>, which is a regex guaranteed to match anything that wasn't matched by an earlier row, and then set the associated type to your preferred default. See \ref apop_regex on details of matching.
\param table_params There is an implicit <tt>create table</tt> in setting up the database. If you want to add a table constraint or key, such as <tt>not null primary key (age, sex)</tt>, put that here.
\param delimiters A string listing the characters that delimit fields. default = <tt>"|,\t"</tt>

\return Returns the number of rows on success, -1 on error.

This function uses the \ref designated syntax for inputs.
\ingroup conversions
*/
APOP_VAR_HEAD int apop_text_to_db(char const *text_file, char *tabname, int has_row_names, int has_col_names, char **field_names, int const *field_ends, apop_data *field_params, char *table_params, char const *delimiters){
    char const *apop_varad_var(text_file, "-")
    char *apop_varad_var(tabname, apop_strip_dots(text_file, 'd'))
    int apop_varad_var(has_row_names, 'n')
    int apop_varad_var(has_col_names, 'y')
    if (has_row_names==1||has_row_names=='Y') has_row_names ='y';
    if (has_col_names==1||has_col_names=='Y') has_col_names ='y';
    int const *apop_varad_var(field_ends, NULL)
    char ** apop_varad_var(field_names, NULL)
    apop_data * apop_varad_var(field_params, NULL)
    char * apop_varad_var(table_params, NULL)
    const char * apop_varad_var(delimiters, apop_opts.input_delimiters);
APOP_VAR_END_HEAD
    int  batch_size  = 10000, not_ok=0,
      	 col_ct, ct = 0, rows = 1;
    FILE *infile;
    apop_data *add_this_line = apop_data_alloc();
    sqlite3_stmt * statement = NULL;
    line_parse_t L={1,0};

	Apop_assert_c(!apop_table_exists(tabname), -1, 0, "table %s exists; not recreating it.", tabname);

    //get names and the first row.
    if (prep_text_reading(text_file, &infile)) return -1;
	use_names_in_file = 0;    //file-global.
    apop_data *fn = apop_data_alloc();
    get_field_names(has_col_names=='y', field_names, infile, 
                                    add_this_line, fn, field_ends, delimiters);
    col_ct = L.ct = add_this_line->textsize[0];
    if (apop_opts.db_engine=='m')
        not_ok = tab_create_mysql(tabname, has_row_names=='y', field_params, table_params, fn);
    else
        not_ok = tab_create_sqlite(tabname, has_row_names=='y', field_params, table_params, fn);
    Apop_stopif(not_ok, return -1, 0, "Creating the table in the database failed.");
#if SQLITE_VERSION_NUMBER < 3003009
    apop_notify(1, "Apophenia was compiled using a version of SQLite from mid-2007 or earlier. "
                    "The code for reading in text files using such an old version is no longer supported, "
                    "so if errors crop up please see about installing a more recent version of SQLite's library.");
    int use_sqlite_prepared_statements = 0;
#else
    int use_sqlite_prepared_statements = (sqlite3_libversion_number() >=3003009
                    && !(apop_opts.db_engine == 'm') 
                    &&  col_ct <= 999); //Arbitrary SQLite limit on blanks in prepared statements.
    if (use_sqlite_prepared_statements){
        char *q=NULL;
        asprintf(&q, "INSERT INTO %s VALUES (?", tabname);
        for (int i = 1; i < col_ct; i++)
            xprintf(&q, "%s, ?", q);
        xprintf(&q, "%s)", q);
        Apop_assert_c(db, -1, 0, "Trouble opening the database; inserting no data.");
        sqlite3_prepare_v2(db, q, -1, &statement, NULL);
        free(q);
    }
#endif
    //done with table & query setup.
    //convert a data line into SQL: insert into TAB values (0.3, 7, "et cetera");
	while(L.ct && !L.eof){
        line_to_insert(L, add_this_line, tabname, statement, rows);
        if (!(ct++ % batch_size)){
            if (apop_opts.db_engine != 'm') apop_query("commit; begin;");
            if (apop_opts.verbose > 0) {Apop_notify(2, ".");fflush(NULL);}
        }
        if (statement){
            int err = sqlite3_step(statement);
            if (err!=0 && err != 101) //0=ok, 101=done
                Apop_notify(0, "sqlite insert query gave error code %i.\n", err);
            Apop_assert_c(!sqlite3_reset(statement), -1, apop_errorlevel, "SQLite error.");
#if SQLITE_VERSION_NUMBER >= 3003009
            Apop_assert_c(!sqlite3_clear_bindings(statement), -1, apop_errorlevel, "SQLite error."); //needed for NULLs
#endif
        }
        do {
            L = parse_a_line(infile, add_this_line, field_ends, delimiters);
            rows ++;
        } while (!L.ct && !L.eof); //skip blank lines
	}
    apop_data_free(add_this_line);
    apop_query("commit;");
	if (use_sqlite_prepared_statements){
        Apop_assert_c(sqlite3_finalize(statement) ==SQLITE_OK, -1, apop_errorlevel, "SQLite error.");
    }
    if (strcmp(text_file,"-")) fclose(infile);
	return rows;
}
