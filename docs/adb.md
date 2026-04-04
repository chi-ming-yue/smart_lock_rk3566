# ADB

所有板级测试统一使用 `ADB` 变量，不在命令里写死 `adb`。

## 默认

```bash
export ADB=adb
```

## WSL 调 Windows adb.exe

```bash
export ADB="/mnt/c/Users/<user>/AppData/Local/Android/Sdk/platform-tools/adb.exe"
```

## 常用部署

```bash
ADB=${ADB:-adb}
app/test/adb/board.sh deploy
```

默认板端目录：

- `/userdata/smart_lock`

## 常用板级命令

```bash
ADB=${ADB:-adb}
app/test/adb/board.sh auto
app/test/adb/board.sh sensor
app/test/adb/board.sh key
app/test/adb/board.sh ctl-keypad
app/test/adb/board.sh ctl-face
```
