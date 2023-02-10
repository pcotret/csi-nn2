/*
 * Copyright (C) 2016-2023 T-Head Semiconductor Co., Ltd. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the License); you may
 * not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an AS IS BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "shl_c908.h"

/*************************************************************************************
 * reorder kernel_data inplace, means the origin kernel_data be destoried.
 * The reason to do this is that the packaging process must not consume more memory.
 **************************************************************************************/
void shl_c908_conv_im2col_gemm_reorder_kernel_pack1ton_fp16(struct csinn_tensor *kernel,
                                                            struct csinn_conv2d_params *params)
{
    shl_rvv_conv_im2col_gemm_reorder_kernel_pack1ton_fp16(kernel, params);
}

int shl_c908_conv_im2col_gemm_pack1ton_fp16(struct csinn_tensor *input, struct csinn_tensor *output,
                                            struct csinn_tensor *kernel, struct csinn_tensor *bias,
                                            struct csinn_conv2d_params *params)
{
    if (input->layout == CSINN_LAYOUT_NC1HWC0) {
        shl_rvv_tensor_nc1xc0_to_ndarray_replace_fp16(input);
    }
    if (output->layout == CSINN_LAYOUT_NCHW) {
        const int packn = csrr_vlenb() / sizeof(__fp16);
        output->dim[1] /= packn;
        output->dim[4] = packn;
        output->dim_count = 5;
        output->layout = CSINN_LAYOUT_NC1HWC0;
    }
    __fp16 *input_data = (__fp16 *)input->data;
    __fp16 *output_data = (__fp16 *)output->data;
    __fp16 *kernel_data = (__fp16 *)kernel->data;
    __fp16 *bias_data = (__fp16 *)bias->data;

    int32_t group = params->group;
    int32_t batch = input->dim[0];
    int32_t in_c = input->dim[1];
    int32_t in_h = input->dim[2];
    int32_t in_w = input->dim[3];
    int32_t out_c = kernel->dim[0];
    int32_t out_h = output->dim[2];
    int32_t out_w = output->dim[3];

    int32_t ksize_h = kernel->dim[2];
    int32_t ksize_w = kernel->dim[3];
    int32_t stride_h = params->stride_height;
    int32_t stride_w = params->stride_width;
    int32_t dilation_h = params->dilation_height;
    int32_t dilation_w = params->dilation_width;

    int32_t m = out_c / group;
    int32_t in_cp = in_c / group;
    int32_t maxk = ksize_h * ksize_w;
    int32_t n = out_h * out_w;

    for (int i = 0; i < batch; i++) {
        for (int g = 0; g < group; g++) {
            // padding
            int padded_in_h = in_h + params->pad_top + params->pad_down;
            int padded_in_w = in_w + params->pad_left + params->pad_right;
            int padded_in_hw = padded_in_h * padded_in_w;
            __fp16 *input_pad_buf = (__fp16 *)shl_mem_alloc(in_cp * padded_in_hw * sizeof(__fp16));
            shl_rvv_pad_input_pack1ton_fp16(input_data, input_pad_buf, in_cp, in_h, in_w,
                                            padded_in_h, padded_in_w, params->pad_top,
                                            params->pad_left);

            // im2col
            const int packn = csrr_vlenb() / sizeof(__fp16);
            int vl = vsetvl_e16m1(packn);

            // [in_c/packn, maxk, out_h, out_w, packn] + [maxk, out_h, out_w, in_c%packn]
            __fp16 *im2col_buf = (__fp16 *)shl_mem_alloc(in_cp * maxk * n * sizeof(__fp16));
            const int tailstep = (padded_in_w * stride_h - out_w * stride_w);

            const __fp16 *img0 = input_pad_buf;
            __fp16 *dst_ptr = im2col_buf;

            int loop_c = in_cp;
            while (loop_c > 0) {
                vl = vsetvl_e16m1(loop_c);

                for (int a = 0; a < ksize_h; a++) {
                    for (int b = 0; b < ksize_w; b++) {
                        const __fp16 *img1 =
                            img0 + a * dilation_h * padded_in_w * vl + b * dilation_w * vl;

                        for (int p = 0; p < out_h; p++) {
                            for (int q = 0; q < out_w; q++) {
                                vfloat16m1_t _tmp = vle16_v_f16m1(img1, vl);
                                img1 += stride_w * vl;
                                vse16_v_f16m1(dst_ptr, _tmp, vl);
                                dst_ptr += vl;
                            }
                            img1 += tailstep * vl;
                        }
                    }
                }
                img0 += padded_in_hw * vl;
                // dst_ptr += maxk * out_h * out_w * vl;
                loop_c -= vl;
            }
            shl_mem_free(input_pad_buf);

            // reorder(pack)
            __fp16 *reorder_buf = (__fp16 *)shl_mem_alloc(in_cp * maxk * n * sizeof(__fp16));
            shl_rvv_reorder_input_z12_pack1ton_fp16(im2col_buf, reorder_buf, in_cp, maxk, n, n);
            shl_mem_free(im2col_buf);

            // gemm
            __fp16 *ker_ptr = kernel_data + g * m * maxk * in_cp;
            __fp16 *bias_ptr = bias_data ? (bias_data + g * m) : NULL;
            // shl_rvv_ncxhwx_gemm_12xpack2n_fp16(output_data, ker_ptr, reorder_buf, bias_ptr, m,
            //                                    in_cp * maxk, n, n);
            shl_c908_ncxhwx_gemm_12xpack2n_fp16(output_data, ker_ptr, reorder_buf, bias_ptr, m,
                                                in_cp * maxk, n, false);
            shl_mem_free(reorder_buf);

            input_data += in_cp * in_h * in_w;
            output_data += m * n;
        }
    }
    // requantize
    shl_rvv_sidcso_op_requantize_fp16(input, output, kernel);
    return CSINN_TRUE;
}
