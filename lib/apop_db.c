#include "apop_db.h"
#include <string.h>

int query_db(const char *q){
char 		*err;
	sqlite3_exec(db, q, NULL,NULL, &err);
	ERRCHECK
	return 1;
}

int table_exists(const char *q, int whattodo){
	//whattodo==1	==>kill table so it can be recreated in main.
	//whattodo==0	==>return error so program can continue.
char 		*err, q2[5000];
int 		isthere=0;

	int tab_exists_callback(void *whatever, int argc, char **argv, char **andever){
	int 		i;
		for(i=argc;i--;)
			if (!strcmp(argv[i],q))
				isthere=1;
		return 0;
	}

	sqlite3_exec(db, "select name from sqlite_master where type='table'",tab_exists_callback,NULL, &err); 
	ERRCHECK
	if (whattodo==1 && isthere)
		sqlite3_exec(db,strcat(strcpy(q2, "DROP TABLE "),q),NULL,NULL, &err); ERRCHECK
	return isthere;
}

int count_cols(const char *name){
char 		*err, q2[5000];
int		colct	= 1;

	int count_cols_callback(void *whatever, int argc, char **argv, char **andever){
	int 		i=0;
		while(argv[0][i]!='\0')
			if (argv[0][i++]==',') 
				colct	++;
		return 0;
	}

	sprintf(q2, "select sql from sqlite_master where type='table' and name=\"%s\"",name);
	sqlite3_exec(db, q2, count_cols_callback,NULL, &err); 
	ERRCHECK
	return colct;
}

int open_db(char *filename){
//char	*err;
	//if (filename==NULL) 	db	=sqlite_open(":memory:",0,&err);
	//else			db	=sqlite_open(filename,0,&err);
	if (filename==NULL) 	sqlite3_open(":memory:",&db);
	else			sqlite3_open(filename,&db);
	return 0;
}

int close_db(int vacuum){
char		*err;
	if (vacuum) sqlite3_exec(db, "VACUUM", NULL, NULL, &err);
//	ERRCHECK
	sqlite3_close(db);
	return 0;
	}

int query_to_matrix(gsl_matrix **output, const char *query){
int		totalrows=0,currentrow=0;
char		*q2, *err=NULL;

	int db_to_table(void *o,int argc, char **argv, char **whatever){
	int		jj;
	gsl_matrix * 	output = (gsl_matrix *) o;
		if (*argv !=NULL){
			for (jj=0;jj<argc;jj++)
				gsl_matrix_set(output,currentrow,jj, atof(argv[jj]));
			currentrow++;
		}
		return 0;
	}

	int length_callback(void *o,int argc, char **argv, char **whatever){
		totalrows=atoi(argv[0]); 
		return 0;
	}

	q2	= malloc(sizeof(char)*(strlen(query)+300));
	table_exists("completely_temporary_table",1);
	sqlite3_exec(db,strcat(strcpy(q2,
		"CREATE TABLE completely_temporary_table AS "),query),NULL,NULL, &err); ERRCHECK
	sqlite3_exec(db,"SELECT count(*) FROM completely_temporary_table",length_callback,NULL, &err);
	ERRCHECK
	if (totalrows==0){
		*output	= NULL;
	} else {
		*output	= gsl_matrix_alloc(totalrows, count_cols("completely_temporary_table"));
		sqlite3_exec(db,"SELECT * FROM completely_temporary_table",db_to_table,*output, &err); ERRCHECK
	}
	sqlite3_exec(db,"DROP TABLE completely_temporary_table",NULL,NULL, &err);  ERRCHECK
	return totalrows;
}

int matrix_to_db(gsl_matrix *data, char *tabname, char **headers){
int		i,j;
char		*q 	=malloc(sizeof(char)*1000);
	sprintf(q, "create table %s (", tabname);
	for(i=0;i< data->size2; i++){
		q	=realloc(q,sizeof(char)*(strlen(q)+1000));
		if(headers == NULL) 	sprintf(q, "%s\n c%i", q,i);
		else			sprintf(q, "%s\n %s", q,headers[i]);
		if (i< data->size2-1) 	sprintf(q, "%s,",q);
		else			sprintf(q,"%s);  begin;",q);
	}
	for(i=0;i< data->size1; i++){
		q	=realloc(q,sizeof(char)*(strlen(q)+(1+data->size2)*1000));
		sprintf(q,"%s \n insert into %s values(",q,tabname);
		for(j=0;j< data->size2; j++)
			if(j< data->size2 -1) 	sprintf(q,"%s %g, ",q,gsl_matrix_get(data,i,j));
			else			sprintf(q,"%s %g);",q,gsl_matrix_get(data,i,j));
	}
	sprintf(q, "%s; end;", q);
	query_db(q);
	return 0;
}