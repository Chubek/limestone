/**
 * @file etk_api.h
 * @brief Versioned host-service table and capability probing.
 * @stability experimental
 * @depends ExtensionTk::types, ExtensionTk::version, InteropTk::cstring
 */
#ifndef ETK_API_H
#define ETK_API_H
#include "etk_version.h"
#define ETK_API_MAX_SERVICES 64
typedef struct etk_service { const char *name; etk_version version; void *value; } etk_service;
typedef struct etk_service_table { etk_service services[ETK_API_MAX_SERVICES]; size_t count; } etk_service_table;
ETK_DEF void etk_service_table_init(etk_service_table *t);
ETK_DEF etk_status etk_service_register(etk_service_table *t,const etk_service *s);
ETK_DEF void *etk_service_lookup(const etk_service_table *t,const char *name,
                                  const char *constraint);
ETK_DEF etk_bool etk_api_probe(const etk_service_table *t,const char *name);
#ifdef ETK_API_IMPLEMENTATION
#include <string.h>
ETK_DEF void etk_service_table_init(etk_service_table*t){if(t)memset(t,0,sizeof(*t));}
ETK_DEF etk_status etk_service_register(etk_service_table*t,const etk_service*s)
{if(!t||!s||!s->name||t->count>=ETK_API_MAX_SERVICES)return ETK_EINVAL;t->services[t->count++]=*s;return ETK_OK;}
ETK_DEF void *etk_service_lookup(const etk_service_table*t,const char*n,const char*c)
{size_t i;if(!t||!n)return NULL;for(i=0;i<t->count;++i)if(strcmp(t->services[i].name,n)==0&&(!c||etk_version_satisfies(&t->services[i].version,c)))return t->services[i].value;return NULL;}
ETK_DEF etk_bool etk_api_probe(const etk_service_table*t,const char*n){return etk_service_lookup(t,n,NULL)!=NULL;}
#endif
#endif
