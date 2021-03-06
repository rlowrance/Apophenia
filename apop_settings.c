/** \file 
         Specifying model characteristics and details of estimation methods. */
/* Copyright (c) 2008--2009, 2011 by Ben Klemens.  Licensed under the modified GNU GPL v2; see COPYING and COPYING2.  */
#include "apop_internal.h"

static size_t get_settings_ct(apop_model *model){
    int ct =0;
    if (!model->settings) return 0;
    while (model->settings[ct].name[0] !='\0') ct++;
    return ct;
}

/** Remove a settings group from a model.

  But use \ref Apop_settings_rm_group instead; that macro uses this function internally.

  If the model has no settings or your preferred settings group is not found, this function does nothing.
 */
void apop_settings_remove_group(apop_model *m, char *delme){
    if (!m->settings)  return;
    int i = 0;
    int ct = get_settings_ct(m);
 
    while (m->settings[i].name[0] !='\0'){
        if (!strcmp(m->settings[i].name, delme)){
            ((void (*)(void*))m->settings[i].free)(m->settings[i].setting_group);
            for (int j=i+1; j< ct+1; j++) //don't forget the null sentinel.
                m->settings[j-1] = m->settings[j];
            i--;
        }
        i++;
    }
   // apop_assert_void(0, 1, 'c', "I couldn't find %s in the input model, so nothing was removed.", delme);
}

/** Don't use this function. It's what the \c Apop_model_add_group macro uses internally. Use that.  */
void *apop_settings_group_alloc(apop_model *model, char *type, void *free_fn, void *copy_fn, void *the_group){
    if(apop_settings_get_grp(model, type, 'c'))  
        apop_settings_remove_group(model, type); 
    int ct = get_settings_ct(model);
    model->settings = realloc(model->settings, sizeof(apop_settings_type)*(ct+2));   
    model->settings[ct] = (apop_settings_type) {
                            .setting_group = the_group,
                            .free= free_fn, .copy = copy_fn };
    strncpy(model->settings[ct].name, type, 100);
    model->settings[ct+1] = (apop_settings_type) { };
    return model->settings[ct].setting_group;
}

/** This function is used internally by the macro \ref Apop_settings_get_group. Use that.  */
void * apop_settings_get_grp(apop_model *m, char *type, char fail){
    if (!m->settings) return NULL;
    int i;
    for (i=0; m->settings[i].name[0] !='\0'; i++)
       if (!strcmp(type, m->settings[i].name))
           return m->settings[i].setting_group;
    if (!strlen(type)) //requesting the blank sentinel.
       return m->settings[i].setting_group;
    Apop_assert(fail != 'f', "I couldn't find the settings group %s in the given model.", type);
    return NULL; //else, just return NULL and let the caller sort it out.
}

/** Copy a settings group with the given name from the second model to
 the first.  (i.e., the arguments are in memcpy order). 

 You probably won't need this often---just use \ref apop_model_copy.
 */
void apop_settings_copy_group(apop_model *outm, apop_model *inm, char *copyme){
    Apop_assert_n(inm->settings, "The input model (i.e., the second argument to this function) has no settings.");
    void *g =  apop_settings_get_grp(inm, copyme, 'f');
    int i;
    for (i=0; inm->settings[i].name[0] !='\0'; i++)//retrieve the index.
       if (!strcmp(copyme, inm->settings[i].name))
           break;
    void *gnew = (inm->settings[i].copy) 
                    ? ((void *(*)(void*))inm->settings[i].copy)(g)
                    : g;
    apop_settings_group_alloc(outm, copyme, inm->settings[i].free, inm->settings[i].copy, gnew);
}
