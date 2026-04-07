以下是根据您提供的代码生成的一份README.md文件，适合直接复制到GitHub仓库中使用。

markdown
# XMultiStreamPlayer

**XMultiStreamPlayer** 是一个基于 FFmpeg + D3D11 的多路视频流硬件加速播放器。支持同时解码并显示最多 32 路视频流，利用 D3D11 硬件解码和 GPU 渲染，实现高效的零拷贝视频合成与显示。

## 特性

- **多路并发解码**：支持最多 32 路视频流同时解码
- **硬件加速**：基于 D3D11VA 的 GPU 解码，降低 CPU 负载
- **零拷贝渲染**：解码后的 GPU 纹理直接用于合成显示，无需 CPU 拷贝
- **可配置布局**：支持动态调整网格布局（行 × 列）
- **线程安全队列**：每个流独立的数据队列，避免互锁
- **模块化设计**：解封装、解码、队列、渲染各模块解耦，易于扩展
- **事件回调**：提供流状态、错误等回调接口

## 系统架构
[文件源] → [解封装] → [Packet队列] → [硬件解码] → [Frame队列] → [合成渲染] → [显示]
↑ ↓
轮询管理 D3D11纹理

text

## 环境要求

- Windows 10/11
- Visual Studio 2019 或更高版本
- NVIDIA/AMD/Intel 显卡（支持 D3D11VA）
- FFmpeg 开发库（已编译为 lib）

## 依赖库

| 库 | 用途 |
|---|---|
| FFmpeg (libavformat, libavcodec, libavutil) | 解封装、解码 |
| D3D11 / DXGI | 硬件解码与渲染 |
| Qt5 | UI 界面 |
| SDL2 | 像素格式转换（可选） |

## 快速开始

### 1. 克隆仓库

```bash
git clone https://github.com/yourname/XMultiStreamPlayer.git
cd XMultiStreamPlayer
2. 配置 FFmpeg
将 FFmpeg 的头文件和库文件路径配置到 Visual Studio 项目中。

3. 编译项目
使用 Visual Studio 打开 XMultiStreamPlayer.sln，选择 Release x64 配置，编译生成。

4. 准备测试视频
在程序运行目录下创建 camera_videos/ 文件夹，放入以下命名的测试视频：

text
camera_videos/
  test_video_01.mp4
  test_video_02.mp4
  ...
  test_video_16.mp4
5. 运行程序
启动程序后，点击 MultiPlay 按钮即可开始播放 16 路视频。

使用方法
添加视频流
cpp
play_engine_->AddStream(stream_id, "path/to/video.mp4");
启动所有流
cpp
play_engine_->StartAllStreams();
启动渲染
cpp
play_engine_->StartRendering();
控制单个流
cpp
play_engine_->StartStream(stream_id);
play_engine_->PauseStream(stream_id);
play_engine_->ResumeStream(stream_id);
play_engine_->StopStream(stream_id);
全局控制
cpp
play_engine_->SetGlobalPause(true);  // 全局暂停
play_engine_->StartRendering();      // 开始渲染
play_engine_->StopRendering();       // 停止渲染
配置说明
播放引擎配置 (PlayEngineConfig)
参数	默认值	说明
max_streams	32	最大流数量
use_hardware_decoder	true	是否启用硬件解码
auto_start	false	自动启动流
render_rows	4	渲染网格行数
render_cols	8	渲染网格列数
render_target_width	800	渲染窗口宽度
render_target_height	600	渲染窗口高度
render_fps	30	渲染帧率
核心模块
模块	文件	功能
解封装管理器	xmulti_demuxer_manager	管理多个文件的读取与包分发
队列管理器	xmulti_queue_manager	管理每个流的 Packet/Frame 队列
解码管理器	xmulti_decoder_manager	管理多个硬件解码器实例
渲染管理器	xrender_manager	合成并显示多路纹理
D3D11 上下文	d3d11_hardware_context	封装 D3D11 设备、着色器、渲染逻辑
图像合成器	ximage_compositor	管理纹理到网格的映射
渲染流程
渲染线程以固定帧率运行

从每个流的 Frame 队列中取出最新帧

更新合成器中的纹理映射

调用 D3D11 渲染器，一次性提交所有纹理

像素着色器根据网格布局采样并合成最终图像

交换链显示

性能优化
零拷贝：解码输出的 AV_PIX_FMT_D3D11 帧直接作为 GPU 纹理使用

单次 Draw Call：一次绘制完成所有网格的合成

纹理数组：Shader 中使用纹理数组 + 常量缓冲区选择当前采样的纹理

有效性掩码：跳过无效单元格，避免采样空纹理

已知限制
仅支持 Windows + D3D11 环境

所有视频流需使用相同的编码格式（如 H.264）

输入视频分辨率建议一致，否则显示会被拉伸

暂不支持音频

后续计划
支持动态增删流

支持不同分辨率的视频流

添加音频同步播放

支持网络流（RTMP、RTSP）

添加统计面板（FPS、队列长度等）

许可证
本项目基于 Apache License 2.0 开源协议，详见 LICENSE 文件。

作者
Your ltsf1984

GitHub: ltsf1984

致谢
FFmpeg

Microsoft DirectX

Qt

