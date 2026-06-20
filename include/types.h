#ifndef VIPER_TYPES_H
#define VIPER_TYPES_H

#include <stdbool.h>
#include <stddef.h>

#define VIPER_TENSOR_MAX_RANK 8

typedef enum {
    TYPE_INT,
    TYPE_FLOAT,
    TYPE_STRING,
    TYPE_BOOL,
    TYPE_TENSOR,
    TYPE_VOID,
    TYPE_UNKNOWN
} TypeKind;

typedef struct {
    int dims[VIPER_TENSOR_MAX_RANK];
    int rank;
} TensorShape;

typedef struct ViperType {
    TypeKind kind;
    TypeKind elem;
    TensorShape shape;
} ViperType;

ViperType viper_type_int(void);
ViperType viper_type_float(void);
ViperType viper_type_string(void);
ViperType viper_type_bool(void);
ViperType viper_type_void(void);
ViperType viper_type_unknown(void);
ViperType viper_type_tensor(TypeKind elem, const int *dims, int rank);

bool viper_type_eq(const ViperType *a, const ViperType *b);
bool tensor_shape_eq(const TensorShape *a, const TensorShape *b);
bool tensor_matmul_result(const ViperType *left, const ViperType *right, ViperType *out);
bool tensor_elementwise_result(const ViperType *left, const ViperType *right, ViperType *out);
bool tensor_transpose_result(const ViperType *src, ViperType *out);
bool tensor_reshape_valid(const ViperType *src, const int *dims, int rank, ViperType *out);
size_t tensor_num_elements(const TensorShape *shape);
int viper_type_to_string(const ViperType *type, char *buf, size_t len);
TypeKind type_kind_from_name(const char *name);

#endif
