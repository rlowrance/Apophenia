/*conversions.h

  Convenience functions to convert among vectors (gsl_vector), matrices (gsl_matrix), 
  arrays (double **), and database tables

  To the extent that it makes sense, all functions are of the form convert_in_to_out(in, out),
  and allocate the output object for you.
*/


#include "db.h"
#include <string.h>
#include <gsl/gsl_matrix.h>

/////////////
//From vector
/////////////
int apop_convert_vector_to_array(gsl_vector *in, double **out);
//Returns the length of the array (i.e., in->size);

////////////
//From array
////////////
void apop_convert_array_to_vector(double *in, gsl_vector **out, int size);
void apop_convert_array_to_matrix(double **in, gsl_matrix **out, int rows, int cols);

///////////
//From text
///////////
int apop_convert_text_to_array(char *text_file, double ***tab, int ct, int has_field_names);

/* text_file: the input file. At the moment, it needs to be comma delimited.
	Lines with a # at the head are taken to be comments and ignored.
	If field_names is NULL, then the first non-comment line of
	the file is taken to be strings giving the (comma-delimited)
	field names.
   tab: the array to hold the data. Do not preallocate.
   ct: the number of fields the table will have
   has_field_names: Is there a line of text at the top that I should ignore?
   returns: the number of rows
*/

int apop_convert_text_to_db(char *text_file, char *tabname, int ct, char **field_names);

/* text_file: the input file. At the moment, it needs to be comma delimited.
	Lines with a # at the head are taken to be comments and ignored.
	If field_names is NULL, then the first non-comment line of
	the file is taken to be strings giving the (comma-delimited)
	field names.
   tabname: the name to give the table in the database
   ct: the number of fields the table will have
   field_names: the list of field names, which will be the columns for
   	the table. If NULL, read the names from the file.
   returns: the number of rows
   [Don't forget to open the db with apop_open_db first.]
*/