#ifndef VIPER_RUNTIME_H
#define VIPER_RUNTIME_H

#include <stdio.h>

#include "types.h"

typedef struct VpTensor {
    ViperType type;
    void *data;
} VpTensor;

typedef struct VpValue {
    ViperType type;
    union {
        long long i;
        double f;
        char *str;
        bool b;
        VpTensor *tensor;
    } as;
} VpValue;

VpTensor *vp_tensor_create(const ViperType *type);
VpTensor *vp_tensor_from_data(const ViperType *type, const void *data);
VpTensor *vp_tensor_clone(const VpTensor *t);
void vp_tensor_free(VpTensor *t);

VpValue vp_value_int(long long v);
VpValue vp_value_float(double v);
VpValue vp_value_string(const char *v);
VpValue vp_value_bool(bool v);
VpValue vp_value_tensor(VpTensor *t);
VpValue vp_value_clone(const VpValue *v);
void vp_value_free(VpValue *v);

size_t vp_tensor_num_elements(const VpTensor *t);
size_t vp_tensor_elem_size(const VpTensor *t);
void vp_tensor_get(const VpTensor *t, const int *indices, void *out);
void vp_tensor_set(VpTensor *t, const int *indices, const void *val);

VpTensor *vp_tensor_transpose(const VpTensor *t);
VpTensor *vp_tensor_reshape(const VpTensor *t, const int *dims, int rank);
typedef enum {
    RT_OP_ADD,
    RT_OP_SUB,
    RT_OP_MUL,
    RT_OP_DIV
} RtBinOp;

VpTensor *vp_tensor_elementwise_binop(RtBinOp op, const VpTensor *a, const VpTensor *b);
VpTensor *vp_tensor_matmul(const VpTensor *a, const VpTensor *b);

void vp_value_print(const VpValue *v, FILE *out);
VpValue vp_value_cast(const VpValue *v, TypeKind target);

#endif
