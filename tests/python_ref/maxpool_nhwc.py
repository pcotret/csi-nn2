#!/usr/bin/python
#-*- coding:utf-8 -*-

import sys
import struct
import numpy as np
from torch import tensor
from torch.nn import functional as fn

def getpackn(test_dtype, test_vlen):
    if int(test_dtype) == 8:
        return int(test_vlen)/int(test_dtype)/2
    else:
        return int(test_vlen)/int(test_dtype)

def maxpool2d_f32(test_dtype, test_vlen, test_type):
    para = []
    # init the input data and parameters
    batch      = int(np.random.randint(1, high=2, size=1))
    channel    = int(np.random.randint(2, high=6, size=1))
    in_height = int(np.random.randint(16, high=32, size=1))
    in_width  = int(np.random.randint(16, high=32, size=1))
    stride_h   = int(np.random.randint(1, high=4, size=1))
    stride_w   = int(np.random.randint(1, high=4, size=1))
    kernel_h   = int(np.random.randint(stride_h, high=9, size=1))
    kernel_w   = int(np.random.randint(stride_w, high=9, size=1))
    pad_left   = int(np.random.randint(0, high=2, size=1))
    pad_right   = int(np.random.randint(0, high=2, size=1))
    pad_top   = int(np.random.randint(0, high=2, size=1))
    pad_down    = int(np.random.randint(0, high=2, size=1))
    c_model = False

    packn = int(getpackn(test_dtype, test_vlen))
    n = int(np.random.randint(1, high=2, size=1))

    if  "2x2s2" in test_type and test_type[-2] != "p":
        stride_h    =  stride_w    = 2
        kernel_h    =  kernel_w    = 2
        pad_left  = pad_top = 0
        pad_down  = pad_right = 1
        in_height = 2 * in_height + 1
        in_width = 2 * in_width + 1
        if test_type == "packn_2x2s2":
            channel    = int(n*packn)
        elif test_type == "pack1_2x2s2":
            channel    = int(n*packn) + 1

    elif "2x2s2p0" in test_type:
        stride_h    =  stride_w   = 2
        kernel_h    =  kernel_w   = 2
        pad_left  = pad_top = 0
        pad_down  = pad_right = 0
        in_height = 2 * in_height + 1
        in_width = 2 * in_width + 1
        c_model = False
        if test_type == "packn_2x2s2p0":
            channel    = int(n*packn)
        elif test_type == "pack1_2x2s2p0":
            channel    = int(n*packn) + 1
        elif test_type == "packn_2x2s2p0c1":
            c_model = True


    elif "2x2s2p1" in test_type:
        stride_h    =  stride_w   = 2
        kernel_h    =  kernel_w   = 2
        pad_left  = pad_top = 1
        pad_down  = pad_right = 1
        in_height = 2 * in_height
        in_width = 2 * in_width
        if test_type == "packn_2x2s2p1":
            channel    = int(n*packn)
        elif test_type == "pack1_2x2s2p1":
            channel    = int(n*packn) + 1


    elif "3x3s2" in test_type and test_type[-2] != "p":
        stride_h    =  stride_w    = 2
        kernel_h    =  kernel_w    = 3
        pad_left  = pad_top = 0
        pad_down  = pad_right = 1
        in_height = 2 * in_height
        in_width = 2 * in_width
        if test_type == "packn_3x3s2":
            channel    = int(n*packn)
        elif test_type == "pack1_3x3s2":
            channel    = int(n*packn) + 1

    elif "3x3s2p0" in test_type:
        stride_h    =  stride_w    = 2
        kernel_h    =  kernel_w    = 3
        pad_left  = pad_top = 0
        pad_down  = pad_right = 0
        in_height = 2 * in_height
        in_width = 2 * in_width
        c_model = False
        if test_type == "packn_3x3s2p0":
            channel    = int(n*packn)
        elif test_type == "pack1_3x3s2p0":
            channel    = int(n*packn) + 1
        elif test_type == "pack1_3x3s2p0c1":
            c_model = True

    elif "3x3s2p1" in test_type:
        stride_h    =  stride_w    = 2
        kernel_h    =  kernel_w     = 3
        pad_left  = pad_top = 1
        pad_down  = pad_right = 1
        in_height = 2 * in_height + 1
        in_width = 2 * in_width + 1
        if test_type == "packn_3x3s2p1":
            channel    = int(n*packn)
        elif test_type == "pack1_3x3s2p1":
            channel    = int(n*packn) + 1

    elif "3x3s1_p1" in test_type:
        stride_h    =  stride_w     = 1
        kernel_h    =  kernel_w     = 3
        pad_left = pad_right = pad_top = pad_down = 1
        if test_type == "packn_3x3s1_p1":
            channel    = int(n*packn)
        elif test_type == "pack1_3x3s1_p1":
            channel    = int(n*packn) + 1

    elif "s3k5" in test_type:
        stride_h    =  stride_w     = 3
        kernel_h    =  kernel_w     = 5
        pad_left = pad_right = pad_top = pad_down = 0
        if test_type == "packn_s3k5":
            channel    = int(n*packn)
            print(channel)
        elif test_type == "pack1_s3k5":
            channel    = int(n*packn) + 1


    elif "global" in test_type:
        if test_type == "packn_global":
            channel    = int(n*packn)
        elif test_type == "global":
            channel    = int(n*packn) + 1
        in_height = kernel_h
        in_width  = kernel_w
        pad_left = pad_right = pad_top = pad_down = 0

    src_in = np.random.uniform(1, 10, (batch, channel, in_height, in_width))

    t_src_in  = tensor(src_in)
    t_src_in1  = fn.pad(t_src_in, (pad_left, pad_right, pad_top, pad_down), 'constant', 0)

    t_src_out = fn.max_pool2d(t_src_in1, kernel_size=(kernel_h, kernel_w), stride=(stride_h, stride_w), ceil_mode=c_model).numpy()

    #permute nchw to nhwc
    src_in_nhwc = np.transpose(src_in, [0, 2, 3, 1])
    out_nhwc    = np.transpose(t_src_out, [0, 2, 3, 1])

    out_height = np.shape(out_nhwc)[1]
    out_width  = np.shape(out_nhwc)[2]

    c_model = 1 if c_model else 0
    src_in_1  = src_in_nhwc.flatten()
    src_out_1 = out_nhwc.flatten()

    total_size = (len(src_in_1) + len(src_out_1)) + 15

    para.append(total_size)
    para.append(batch)
    para.append(in_height)
    para.append(in_width)
    para.append(channel)
    para.append(stride_h)
    para.append(stride_w)
    para.append(kernel_h)
    para.append(kernel_w)
    para.append(pad_left)
    para.append(pad_right)
    para.append(pad_top)
    para.append(pad_down)
    para.append(out_height)
    para.append(out_width)
    para.append(c_model)
    print(para)
    print(len(src_out_1))

    with open("maxpool_nhwc_data_f32.bin", "wb") as fp:
        data = struct.pack(('%di' % len(para)), *para)
        fp.write(data)
        data = struct.pack(('%df' % len(src_in_1)), *src_in_1)
        fp.write(data)
        data = struct.pack(('%df' % len(src_out_1)), *src_out_1)
        fp.write(data)
        fp.close()

    return 0


if __name__ == '__main__':
    test_dtype = sys.argv[1]
    test_vlen = sys.argv[2]
    test_type = sys.argv[3]
    maxpool2d_f32(test_dtype, test_vlen, test_type)
    print("end")
