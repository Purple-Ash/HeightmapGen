import onnx
from onnx import helper, TensorProto

graph = helper.make_graph(
    [helper.make_node("Identity", ["input"], ["output"])], "test",
    [helper.make_tensor_value_info("input", TensorProto.FLOAT, [1, 1, 3, 3])],
    [helper.make_tensor_value_info("output", TensorProto.FLOAT, [1, 1, 3, 3])],
)
model = helper.make_model(graph, opset_imports=[helper.make_opsetid("", 13)], ir_version=8)
onnx.checker.check_model(model)
onnx.save(model, "identity.onnx")
