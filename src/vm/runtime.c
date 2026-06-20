#include "runtime.h"

#include <stdlib.h>
#include <string.h>

size_t vp_tensor_num_elements(const VpTensor *t) {
    size_t count = 1;
    for (int i = 0; i < t->type.shape.rank; i++) {
        count *= (size_t)t->type.shape.dims[i];
    }
    return count;
}

size_t vp_tensor_elem_size(const VpTensor *t) {
    return t->type.elem == TYPE_INT ? sizeof(long long) : sizeof(double);
}

VpTensor *vp_tensor_create(const ViperType *type) {
    VpTensor *t = (VpTensor *)calloc(1, sizeof(VpTensor));
    if (!t) {
        return NULL;
    }
    t->type = *type;
    size_t count = vp_tensor_num_elements(t);
    size_t esize = vp_tensor_elem_size(t);
    if (count > 0) {
        t->data = calloc(count, esize);
        if (!t->data) {
            free(t);
            return NULL;
        }
    }
    return t;
}

VpTensor *vp_tensor_from_data(const ViperType *type, const void *data) {
    VpTensor *t = vp_tensor_create(type);
    if (!t || !data) {
        return t;
    }
    memcpy(t->data, data, vp_tensor_num_elements(t) * vp_tensor_elem_size(t));
    return t;
}

VpTensor *vp_tensor_clone(const VpTensor *t) {
    if (!t) {
        return NULL;
    }
    return vp_tensor_from_data(&t->type, t->data);
}

void vp_tensor_free(VpTensor *t) {
    if (!t) {
        return;
    }
    free(t->data);
    free(t);
}

VpValue vp_value_int(long long v) {
    VpValue val = {viper_type_int(), {.i = v}};
    return val;
}

VpValue vp_value_float(double v) {
    VpValue val = {viper_type_float(), {.f = v}};
    return val;
}

VpValue vp_value_string(const char *v) {
    VpValue val = {viper_type_string(), {.str = v ? strdup(v) : NULL}};
    return val;
}

VpValue vp_value_bool(bool v) {
    VpValue val = {viper_type_bool(), {.b = v}};
    return val;
}

VpValue vp_value_tensor(VpTensor *t) {
    VpValue val = {t ? t->type : viper_type_unknown(), {.tensor = t}};
    return val;
}

VpValue vp_value_clone(const VpValue *v) {
    switch (v->type.kind) {
    case TYPE_INT:
        return vp_value_int(v->as.i);
    case TYPE_FLOAT:
        return vp_value_float(v->as.f);
    case TYPE_STRING:
        return vp_value_string(v->as.str);
    case TYPE_BOOL:
        return vp_value_bool(v->as.b);
    case TYPE_TENSOR:
        return vp_value_tensor(vp_tensor_clone(v->as.tensor));
    default:
        return (VpValue){viper_type_unknown(), {0}};
    }
}

void vp_value_free(VpValue *v) {
    if (!v) {
        return;
    }
    if (v->type.kind == TYPE_STRING) {
        free(v->as.str);
        v->as.str = NULL;
    } else if (v->type.kind == TYPE_TENSOR) {
        vp_tensor_free(v->as.tensor);
        v->as.tensor = NULL;
    }
}

static int tensor_offset(const VpTensor *t, const int *indices) {
    int offset = 0;
    int stride = 1;
    for (int d = t->type.shape.rank - 1; d >= 0; d--) {
        offset += indices[d] * stride;
        stride *= t->type.shape.dims[d];
    }
    return offset;
}

void vp_tensor_get(const VpTensor *t, const int *indices, void *out) {
    int off = tensor_offset(t, indices);
    size_t esize = vp_tensor_elem_size(t);
    memcpy(out, (char *)t->data + off * (int)esize, esize);
}

void vp_tensor_set(VpTensor *t, const int *indices, const void *val) {
    int off = tensor_offset(t, indices);
    size_t esize = vp_tensor_elem_size(t);
    memcpy((char *)t->data + off * (int)esize, val, esize);
}

VpTensor *vp_tensor_transpose(const VpTensor *t) {
    if (!t || t->type.shape.rank != 2) {
        return NULL;
    }
    int dims[2] = {t->type.shape.dims[1], t->type.shape.dims[0]};
    ViperType out_type = viper_type_tensor(t->type.elem, dims, 2);
    VpTensor *out = vp_tensor_create(&out_type);
    if (!out) {
        return NULL;
    }
    int rows = t->type.shape.dims[0];
    int cols = t->type.shape.dims[1];
    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            int src_idx[2] = {r, c};
            int dst_idx[2] = {c, r};
            if (t->type.elem == TYPE_INT) {
                long long v;
                vp_tensor_get(t, src_idx, &v);
                vp_tensor_set(out, dst_idx, &v);
            } else {
                double v;
                vp_tensor_get(t, src_idx, &v);
                vp_tensor_set(out, dst_idx, &v);
            }
        }
    }
    return out;
}

VpTensor *vp_tensor_reshape(const VpTensor *t, const int *dims, int rank) {
    if (!t) {
        return NULL;
    }
    ViperType out_type = viper_type_tensor(t->type.elem, dims, rank);
    VpTensor *out = vp_tensor_create(&out_type);
    if (!out) {
        return NULL;
    }
    memcpy(out->data, t->data, vp_tensor_num_elements(t) * vp_tensor_elem_size(t));
    return out;
}

VpTensor *vp_tensor_elementwise_binop(RtBinOp op, const VpTensor *a, const VpTensor *b) {
    if (!a || !b) {
        return NULL;
    }
    VpTensor *out = vp_tensor_clone(a);
    if (!out) {
        return NULL;
    }
    size_t count = vp_tensor_num_elements(a);
    if (a->type.elem == TYPE_INT) {
        long long *ad = (long long *)a->data;
        long long *bd = (long long *)b->data;
        long long *od = (long long *)out->data;
        for (size_t i = 0; i < count; i++) {
            switch (op) {
            case RT_OP_ADD: od[i] = ad[i] + bd[i]; break;
            case RT_OP_SUB: od[i] = ad[i] - bd[i]; break;
            case RT_OP_MUL: od[i] = ad[i] * bd[i]; break;
            case RT_OP_DIV: od[i] = bd[i] == 0 ? 0 : ad[i] / bd[i]; break;
            }
        }
    } else {
        double *ad = (double *)a->data;
        double *bd = (double *)b->data;
        double *od = (double *)out->data;
        for (size_t i = 0; i < count; i++) {
            switch (op) {
            case RT_OP_ADD: od[i] = ad[i] + bd[i]; break;
            case RT_OP_SUB: od[i] = ad[i] - bd[i]; break;
            case RT_OP_MUL: od[i] = ad[i] * bd[i]; break;
            case RT_OP_DIV: od[i] = bd[i] == 0.0 ? 0.0 : ad[i] / bd[i]; break;
            }
        }
    }
    return out;
}

VpTensor *vp_tensor_matmul(const VpTensor *a, const VpTensor *b) {
    if (!a || !b || a->type.shape.rank != 2 || b->type.shape.rank != 2) {
        return NULL;
    }
    int m = a->type.shape.dims[0];
    int n = a->type.shape.dims[1];
    int p = b->type.shape.dims[1];
    if (b->type.shape.dims[0] != n) {
        return NULL;
    }
    int dims[2] = {m, p};
    ViperType out_type = viper_type_tensor(a->type.elem, dims, 2);
    VpTensor *out = vp_tensor_create(&out_type);
    if (!out) {
        return NULL;
    }
    for (int i = 0; i < m; i++) {
        for (int j = 0; j < p; j++) {
            if (a->type.elem == TYPE_INT) {
                long long sum = 0;
                for (int k = 0; k < n; k++) {
                    long long av, bv;
                    vp_tensor_get(a, (int[]){i, k}, &av);
                    vp_tensor_get(b, (int[]){k, j}, &bv);
                    sum += av * bv;
                }
                vp_tensor_set(out, (int[]){i, j}, &sum);
            } else {
                double sum = 0.0;
                for (int k = 0; k < n; k++) {
                    double av, bv;
                    vp_tensor_get(a, (int[]){i, k}, &av);
                    vp_tensor_get(b, (int[]){k, j}, &bv);
                    sum += av * bv;
                }
                vp_tensor_set(out, (int[]){i, j}, &sum);
            }
        }
    }
    return out;
}

void vp_value_print(const VpValue *v, FILE *out) {
    switch (v->type.kind) {
    case TYPE_INT:
        fprintf(out, "%lld", v->as.i);
        break;
    case TYPE_FLOAT:
        fprintf(out, "%g", v->as.f);
        break;
    case TYPE_STRING:
        fprintf(out, "%s", v->as.str ? v->as.str : "");
        break;
    case TYPE_BOOL:
        fprintf(out, "%s", v->as.b ? "true" : "false");
        break;
    case TYPE_TENSOR: {
        const VpTensor *t = v->as.tensor;
        if (!t || t->type.shape.rank != 2) {
            fprintf(out, "<tensor>");
            break;
        }
        int rows = t->type.shape.dims[0];
        int cols = t->type.shape.dims[1];
        fputc('[', out);
        for (int r = 0; r < rows; r++) {
            if (r > 0) {
                fputs(", ", out);
            }
            fputc('[', out);
            for (int c = 0; c < cols; c++) {
                if (c > 0) {
                    fputs(", ", out);
                }
                int idx[2] = {r, c};
                if (t->type.elem == TYPE_INT) {
                    long long val;
                    vp_tensor_get(t, idx, &val);
                    fprintf(out, "%lld", val);
                } else {
                    double val;
                    vp_tensor_get(t, idx, &val);
                    fprintf(out, "%g", val);
                }
            }
            fputc(']', out);
        }
        fputc(']', out);
        break;
    }
    default:
        fprintf(out, "<unknown>");
        break;
    }
}

VpValue vp_value_cast(const VpValue *v, TypeKind target) {
    switch (target) {
    case TYPE_INT:
        if (v->type.kind == TYPE_FLOAT) {
            return vp_value_int((long long)v->as.f);
        }
        if (v->type.kind == TYPE_INT) {
            return vp_value_clone(v);
        }
        if (v->type.kind == TYPE_BOOL) {
            return vp_value_int(v->as.b ? 1 : 0);
        }
        break;
    case TYPE_FLOAT:
        if (v->type.kind == TYPE_INT) {
            return vp_value_float((double)v->as.i);
        }
        if (v->type.kind == TYPE_FLOAT) {
            return vp_value_clone(v);
        }
        if (v->type.kind == TYPE_BOOL) {
            return vp_value_float(v->as.b ? 1.0 : 0.0);
        }
        break;
    case TYPE_BOOL:
        if (v->type.kind == TYPE_INT) {
            return vp_value_bool(v->as.i != 0);
        }
        if (v->type.kind == TYPE_FLOAT) {
            return vp_value_bool(v->as.f != 0.0);
        }
        if (v->type.kind == TYPE_BOOL) {
            return vp_value_clone(v);
        }
        break;
    default:
        break;
    }
    return (VpValue){viper_type_unknown(), {0}};
}
