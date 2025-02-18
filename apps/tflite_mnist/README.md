# MNIST Example

## Description

The *MNIST* database (Modified National Institute of Standards and Technology database) is a large collection of handwritten digits. In this tutorial, MNIST database is used to show a full process from training a model to deploying it and run inference on Ameba SoCs with tensorflow-lite.

## Supported IC
1. AmebaSmart CA32

## Tutorial

Step 1-4  are for preparing necessary files on a development machine (server or PC etc.). You can skip them and use prepared files to build the image.

### Step 1. Train a Model

First train and evaluate a classification model for 10 digits of MNIST dataset. You can choose either keras(tensorflow) or pytorch framework by running `python keras_train_eval.py --output keras_mnist_conv` or `python torch_train_eval.py --output torch_mnist_conv`.

A simple convolution based model will be trained for several epochs and then accuracy will be tested.

Use *keras_flops* library under tensorflow/keras framework:

```
from keras_flops import get_flops

model.summary()
flops = get_flops(model, batch_size=1)
```

Use *ptflops* library under pytorch framework:

```
from ptflops import get_model_complexity_info

macs, params = get_model_complexity_info(model, (1,28,28), as_strings=False)
```

After training, keras model is saved in SavedModel format. Pytorch model is saved in .pt format, while a .onnx file is also exported for later conversion stage.

### Step 2. Convert to Tflite

In this stage, **post-training integer quantization** is applied on the trained model and output .tflite format. Float model inference is also supported on Ameba SoCs, however, we recommend using integer quantization which can extremely reduce computation and memory with little performance degradation.

For models trained by keras(tensorflow), run `python convert.py --input-path keras_mnist_conv/saved_model --output-path keras_mnist_conv`.

For models trained by pytorch, run `python convert.py --input-path torch_mnist_conv/model.onnx --output-path torch_mnist_conv`. An additional step will run to convert from .onnx to SavedModel format. 

tf.lite.TFLiteConverter is used to convert SavedModel into int8 .tflite given a representative dataset:

```
converter = tf.lite.TFLiteConverter.from_saved_model(saved_model_dir)
converter.optimizations = [tf.lite.Optimize.DEFAULT]
converter.representative_dataset = repr_dataset
converter.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS_INT8]
converter.inference_input_type = tf.int8
converter.inference_output_type = tf.int8
tflite_int8_model = converter.convert()
```

Refer to https://ai.google.dev/edge/litert/models/post_training_integer_quant#convert_using_integer-only_quantization for more details.

After conversion, the performance on test set will be validated using int8 .tflite model and two .npy files containing input array and label array of 100 test images are generated for later use on SoC.

In *convert.py*, onnx_tf library (https://github.com/onnx/onnx-tensorflow) is used for converting from onnx to SavedModel. Other convert libraries are available with similar purpose:

onnx2tf https://github.com/PINTO0309/onnx2tf

ai-edge-torch https://github.com/google-ai-edge/ai-edge-torch

nobuco https://github.com/AlexanderLutsenko/nobuco

onnx2tflite https://github.com/MPolaris/onnx2tflite

### Step 3. Convert Model to C++

Convert .tflite model and .npy test data to .cc and .h files for deployment:

```
python generate_cc_arrays.py models keras_mnist_conv.tflite
python generate_cc_arrays.py testdata input_int8.npy input_int8.npy label_int8.npy label_int8.npy
```

### Step 4. Inference on SoC with Tflite

*example_tflite_mnist.cc* shows how to run inference with the trained model on test data, calculate accuracy, profile memory and latency. 

Use https://netron.app/ to visualize the .tflite file and check the operations used by the model. Load model and create an interpreter using the following methods. 

```
    // Load model
    std::unique_ptr<tflite::FlatBufferModel> model =
        tflite::FlatBufferModel::BuildFromBuffer(
            (const char *)g_keras_mnist_conv_model_data,
            g_keras_mnist_conv_model_data_size);

    // Build the interpreter with the InterpreterBuilder.
    tflite::ops::builtin::BuiltinOpResolver resolver;
    tflite::InterpreterBuilder builder(*model, resolver);
    std::unique_ptr<tflite::Interpreter> interpreter;
    builder(&interpreter);
```
You can also build the model based on a file, refer to https://github.com/tensorflow/tensorflow/blob/master/tensorflow/lite/g3doc/guide/inference.md#load-and-run-a-model-in-c for more details about running inference with tensorflow-lite.

### Step 5. Build Example

1. There are dependencies between libraries, refer to rtk-tflite-mnist_1.0.bb.
   bb file is pgos/sources/yocto/meta-realtek/meta-sdk/recipes-rtk/tflite-mnist/rtk-tflite-mnist_1.0.bb.

2. use "bitbake rtk-tflite-mnist" to compile the tflite example module,
   the generated executable is rtk_tflite_mnist.

# Expected Result
* rtk_tflite_mnist
[TFLITE-MNIST] Post-invoke Interpreter State
Interpreter has 1 subgraphs.

-----------Subgraph-0 has 31 tensors and 7 nodes------------
1 Inputs: [0] -> 784B (0.00MB)
1 Outputs: [16] -> 10B (0.00MB)

Tensor  ID Name                      Type            AllocType          Size (Bytes/MB)    Shape      MemAddr-Offset  
Tensor   0 serving_default_input_1:0 kTfLiteInt8     kTfLiteArenaRw     784      / 0.00 [1,28,28,1] [0, 784)
Tensor   1 sequential/flatten/Const  kTfLiteInt32    kTfLiteMmapRo      8        / 0.00 [2] [57184, 57192)
Tensor   2 sequential/dense_1/Bia... kTfLiteInt32    kTfLiteMmapRo      40       / 0.00 [10] [57124, 57164)
Tensor   3 sequential/dense_1/MatMul kTfLiteInt8     kTfLiteMmapRo      640      / 0.00 [10,64] [56472, 57112)
Tensor   4 sequential/dense/BiasA... kTfLiteInt32    kTfLiteMmapRo      256      / 0.00 [64] [56204, 56460)
Tensor   5 sequential/dense/MatMul   kTfLiteInt8     kTfLiteMmapRo      51200    / 0.05 [64,800] [4992, 56192)
Tensor   6 sequential/conv2d_1/Bi... kTfLiteInt32    kTfLiteMmapRo      128      / 0.00 [32] [4852, 4980)
Tensor   7 sequential/conv2d_1/Co... kTfLiteInt8     kTfLiteMmapRo      4608     / 0.00 [32,3,3,16] [232, 4840)
Tensor   8 sequential/conv2d/Bias... kTfLiteInt32    kTfLiteMmapRo      64       / 0.00 [16] [156, 220)
Tensor   9 sequential/conv2d/Conv2D  kTfLiteInt8     kTfLiteMmapRo      144      / 0.00 [16,3,3,1] [0, 144)
Tensor  10 sequential/conv2d/Relu... kTfLiteInt8     kTfLiteArenaRw     10816    / 0.01 [1,26,26,16] [832, 11648)
Tensor  11 sequential/max_pooling... kTfLiteInt8     kTfLiteArenaRw     2704     / 0.00 [1,13,13,16] [22208, 24912)
Tensor  12 sequential/conv2d_1/Re... kTfLiteInt8     kTfLiteArenaRw     3872     / 0.00 [1,11,11,32] [18304, 22176)
Tensor  13 sequential/max_pooling... kTfLiteInt8     kTfLiteArenaRw     800      / 0.00 [1,5,5,32] [832, 1632)
Tensor  14 sequential/flatten/Res... kTfLiteInt8     kTfLiteArenaRw     800      / 0.00 [1,800] [1664, 2464)
Tensor  15 sequential/dense/MatMu... kTfLiteInt8     kTfLiteArenaRw     64       / 0.00 [1,64] [832, 896)
Tensor  16 StatefulPartitionedCall:0 kTfLiteInt8     kTfLiteArenaRw     10       / 0.00 [1,10] [896, 906)
Tensor  17 (nil)                     kTfLiteNoType   kTfLiteMemNone     0        / 0.00 (null) [-1, -1)
Tensor  18 (nil)                     kTfLiteNoType   kTfLiteMemNone     0        / 0.00 (null) [-1, -1)
Tensor  19 (nil)                     kTfLiteNoType   kTfLiteMemNone     0        / 0.00 (null) [-1, -1)
Tensor  20 (nil)                     kTfLiteNoType   kTfLiteMemNone     0        / 0.00 (null) [-1, -1)
Tensor  21 (nil)                     kTfLiteNoType   kTfLiteMemNone     0        / 0.00 (null) [-1, -1)
Tensor  22 (nil)                     kTfLiteNoType   kTfLiteMemNone     0        / 0.00 (null) [-1, -1)
Tensor  23 (nil)                     kTfLiteNoType   kTfLiteMemNone     0        / 0.00 (null) [-1, -1)
Tensor  24 (nil)                     kTfLiteNoType   kTfLiteMemNone     0        / 0.00 (null) [-1, -1)
Tensor  25 (nil)                     kTfLiteNoType   kTfLiteMemNone     0        / 0.00 (null) [-1, -1)
Tensor  26 (nil)                     kTfLiteNoType   kTfLiteMemNone     0        / 0.00 (null) [-1, -1)
Tensor  27 (nil)                     kTfLiteNoType   kTfLiteMemNone     0        / 0.00 (null) [-1, -1)
Tensor  28 (nil)                     kTfLiteNoType   kTfLiteMemNone     0        / 0.00 (null) [-1, -1)
Tensor  29 (nil)                     kTfLiteInt8     kTfLiteArenaRw     6084     / 0.01 [1,26,26,9] [11648, 17732)
Tensor  30 (nil)                     kTfLiteInt8     kTfLiteArenaRw     17424    / 0.02 [1,11,11,144] [832, 18256)

kTfLiteArenaRw Info: 
Tensor 30 has the max size 17424 bytes (0.017 MB).
This memory arena is estimated as[0x44dc50, 0x447b00), taking 24912 bytes (0.024 MB).
One possible set of tensors that have non-overlapping memory spaces with each other, and they take up the whole arena:
Tensor 0 -> 30 -> 12 -> 11.

kTfLiteArenaRwPersistent Info: not holding any allocation.

kTfLiteMmapRo Info: 
Tensor 5 has the max size 51200 bytes (0.049 MB).

utput Tensors:[14] -> 800B (0.00MB)
Node   5 Operator Builtin Code   9 FULLY_CONNECTED (not delegated)
  3 Input Tensors:[14,5,4] -> 52256B (0.05MB)
  1 Output Tensors:[15] -> 64B (0.00MB)
Node   6 Operator Builtin Code   9 FULLY_CONNECTED (not delegated)
  3 Input Tensors:[15,3,2] -> 744B (0.00MB)
  1 Output Tensors:[16] -> 10B (0.00MB)

Execution plan as the list of 7 nodes invoked in-order: [0-6]
--------------Subgraph-0 dump has completed--------------

--------------Memory Arena Status Start--------------
Total memory usage: 25168 bytes (0.024 MB)

Subgraph#0   Arena (Normal)          25040 (99.49%)
Subgraph#0   Arena (Persistent)        128 (0.51%)
--------------Memory Arena Status End--------------



[TFLITE-MNIST] Accuracy: 100/100
[TFLITE-MNIST] Total Time: 101.000000 ms

[TFLITE-MNIST] Test Completed!