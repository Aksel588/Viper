#ifndef VIPER_RUNTIME_H
#define VIPER_RUNTIME_H

#include <stdio.h>

#include "types.h"

typedef struct VpTensor {
    ViperType type;
    void *data;
} VpTensor;

typedef struct VpValue VpValue;

typedef struct VpList {
    ViperType type;
    VpValue *items;
    int count;
    int capacity;
} VpList;

typedef struct VpStruct {
    char *type_name;
    char **field_names;
    VpValue *fields;
    int field_count;
} VpStruct;

typedef struct VpValue {
    ViperType type;
    union {
        long long i;
        double f;
        char *str;
        bool b;
        VpTensor *tensor;
        VpList *list;
        VpStruct *strukt;
    } as;
} VpValue;

VpTensor *vp_tensor_create(const ViperType *type);
VpTensor *vp_tensor_from_data(const ViperType *type, const void *data);
VpTensor *vp_tensor_clone(const VpTensor *t);
void vp_tensor_free(VpTensor *t);

VpList *vp_list_create(TypeKind elem);
VpList *vp_list_clone(const VpList *list);
void vp_list_free(VpList *list);
bool vp_list_push(VpList *list, const VpValue *val);
int vp_list_len(const VpList *list);
VpValue vp_list_get(const VpList *list, int index);

VpStruct *vp_struct_create(const char *type_name, char **field_names, const VpValue *fields, int field_count);
VpStruct *vp_struct_clone(const VpStruct *s);
void vp_struct_free(VpStruct *s);
VpValue vp_struct_get_field(const VpStruct *s, const char *field_name);

VpValue vp_value_int(long long v);
VpValue vp_value_float(double v);
VpValue vp_value_string(const char *v);
VpValue vp_value_bool(bool v);
VpValue vp_value_tensor(VpTensor *t);
VpValue vp_value_list(VpList *list);
VpValue vp_value_struct(VpStruct *s);
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

int vp_runtime_len(const VpValue *v);
VpValue vp_runtime_append(VpList *list, const VpValue *elem);
VpValue vp_runtime_abs(const VpValue *v);
VpValue vp_runtime_sqrt(const VpValue *v);
VpValue vp_runtime_floor(const VpValue *v);
VpValue vp_runtime_ceil(const VpValue *v);
VpValue vp_runtime_read_file(const char *path);
VpValue vp_runtime_write_file(const char *path, const char *content);
VpValue vp_runtime_input(void);

#endif
