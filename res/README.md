# 资源目录

默认运行资源统一放在 `res/`。

- `model/det.rknn`：检测模型
- `model/rec_fp32.rknn`：默认识别模型
- `model/rec_i8.rknn`：保底识别模型
- `model/rec.rknn`：当前选中的识别模型副本
- `db/face.db`：默认人脸库
- `sql/face.sql`：数据库 SQL 导出，可重建 `db/face.db`
- `face/zw`：邹威样本图片
- `face/hjj`：黄俊杰样本图片

初始化资源并生成板端部署目录：

```bash
./scripts/init_res.sh
```

切到 `i8`：

```bash
REC_MODE=i8 ./scripts/init_res.sh
```

生成结果在 `out/board/`：

- `out/board/model/det.rknn`
- `out/board/model/rec_fp32.rknn`
- `out/board/model/rec_i8.rknn`
- `out/board/model/rec.rknn`
- `out/board/db/face.db`
- `out/board/debug/`
