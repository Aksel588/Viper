#include "types.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

ViperType viper_type_int(void) {
    ViperType t = {TYPE_INT, TYPE_UNKNOWN, {{0}, 0}, NULL};
    return t;
}

ViperType viper_type_float(void) {
    ViperType t = {TYPE_FLOAT, TYPE_UNKNOWN, {{0}, 0}, NULL};
    return t;
}

ViperType viper_type_string(void) {
    ViperType t = {TYPE_STRING, TYPE_UNKNOWN, {{0}, 0}, NULL};
    return t;
}

ViperType viper_type_bool(void) {
    ViperType t = {TYPE_BOOL, TYPE_UNKNOWN, {{0}, 0}, NULL};
    return t;
}

ViperType viper_type_void(void) {
    ViperType t = {TYPE_VOID, TYPE_UNKNOWN, {{0}, 0}, NULL};
    return t;
}

ViperType viper_type_unknown(void) {
    ViperType t = {TYPE_UNKNOWN, TYPE_UNKNOWN, {{0}, 0}, NULL};
    return t;
}

ViperType viper_type_tensor(TypeKind elem, const int *dims, int rank) {
    ViperType t = {TYPE_TENSOR, elem, {{0}, 0}, NULL};
    if (rank > VIPER_TENSOR_MAX_RANK) {
        rank = VIPER_TENSOR_MAX_RANK;
    }
    t.shape.rank = rank;
    for (int i = 0; i < rank; i++) {
        t.shape.dims[i] = dims[i];
    }
    return t;
}

ViperType viper_type_list(TypeKind elem) {
    ViperType t = {TYPE_LIST, elem, {{0}, 0}, NULL};
    return t;
}

ViperType viper_type_struct(const char *name) {
    ViperType t = {TYPE_STRUCT, TYPE_UNKNOWN, {{0}, 0}, name ? strdup(name) : NULL};
    return t;
}

ViperType viper_type_copy(const ViperType *src) {
    ViperType t = *src;
    if (src->name) {
        t.name = strdup(src->name);
    }
    return t;
}

void viper_type_free_name(ViperType *type) {
    if (type && type->name) {
        free(type->name);
        type->name = NULL;
    }
}

bool tensor_shape_eq(const TensorShape *a, const TensorShape *b) {
    if (a->rank != b->rank) {
        return false;
    }
    for (int i = 0; i < a->rank; i++) {
        if (a->dims[i] != b->dims[i]) {
            return false;
        }
    }
    return true;
}

bool viper_type_eq(const ViperType *a, const ViperType *b) {
    if (a->kind != b->kind) {
        return false;
    }
    if (a->kind == TYPE_TENSOR) {
        if (a->elem != b->elem) {
            return false;
        }
        return tensor_shape_eq(&a->shape, &b->shape);
    }
    if (a->kind == TYPE_LIST) {
        return a->elem == b->elem;
    }
    if (a->kind == TYPE_STRUCT) {
        if (!a->name || !b->name) {
            return a->name == b->name;
        }
        return strcmp(a->name, b->name) == 0;
    }
    return true;
}

bool tensor_matmul_result(const ViperType *left, const ViperType *right, ViperType *out) {
    if (left->kind != TYPE_TENSOR || right->kind != TYPE_TENSOR) {
        return false;
    }
    if (left->elem != right->elem) {
        return false;
    }
    if (left->shape.rank != 2 || right->shape.rank != 2) {
        return false;
    }

    int m = left->shape.dims[0];
    int n_left = left->shape.dims[1];
    int n_right = right->shape.dims[0];
    int p = right->shape.dims[1];

    if (n_left != n_right) {
        return false;
    }

    int dims[2] = {m, p};
    *out = viper_type_tensor(left->elem, dims, 2);
    return true;
}

size_t tensor_num_elements(const TensorShape *shape) {
    size_t count = 1;
    for (int i = 0; i < shape->rank; i++) {
        count *= (size_t)shape->dims[i];
    }
    return count;
}

bool tensor_elementwise_result(const ViperType *left, const ViperType *right, ViperType *out) {
    if (!viper_type_eq(left, right) || left->kind != TYPE_TENSOR) {
        return false;
    }
    *out = *left;
    return true;
}

bool tensor_transpose_result(const ViperType *src, ViperType *out) {
    if (src->kind != TYPE_TENSOR || src->shape.rank != 2) {
        return false;
    }
    int dims[2] = {src->shape.dims[1], src->shape.dims[0]};
    *out = viper_type_tensor(src->elem, dims, 2);
    return true;
}

bool tensor_reshape_valid(const ViperType *src, const int *dims, int rank, ViperType *out) {
    if (src->kind != TYPE_TENSOR) {
        return false;
    }
    size_t src_count = tensor_num_elements(&src->shape);
    size_t dst_count = 1;
    for (int i = 0; i < rank; i++) {
        dst_count *= (size_t)dims[i];
    }
    if (src_count != dst_count) {
        return false;
    }
    *out = viper_type_tensor(src->elem, dims, rank);
    return true;
}

TypeKind type_kind_from_name(const char *name) {
    if (strcmp(name, "int") == 0) {
        return TYPE_INT;
    }
    if (strcmp(name, "float") == 0) {
        return TYPE_FLOAT;
    }
    if (strcmp(name, "string") == 0) {
        return TYPE_STRING;
    }
    if (strcmp(name, "bool") == 0) {
        return TYPE_BOOL;
    }
    if (strcmp(name, "void") == 0) {
        return TYPE_VOID;
    }
    return TYPE_UNKNOWN;
}

static void append_shape(char *buf, size_t len, const TensorShape *shape) {
    size_t pos = strlen(buf);
    for (int i = 0; i < shape->rank && pos < len - 1; i++) {
        int written = snprintf(buf + pos, len - pos, "%s%d", i == 0 ? "" : ", ", shape->dims[i]);
        if (written < 0) {
            break;
        }
        pos += (size_t)written;
    }
}

int viper_type_to_string(const ViperType *type, char *buf, size_t len) {
    switch (type->kind) {
    case TYPE_INT:
        return snprintf(buf, len, "int");
    case TYPE_FLOAT:
        return snprintf(buf, len, "float");
    case TYPE_STRING:
        return snprintf(buf, len, "string");
    case TYPE_BOOL:
        return snprintf(buf, len, "bool");
    case TYPE_VOID:
        return snprintf(buf, len, "void");
    case TYPE_LIST: {
        char elem[16];
        viper_type_to_string(&(ViperType){type->elem, TYPE_UNKNOWN, {{0}, 0}, NULL}, elem, sizeof(elem));
        return snprintf(buf, len, "list[%s]", elem);
    }
    case TYPE_STRUCT:
        return snprintf(buf, len, "%s", type->name ? type->name : "struct");
    case TYPE_TENSOR: {
        char elem[16];
        viper_type_to_string(&(ViperType){type->elem, TYPE_UNKNOWN, {{0}, 0}, NULL}, elem, sizeof(elem));
        int n = snprintf(buf, len, "tensor[%s, ", elem);
        if (n < 0 || (size_t)n >= len) {
            return n;
        }
        append_shape(buf, len, &type->shape);
        size_t pos = strlen(buf);
        if (pos < len - 1) {
            buf[pos++] = ']';
            buf[pos] = '\0';
        }
        return (int)pos;
    }
    default:
        return snprintf(buf, len, "unknown");
    }
}
