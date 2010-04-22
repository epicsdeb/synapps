#ifndef NDDXP_H
#define NDDXP_H

#ifdef __cplusplus
extern "C"
{
#endif

int NDDxp_config(const char *portName, int nChannels, int maxBuffers, size_t maxMemory);

#ifdef __cplusplus
}
#endif

#endif
