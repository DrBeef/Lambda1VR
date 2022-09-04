#ifndef _GL4ES_FPE_SHADER_H_
#define _GL4ES_FPE_SHADER_H_

#include "fpe.h"

extern const char* fpeshader_signature;

const char* const* fpe_VertexShader(fpe_state_t *state);
const char* const* fpe_FragmentShader(fpe_state_t *state);

const char* const* fpe_CustomVertexShader(const char* initial, fpe_state_t* state);
const char* const* fpe_CustomFragmentShader(const char* initial, fpe_state_t* state);

#endif // _GL4ES_FPE_SHADER_H_
