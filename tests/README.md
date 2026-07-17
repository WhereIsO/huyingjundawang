# 文件备份软件自动化测试

本目录包含面向 `backend/pbackup_core.exe` 和 Python GUI 的自动化测试套件。当前测试脚本定义 54 个独立用例，主要覆盖：

- 后端能力查询和 `PBACKUP-CPP3` 格式；
- Stored、MSZIP、XPRESS、XPRESS Huffman、LZMS 五种压缩；
- 不加密、AES-256-GCM、ChaCha20-Poly1305 三种加密状态；
- 单文件和完整文件夹备份恢复；
- 中文路径、中文密码、包含/排除/通配符/类型筛选；
- 恢复过程中的 AEAD 认证、归档 SHA-256 和逐文件 SHA-256 自动校验；
- 错误密码、损坏包、覆盖冲突、非法算法和非法 KDF 参数；
- GUI 三页面、ChaCha20-Poly1305 下拉选项和自定义背景控件。

运行方式：

```powershell
powershell -ExecutionPolicy Bypass -File .\tests\run_tests.ps1
```

执行后生成：

- `test_cases.csv`：测试用例清单；
- `results/latest.log`：适合直接阅读的结果日志；
- `results/latest.csv`：表格化测试结果；
- `results/latest.json`：JSON 测试结果；
- `results/backend-events.log`：C++ 后端原始 JSON Lines 事件；
- `results/summary.md`：Markdown 测试报告摘要；
- `work/`：自动生成的测试夹具、备份包和恢复结果，可删除并重新生成。

脚本退出码为 0 表示全部通过，退出码为 1 表示至少一个用例失败。
