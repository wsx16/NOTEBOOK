import onnxruntime as ort
import numpy as np
from ultralytics import YOLO


session = ort.InferenceSession("yolo11n.onnx")
img = np.random.randn(1, 3, 320, 320).astype(np.float32)
onnx_out = session.run(None, {"images": img})[0]

# PyTorch 输出
import torch

pt_model = YOLO("yolo11n.pt").model
pt_out = pt_model(torch.from_numpy(img))[0].detach().numpy()

diff = np.abs(onnx_out - pt_out).max()
print(f"最大误差: {diff}")  # 应 < 1e-5
