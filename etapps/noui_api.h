#ifndef NOUI_API_H
#define NOUI_API_H

#ifdef __cplusplus
extern "C" {
#endif

const char* noui_name(void);
const char* noui_version(void);
const char* noui_description(void);
int         noui_init(void);
void        noui_uninit(void);

#ifdef __cplusplus
}
#endif

#endif
